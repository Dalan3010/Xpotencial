# 04 — Software architecture

## 1. Modules and ownership

| Module | Owns | Depends on | Evidence |
|---|---|---|---|
| `dipole/physics.hpp`, `src/physics.cpp` | All physical/numerical data types and algorithms; pure `double` computation; validation | C++ standard library only | `physics.hpp:1-93`; `physics.cpp:12-15` |
| `src/controls.*` | Authoritative `Parameters`, `Visibility`, current view, help state, probe point, editing/scroll state | physics, raylib | `controls.hpp:13-34`; `controls.hpp:2-4` |
| `src/renderer.*` | A copy of `Parameters`, every derived cache, GPU handles, per-camera orbit state | `Visibility` from controls, raylib/raymath/rlgl | `renderer.hpp:19-34`; `renderer.cpp:1-9` |
| `src/ui_text.*` | Font atlas and `DrawTextUi`/`MeasureTextUi` | raylib | `ui_text.hpp:3-9` |
| `src/main.cpp` | Window/lifecycle, CLI parsing, frame loop, dirty flag | all of the above | `main.cpp:9-73` |
| `tests/core_tests.cpp` | Behavioural checks | **only** `dipole_core` | `cpp/CMakeLists.txt:16-20` |

Dependency direction is acyclic:

```
physics  ←  controls  ←  main
   ↑            ↑          ↑
   └──── renderer ─────────┘
          ui_text
```

The GUI never reaches into the core's internals: everything crosses the public
API declared in `physics.hpp`. That is what makes the core testable without a
display, and it is the property to highlight when describing the architecture.

## 2. State

`Parameters` is a flat struct of `double`s plus one `int` (`chargeCount`), with
defaults compiled in (`physics.hpp:13-26`):

| Field | Default | Range enforced |
|---|---|---|
| `q` | 1 | 0.1..5 |
| `k` | 8.99 | 0.5..20 |
| `d` | 2 | 0.2..6 |
| `azimuth` / `polar` | 0 / 90 | 0..360 / 0..180 |
| `chargeCount` | 2 | 1 or 2 |
| `center` | (0,0,0) | each component ±10 000 |
| domain `xmin..ymax` | ±5 | spans ≥ 0.001; coords ±10 000 |
| `resolution` | 70 | 15..150 |
| `fieldDensity` | 14 | 4..30 |
| `sphereRadius` / `sphereResolution` | 3 / 45 | 0.5..10 / 10..100 |
| `potentialLevel` / `volumeHalfExtent` / `volumeResolution` | 4.5 / 3 / 37 | ±1e6 / 0.1..100 / 10..60 |
| `samplingRadius` / `visualRadius` / `fieldContrast` / `angularResolution` | 2.5 / 2 / 0.45 / 40 | 0.5..20 / 0.1..10 / 0..0.9 / 10..80 |

Validation lives in one function, `validate` (`physics.cpp:32-57`), and every
public entry calls `require` before doing work (`physics.cpp:12-15`).

`Visibility` is a separate struct so that display toggles never invalidate
numerical caches (`controls.hpp:7-11`).

## 3. Frame loop

```
for each frame:
    if dirty:  renderer.update(parameters)      // recompute caches
    BeginDrawing(); ClearBackground()
    renderer.draw(viewport, view, visibility, controls.blocksCamera())
    dirty = controls.draw(renderer.plotRectangle())   // input → parameters
    update window title on source change
    EndDrawing(); ++frame
```

Evidence: `cpp/src/main.cpp:41-62`.

**Consequence that matters for describing interaction latency:** a parameter
edited inside `controls.draw` is not incorporated until `renderer.update` runs
on the *next* frame. The same applies to a view-tab click. At the 60 FPS target
(`main.cpp:18-24`) this is a one-frame latency of about 16 ms, invisible to the
user but real; it is the reason a single-frame screenshot after an edit may show
the previous state.

Window configuration: 1200×700, resizable, minimum 1000×640, MSAA 4×,
Esc disabled as a close key so it can cancel text editing
(`main.cpp:18-24`).

CLI surface is three cases: `--smoke`, `--help`, and an unknown argument that
exits with code 2 (`main.cpp:10-17`).

## 4. Cache invalidation

Two predicates gate all rebuilding (`renderer.cpp:110-114`):

```
physicsChanged = !initialized
              || q, k, d, chargeCount, azimuth, polar, center.x/y/z changed
planeChanged   = physicsChanged
              || xmin, xmax, ymin, ymax, resolution, fieldDensity changed
```

| Change | Rebuilds | Does not touch |
|---|---|---|
| `q`, `k`, `d`, orientation, `center`, `chargeCount` | streamlines, phase, (camera on source switch), then everything below | camera otherwise, visibility, view |
| domain bounds, `resolution`, `fieldDensity` | grid, `displayLimit`, 17 contour levels, arrows + `fieldMax`, height mesh, heatmap texture | streamlines, phase, camera |
| visibility toggles, view, help, scroll, speed, pause | nothing | all caches |
| camera orbit/zoom | nothing | all caches |

The intended benefit is that camera motion, visibility and animation never
trigger recomputation, so those interactions stay frame-rate-bound rather than
compute-bound. The cost, already noted in section 03, is that `fieldDensity`
alone forces a full plane rebuild even though it affects only the arrow lattice.

## 5. GPU resource lifecycle

Creation and destruction are explicit; there is no RAII wrapper beyond the
renderer's destructor.

- Constructor loads raylib's default material (`renderer.cpp:98`).
- Each rebuild unloads the previous surface mesh and heatmap texture before
  creating replacements (`renderer.cpp:156-157`, `162-165`).
- `gpuMesh` allocates vertex and colour buffers with `MemAlloc`, uploads a
  **non-indexed** mesh, and throws if the VAO/VBO is missing — the failure mode
  for a driver without OpenGL 3.3 (`renderer.cpp:26-47`).
- Destructor order: heatmap texture → surface mesh → render target → material
  `maps` array (`renderer.cpp:99-106`).
- **Ownership caveat**: raylib owns the default shader and default white
  texture; only the map array allocated by `LoadMaterialDefault` is freed
  (`renderer.cpp:103-105`).
- The heatmap `Image` **borrows** the pixel vector's memory and must not be
  unloaded (`renderer.cpp:158-163`).
- Teardown order in `main`: the `Renderer` is destroyed before `UnloadUiFont`
  and `CloseWindow` (`main.cpp:32`, `70-71`), so GPU objects die while the
  context is still valid.

## 6. Resize and render target

- The viewport is recomputed every frame as
  `{sidebar+15, 100, screenW−360, screenH−180}` with a 330 px sidebar
  (`controls.cpp:12`, `76-78`).
- 3D views render into an off-screen target sized to the current viewport; on a
  size mismatch the old target is unloaded and a new one created, with a throw
  on failure (`renderer.cpp:243-248`).
- The target is presented with a vertically flipped source rectangle
  (`renderer.cpp:374`), which is why text and axes appear upright in the 3D
  panes.
- 2D views fit the physical domain with an **equal x/y scale**, never stretching
  distances (`renderer.cpp:169-174`).

## 7. UI layer

### Layout

- 90 px top bar; source buttons, five tabs at `15 + i·148`, help button
  (`controls.cpp:126-139`).
- 14 fixed parameter rows with label, numeric box `{155, y+23, 112, 26}`,
  `−`/`+` buttons, slider track `{18, y+34, 126, 7}` (`controls.cpp:13-29`,
  `96-121`).
- Rows are context-filtered: the height view shows the first 8 parameters; the
  other views show all 14; single-charge mode hides `d`, azimuth and polar in
  every view (`controls.cpp:144-150`).
- Toggles depend on the view: non-stream views expose charges, labels, contours,
  grid, field overlay; the stream view exposes charges, labels, markers, pause
  (`controls.cpp:164-177`).
- The speed slider exists only for the dipole stream view, range 0.25..2.0
  (`controls.cpp:179-191`).
- Wheel scrolling moves 40 px per notch and clamps to the content height; a
  scrollbar is drawn only when content overflows (`controls.cpp:141-155`,
  `198-202`).

### Numeric editing and validation pipeline

1. Click a numeric box: the buffer is prefilled with `%.6g`, fully selected
   (`controls.cpp:100-107`).
2. Accepted characters `0-9 . - + e E`, maximum 24 characters; Ctrl+A selects;
   Backspace edits; Esc cancels (`controls.cpp:204-225`).
3. Enter parses with `strtod` and requires the **entire** string to be consumed
   (`controls.cpp:220-225`).
4. Integer fields must satisfy `floor(v) == v` and be inside the displayed range
   **before** the cast.
5. The candidate is written into a copy, `dipole::validate` is called, and only
   a valid candidate is committed; an unchanged value is a no-op
   (`controls.cpp:79-95`).

This ordering — parse, range-check, copy, validate, commit — is why a rejected
edit leaves the previous simulation untouched rather than half-applied.

Slider drag and `±` buttons rely on the same `validate` path
(`controls.cpp:108-121`). Errors are shown once, in orange, at the bottom of the
screen, and persist until a successful apply, a new edit, or reset
(`controls.cpp:106`, `220`, `229`).

### Camera isolation

`controls.blocksCamera()` is true while help is open, a number is being edited,
or a slider is being dragged (`controls.hpp:20`). The renderer skips orbit and
zoom in that state (`renderer.cpp:234-242`), so a drag that starts in the
sidebar never rotates the plot.

### Reset semantics

Reset restores `Parameters{}` — but **preserves the source selector** — and
restores `Visibility{}`, clearing editing, dragging and errors. It does not
reset camera, view, help, scroll, or the help panel's position
(`controls.cpp:192-197`; `cpp/README.md:104-105`).

## 8. Threading and dependencies

The program is single-threaded. There is no async work, no worker pool, and no
networking at runtime. raylib is the only third-party runtime dependency and is
fetched as a pinned source release (`cpp/CMakeLists.txt:26-45`); its audio path
is disabled. The core target has no third-party dependency at all.
