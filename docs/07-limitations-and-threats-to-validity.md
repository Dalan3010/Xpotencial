# 07 — Limitations and threats to validity

Everything here is either observable in the code or was measured while
documenting. Nothing is speculative. This is the section a reviewer will mine,
so it is deliberately blunt.

## 1. Model limitations

| Limitation | Why it matters | Evidence |
|---|---|---|
| Charges are smeared, not point-like | The potential near a charge is finite and its gradient vanishes there; the system cannot reproduce the `1/r` divergence or the divergent field | `physics.cpp:17-26`; reference values in section 02 |
| One charge magnitude for both poles | A dipole with unequal magnitudes is not representable | `physics.hpp:13-26` |
| Charge magnitude is always positive in the UI | `q ∈ [0.1, 5]`, so a negative or neutral source cannot be explored; one branch of `chargeIsPositive` is unreachable | `physics.cpp:37`; `renderer.cpp:72-74` |
| Free space only | No dielectrics, conductors, grounded planes, or boundary conditions | by construction |
| Static only | No displacement current, radiation, or time evolution; markers are illustrative | `renderer.cpp:388` |
| Softening length is fixed by `L` | The user cannot choose the regularization; it is `0.001·L`, so it scales with `d` or the domain, not with a physical screening length | `physics.cpp:65-69` |
| Only one or two charges | `chargeCount ∈ {1, 2}` | `physics.cpp:33` |
| Field lines are illustrative, not a complete family | One ring of 16 seeds at 72° for the dipole, one 16-point sphere for a monopole | `renderer.cpp:121-124` |

## 2. Numerical limitations

| Limitation | Detail |
|---|---|
| Gradient is a finite difference, not the analytic derivative | `h = max(1e-6, 0.05ε)`; truncation `O(h²)`; agreement with the analytic derivative is asserted only at one configuration and only to `1e-5` (`core_tests.cpp:42-44`) |
| No convergence study | Neither `h`, grid resolution, nor the RK4 step was refined against a reference solution. The constants are engineering choices |
| `h` is tied to `ε` | Improving gradient accuracy independently of the regularization is impossible without changing `ε` |
| Field at the charge centre is exactly zero | A consequence of the smooth softened kernel; a figure of the near-field peak is flat, not peaked (`section 02`, reference values) |
| Streamline integrator is unvalidated | RK4 on the normalized field with step `0.02L`; no error estimate, no comparison with an analytic field line |
| Level-set deduplication is exact-coordinate | Segments shared by two cells merge only if interpolation is bit-identical (`physics.cpp:193-198`); tolerance welding is not implemented |
| Colour clipping is statistical | 5 % of samples are saturated by construction; the visible near-charge region is always clipped (`physics.cpp:406-417`) |
| Collection is single-precision at the GPU boundary | Coordinates are bounded to ±10 000 m to keep float precision usable (`physics.cpp:39-44`) |
| No incremental recomputation | All 17 contour levels and the full height mesh are rebuilt on any plane change (`renderer.cpp:137-165`) |

## 3. Visual limitations

| Limitation | Detail | Evidence |
|---|---|---|
| Legend bottom label is wrong | The negative end reuses the positive string, so it prints `+<limit> V` at the bottom of the gradient | `renderer.cpp:53-56` |
| Arrow caption reports a different quantity | The caption prints max full-3D `|E|` while arrows encode `|E_xy|` | `renderer.cpp:151`, `194`, `227` |
| Equipotential spheres are illustrative wireframes | Five `DrawSphereWires` globes at fixed radii with a camera-dependent tangent silhouette, not extracted isosurfaces of the numerical field | `renderer.cpp:78-109` |
| No quantitative 3D axes | Labels only, no ticks; a reader cannot read a value off the 3D pane | `renderer.cpp:362-367` |
| Unlit rendering, no normals or texture coordinates | Shape is conveyed by silhouette and vertex colour only | `renderer.cpp:26-47` |
| The vector view draws the heatmap underneath | Arrows compete with a colour field using the same variable | `renderer.cpp:178` |
| One-frame interaction latency | A parameter edit is rendered on the following frame | `main.cpp:50-54` |

## 4. Software limitations

| Limitation | Detail |
|---|---|
| No automated GUI tests | Tests link only the core (`cpp/CMakeLists.txt:17-19`) |
| Smoke covers the dipole only | `--smoke` never sets `chargeCount = 1`, so the single-charge view path is unexercised by the runtime check (`main.cpp:28-31`) |
| `fieldDensity` causes a full rebuild | Folded into `planeChanged` (`renderer.cpp:113-114`) |
| Reset is partial | Preserves the source selector; does not reset camera, view, help, scroll, or panel position (`controls.cpp:192-197`) |
| No export or persistence | Screenshots, data export, and session files are absent |
| No install rules, no CI, no presets | Distribution and automated verification are out of scope (`cpp/CMakeLists.txt:1-58`) |
| Single machine verification | Build, tests, and GUI were verified on one Linux environment (`cpp/README.md:24-46`, `171-179`) |
| Retained unrendered code | `sampleFieldAngles`, `radialSurface`, `makeGrid3D`/`equipotentials` and `sphere` are tested but unused by any view; `potentialLevel` and `pairedLevels` have no consumers. A reviewer may read these as intended features |

## 5. Documentation/code mismatches found while writing these documents

These are factual discrepancies, not opinions. One is corrected in this branch;
the others are recorded so they cannot silently propagate into the article.

| Mismatch | Code says | Prose said | Status |
|---|---|---|---|
| Height law | Surface, lifted contours and markers share `visualHeight() = sign(V)·sqrt(|V|/peak)·H` (`physics.cpp:386-391`, `renderer.cpp:319`, `334`) | Prose previously described a linear `H·clamp(V/colorLimit,−1,1)` overlay mismatch | **Resolved in this branch** — unified peak-normalized law |
| Repository layout | `odd/` is gitignored (`.gitignore`) | Listed as part of the layout (`README.md:25`) | Corrected in this branch |
| Help panel `h` | `h = max(1e-6, 0.05ε)` (`physics.cpp:78`) | `h = 0.05·epsilon`, floor omitted (`controls.cpp:371`) | Recorded; only matters for a single charge with a near-minimum domain |
| Legend bottom label | Correct gradient, wrong text (`renderer.cpp:53-56`) | Documented as a gradient from `+limit` to `−limit` | Recorded; a known display defect |
| "Five equipotential spheres" | Wireframe globes + exact tangent silhouette (`renderer.cpp:78-109`) | Previously camera-facing billboard circles | Resolved in this branch; still illustrative, not extracted isosurfaces |

## 6. Threats to validity (for a paper)

### Construct validity

The instrument measures what the code computes, not what electrostatics says.
Any statement of the form "the simulator shows the field of a dipole" must be
qualified: it shows the field of **two softened charges superposed**, sampled on
a grid, differentiated by finite differences, and colour-mapped with 5 %
clipping. Each step is a construct shift.

### Internal validity

- Test tolerances are absolute, not relative. At large magnitudes an absolute
  `1e-8` is vacuous; at small magnitudes it is strict. The suite is consistent
  but not scale-normalized.
- The large assertion count comes from loops over dense meshes, so it does not
  imply broad behavioural diversity.
- No test isolates the renderer; visual regressions are invisible to CI.

### External validity

- One Linux machine, one toolchain, one GPU driver. Windows and macOS commands
  are documented but were not executed while preparing this material.
- No user study, no classroom trial, no comparison against an independent
  implementation.
- No performance numbers, so "real time" is a design intent, not a measurement.

### Conclusion validity

The following conclusions are **not** supported by the current evidence and
should not appear in an article without new experiments:

1. that the simulator is numerically accurate to a stated error bound;
2. that the visuals are faithful (encodings are documented but not visually verified);
3. that it improves learning outcomes;
4. that it scales to larger grids in real time;
5. that it is portable beyond the tested environment.

## 7. Recommended experiments before submission

| Experiment | What it would establish | Effort |
|---|---|---|
| Gradient convergence: refine `h` over a range and plot error against the analytic softened derivative | A defensible choice for `h` and a measured `O(h²)` slope | Low — the analytic reference already exists in the test at `core_tests.cpp:42-44` |
| Grid convergence: refine `n` and compare contour area/perimeter and extracted volumes | Spatial convergence order | Low–medium |
| Streamline convergence: halve `stepSize` and compare endpoint error and point count | Integrator reliability | Low |
| Timing sweep over `n`, density, seed count | Real performance claims and a work model | Low |
| Screenshot set with fixed parameters | Visual reproducibility, figure generation | Low |
| ~~Corrected height-view overlays~~ — **done** | Surface, lifted contours and charge markers now all call `visualHeight()` normalized by the sampled peak, so the former overlay mismatch is resolved; what remains is visual verification of the rendered figure | Implemented |
| Manual usability protocol | Any didactic claim | High |
