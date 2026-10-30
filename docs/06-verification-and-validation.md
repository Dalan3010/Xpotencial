# 06 — Verification and validation

This document separates three different things that are easy to conflate in an
article:

- **Verification** — does the code compute what it claims to compute? (tests)
- **Runtime evidence** — does it run on a real machine? (build, CTest, smoke)
- **Validation** — is the model right, and are the figures faithful? (largely
  absent; see section 07)

## 1. Test harness

| Property | Value | Evidence |
|---|---|---|
| Executable | `dipole_tests`, built from one file | `cpp/CMakeLists.txt:16-20` |
| Links | `dipole_core` only — no raylib, no display, no network | `cpp/CMakeLists.txt:17-19` |
| CTest registration | one test, `dipole_core_behavior` | `cpp/CMakeLists.txt:20` |
| Assertion style | custom `check(bool, msg)` increments a global counter and throws on failure | `core_tests.cpp:11-15` |
| Tolerance helper | `near(a, b, tol = 1e-8)`, absolute tolerance | `core_tests.cpp:16-18` |
| `assert()` | never used, so Release builds still test | `cpp/README.md:57` |
| Driver | runs groups in fixed order; prints `PASS: <N> active behavior checks` or exits 1 on the first failure | `core_tests.cpp:676-681` |

**Read the count correctly.** `N` counts every executed `check` inside loops, not
distinct behaviours. The current run reports `1 110 185`, which mostly reflects
per-vertex and per-probe assertions over dense grids and meshes. An article
should say "about 1.1 million assertions from a fixed suite of behavioural
groups", never "1.1 million tests".

## 2. Reference run on the development machine

```
$ ctest --test-dir cpp/build-linux --output-on-failure
1/1 Test #1: dipole_core_behavior ... Passed  0.09 sec
100% tests passed out of 1
Total Test time (real) = 0.10 sec

$ ./dipole_tests
PASS: 1110185 active behavior checks
```

Both were executed while preparing this documentation, on the Linux build
described in section 08. The count is deterministic for a given build; the time
is not, and should not be quoted as a benchmark.

## 3. Inventory by area

Line ranges are the function bodies in `cpp/tests/core_tests.cpp`.

### 3.1 Physics and input validation (`physicsTests`, 19-74)

Default charge geometry and separation; bisector symmetry (`V = 0`); the
`nC × 10⁹` convention checked numerically; odd symmetry under mirrored
evaluation; `q` and `k` scaling; azimuth and polar orientation; translation
invariance; finiteness at the softened singularity; adaptive `ε = 0.002` at
defaults; field direction; central-difference agreement with the analytic
derivative at `1e-5`; acceptance of the default parameter set; rejection of
empty domains, `NaN` charge, infinite centre, `resolution = 151`,
`sphereResolution = 101`, zero separation, density 31, negative radius,
`xmax = 20000`, `polar = 181`; acceptance of both resolution extremes.

### 3.2 Radial surface (`radialTests`, 79-190)

Effective sampling radius; association between sampled magnitude and displayed
radius; finite bounded radii; bulges and depressions; zero and negligible
contrast; non-finite rejection; topology at `n = 10, 40, 80` (outward winding,
non-parallel normals, two-face edge incidence, exact vertex count); `q`/`k`
scaling and normalized invariance; translation; azimuth and polar rotation;
reflection symmetry; effective-radius safety; ten invalid parameter modes;
boundary parameters accepted.

### 3.3 Dipole streamlines (`streamlineTests`, 191-279)

Seed count; every path terminates on the negative charge; bounded point count;
seed shell radius; endpoint proximity; finiteness; tangent alignment with the
field above 0.95; mirror symmetry; translation and rotation equivariance;
step-limit and arc-limit terminations; an alternate 3D orientation; escape
termination; invalid options and parameters rejected.

### 3.4 Single charge (`singleChargeTests`, 280-338)

Centre placement; unused partner at the centre; `L = 5`; `ε = 0.005`;
`V = k·q/r`; `|E| = k·q/r²`; radial direction; translated centre; 225 positive
grid values; no zero contour; `chargeCount` validation; and a dipole regression
check that the two-charge path is unchanged.

### 3.5 Monopole streamlines (`monopoleStreamlineTests`, 339-394)

Seed count; never reaches a sink; all paths escape; bounded; seeds on the
reference sphere; finiteness; strictly increasing radius; translation; step and
arc limits; invalid count rejected.

### 3.6 Volume and equipotential surfaces (`volumeTests`, 512-613)

`21³` count and endpoints; reflection antisymmetry; zero-plane area 36 with no
paired duplication; `±4.5` lobes on the correct sides; empty results above
range; boundary clipping; rotated and translated plane within 0.035;
affine linear-field interpolation exact to `1e-12`; coplanar face
deduplication; constant volume empty; `60³` maximum; fine-grid residual
improvement and residual `< 0.12`; surface normal alignment with `E` above
0.97; more than 100 probes; bounds rejected.

### 3.7 Affine topology (`affinePlaneTopologyTests`, 395-511)

Four analytic sections at grid sizes 5 and 9: vertices on the plane, finite and
inside the box, no duplicate faces, positive area, analytic area and perimeter
to `1e-10`, boundary and interior edge incidence, shared grid-face edges
exercised, `13×13` projected probes covered exactly once.

### 3.8 2D geometry, sphere and heightfield (`geometryTests`, 614-674)

Grid count and endpoints; zero bisector non-degenerate; constant cell empty;
saddle yields two segments; exact edge yields one segment; isolated exact vertex
yields nothing; non-finite level rejected; sphere vertex count, radius, poles,
watertightness, outward winding, `q`-independent geometry and `q`-scaled vertex
values; `150²` heightfield vertex count and bounded `|z|`. The height mapping is
covered separately by `heightEncodingTests`: `peakMagnitude` returns the max
`|V|` and rejects empty/non-finite samples; `visualHeight` maps `0 → 0`,
`±peak → ±H`, follows the square root at half scale, is strictly increasing
below the peak with no shared heights, and rejects invalid scales; a synthetic
grid spanning the peak yields `max |z| = H` with full height attained by exactly
the peak samples only (no percentile plateau).

## 4. Tolerances actually used

| Tolerance | Used for | Example |
|---|---|---|
| `1e-8` (default) | most exact geometric and scalar identities | `core_tests.cpp:16` |
| `1e-12` | shared grid coordinates, affine interpolation, projected denominator | `core_tests.cpp:466`, `497`, `562` |
| `1e-10` | analytic area and perimeter of plane sections | `core_tests.cpp:455`, `479` |
| `1e-9` | volume reflection, sphere radius, axial field components | `core_tests.cpp:518`, `642` |
| `2e-6` | radial scaling and rotation invariance | `core_tests.cpp:148-171` |
| `2e-5` | streamline mirror and transform equivariance | `core_tests.cpp:215`, `227` |
| `1e-5` | potential and central-difference accuracy | `core_tests.cpp:44` |
| `1e-4` | single-charge `|E|` | `core_tests.cpp:298` |
| `0.95` ratio | streamline tangent vs field direction | `core_tests.cpp:208` |
| `0.97` ratio | extracted-surface normal vs `E` | `core_tests.cpp:594` |
| `0.035` | rotated/translated zero-plane residual | `core_tests.cpp:553` |
| `0.12` | fine-grid extraction residual | `core_tests.cpp:592` |

## 5. What the suite establishes

- The potential kernel and its unit convention behave as documented under
  scaling, translation, rotation and reflection.
- The softened singularity is finite and the field is a consistent numerical
  gradient of the implemented potential.
- Level-set extraction handles the degenerate and ambiguous cases
  deterministically.
- Extracted 3D surfaces have correct topology and analytic area/perimeter for
  affine test fields.
- Generated meshes are watertight, outward-wound and correctly sized.
- Invalid parameters are rejected before allocation.

## 6. What the suite does not establish

| Gap | Consequence |
|---|---|
| Only `dipole_core` is tested | `renderer.cpp`, `controls.cpp`, `main.cpp`, `ui_text.cpp` have no automated tests |
| No golden/screenshot comparison | Visual output is unverified; a regression in colour, layout or camera would pass |
| No convergence study | `h = 0.05ε` is a code choice, not a validated optimum |
| No integrator error study | RK4 step `0.02L` is unvalidated against a reference solution |
| No performance measurement | No timing, memory, or frame-rate data exists in the repository |
| No long-session test | Leak absence is unverified |
| No manual UI protocol | Interaction correctness is explicitly pending (`cpp/README.md:191-194`) |
| `--smoke` is not in CTest | The only GUI runtime check must be run by hand |

## 7. The smoke run

`--smoke` forces `resolution = 150` and `fieldDensity = 30`, renders **8 frames
per view for all five views**, and exits after 40 frames with `SMOKE PASS`
(`main.cpp:10-12`, `28-31`, `37-48`, `61-65`). It aborts with exit code 1 if the
run exceeds 25 seconds or stops before 40 frames.

It establishes: window and GL context creation, that every render path executes
without throwing at maximum default grid and arrow settings, that cached
streamlines and animated markers draw, and that runtime is bounded.

It does **not** establish: pixel correctness, interaction, leak absence,
measured performance, or the single-charge circle path — the smoke run never
sets `chargeCount = 1`, so view 4's single-charge branch is not exercised.

## 8. Claim strength for the article

| Claim | Strength |
|---|---|
| "The numerical core reproduces the documented softened model" | **Supported** — tested |
| "Level-set extraction is deterministic including degenerate cases" | **Supported** — tested |
| "The system builds and runs on Linux with a real GPU" | **Supported on one machine** — verified |
| "The GUI is correct" | **Not supported** — untested |
| "The visuals are accurate to the model" | **Partially** — encodings documented and the height law is shared across overlays, but there is no visual verification |
| "Performance is adequate for real-time use" | **Not measured** — do not claim |
| "The regularized model is physically valid near the charges" | **Reframe required** — it is a smeared-charge model, not point charges |
