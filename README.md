# Thread Contention Visualizer (C++)

A resume-ready C++ project that simulates and visualizes thread contention, semaphore bottlenecks, and potential deadlock-like stalls in concurrent systems.

## What this project demonstrates

- Multithreading with `std::thread`
- Mutex contention with `std::timed_mutex`
- Limited shared capacity using `std::counting_semaphore`
- Runtime contention monitoring and deadlock warning heuristics
- Performance analysis via per-thread wait/ retry metrics

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Windows (step-by-step)

Yes — this project is designed to run on Windows directly with a C++20 compiler.

### Option A: Visual Studio 2022 (recommended)

1. Install **Visual Studio 2022 Community**.
2. In the installer, enable **Desktop development with C++**.
3. Open **x64 Native Tools Command Prompt for VS 2022**.
4. Go to the project folder:

   ```bat
   cd C:\path\to\Thread-contention-visualizer
   ```

5. Configure with CMake:

   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   ```

6. Build:

   ```bat
   cmake --build build --config Release
   ```

7. Run:

   ```bat
   .\build\Release\thread_contention_visualizer.exe
   ```

8. Run with custom parameters:

   ```bat
   .\build\Release\thread_contention_visualizer.exe 10 3 2 120
   ```

### Option B: MinGW-w64 + CMake

1. Install **MSYS2** (or another MinGW-w64 distribution).
2. Install `mingw-w64-x86_64-gcc` and `mingw-w64-x86_64-cmake`.
3. Open **MSYS2 MinGW x64** shell.
4. Go to the project folder and run:

   ```bash
   cmake -S . -B build -G "MinGW Makefiles"
   cmake --build build -j
   ./build/thread_contention_visualizer.exe
   ```

### Notes for Windows users

- Requires a compiler with **C++20** support (needed for `std::counting_semaphore`).
- If you see compiler-standard errors, rebuild with a modern toolchain (VS 2022 v143+ or recent GCC/Clang).

## Run

```bash
./build/thread_contention_visualizer [threads] [resources] [semaphore_slots] [iterations]
```

Example:

```bash
./build/thread_contention_visualizer 10 3 2 120
```

Arguments (all optional):

1. `threads` (default `8`)
2. `resources` (default `4`, must be > 1)
3. `semaphore_slots` (default `2`)
4. `iterations` (default `80`)

## How the simulation works

Each worker thread repeatedly:

1. Performs short non-critical work.
2. Acquires a semaphore slot (simulates a bounded shared service like DB connections).
3. Attempts to lock two shared resources with timed mutexes.
4. Retries on contention and records wait/retry metrics.
5. Enters a critical section and then releases locks.

A monitor thread checks blocked durations and prints a warning when threads stay blocked beyond a threshold, helping you discuss deadlock-risk patterns and contention hotspots.

## Resume bullet ideas

- Built a C++20 concurrency simulator to visualize thread contention and synchronization behavior under configurable workloads.
- Implemented mutex and semaphore coordination mechanisms and instrumented wait-time/retry metrics for performance analysis.
- Added a runtime monitor to flag potential deadlock-like stalls and identify bottlenecks caused by resource contention.
- Used empirical output to compare contention scenarios by tuning thread count, resource pool size, and iteration volume.

## Suggested extensions

- Export metrics as CSV and graph with Python/gnuplot.
- Add starvation detection and fairness metrics.
- Add a simple web or terminal dashboard for live charts.
