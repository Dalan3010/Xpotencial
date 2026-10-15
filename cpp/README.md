# Native electric-dipole simulator

A standalone C++17 desktop simulator: five views, native raylib controls, and a
double-precision numerical library that needs no graphics or network access. Run
commands below from the repository root; `cpp/` is a CMake project, not a separate
Git repository.

## Quick path: Windows

Prerequisites: CMake **3.20+**, Git on PATH for the first GUI configure, Visual
Studio 2019 Build Tools with **MSVC v142**, and a Windows SDK. A normal user session
and an **OpenGL 3.3** graphics driver are needed to run the GUI. Use a terminal with
`cmake` and `ctest` on PATH (or their full paths).

```powershell
cmake -S cpp -B cpp/build -G "Visual Studio 16 2019" -A x64 -DDIPOLE_BUILD_GUI=ON
cmake --build cpp/build --config Release
ctest --test-dir cpp/build -C Release --output-on-failure
.\cpp\build\Release\dipole_simulator.exe
# Bounded runtime check: all five views, then automatic exit.
.\cpp\build\Release\dipole_simulator.exe --smoke
```

## Quick path: Linux (verified in the current development environment)

Prerequisites: CMake 3.20+, a C++17 GCC/Clang toolchain, Make (or another chosen
CMake generator), Git, OpenGL development files, and X11 development files for
raylib's default GLFW backend. On Debian/Ubuntu, the relevant prerequisite package
names commonly include `build-essential`, `cmake`, `git`, `libgl1-mesa-dev`,
`libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, and `libxi-dev`.
Package availability varies by distribution; installing packages is a separate
administrator decision, not part of this project's build. Audio is disabled.

```sh
cmake -S cpp -B cpp/build-linux -DCMAKE_BUILD_TYPE=Release -DDIPOLE_BUILD_GUI=ON
cmake --build cpp/build-linux
ctest --test-dir cpp/build-linux --output-on-failure
./cpp/build-linux/Release/dipole_simulator
./cpp/build-linux/Release/dipole_simulator --smoke
```

Linux GUI execution requires a working display session and OpenGL 3.3; SSH,
containers, CI without a display, and remote desktops may not provide them.
The build and smoke run are verified on the current development system; this does
not establish compatibility with every Linux distribution or graphics driver.

## Headless core: no raylib, display, or download

```powershell
cmake -S cpp -B cpp/build-headless -G "Visual Studio 16 2019" -A x64 -DDIPOLE_BUILD_GUI=OFF
cmake --build cpp/build-headless --config Release
ctest --test-dir cpp/build-headless -C Release --output-on-failure
```

On Linux, omit the Visual Studio generator and architecture options and set
`-DCMAKE_BUILD_TYPE=Release`. `DIPOLE_BUILD_GUI=OFF` bypasses raylib entirely.
Tests use explicit failure checks, not `assert`, so Release tests remain active.

## Controls and five views

| Action | Result |
|---|---|
| Top tabs | Potential map, equipotential contours, vector field, height surface, dipole streamlines 3D |
| Sidebar slider or +/- | Adjust parameter within validated bounds |
| Click numeric value, type, Enter | Replace value; scientific notation accepted; Escape cancels |
| Ctrl+A while editing | Select value for replacement |
| Wheel over sidebar | Scroll to remaining controls, visibility toggles, and reset |
| Left-drag inside a 3D plot | Orbit; a gesture starting in the sidebar does not move the camera |
| Wheel inside a 3D plot | Zoom; camera orbit/zoom survives parameter updates |
| Help / physics | Display equations and numerical/visual conventions, including streamline animation limits |
| Window close button | Exit; Escape only cancels text editing |

The first four views retain their defaults and slider ranges: q=1 nC (0.1..5), k=8.99
(0.5..20), d=2 m (0.2..6), azimuth=0 degrees (0..360), polar=90 degrees
(0..180), center=(0,0,0) m, x/y=-5..5 m, grid=70 (15..150), arrows=14
(4..30). The map field overlay remains off. View five keeps q, k, d, orientation,
and center controls; plane-domain and arrow-density sliders do not affect it.

| Streamline control | Default | Effect |
|---|---|---|
| Show charges / labels | On | Shows +q and -q at their actual physical coordinates |
| Show direction markers | On | Hides or shows markers moving along integrated curves |
| Pause marker animation | Off | Freezes marker phase without changing cached curves |
| Marker speed | 1.0x | Slider range 0.25x..2.0x; affects illustration only |

Streamlines are integrated in physical 3D coordinates from the dipole field and
cached when physical parameters change. Markers indicate positive-to-negative
field direction; they do **not** represent particle velocity. Reset restores
physical and visibility defaults; camera state is retained.

The web's unbounded numeric center/domain inputs have a native safety limit of
+/-10000 m, with domain spans >=0.001 m. Their sliders cover -10..10 m; numeric
entry permits the wider safe range. Every minimum must be strictly less than its
maximum. Nonfinite input, fractional resolutions, and out-of-range values show
an error and leave the previous simulation intact. Reset restores all physical
parameters and visibility defaults, without forcibly resetting the camera.

## Read the plots correctly

- **Potential convention:** numerical `k * q` already compensates q in nC and
  k in units of 10^9 SI. No extra 1e-9 multiplier is applied. Potential is in V;
  electric field is in V/m, equivalent to N/C.
- **Physics:** `V=kq(1/r+ - 1/r-)`, softened distances
  `r=sqrt(dx^2+dy^2+dz^2+epsilon^2)`, `epsilon=max(0.001*d,1e-6 m)`.
  Centered differences compute `E=-grad V`, with `h=max(1e-6 m,0.05*epsilon)`.
  Softening makes charge singularities finite; this is an educational numerical
  regularization, not an exact point-charge model near the charges.
- **2D:** slice at physical z=center.z; equal x/y visual scale, meter ticks,
  potential color legend. Charge markers are XY projections, including charges
  outside the slice. Contours have 17 levels and use CPU marching squares;
  exact-level edges and deterministic saddle connectivity avoid NaN segments.
- **Field:** arrows depict the XY projection of E. Their physical length is
  `0.045 * min(domain spans) * |E_xy|/(|E_xy|+kq/d^2)`; density changes sampling,
  not this scale. The caption reports the sampled maximum full |E| in V/m.
  The map's field-overlay toggle actually draws these same arrows.
- **Color:** symmetric blue/white/red V scale, clipped at the 95th percentile of
  absolute samples. Clipping affects visualization only, not physics.
- **Height surface:** x/y are in meters, but the vertical coordinate is
  **visual normalized potential**, NOT physical z or potential in meters:
  `height=H*clamp(V/colorLimit,-1,1)`, `H=0.3*min(domain spans)`.
  The equation is displayed on the plot. Charge markers follow their XY
  positions on this heightfield, not their physical z.
- **Retained core APIs:** radial-field and volume/equipotential routines remain
  available and tested as numerical functionality; no radial-magnitude surface,
  transported-arrow overlay, or related caption is shown in the native views.

## Dependencies, offline builds, and ownership

CMake fetches **raylib Git tag 5.5** only for the GUI, with bundled GLFW.
The first configure requires Git and network access and can take several minutes.
It does not install system packages. All sources and caches stay in `cpp/build*`.
Examples, audio, automatic screenshots, GIF recording, and clipboard images are
disabled. Attribution and the raylib license are in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); keep bundled dependency notices
when redistributing.

For an offline GUI build, prepare an unmodified raylib 5.5 source tree under,
for example, `cpp/build-offline/vendor/raylib`, then pass
`-DFETCHCONTENT_SOURCE_DIR_RAYLIB=<absolute-path-to-that-tree>` to configure.
Alternatively use `-DDIPOLE_USE_SYSTEM_RAYLIB=ON` and `-DCMAKE_PREFIX_PATH=<prefix>`
for a preinstalled raylib **5.5** CMake package. Its GLFW/OpenGL linkage and runtime
libraries must also be available. The FetchContent option requires no installed
raylib. The core-only configuration always works without raylib.

`include/dipole/physics.hpp` and `src/physics.cpp` own numerical data.
`src/renderer.cpp` owns cached grids, contours, arrows, textures, meshes, cached
3D streamlines, and a resize-aware render target. Streamline integration runs
only when physical charge parameters, center, or orientation change; animation
phase, visibility, camera motion, and speed never reintegrate paths. The first four
views retain their existing numerical caches. Tested radial and volume numerical
APIs remain intact even though radial-surface rendering is retired.
No measured performance guarantee is claimed.

Mesh triangle soup avoids 16-bit GPU index overflow at grid 150x150 and radial
resolution 80 (160 azimuth slices). Mesh CPU/GPU buffers are unloaded once; the material's map array is
released without destroying raylib's shared default shader/texture. GPU resources
are destroyed before closing the window. Rendering is intentionally unlit vertex
color, with no extra GUI framework, assets, export files, or threading dependency.

## Verification status

The `--smoke` option renders all five views, including streamlines with animated
direction markers, then exits after 40 frames (with a 25-second loop deadline).
It verifies bounded runtime and GPU resource creation, **not** screenshot quality,
interaction correctness, long-session leak absence, or measured performance.
The Linux GUI build, CTest, smoke run, and default-view screenshot were verified
in the current development environment; manual interaction and long-session
behavior remain outside that evidence.

The tests cover orientation, translation, symmetry, q/k scaling, finite
softening, centered field direction/accuracy, invalid inputs, contour degeneracies,
constant-radius spheres, outward winding, two-face edge incidence at seam/poles,
large heightfields, volume reflection, sampled-range exclusion, box clipping,
exact/coplanar plane de-duplication, signed pairing, finite nondegenerate extracted
triangles, interpolation residuals, and legacy equipotential field orthogonality.
Radial tests additionally cover actual sampled magnitude/radius association,
bulges/depressions, q/k magnitude scaling and normalized invariance, translations,
rotations/reflections, positive finite radius, min/default/max topology and winding,
zero/numerically uniform contrast, safety adjustment, invalid parameters, and
normals that do not generally follow E. **Exhaustive human manual UI validation remains
pending; Linux build, CTest, and bounded GUI smoke have passed.** A useful manual pass exercises numeric rejection, every toggle, polar
rotation, maximum resolution, reset, sidebar scrolling, camera isolation, and
window resizing; smoke alone does not establish these behaviors.
