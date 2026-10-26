# Xpotencial — research documentation

This directory documents the simulator at a level suitable for writing a
**research article**: the physical model, the numerical methods, the software
architecture, the visual encodings, the verification evidence, the limitations,
and a blueprint mapping paper sections to the evidence available in this
repository.

It is deliberately *not* a user manual. Usage, controls and build instructions
stay in [`../cpp/README.md`](../cpp/README.md).

## Document map

| # | Document | Answers |
|---|---|---|
| 01 | [Objectives and scope](01-objectives-and-scope.md) | What the system claims to be, what it contributes, what it does not cover |
| 02 | [Physical model](02-physical-model.md) | Exact equations, units, softening, charge geometry, reference values |
| 03 | [Numerical methods](03-numerical-methods.md) | Grids, finite differences, marching squares, marching tetrahedra, RK4 streamlines, meshes |
| 04 | [Software architecture](04-software-architecture.md) | Modules, data ownership, frame loop, cache invalidation, GPU lifecycle |
| 05 | [Visualization and encodings](05-visualization-and-encodings.md) | The five views, colour scale, arrow length, height normalization, probe panel |
| 06 | [Verification and validation](06-verification-and-validation.md) | Test inventory, tolerances, measured output, coverage gaps |
| 07 | [Limitations and threats to validity](07-limitations-and-threats-to-validity.md) | Everything a reviewer will ask about, including known code/doc mismatches |
| 08 | [Reproducibility](08-reproducibility.md) | Build matrix, commands, dependencies, artifact expectations |
| 09 | [Article blueprint](09-article-blueprint.md) | Section plan, figures, tables, claims-to-evidence matrix, missing experiments |

## Evidence convention

Every technical statement carries a reference in the form
`path/to/file.cpp:line` or names the command that produced it. If a statement
cannot be traced to code, tests, or an executed command, it is marked
**assumption** or **not implemented**.

Citations were accurate at commit `d747f8b` (branch `docs/research-article`).
Line numbers may drift after later edits; the symbol and file names remain the
stable anchors.

## What these documents are worth

They are a faithful description of **what the code does**, not proof that the
code is right. The distinction matters for a paper:

- Section 06 separates what is *asserted by tests* from what is *verified on a
  machine* from what is *unverified*.
- Section 07 lists the places where the existing prose documentation and the
  implementation disagree.
- Section 09 states which paper claims are already supportable and which still
  require experiments that have not been run.

## Provenance of the numbers

Reference values in sections 02, 03 and 06 were produced by linking
`cpp/build-linux/libdipole_core.a` with a throwaway probe program that calls the
public API (`dipole::potential`, `dipole::field`, `dipole::makeGrid`,
`dipole::displayLimit`, `dipole::streamlines`) using default parameters, built
with `g++ -std=c++17 -O2`. The probe is not committed; section 08 gives the
commands needed to rebuild the library and reproduce the values.
