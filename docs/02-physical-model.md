# 02 — Physical model

All equations below are transcribed from the implementation, not from a
textbook. Where the code and the classic formula differ, the difference is
called out.

## 1. Potential

Let `r₊` and `r₋` be the positions of the positive and negative charge, and let
`q` be the charge magnitude, `k` the Coulomb constant. The implemented potential
at a point `p` is

```
d_i  = sqrt( |p − r_i|² + ε² )                  (softened distance)
V(p) = k·q·( 1/d₊ − 1/d₋ )                       (dipole)
V(p) = k·q·( 1/d₊ )                              (single charge)
```

Evidence: `cpp/src/physics.cpp:17-26`. The softened distance is computed inline
at `physics.cpp:19-22`; the single-charge branch skips the negative term at
`physics.cpp:23-24`; the dipole branch is `physics.cpp:25`.

The dipole is a **pair of equal-magnitude opposite charges**. There is no
independent per-charge magnitude in the data model
(`cpp/include/dipole/physics.hpp:13-26`): the same `q` scales both terms.

## 2. Softening and the reference length

```
L       = d                                (dipole)
L       = 0.5 · min( xmax−xmin, ymax−ymin ) (single charge)
ε       = max( 0.001·L, 1e-6 )  [m]
```

Evidence: `cpp/src/physics.cpp:65-69`; the reference length is described as a
"hidden length scale" in `cpp/include/dipole/physics.hpp:31`.

Consequences at the defaults (`q=1 nC`, `k=8.99`, `d=2 m`, domain ±5 m):

| Mode | `L` (m) | `ε` (m) | `h = max(1e-6, 0.05ε)` (m) |
|---|---|---|---|
| Dipole | 2 | 0.002 | 1e-4 |
| Single charge (span 10 m) | 5 | 0.005 | 2.5e-4 |

These values are asserted by tests (`cpp/tests/core_tests.cpp:39`, `290`, `326`).

### Interpretation of the softened kernel

`V = k·q / sqrt(r² + ε²)` is, mathematically, the potential of a **Plummer
sphere** of total charge `q` and softening length `ε`, whose volume density is
`ρ(r) = 3qε² / (4π (r² + ε²)^{5/2})`. The dipole is the superposition of a
positive and a negative Plummer kernel.

This identity is a standard result and is **not** stated in the code — the code
only documents that softening keeps the singularity finite. It matters for two
reasons in an article:

1. It gives a defensible physical reading of the regularization: the simulator
   is not integrating a point charge badly, it is integrating a smeared charge
   distribution exactly, then sampling it.
2. It predicts the observed behaviour at the charge centre: the softened
   potential has a smooth maximum there, so the numerical gradient is zero
   (see §5), whereas the point-charge field diverges.

### What softening does *not* touch

`ε` enters only through `evaluate` (`physics.cpp:17-26`). It therefore affects
every quantity derived from the potential — map, contours, height surface,
arrows, volume extraction and the probe. It is **not** applied to streamline
seed radii, termination radii, or the arrow saturation constant
(`physics.cpp:115-120`; `renderer.cpp:190-192`).

## 3. Units and the `k·q` convention

The core stores plain `double`s and documents no units
(`cpp/include/dipole/physics.hpp:13-26`). Units exist only in the UI labels
(`cpp/src/controls.cpp:15-16`) and in the prose documentation.

The convention is:

- `q` is entered in **nanocoulombs** (`1 nC = 1e-9 C`);
- `k` is entered in **units of 10⁹ N·m²/C²**, so the displayed `8.99` means
  `8.99e9`;
- the product is used **directly**, with no `1e-9` factor
  (`cpp/src/physics.cpp:24-25`).

This is dimensionally consistent:

```
V_SI = k_SI · q_SI / r
     = (k₉ · 1e9) · (q_nC · 1e-9) / r
     = k₉ · q_nC / r
```

The test suite pins the convention numerically: at `(2,0,0)` with defaults it
asserts `V = 8.99·(1/sqrt(1+ε²) − 1/sqrt(9+ε²))`
(`cpp/tests/core_tests.cpp:26`). Potential is reported in volts and field in
V/m (`cpp/src/controls.cpp:354`, `365`).

The only `1e-9` literal in the core is the colour-limit floor
(`cpp/src/physics.cpp:416`), which is unrelated to units.

## 4. Charge geometry

With azimuth `az` and polar `pol` in degrees, and `π` as a 21-digit literal
(`cpp/src/physics.cpp:11`):

```
az  = azimuth·π/180
pol = polar·π/180
offset = ( sin(pol)·cos(az), sin(pol)·sin(az), cos(pol) ) · d/2
r₊ = center + offset
r₋ = center − offset
```

Evidence: `cpp/src/physics.cpp:58-64`.

- `d` is the **full** separation, not the half-separation
  (`physics.cpp:62`; asserted at `core_tests.cpp:23`).
- The default orientation `az=0°, pol=90°` puts the charges at `(±1,0,0)` for
  `d=2` (`core_tests.cpp:22`).
- In single-charge mode both `r₊` and `r₋` are set to `center`; `r₋` is unused
  (`physics.cpp:59-60`).

## 5. Electric field

The field is obtained by **central differences of the analytic potential**, not
by an analytic derivative and not from a precomputed field grid:

```
h   = max( 1e-6, 0.05·ε )
E_i = −( V(p + h·ê_i) − V(p − h·ê_i) ) / (2h),   i ∈ {x,y,z}
```

Evidence: `cpp/src/physics.cpp:75-82`.

Properties worth stating in an article:

- **No boundary handling.** The stencil evaluates the analytic potential at
  `p ± h` regardless of whether those points lie outside the sampling domain.
  There is no one-sided stencil and no clamping (`physics.cpp:75-82`).
- **Truncation error is O(h²)** for a smooth potential. The suite compares the
  computed component against the analytic derivative of the softened potential
  at one configuration and requires agreement within `1e-5`
  (`cpp/tests/core_tests.cpp:42-44`).
- **The field is recomputed per evaluation.** Nothing caches a field grid; the
  arrow layer calls `field()` per sample (`renderer.cpp:148-153`).
- **At the charge centre the field is exactly zero.** With the softened kernel
  the potential is smooth and symmetric there, so the central difference cancels.
  This is a direct consequence of the model, not a bug, and it is visible in the
  reference values below.

## 6. Derived quantities exposed in the UI

| Quantity | Definition | Evidence |
|---|---|---|
| Electric moment | `p = q·d`, labelled `nC·m` with no SI conversion | `controls.cpp:344-348` |
| Softened radii | `r± = sqrt(raw² + ε²)` | `controls.cpp:251-254` |
| Field magnitude | `|E| = sqrt(Ex² + Ey² + Ez²)` | `controls.cpp:257`, `362-366` |
| Probe potential | `evaluate` at the probe point | `controls.cpp:255` |
| Probe field | `field` at the probe point | `controls.cpp:256` |

## 7. Reference values

Produced by calling the public core API with default parameters (see
`docs/README.md` for provenance). Dipole: charges at `(±1,0,0)`, `L=2`,
`ε=0.002`, `displayLimit = 5.5556275131 V`.

| Point (m) | `V` (V) | `E` (V/m) | `|E|` (V/m) |
|---|---|---|---|
| `(0,0,0)` bisector | 0 | `(-17.979892, 0, 0)` | 17.979892 |
| `(2,0,0)` near `+q` | 5.99331602 | `(7.991058, 0, 0)` | 7.991058 |
| `(-2,0,0)` near `−q` | −5.99331602 | `(7.991058, 0, 0)` | 7.991058 |
| `(0,1,0)` bisector | 0 | `(-6.356871, 0, 0)` | 6.356871 |
| `(5,0,0)` boundary | 0.74916647 | `(0.312153, 0, 0)` | 0.312153 |
| `(0,0,4)` off the slice plane | 0 | `(-0.256517, 0, 0)` | 0.256517 |

Single charge at the origin, `L=5`, `ε=0.005`, `displayLimit = 7.0920973179 V`:

| Point (m) | `V` (V) | `E` (V/m) | `|E|` (V/m) |
|---|---|---|---|
| `(0,0,0)` centre | 1798.00000000 | `(0,0,0)` | 0 |
| `(1,0,0)` | 8.98988763 | `(8.989663, 0, 0)` | 8.989663 |
| `(2,0,0)` | 4.49498595 | `(2.247479, 0, 0)` | 2.247479 |
| `(5,5,0)` corner | 1.27137767 | `(0.127138, 0.127138, 0)` | 0.179800 |
| `(0.01,0,0)` | 804.09004471 | `(64343.284007, 0, 0)` | 64343.284007 |

Two observations that belong in a results section:

- The dipole field on the bisector points from `+q` to `−q` and its magnitude
  falls with distance from the axis, as expected.
- The single-charge potential at the centre is large but finite
  (`k·q/ε = 8.99/0.005 = 1798 V`), and the field there is zero. Any figure of
  the near-charge region therefore shows a flattened peak, not a spike. That is
  the visual signature of softening and must be explained, not hidden.

## 8. Model assumptions a reviewer will ask about

| Assumption | Status |
|---|---|
| Electrostatics, no time dependence | By construction: no time derivative anywhere |
| Free space, no dielectric or conductors | By construction: only two charges exist |
| Equal-magnitude opposite charges | By construction: one `q` scales both terms |
| Charge magnitude always positive in the UI | `q ∈ [0.1, 5]` (`physics.cpp:37`); a negative `q` is unreachable |
| Smeared charges of radius `ε` | Consequence of the softened kernel (see §2) |
| Infinitesimal `h` for the gradient | `h = 0.05ε`, i.e. 5 % of the smoothing scale; chosen in code, not derived |
| Field lines are illustrative, not complete | 16 seeds on one ring/one sphere (see section 03) |
