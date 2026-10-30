# 05 — Visualization and visual encodings

This document describes exactly what each view draws and how numerical values
become visual variables. It is the part of the system an article's figures will
rely on, so it separates *what is encoded* from *what the encoding can mislead
about*.

## 1. The five views

| # | Tab label | Encoding | Evidence |
|---|---|---|---|
| 0 | Mapa de potencial | Heatmap of `V` over the plane `z = center.z`, with 10×10 grid, 17 contours, charges, legend | `renderer.cpp:178-230` |
| 1 | Equipotenciales | Same contours on a flat background, coloured by level, no heatmap | `renderer.cpp:179`, `186-188` |
| 2 | Campo vectorial | Heatmap **plus** XY arrows at every sample | `renderer.cpp:178`, `189-199` |
| 3 | Superficie altura | 3D vertex-coloured surface of `sign(V)·sqrt(|V|/peak)·H`, optional grid, optional lifted contours, charge markers, all sharing `visualHeight()` | `renderer.cpp:309-339`; `physics.cpp:386-433` |
| 4 | Líneas campo 3D | Dipole: cached streamlines with animated direction markers. Single charge: five concentric 3D wireframe spheres with smooth perspective silhouettes | `renderer.cpp:313-317`, `386-405` |

View selection is a plain integer in `controls` (`controls.cpp:137-138`).

Notes on per-view behaviour:

- The heatmap is drawn for views 0, 2 and 3's sibling plane logic
  (`if (view != 1)`, `renderer.cpp:178`); view 1 is the only flat-background
  view.
- Contours are suppressed in the vector view regardless of the toggle
  (`if (v.contours && view != 2)`, `renderer.cpp:186`).
- In the vector view arrows are always drawn; in the map view they are drawn
  only when the field toggle is on (`renderer.cpp:189`).

## 2. Colour encoding

```
t = clamp( V / limit , −1, +1 )
neutral  = (242, 243, 236)     off-white
positive = (211,  48,  56)     red
negative = ( 33,  87, 181)     blue
colour   = neutral + (end − neutral)·|t|
```

Evidence: `renderer.cpp:13-21`. The mix is a per-channel linear interpolation
towards the signed endpoint; `limit` is the same `displayLimit` used by colour,
contours and the 2D views. The view-3 height scale is separate (`peak`, §4).

`displayLimit` is the **95th percentile of |V|** over the sampled grid, computed
with `nth_element` at index `floor(0.95·(N−1))` and floored at `1e-9`
(`physics.cpp:406-417`).

Consequences to state explicitly in a caption:

- The colour scale is **relative to the current configuration**, not absolute in
  volts. Changing `q` or the domain changes what a given colour means; the
  legend prints the numeric limit, and the plane caption repeats it
  (`renderer.cpp:222-224`).
- Roughly 5 % of samples are clipped by construction. Clipping is a display
  device only; the underlying values are untouched (`physics.cpp:406-417`).
- Because clipping is percentile-based, the visible near-charge region is
  *always* saturated, even though its true value is finite under softening.

Measured limits at defaults: dipole `5.5556275131 V`, single charge
`7.0920973179 V` (section 02, reference run).

## 3. Arrow encoding (views 0 with overlay, and 2)

```
reference = k·q / L²
maxLength = 0.045 · min(spanX, spanY)          // independent of density
offset    = (Ex, Ey, 0) · maxLength / (|E_xy| + reference)
segment   = from p − offset/2 to p + offset/2
```

Evidence: `renderer.cpp:190-197`.

Properties:

- Only the **XY projection** of the field is encoded; the `z` component is
  discarded (`renderer.cpp:194`).
- The segment is **centred** on the sample point, not rooted at it.
- Length saturates: as `|E_xy| → ∞`, `offset → maxLength`. The saturation
  constant is `k·q/L²`, a configuration-level scale, not a local one.
- Arrow density changes the number and position of samples, not the length
  scale (`renderer.cpp:192`).

Caption (verbatim): `Flechas XY | máx |E| muestreado <x> V/m | longitud satura
al 4.5% de la extensión` (`renderer.cpp:227`).

**Caption/encoding mismatch.** `fieldMax_` is the maximum of the **full 3D**
`|E|` over the samples (`renderer.cpp:151`), while the arrows encode `|E_xy|`.
The printed maximum can therefore exceed the magnitude any arrow displays. A
paper figure reusing this caption should reword it.

## 4. Height encoding (view 3)

```
H    = 0.3 · min(spanX, spanY)
peak = max |V| over the sampled grid          (floored at 1e-9)
z    = sign(V) · (|V|/peak)^0.5 · H
```

Evidence: `physics.cpp:386-391` (`visualHeight`), `physics.cpp:392-410`
(`heightfield`), `physics.cpp:422-431` (`peakMagnitude`); `peak` is set at
`renderer.cpp:140`, `H` at `renderer.cpp:155`.

The height scale is normalized by the **maximum sampled magnitude**, not by the
95th-percentile colour limit. Exactly the sample(s) at `|V| = peak` map to
`|z| = H`; below the peak the map is a strictly monotonic square root with no
clipping plateau. This turns the near-charge region into a needle instead of a
flat mesa of radius ~1.26 m at defaults. The far field is **not** inflated by
the old percentile clamp.

What the axes mean:

- Horizontal axes are physical metres (`x`, `y`).
- The vertical axis is **visual** height, labelled `altura visual V`
  (`renderer.cpp:363-368`). It is neither physical `z` nor a linear voltage.
- The square root is a deliberate compression; without it the near-charge peak
  would dominate the frame.

The surface, the lifted contours (`renderer.cpp:319`) and the charge markers
(`renderer.cpp:334`) all call the same `visualHeight(value, peak, H)` helper, so
the overlays lie on the surface by construction. The caption reports the law and
the two scales (`renderer.cpp:383`).

Charge markers in this view follow their XY position on the surface, not their
physical `z` (`renderer.cpp:326-338`).

## 5. World mapping and 3D axes

Physical `(x, y, z)` maps to raylib world coordinates as

```
world = (x, z, −y)
```

Evidence: `renderer.cpp:22-25`. Axis colours follow raylib's convention after
this mapping: red is `+x`, blue is world `+y` which is physical `+z`, green is
world `−z` which is physical `+y` (`renderer.cpp:323-325`, `364-366`).

Axis labels are placed at `origin + 0.65·extent`, not at the domain corners, and
there are **no tick marks** on the 3D axes (`renderer.cpp:322`, `362-367`). Any
figure claiming quantitative reading from the 3D pane needs an overlaid scale.

## 6. Legend

Vertical 16-px gradient from `+limit` at the top to `−limit` at the bottom, with
labels `+limit`, `0 V`, and a caption `Color recortado`
(`renderer.cpp:49-58`).

**Known defect:** the bottom label is produced from the same `char` buffer as
the top label, so it prints `+<limit> V` at the negative end
(`renderer.cpp:53-56`). The colour gradient itself is correct; only the text is
wrong. Do not reproduce this label in a figure without correcting it.

## 7. 3D camera

- Left-drag orbits, wheel zooms, and only inside a 3D plot; a gesture starting
  in the sidebar is ignored through `blocksCamera()`
  (`renderer.cpp:234-242`; `controls.hpp:20`).
- Camera state is per-view (`yaw_[2]`, `pitch_[2]`, `zoom_[2]`) and survives
  parameter changes, but resets when the charge source switches
  (`renderer.cpp:128-133`).
- Stream-view framing auto-fits the streamline bounding box using a 45° vertical
  FOV and an aspect-limited limiting half-FOV, with a 1.12 margin factor
  (`renderer.cpp:254-286`).
- Clip planes are derived from the scene extent:
  `near = max(1e-7, extent·0.001)`, `far = radius + 4·extent`
  (`renderer.cpp:292-294`).

## 8. Probe and help panel

The panel is a draggable, collapsible floating window clamped to the screen
(`controls.cpp:259-301`). Its content is the most didactic part of the system
because it exposes the intermediate values:

| Shown | Detail |
|---|---|
| Evaluation point | `x, y, z`, 4 decimals |
| Softened radii | `r` (single) or `r₊`, `r₋` (dipole), 5 decimals |
| Moment | `p = q·d` with substituted values, result in `nC·m` |
| Potential | Formula with substituted `k`, `q`, `r`, result in `V` |
| Field magnitude | `|E| = |(Ex,Ey,Ez)| = sqrt(Ex²+Ey²+Ez²)`, result in `V/m` |
| Field vector | `E = (Ex, Ey, Ez) V/m` |
| Softening | `epsilon = max(0.001·L, 1e-6)` with the current value |
| Conventions | `q` in nC, `k` in 10⁹ SI, `E = −grad V` by centred differences, `h = 0.05·epsilon`, slice at `z = center.z`, view-5 description |

Evidence: `controls.cpp:339-374`.

Two honest caveats:

- The panel's `h = 0.05·epsilon` line omits the `1e-6` floor that the code
  applies (`controls.cpp:371` vs `physics.cpp:78`). At the defaults the 5 % rule
  dominates, so the printed number is right. It becomes wrong in one reachable
  corner: a single charge whose domain span is near the 0.001 m minimum gives
  `L = 5e-4`, `ε = max(5e-7, 1e-6) = 1e-6`, and the code uses
  `h = max(1e-6, 5e-8) = 1e-6`, while the panel prints `5e-8` — a factor of 20.
- In 3D views the probe keeps the last 2D point and says so
  (`controls.cpp:336-337`); it does not ray-cast into the 3D scene.

## 9. Typography and legibility

Text uses IBM Plex Sans when the font file is present next to the executable,
with a fallback to raylib's default font and a one-time stderr warning
(`ui_text.cpp:83-125`). Sizes in the help panel were raised for mobile-scale
legibility (28/16/24 px for equations and lines, `controls.cpp:344-374`), and
that change is visible in the merged conflict resolution described in section
07.

## 10. Which view to use for which claim

| Claim in the article | Best view | Why |
|---|---|---|
| Potential topology, sign structure | 0 | Colour + contours together |
| Exact level-set geometry, saddle behaviour | 1 | No colour competition |
| Direction and relative magnitude of `E` | 2 | Arrows plus underlying scalar |
| Shape of `1/r` divergence and softening effect | 3 | The sqrt compression exposes mid-field structure |
| Field-line topology, source/sink behaviour | 4 (dipole) | Integrated paths with termination |
| Equipotential shells around one charge | 4 (single) | Five concentric wireframe globes (`DrawSphereWires`, 8×16) |

Do **not** use view 3 to argue quantitative agreement: its vertical axis is
deliberately non-linear. Its overlaid contours and markers share the same
`visualHeight()` mapping, so they are consistent with the surface but remain a
visual, peak-normalized encoding rather than physical height.
