Python Parallelism Benchmark

### CPU-bound benchmark results

#### Python: 3.13.5 | PID: 22609 | CPU cores: 12
#### Config: tasks=2000, concurrency=8, repeats=5

| Model      | Median   | Min      | Max      | Runs |
|------------|----------|----------|----------|------|
| threads    | 26.401 s | 26.345 s | 27.836 s | 5    |
| processes  | 4.370 s  | 4.345 s  | 4.395 s  | 5    |
| asyncio    | 26.680 s | 26.328 s | 27.391 s | 5    |

