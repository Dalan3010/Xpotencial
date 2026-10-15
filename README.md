# Native electric-dipole simulator

A standalone **C++17 / raylib 5.5** desktop simulator for the electrostatic
potential of an electric dipole: five interactive views, a double-precision
numerical core, and no runtime dependency beyond an OpenGL 3.3 driver.

The project is a plain CMake project under `cpp/`. Detailed build, run, control
and physics documentation lives in **[cpp/README.md](cpp/README.md)** — this
file is only the entry point.

## Repository layout

```
.
├── cpp/
│   ├── CMakeLists.txt            # dipole_core (library) + dipole_simulator (GUI) + dipole_tests
│   ├── include/dipole/physics.hpp # numerical core public API
│   ├── src/
│   │   ├── physics.cpp           # V, E, grids, contours, sphere geometry
│   │   ├── renderer.cpp          # five views, cached GPU resources, streamlines
│   │   ├── controls.cpp          # native sidebar, numeric editing, help overlay
│   │   └── main.cpp              # window, --smoke flag, frame loop
│   ├── tests/core_tests.cpp      # CTest suite; no graphics and no network
│   └── README.md                 # full documentation
├── odd/                          # development task records
└── README.md                     # this file
```

`cpp/` is not a nested Git repository; everything is versioned from the root.

## Quick start

### Headless core — no raylib, no display, no download

```sh
cmake -S cpp -B cpp/build-headless -DCMAKE_BUILD_TYPE=Release -DDIPOLE_BUILD_GUI=OFF
cmake --build cpp/build-headless
ctest --test-dir cpp/build-headless --output-on-failure
```

### Desktop GUI

```sh
cmake -S cpp -B cpp/build-linux -DCMAKE_BUILD_TYPE=Release -DDIPOLE_BUILD_GUI=ON
cmake --build cpp/build-linux
./cpp/build-linux/Release/dipole_simulator
./cpp/build-linux/Release/dipole_simulator --smoke   # renders all five views, then exits
```

Windows/MSVC commands, system prerequisites and offline builds are documented in
[cpp/README.md](cpp/README.md).

## The five views

| # | View |
|---|---|
| 1 | 2D potential map with a color legend |
| 2 | Equipotential contours (17 levels, CPU marching squares) |
| 3 | Electric-field vector overlay |
| 4 | Potential height surface `V(x,y)` |
| 5 | 3D dipole with cached field streamlines and direction markers |

## Physics conventions

```
V = k·q·(1/r₊ − 1/r₋)
r = √(dx² + dy² + dz² + ε²),   ε = max(0.001·d, 1e-6 m)
E = −∇V   (centered finite differences)
```

Numerical `k·q` already compensates `q` in nC and `k` in units of 10⁹ SI, so no
extra `1e-9` factor is applied. Softening makes charge singularities finite: an
educational numerical regularization, not an exact point-charge model near the
charges. The full interpretation of every plot is in
[cpp/README.md](cpp/README.md#read-the-plots-correctly).

## Verification status

- Headless configure, build and `ctest` (1/1 `dipole_core_behavior`) pass on Linux.
- The GUI target compiles and links on Linux against a locally available raylib
  5.5 source tree.
- GUI runtime, screenshot quality, manual interaction, long-session behaviour
  and distribution/driver portability are **not** established here; see the
  verification section of [cpp/README.md](cpp/README.md#verification-status).

## History

Earlier revisions of this repository carried a browser version
(`index.html` + `js/` + `css/`, Plotly.js and KaTeX from a CDN). That layer has
been removed; it remains recoverable from git history.
