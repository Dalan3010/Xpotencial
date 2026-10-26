# 09 — Article blueprint

A concrete plan for turning the other documents into a paper. It states what can
be written **today**, what needs **new experiments**, and what must **not** be
claimed.

## 1. Positioning

The honest framing is a **software-and-methods paper**, not a physics discovery
paper. Nothing here is a new law or a new numerical scheme; the contribution is
an integrated instrument with an unusually explicit numerical/didactic layer.

Candidate framings, strongest first:

1. **"A multi-encoding instrument for electrostatic field intuition, with an
   auditable numerical core."**
2. "Five synchronized visual encodings of one potential field: design,
   implementation, and verification."
3. "Regularized point-charge visualization: making softening explicit."

Avoid titles implying accuracy guarantees, isosurface extraction, or learning
outcomes — none of those are supported.

## 2. Abstract skeleton (150–200 words)

1. Problem: static field concepts are taught through formulas or static figures,
   neither of which supports perturbation or traceability.
2. Artifact: a C++17/raylib desktop simulator with five coupled encodings and a
   GUI-free double-precision core.
3. Method: one softened-Coulomb kernel, central-difference gradients, marching
   squares, RK4 field lines, percentile-clipped colour mapping.
4. Evidence: a GUI-free behavioural suite, a bounded GUI smoke run, and
   reproducible reference values.
5. Honest boundary: regularized rather than point-charge physics; GUI not
   automatically tested; no performance claim.
6. Availability: repository, build commands, exact reference values.

## 3. Section plan

| Section | Content | Pull from |
|---|---|---|
| 1. Introduction | Motivation, gap, contributions C1-C8 | doc 01 |
| 2. Related work | Compare with field-visualization tools and teaching simulators. **Pending** — needs a literature pass | — |
| 3. Physical model | Equations, units, softening, Plummer interpretation, assumptions | doc 02 |
| 4. Numerical methods | Grids, contours, height, tetrahedra, sphere mesh, RK4, cost model | doc 03 |
| 5. Software architecture | Module split, state ownership, caching, GPU lifecycle, UI validation | doc 04 |
| 6. Visual encodings | The five views, colour, arrows, height, known distortions | doc 05 |
| 7. Verification | Suite design, inventory, tolerances, reference run, gaps | doc 06 |
| 8. Reference results | Reference values, streamline statistics, (optionally) convergence and timing | docs 02, 03, 06 |
| 9. Discussion | What the encodings teach, where they mislead, design trade-offs | docs 05, 07 |
| 10. Limitations | Softening, unvalidated constants, untested GUI, single-machine evidence | doc 07 |
| 11. Conclusion and future work | Convergence, corrected overlays, usability study, export | doc 07 §7 |

Suggested length: 8–10 pages in a two-column format, with 6–8 figures.

## 4. Figures

| ID | Content | Source | Status |
|---|---|---|---|
| F1 | System pipeline: controls → core → caches → GPU | derived from doc 04 | ready to draw |
| F2 | Three 2D views side by side (map, contours, vectors), defaults | views 0-2 | capture needed |
| F3 | Height surface with the sqrt law and its caption | view 3 | capture needed; **fix overlay mismatch first** |
| F4 | Dipole field lines with markers | view 4, dipole | capture needed |
| F5 | Single-charge concentric circles | view 4, single charge | capture needed |
| F6 | Help/probe panel showing substituted values | help overlay | capture needed |
| F7 | Gradient error vs step `h` (log-log) | new experiment | **must be run** |
| F8 | Work per rebuild or frame time vs resolution | new experiment | **must be run** |

Caption rules that follow from the code:

- Always state `q`, `k`, `d`, orientation, domain, `resolution`, and the
  **display limit** in volts. Without the limit the colours are unreadable.
- For view 3, state that the vertical axis is `sign(V)·sqrt(|V|/limit)·H`.
- For view 4, state the seed count, seed angle, step size, and that markers show
  direction, not velocity.
- Never label the single-charge circles as extracted isosurfaces.

## 5. Tables

| ID | Content | Source |
|---|---|---|
| T1 | Parameters, defaults, enforced ranges | doc 04 §2 |
| T2 | Numerical methods summary (method, parameter, value) | doc 03 |
| T3 | Test inventory by area | doc 06 §3 |
| T4 | Tolerances used by the suite | doc 06 §4 |
| T5 | Reference values (dipole and single charge) | doc 02 §7 |
| T6 | Coverage gaps | doc 06 §6 |
| T7 | Measured behaviour (16/16 sinks, 2 624 points) | doc 03 §6 |

## 6. Claims-to-evidence matrix

| Claim you may want to write | Evidence available | Verdict |
|---|---|---|
| One kernel drives every view | `physics.cpp:17-26` plus all consumers | **Write it** |
| Softening is adaptive and configuration-aware | `physics.cpp:65-69`; tests at `core_tests.cpp:39`, `290`, `326` | **Write it** |
| Level sets handle saddles deterministically | `physics.cpp:213-218`; tests `614-634` | **Write it** |
| Extracted 3D surfaces match analytic area/perimeter | tests `395-511`, `512-613` to `1e-10` | **Write it** |
| Field lines all reach the sink for a dipole | reference run: 16/16 | **Write it** (defaults only) |
| The gradient is accurate to 1e-5 | `core_tests.cpp:42-44`, one configuration | **Write it with the qualifier** |
| The system renders in real time | no measurement | **Do not write** |
| The simulation is accurate near the charges | softening changes the model | **Reframe** as smeared charge |
| The 3D view is a quantitative surface | sqrt law + linear overlays | **Do not write** |
| The GUI behaves correctly | untested | **Do not write** |
| The tool improves learning | no study | **Do not write** |

## 7. Results you can report right now

These numbers were produced while documenting and are reproducible from the
public API (section 08 §6):

- Default dipole: `L = 2 m`, `ε = 0.002 m`, `h = 1e-4 m`,
  display limit `5.5556275131 V`.
- Single charge on a ±5 m domain: `L = 5 m`, `ε = 0.005 m`,
  display limit `7.0920973179 V`.
- Dipole bisector field at the origin: `−17.979892 V/m` (pointing from `+q`
  to `−q`).
- Single charge: `V(0) = 1798 V`, `E(0) = 0`; `V(1 m) = 8.9899 V`,
  `|E(1 m)| = 8.9897 V/m`.
- Streamlines: 16 paths, 2 624 points, all 16 terminate on `−q`, no path
  escapes or hits a budget limit.
- Test suite: 1 110 185 assertions, CTest 0.09 s, one test.

## 8. Results that require new experiments

See doc 07 §7 for the full list. The three that most strengthen a submission:

1. **Gradient convergence** — refine `h` and plot the error against the analytic
   softened derivative. The reference already exists in `core_tests.cpp:42-44`,
   so this is a small harness.
2. **Spatial convergence** — refine the grid and measure contour area/perimeter
   and extracted surface residual. The suite already computes a residual
   improvement; report the curve, not just the pass.
3. **Cost model** — time a plane rebuild and a physics rebuild across
   resolutions and seed counts. Doc 03 §8 gives the expected shape; measurement
   would replace "derived arithmetic" with data.

## 9. Writing cautions

- **Say "softened" every time you say "point charge".** The model is a pair of
  Plummer kernels.
- **Never quote the assertion count as a test count.** It is a loop-expanded
  counter.
- **Do not present `--smoke` as validation.** It proves execution, not output.
- **Do not use view 3 quantitatively** until the overlay mapping is unified.
- **State the colour clipping.** A reader who assumes a linear absolute colour
  scale will misread every figure.
- **Report the single-machine limitation** in the threats-to-validity
  subsection, not in a footnote.
- **Distinguish implemented-but-unrendered code** (volume extraction, radial
  surface, sphere mesh) from features the user can see. A reviewer who reads the
  tests will notice them and ask.

## 10. Suggested contribution statements

Copy-ready, each defensible from the code:

1. We present an interactive simulator that derives five visual encodings from a
   single softened-Coulomb kernel, with a GUI-free numerical core.
2. We make the numerical regularization explicit in the interface: the probe
   panel reports softened radii, the gradient step, and the substituted
   equations, so every rendered feature is traceable to its model.
3. We describe a deterministic level-set pipeline that resolves saddle ambiguity
   with an asymptotic decider and deduplicates shared segments by exact
   coordinates.
4. We report a behavioural test suite of about 1.1 million assertions covering
   the numerical core, together with an explicit inventory of what remains
   unverified.
