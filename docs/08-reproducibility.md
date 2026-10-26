# 08 — Reproducibility

## 1. What is reproducible today

| Artifact | Reproducible | How |
|---|---|---|
| Numerical core build | Yes | `cmake` + any C++17 compiler, no dependencies |
| Test suite and its count | Yes | `ctest` or the `dipole_tests` binary |
| GUI build | Yes, with network or a local raylib 5.5 tree | `-DDIPOLE_BUILD_GUI=ON` |
| GUI runtime | Only on a machine with a display and OpenGL 3.3 | run the binary |
| Screenshots | Not captured by the project | manual |
| Timing/memory numbers | Not available | must be measured (section 07) |

There is no CI workflow, no `CMakePresets.json`, no install rule, and no
dependency lockfile beyond the raylib git tag.

## 2. Toolchain requirements

| Requirement | Version | Evidence |
|---|---|---|
| CMake | ≥ 3.20 | `cpp/CMakeLists.txt:1` |
| C++ | C++17, extensions off | `cpp/CMakeLists.txt:3-5` |
| raylib | 5.5 exactly, GUI only | `cpp/CMakeLists.txt:22-45` |
| OpenGL | 3.3, runtime | `renderer.cpp:45`; `cpp/README.md:11-13` |
| Linux X11 dev packages | for raylib's default GLFW backend | `cpp/README.md:26-32` |
| Git | first GUI configure (FetchContent) | `cpp/CMakeLists.txt:41-45` |

Audio is disabled in the raylib build; examples, games, screen capture, GIF and
clipboard-image paths are also disabled (`cpp/CMakeLists.txt:26-33`).

## 3. Build matrix

| Option | Default | Effect |
|---|---|---|
| `DIPOLE_BUILD_GUI` | `ON` | `OFF` skips raylib entirely and builds only the core plus tests |
| `DIPOLE_USE_SYSTEM_RAYLIB` | `OFF` | `ON` uses `find_package(raylib 5.5 EXACT)` |
| `FETCHCONTENT_SOURCE_DIR_RAYLIB` | unset | points at a local unmodified raylib 5.5 source tree for offline builds |

Evidence: `cpp/CMakeLists.txt:6-7`, `22-45`.

Targets:

- `dipole_core` — static library from `src/physics.cpp` (`CMakeLists.txt:8-9`)
- `dipole_tests` — from `tests/core_tests.cpp`, links `dipole_core` (`16-19`)
- `dipole_simulator` — from `main.cpp`, `renderer.cpp`, `controls.cpp`,
  `ui_text.cpp`, links `dipole_core` and raylib (`47-52`)

Warnings: `/W4` under MSVC; `-Wall -Wextra -Wpedantic` for the core library on
other toolchains (`CMakeLists.txt:10-14`).

## 4. Commands

### 4.1 Headless core and tests (no display, no download)

```sh
cmake -S cpp -B cpp/build-headless -DCMAKE_BUILD_TYPE=Release -DDIPOLE_BUILD_GUI=OFF
cmake --build cpp/build-headless
ctest --test-dir cpp/build-headless --output-on-failure
```

Expected: `1/1 Test #1: dipole_core_behavior ... Passed`, ~0.1 s total.

### 4.2 GUI on Linux

```sh
cmake -S cpp -B cpp/build-linux -DCMAKE_BUILD_TYPE=Release -DDIPOLE_BUILD_GUI=ON
cmake --build cpp/build-linux
./cpp/build-linux/Release/dipole_simulator
./cpp/build-linux/Release/dipole_simulator --smoke
```

### 4.3 GUI on Windows (MSVC)

```powershell
cmake -S cpp -B cpp/build -G "Visual Studio 16 2019" -A x64 -DDIPOLE_BUILD_GUI=ON
cmake --build cpp/build --config Release
ctest --test-dir cpp/build -C Release --output-on-failure
.\cpp\build\Release\dipole_simulator.exe --smoke
```

### 4.4 Offline GUI build

Two supported routes (`cpp/CMakeLists.txt:34-40`, `cpp/README.md:146-152`):

```sh
# (a) point at an unmodified raylib 5.5 source tree
cmake -S cpp -B cpp/build-offline -DDIPOLE_BUILD_GUI=ON \
      -DFETCHCONTENT_SOURCE_DIR_RAYLIB=/abs/path/to/raylib-5.5

# (b) use an installed raylib 5.5 CMake package
cmake -S cpp -B cpp/build-system -DDIPOLE_BUILD_GUI=ON \
      -DDIPOLE_USE_SYSTEM_RAYLIB=ON -DCMAKE_PREFIX_PATH=/abs/prefix
```

## 5. Expected outputs

| Command | Expected |
|---|---|
| `./dipole_tests` | `PASS: 1110185 active behavior checks`, exit 0 |
| `ctest` | `1/1 ... Passed`, exit 0 |
| `--smoke` | one log line per view and stage, then `SMOKE PASS`; exit 0 within 25 s |
| `--help` | usage text, exit 0 |
| unknown argument | exit code 2 |

The assertion count is deterministic for a given source revision. If it changes,
either tests were added or a loop bound changed; it is not a pass/fail signal by
itself.

## 6. Reproducing the reference values in sections 02 and 03

The numbers in this documentation came from a throwaway program linked against
the built core. The same approach reproduces them without a display:

```cpp
// build: g++ -std=c++17 -O2 -I cpp/include probe.cpp cpp/build-linux/libdipole_core.a -o probe
#include "dipole/physics.hpp"
#include <cstdio>
using namespace dipole;

int main() {
    Parameters p;                                  // dipole defaults
    std::printf("L=%.6f eps=%.9f limit=%.9f\n",
                referenceLength(p), softening(p), displayLimit(makeGrid(p).values));
    Vec3 q{0,0,0};
    Vec3 e = field(q, p);
    std::printf("V(0)=%.9f E=(%.6f,%.6f,%.6f)\n", potential(q, p), e.x, e.y, e.z);

    StreamlineOptions o; o.seedCount = 16; o.seedAngleDegrees = 72; o.stepSize = 0.02;
    size_t points = 0, sinks = 0;
    for (const auto& line : streamlines(p, o)) {
        points += line.points.size();
        if (line.termination == StreamlineTermination::ReachedNegativeCharge) ++sinks;
    }
    std::printf("paths=16 points=%zu sinks=%zu\n", points, sinks);
}
```

Expected with defaults: `L=2.000000 eps=0.002000000 limit=5.555627513`,
`V(0)=0.000000000`, `E=(-17.979892,0,0)`, `paths=16 points=2624 sinks=16`.

## 7. What a replication package should add

The repository as shipped is a source dump, not a replication package. Before
submission it should add, at minimum:

1. a frozen commit hash and a build script that pins the raylib tag;
2. the probe program used for reference values, under `tools/`;
3. the convergence and timing harnesses, with raw CSV output;
4. the exact command lines and environment (`compiler --version`, `cmake
   --version`, GPU driver version);
5. the screenshot set with the parameter values used to produce each figure.

Nothing in items 1-5 exists today; stating that plainly is stronger than
implying reproducibility that was never exercised.
