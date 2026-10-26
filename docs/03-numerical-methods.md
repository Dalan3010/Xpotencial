# 03 — Numerical methods

This section describes every numerical algorithm in the core, with the
parameters that control it and the work it performs. All line references are to
`cpp/src/physics.cpp` unless stated otherwise.

## 1. Sampling grid

| Property | Value | Evidence |
|---|---|---|
| Resolution parameter | `n = resolution`, default 70, range 15..150 | `physics.cpp:46-47`; `physics.hpp:18` |
| Samples | `N = n²`, row-major `values[y·n + x]` | `physics.cpp:176-186` |
| Coordinates | `x = xmin + (xmax−xmin)·x/(n−1)`, endpoints inclusive | `physics.cpp:173-175` |
| Slice plane | Every sample is taken at `z = center.z` | `physics.cpp:183` |
| Stored scalar | Potential `V` only — no field grid is stored | `physics.cpp:182-185` |
| Default domain | `x, y ∈ [−5, 5]` m | `physics.hpp:17` |
| Domain guards | spans ≥ 0.001 m; coordinates within ±10 000 m | `physics.cpp:42-45` |

The ±10 000 m bound exists to preserve float precision when coordinates reach
the GPU (`physics.cpp:39-40`).

The arrow layer uses a **separate, independent lattice** at cell centres:

```
point = ( xmin + spanX·(x+0.5)/n ,  ymin + spanY·(y+0.5)/n ,  center.z )
n = fieldDensity  (default 14, range 4..30)
```

Evidence: `cpp/src/renderer.cpp:146-153`. Arrow density therefore changes the
*number of samples*, never the sample values at a given point, and never the
arrow length scale (`renderer.cpp:192`).

## 2. Level sets: marching squares

Seventeen levels are extracted per plane, generated in `renderer.cpp:141-145`:

```
level_i = displayLimit · i / 8,   i = −8 … +8
```

so the levels are `−limit, −7/8·limit, …, 0, …, +limit`, with `limit` the 95th
percentile of `|V|` (`physics.cpp:406-417`). The zero level is drawn thicker
(`renderer.cpp:186-188`).

### Algorithm

1. Each grid cell has corners `p0=(x,y), p1=(x+1,y), p2=(x+1,y+1), p3=(x,y+1)`,
   and the scalar used is `V − level` (`physics.cpp:200-201`).
2. A cell whose four values are all exactly zero is skipped (`physics.cpp:202`).
3. If both endpoints of a grid edge sit exactly on the level, that edge is
   emitted directly (`physics.cpp:206`).
4. Otherwise a crossing exists iff the sign test
   `(v[e] >= 0) != (v[next] >= 0)` holds; zero is assigned to the positive side
   (`physics.cpp:207-208`).
5. The crossing point is interpolated linearly:
   `t = v[e] / (v[e] − v[next])`, hit `= p[e] + (p[next] − p[e])·t`
   (`physics.cpp:209-210`).
6. Two hits produce one segment (`physics.cpp:212`). Four hits — the ambiguous
   saddle case — are resolved with the **asymptotic decider**
   `det = v0·v2 − v1·v3`: `det >= 0` pairs hits `(0,1)` and `(2,3)`, otherwise
   `(0,3)` and `(1,2)`. An exactly-zero determinant takes the `>= 0` branch, so
   the tie is deterministic, not arbitrary (`physics.cpp:213-218`).

### Robustness measures

- **Degenerate segments** shorter than `1e-12` are dropped
  (`physics.cpp:195`), which removes isolated exact-touch vertices.
- **Duplicate suppression** canonicalises each segment by ordering its endpoints
  by `(x, y)` and inserting the tuple into a `std::set`
  (`physics.cpp:193-198`). This is exact-coordinate deduplication, not
  tolerance welding: two segments merge only if interpolation is bit-identical.
- **NaN avoidance**: the grid and the level must be finite, otherwise an
  exception is thrown (`physics.cpp:189-191`). Since a crossing guarantees
  `v[e] ≠ v[next]`, the denominator can never be zero.
- **Measured behaviour**: the suite asserts a constant cell yields nothing, a
  saddle yields two segments, an exact edge yields one segment, and an isolated
  exact vertex yields nothing (`core_tests.cpp:614-634`).

### Cost

The extraction is `17` passes over `(n−1)²` cells: 80 937 cell tests at the
default `n=70`, 377 417 at the maximum `n=150`. The `O(n²)` potential
evaluations were already paid by the grid build; all seventeen passes reuse
`grid.values` and add no physics work.

## 3. Height surface

The 3D surface uses a **non-linear** height mapping (`physics.cpp:386-404`):

```
t  = clamp( |V| / limit , 0, 1 )
z  = sign(V) · t^0.5 · H
H  = 0.3 · min( spanX, spanY )
```

Evidence for `H`: `renderer.cpp:154`. The square root compresses the large
values near the charges so that the mid-field structure remains visible.

Geometry: two triangles per grid cell, emitted directly as a **triangle soup**
(`a,b,c` and `a,c,d`), six vertices per cell, no index buffer
(`physics.cpp:400-403`). At `n=150` that is 133 206 vertices.

**Consistency caveat (important for figures).** The surface uses the `sqrt` law,
but the contour curves lifted onto that surface and the charge markers in that
view use a **linear** mapping:

```
z_lifted_contour = clamp( level / limit , −1, 1 ) · H + 0.002·span   (renderer.cpp:315-320)
z_marker         = clamp( V / limit , −1, 1 ) · H                    (renderer.cpp:332-334)
```

The three coincide only at `V = 0` and `V = ±limit`. A screenshot of that view
therefore shows contours and markers floating off the surface in the
intermediate range. This must be described as a known limitation
(section 07), not as a physical feature.

## 4. Volume extraction: marching tetrahedra

A separate 3D path builds an `n³` potential volume and extracts a level surface:

| Property | Value | Evidence |
|---|---|---|
| Volume resolution | `volumeResolution`, default 37, range 10..60 | `physics.hpp:16`; `physics.cpp:50-52` |
| Extent | `center ± volumeHalfExtent` (default 3 m, range 0.1..100) | `physics.cpp:48-52`, `285-292` |
| Cell decomposition | 6 tetrahedra per cube, sharing the body diagonal `0–6` | `physics.cpp:300-320` |
| Edge interpolation | Linear with `t` clamped to `[0,1]` | `physics.cpp:337-338` |
| Deduplication | Canonical endpoint ordering by global vertex id; exact-vertex dedupe | `physics.cpp:330-350` |
| Triangulation | Centroid fan per tetrahedron | `physics.cpp:345-355` |
| Face dedupe | Sorted coordinate key | `physics.cpp:355-363` |

This path is **implemented and tested but not rendered** by any view
(`cpp/README.md:132-134`). Its tests cover analytic area and perimeter of
affine plane sections at `1e-10`, reflection antisymmetry, coplanar face
deduplication, seam/edge incidence, and residual improvement with resolution
(`core_tests.cpp:395-613`).

Reported results from the suite (default analytic plane section): area 36,
`13×13` projected probes covered exactly once, fine-grid mean residual below
`0.12` and strictly better than the coarse grid (`core_tests.cpp:455`, `479`,
`554-592`).

## 5. Sphere mesh and the "3D spheres" view

There are **two different things** with the word "sphere" in this project. An
article must not confuse them.

### 5.1 The core sphere mesh (not rendered)

`dipole::sphere(p)` builds a potential-coloured UV sphere
(`physics.cpp:364-385`):

- `rings = sphereResolution` (default 45, range 10..100);
- `slices = 2·rings`;
- vertices `= 6·slices·(rings−1)`, i.e. `118 800` at the maximum;
- poles forced exactly to `(0,0,±R)`;
- periodic seam via `f mod slices`;
- cap rings emit a single triangle to avoid degenerate pole triangles;
- outward winding for every triangle; no normals are generated.

Radius is an **absolute** value, `sphereRadius` (default 3 m, range 0.5..10),
not a multiplier (`physics.hpp:19`; `physics.cpp:48-49`).

### 5.2 The rendered single-charge view (billboards)

The GUI draws **five billboard circles**, i.e. the silhouette of five spheres,
not meshes:

- radii `{0.2, 0.4, 0.6, 0.8, 1.0} × L`;
- 96 segments per circle, drawn with `DrawLine3D`;
- each circle lies in the plane through the charge perpendicular to the camera
  forward vector, with orthonormal in-plane basis `u, v`
  (`renderer.cpp:75-96`, `298-301`).

Because the plane is rebuilt from the camera basis every frame, the circles look
like perfect spheres from any orbit angle. They are an **illustration of
concentric equipotential surfaces**, not a sampled isosurface of the numerical
field. Any caption claiming "isosurface extraction" would be wrong.

## 6. Field lines: RK4 on the normalized field

### Seeds

| Mode | Distribution | Evidence |
|---|---|---|
| Dipole | Ring about the `+q → −q` axis at `72°`, `seedCount` azimuths, radius `seedRadius·L` | `physics.cpp:105-114`, `133-137` |
| Single charge | Fibonacci sphere (equal-area), radius `seedRadius·L` | `physics.cpp:124-132` |

The golden angle used by the spherical distribution is
`2.39996322972865332 ≈ π(3−√5)` (`physics.cpp:125`).

Defaults in the core: `seedCount 16`, `seedAngleDegrees 60`, `seedRadius 0.08`,
`stepSize 0.02`, `endpointRadius 0.08`, `escapeRadius 8`, `maxArcLength 20`,
`maxSteps 4000` (`physics.hpp:36-45`). The renderer overrides three of them:
**16 seeds, 72°, step 0.02** (`renderer.cpp:121-124`).

Every length-like option except `maxSteps` is **relative to `L`** and multiplied
by it before use (`physics.cpp:115-120`).

### Integrator

Directions, not magnitudes, are integrated:

```
dir(p) = Ê(p) = E(p) / |E(p)|
h      = min( stepSize·L , maxArcLength − arcLength )
k1 = dir(p);           k2 = dir(p + k1·h/2)
k3 = dir(p + k2·h/2);  k4 = dir(p + k3·h)
inc = (k1 + 2k2 + 2k3 + k4)·h/6
p ← p + inc;   arcLength ← arcLength + |inc|
```

Evidence: `physics.cpp:157-166`. Because the right-hand side is normalized, the
scheme traces field direction while advancing approximately one step length per
iteration; it is an arc-length-like parametrization rather than an integration
of `E` itself. **No convergence or error study of this integrator exists.**

### Termination

Tested at the top of each step, in this order (`physics.cpp:143-168`):

1. dipole only: `|p − r₋| ≤ endpointRadius·L` → `ReachedNegativeCharge`;
2. `|p − center| ≥ escapeRadius·L` → `Escaped`;
3. `arcLength ≥ maxArcLength·L` → `ArcLengthLimit`;
4. after appending the last allowed point, `step+1 == maxSteps` → `StepLimit`.

A non-finite result throws (`physics.cpp:164`); a null or non-finite field
throws before integration (`physics.cpp:99-104`).

### Measured behaviour (defaults)

From the reference run (16 seeds, 72°, step `0.02L`):

```
paths=16  totalPoints=2624  reached=16  escaped=0  arcLimit=0  stepLimit=0
```

All sixteen paths terminate on the negative charge, with about 164 points per
path. A reviewer can reproduce this from the public API in a few lines
(section 08).

### Cache and animation coupling

`renderer.cpp:110-134` rebuilds streamlines only when one of
`q, k, d, chargeCount, azimuth, polar, center.{x,y,z}` changes. Domain,
resolution, arrow density, visibility, camera, view, pause and speed do **not**
invalidate the geometry. Marker phase advances only in the stream view and only
when not paused: `phase += dt·0.42·speed` (`renderer.cpp:295`), and resets to
zero on any physics change (`renderer.cpp:119`, `126`).

## 7. Retained but unrendered numerical routines

| Routine | What it computes | Tested |
|---|---|---|
| `sampleFieldAngles` | `|E|` on `2 + (n−1)·2n` directions about a centre, at `max(samplingRadius, 0.75·d)` | Yes (`core_tests.cpp:79-190`) |
| `radialSurface` | Radius perturbation `base·(1 + contrast·(2s−1))` from normalized `|E|` | Yes (`core_tests.cpp:79-190`) |
| `makeGrid3D` + `equipotentials` | Volume sampling and marching-tetrahedra isosurfaces | Yes (`core_tests.cpp:395-613`) |
| `sphere` | UV sphere mesh with potential-derived vertex colours | Yes (`core_tests.cpp:636-668`) |

`potentialLevel` and `pairedLevels` exist in `Parameters` and are validated but
have **no consumers** (`physics.hpp:21-23`; `physics.cpp:50-52`).

## 8. Arithmetic cost per rebuild

Derived by counting calls in the code paths, not measured:

| Operation | Default (`n=70`, density 14) | Maximum (`n=150`, density 30) |
|---|---|---|
| Grid: potential evaluations | 4 900 | 22 500 |
| Arrows: `field()` calls × 6 potential evaluations | 196 × 6 = 1 176 | 900 × 6 = 5 400 |
| Contour cell tests (17 passes) | 80 937 | 377 217 |
| Height mesh vertices | 28 566 | 133 206 |
| Streamlines: potential evaluations (observed 2 608 steps × 4 stages × 6) | ≈ 62 600 | worst case 1 536 000 |

Consequences worth stating: the **streamline build dominates** any physics
change, the **contour extraction dominates** a plane change, and arrow density
affects only the small second term. This asymmetry is why the code separates
`physicsChanged` from `planeChanged` (`renderer.cpp:110-114`).

Two inefficiencies to report honestly:

- changing only `fieldDensity` still rebuilds the grid, contours, height mesh and
  heatmap texture, because density is folded into `planeChanged`
  (`renderer.cpp:113-114`, `137-165`);
- contours are re-extracted in full for all 17 levels on every plane change,
  with no incremental update.

## 9. Precision and overflow decisions

- All core computation is `double`.
- World coordinates are bounded to ±10 000 m so that the float conversion for
  the GPU keeps usable resolution (`physics.cpp:39-44`;
  `core_tests.cpp:66-67`).
- Generated meshes are **triangle soup** rather than indexed, to avoid 16-bit
  index overflow at `150×150` and at the maximum sphere/radial resolutions
  (`physics.hpp:65`; `renderer.cpp:41-42`). The cost is duplicated vertices, not
  correctness.
- Rendering is unlit vertex colour with no normals or texture coordinates
  (`renderer.cpp:26-47`).
