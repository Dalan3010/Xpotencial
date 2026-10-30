# 01 — Objectives and scope

## 1. What this artifact is

Xpotencial is a standalone desktop simulator that visualizes the electrostatic
potential and field of a two-point-charge system in five synchronized views. It
is written in C++17 with raylib 5.5 for windowing and rendering, and it keeps a
double-precision numerical core that compiles and tests **without** graphics,
display, or network access.

The system is educational and exploratory. It is a numerical instrument for
building intuition about scalar fields, level sets, gradients, and field-line
geometry — not a metrology tool.

## 2. Problem statement

Static potential fields are usually taught through two incompatible
representations: analytic formulas on a blackboard, and pre-rendered textbook
figures. The first gives no spatial intuition; the second gives no ability to
perturb parameters. Bridging them requires a tool where the parameter set, the
representation, and the numerical derivation are visible at the same time.

The specific problem this system addresses:

> Given a user-chosen charge configuration, derive the potential field
> numerically, present it through several complementary encodings in real time,
> and expose the intermediate quantities (softened distances, substituted
> formulas, gradient components) so that every rendered feature can be traced
> back to the model that produced it.

## 3. Contributions as implemented

Each item below is present in the code and traceable. None of them is claimed as
a novel scientific contribution; they are engineering and didactic contributions
that an article can describe, evaluate, and critique.

| # | Contribution | Evidence |
|---|---|---|
| C1 | A single potential kernel reused for every derived quantity: heatmap, contours, height surface, arrows, streamlines, volume extraction and the live probe. | `cpp/src/physics.cpp:17-26`; every consumer calls `evaluate` (`physics.cpp:70-82`, `181-185`, `290-292`) |
| C2 | Adaptive softening tied to a configuration-dependent reference length, making charge singularities finite without changing the visual semantics between dipole and single-charge modes. | `cpp/src/physics.cpp:65-69` |
| C3 | Five coupled encodings of the same field (scalar map, level sets, vector overlay, height field, 3D field lines) sharing one parameter state. | `cpp/src/renderer.cpp:167-230`, `231-393`; view selection `controls.cpp:137-138` |
| C4 | Real-time 2D level-set extraction with a deterministic asymptotic decider and exact-coordinate segment deduplication. | `cpp/src/physics.cpp:189-221` |
| C5 | 3D field-line integration with RK4 on the normalized field, seeded on a ring (dipole) or an equal-area spherical distribution (single charge), with cached geometry and decoupled animation. | `cpp/src/physics.cpp:96-172`; cache `renderer.cpp:110-134` |
| C6 | A live "show the substitution" panel that prints the evaluated point, softened radii, `p = q·d`, `V`, `|E|`, the numerical step `h`, and the conventions used. | `cpp/src/controls.cpp:233-374` |
| C7 | Strict separation of a GUI-free numerical core: the test executable links only the core and runs with no display. | `cpp/CMakeLists.txt:8-20` |
| C8 | Input validation at every public entry point with explicit domain bounds, so invalid parameters are rejected before any allocation. | `cpp/src/physics.cpp:32-57`; `controls.cpp:79-95` |

## 4. Non-goals

The code explicitly does not attempt the following. Stating this now prevents
over-claiming in an article.

- **No exact point-charge physics near the charges.** Softening regularizes the
  singularity by construction (`cpp/src/physics.cpp:17-26`). The README calls it
  an "educational numerical regularization" (`cpp/README.md:114-116`).
- **No dielectric, conducting, or boundary-condition modelling.** There are two
  fixed charges in free space and nothing else.
- **No time-dependent electromagnetism.** The field is static; the animated
  markers along streamlines illustrate direction, not propagation
  (`cpp/src/renderer.cpp:388`).
- **No measured performance guarantee.** No benchmark exists
  (`cpp/README.md:163`).
- **No automated visual validation.** Rendering, interaction, and long-session
  behaviour are outside the test target (`cpp/CMakeLists.txt:16-20`;
  `cpp/README.md:171-194`).
- **No export, persistence, or scripting layer.** No file output, no session
  format, no automation API.

## 5. System at a glance

```
                 +---------------------------+
                 |   user input (mouse/kbd)  |
                 +-------------+-------------+
                               |
                     controls.cpp  (authoritative state)
                               |
              +----------------+----------------+
              |                                 |
      physics.cpp (pure core)            renderer.cpp (GPU/caches)
              |                                 |
        tests/core_tests.cpp                raylib / OpenGL 3.3
```

- `physics.cpp` is pure computation over `double`; it never touches raylib
  (`cpp/src/physics.cpp:12-15`).
- `controls.cpp` owns the authoritative `Parameters`, `Visibility`, current view,
  help state, probe state, and editing state (`cpp/src/controls.hpp:13-34`).
- `renderer.cpp` keeps a copy of the parameters plus every cache derived from
  them (`cpp/src/renderer.hpp:19-34`).
- `main.cpp` owns the window, the CLI, the frame loop and the dirty flag
  (`cpp/src/main.cpp:9-73`).

## 6. Terminology used throughout these documents

| Term | Meaning here |
|---|---|
| Reference length `L` | `d` for the dipole; `0.5·min(spanX, spanY)` for a single charge (`physics.cpp:65-68`) |
| Softening `ε` | `max(0.001·L, 1e-6)` metres (`physics.cpp:69`) |
| Display limit | 95th percentile of `|V|` over the grid, floored at `1e-9`, used for colour and contour levels (`physics.cpp:410-421`) |
| Height peak | Maximum sampled `|V|` over the grid, floored at `1e-9`; the height surface, its lifted contours and its charge markers all use `visualHeight(V, peak, H)` (`physics.cpp:422-431`) |
| Plane plot | The 2D views (indices 0-2) rendered at `z = center.z` |
| Stream view | View index 4, the 3D field-line / equipotential-circle view |
| Height view | View index 3, the 3D surface |
| Probe | The mouse-anchored evaluation point reported in the help panel (`controls.cpp:233-245`) |

## 7. Evidence levels used in these documents

| Level | Meaning |
|---|---|
| **Implemented** | Present in code, cited by line |
| **Tested** | Asserted by `dipole_tests` (see section 06) |
| **Verified** | Observed by a command run on the development machine |
| **Assumption** | Plausible but not established by code, tests, or measurement |
| **Not implemented** | Absent; do not claim it |
