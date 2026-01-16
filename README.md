# Concurrency Benchmarks: Python vs Go vs C++

This document describes a set of **comparable concurrency benchmarks** designed to explain
how different concurrency models behave in **CPU-bound** and **I/O-bound** workloads across
three ecosystems:

- **Python** — threads, processes, asyncio  
- **Go** — OS threads, processes, goroutines  
- **C++** — OS threads, processes, C++20 coroutines (kqueue / epoll)

---

## Benchmarks Overview

Each language implements the **same logical workloads**.

### CPU-bound benchmark

- Pure computation
- No I/O
- Sensitive to:
  - Global Interpreter Lock (Python)
  - Kernel scheduling
  - True parallelism vs cooperative scheduling

### I/O-bound benchmark

- Local TCP echo server (`127.0.0.1`)
- Many short-lived client requests
- Sensitive to:
  - Context-switch cost
  - Kernel event notification mechanism
  - User-space scheduling efficiency

---

## Running the Benchmarks

### Python

**Requirements**
- Python 3.10+
- Linux or macOS

**Launch**

`python3 cpu_bound.py --tasks 2000 --concurrency 200 --repeats 5`
`python3 io_bound.py --tasks 2000 --concurrency 200 --repeats 5`

### Go

**Requirements**
- Go 1.20+
- Linux or macOS

**Launch**

`go run bench.go --tasks 2000 --concurrency 200 --repeats 5`

Compared models:
- `threads` — OS threads (via `sync.WaitGroup`)
- `processes` — `os/exec`
- `goroutines`


### C++

**Requirements**
- С++20
- macOS

**Launch**

`g++ -O2 -std=gnu++20 bench.cpp -o bench`
`./bench --tasks 2000 --concurrency 200 --repeats 5 --warmup 1 --cpu-units 200000 --payload-size 256`

Compared models:
- `threads` — `std::thread`
- `processes` — `fork`
- `coroutines`


## Implementation Notes

### Go: Threads vs Goroutines

In Go:
- Threads are OS-level threads
- Goroutines are lightweight coroutines
- The runtime scheduler maps **many goroutines onto few threads**

Consequences:
- Goroutines are extremely cheap to create
- Blocking I/O does not block the entire runtime
- True parallelism exists (no GIL)

This is why goroutines typically scale very well for I/O-heavy workloads.

---

### C++: Coroutines and kqueue / epoll

C++ provides **language-level coroutines**, but no built-in async runtime.

In these benchmarks:
- Coroutines are explicit state machines
- Scheduling is implemented manually
- Kernel readiness notification is handled via:
  - `kqueue` on macOS
  - `epoll` on Linux
- All I/O is non-blocking


## Why the Results Look Like This

### CPU-bound Workloads

| Language | Best-performing model | Reason |
|--------|-----------------------|--------|
| Python | Processes | GIL prevents parallel execution in threads and asyncio |
| Go | Threads / Goroutines | No GIL, true parallelism |
| C++ | Threads / Processes | Direct CPU execution, no runtime lock |

**Conclusion**  
Async models do not help with CPU-bound workloads.

---

### I/O-bound Workloads

| Language | Best-performing model | Reason |
|--------|-----------------------|--------|
| Python | asyncio | Minimal context switching, kernel polling |
| Go | goroutines | Integrated async runtime and cheap scheduling |
| C++ | coroutines | Direct kernel integration via kqueue / epoll |

**Conclusion**  
I/O scalability is dominated by **how efficiently the program waits**, not by raw CPU speed.

---

## What to Use and When

### Python
- CPU-bound workloads: `multiprocessing`
- I/O-bound workloads: `asyncio`
- Avoid: threads for heavy CPU work

### Go
- CPU-bound workloads: goroutines
- I/O-bound workloads: goroutines
- Avoid: manual thread management unless strictly necessary

### C++
- CPU-bound workloads: threads or processes
- I/O-bound workloads: coroutines + kqueue / epoll
- Avoid: blocking I/O in scalable servers

## Disclaimer

These benchmarks are:
- Localhost-only
- Synthetic
- Designed for teaching and architectural analysis

They are not intended to represent real-world production workloads.


