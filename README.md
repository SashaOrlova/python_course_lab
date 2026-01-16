Python Parallelism Benchmark

### CPU-bound benchmark results

#### Python: 3.13.5 | PID: 22609 | CPU cores: 12
#### Config: tasks=2000, concurrency=8, repeats=5

| Model      | Median   | Min      | Max      | Runs |
|------------|----------|----------|----------|------|
| threads    | 26.401 s | 26.345 s | 27.836 s | 5    |
| processes  | 4.370 s  | 4.345 s  | 4.395 s  | 5    |
| asyncio    | 26.680 s | 26.328 s | 27.391 s | 5    |

### I/O-bound benchmark (local TCP echo; kernel I/O wait)

#### Python: 3.13.5 | PID: 50072 | CPU cores: 12
#### Config: tasks=200, concurrency=20, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.022 s | 0.021 s | 0.026 s | 10   |
| processes  | 0.195 s | 0.188 s | 0.218 s | 10   |
| asyncio    | 0.018 s | 0.018 s | 0.020 s | 10   |
