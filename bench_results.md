## Python Parallelism Benchmark

### CPU-bound benchmark results

#### Python: 3.13.5 | PID: 22609 | CPU cores: 12

#### Config: tasks=2000, concurrency=2, repeats=5

| Model      | Median   | Min      | Max      | Runs |
|------------|----------|----------|----------|------|
| threads    | 26.100 s | 25.935 s | 26.229 s | 5    |
| processes  | 13.978 s | 13.793 s | 14.263 s | 5    |
| asyncio    | 26.566 s | 26.047 s | 28.320 s | 5    |

#### Config: tasks=2000, concurrency=8, repeats=5

| Model      | Median   | Min      | Max      | Runs |
|------------|----------|----------|----------|------|
| threads    | 26.401 s | 26.345 s | 27.836 s | 5    |
| processes  | 4.370 s  | 4.345 s  | 4.395 s  | 5    |
| asyncio    | 26.680 s | 26.328 s | 27.391 s | 5    |

#### Config: tasks=2000, concurrency=12, repeats=5

| Model      | Median   | Min      | Max      | Runs |
|------------|----------|----------|----------|------|
| threads    | 26.102 s | 26.079 s | 26.170 s | 5    |
| processes  | 3.799 s  | 3.713 s  | 3.989 s  | 5    |
| asyncio    | 26.324 s | 26.092 s | 27.872 s | 5    |

#### Config: tasks=2000, concurrency=20, repeats=5

| Model      | Median   | Min      | Max      | Runs |
|------------|----------|----------|----------|------|
| threads    | 25.762 s | 25.668 s | 25.825 s | 5    |
| processes  | 3.857 s  | 3.823 s  | 3.869 s  | 5    |
| asyncio    | 25.981 s | 25.780 s | 28.026 s | 5    |

#### Config: tasks=2000, concurrency=30, repeats=5

| Model      | Median   | Min      | Max      | Runs |
|------------|----------|----------|----------|------|
| threads    | 26.622 s | 26.114 s | 26.741 s | 5    |
| processes  | 4.041 s  | 3.954 s  | 4.093 s  | 5    |
| asyncio    | 27.732 s | 26.076 s | 28.649 s | 5    |

### I/O-bound benchmark (local TCP echo; kernel I/O wait)

#### Python: 3.13.5 | PID: 50072 | CPU cores: 12

#### Config: tasks=200, concurrency=2, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.028 s | 0.026 s | 0.036 s | 10   |
| processes  | 0.086 s | 0.084 s | 0.110 s | 10   |
| asyncio    | 0.028 s | 0.027 s | 0.029 s | 10   |

#### Config: tasks=200, concurrency=8, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.022 s | 0.021 s | 0.023 s | 10   |
| processes  | 0.113 s | 0.105 s | 0.125 s | 10   |
| asyncio    | 0.019 s | 0.019 s | 0.019 s | 10   |

#### Config: tasks=200, concurrency=12, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.022 s | 0.021 s | 0.025 s | 10   |
| processes  | 0.151 s | 0.124 s | 0.196 s | 10   |
| asyncio    | 0.022 s | 0.020 s | 0.024 s | 10   |

#### Config: tasks=200, concurrency=20, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.022 s | 0.021 s | 0.026 s | 10   |
| processes  | 0.195 s | 0.188 s | 0.218 s | 10   |
| asyncio    | 0.018 s | 0.018 s | 0.020 s | 10   |

#### Config: tasks=200, concurrency=30, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.022 s | 0.019 s | 0.029 s | 10   |
| processes  | 0.299 s | 0.268 s | 0.330 s | 10   |
| asyncio    | 0.021 s | 0.020 s | 0.022 s | 10   |

## Go Parallelism Benchmark

### CPU-bound benchmark results

#### Config: tasks=2000, concurrency=2, repeats=5

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.203 s | 0.203 s | 0.208 s | 5    |
| processes  | 1.768 s | 1.730 s | 1.782 s | 5    |
| goroutines | 0.223 s | 0.221 s | 0.224 s | 5    |

#### Config: tasks=2000, concurrency=8, repeats=5

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.060 s | 0.060 s | 0.060 s | 5    |
| processes  | 0.862 s | 0.826 s | 0.932 s | 5    |
| goroutines | 0.062 s | 0.061 s | 0.063 s | 5    |

#### Config: tasks=2000, concurrency=12, repeats=5

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.045 s | 0.045 s | 0.047 s | 5    |
| processes  | 0.884 s | 0.875 s | 0.912 s | 5    |
| goroutines | 0.044 s | 0.044 s | 0.044 s | 5    |

#### Config: tasks=2000, concurrency=20, repeats=5

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.045 s | 0.045 s | 0.046 s | 5    |
| processes  | 0.929 s | 0.915 s | 0.967 s | 5    |
| goroutines | 0.044 s | 0.044 s | 0.046 s | 5    |

#### Config: tasks=2000, concurrency=30, repeats=5

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.046 s | 0.045 s | 0.048 s | 5    |
| processes  | 0.941 s | 0.935 s | 0.945 s | 5    |
| goroutines | 0.045 s | 0.044 s | 0.047 s | 5    |

### I/O-bound benchmark (local TCP echo; kernel I/O wait)

#### Config: tasks=200, concurrency=2, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.011 s | 0.011 s | 0.014 s | 10   |
| processes  | 0.201 s | 0.191 s | 0.243 s | 10   |
| goroutines | 0.016 s | 0.013 s | 0.023 s | 10   |

#### Config: tasks=200, concurrency=8, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.006 s | 0.006 s | 0.006 s | 10   |
| processes  | 0.109 s | 0.102 s | 0.136 s | 10   |
| goroutines | 0.012 s | 0.011 s | 0.012 s | 10   |

#### Config: tasks=200, concurrency=12, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.006 s | 0.006 s | 0.007 s | 10   |
| processes  | 0.104 s | 0.100 s | 0.152 s | 10   |
| goroutines | 0.012 s | 0.011 s | 0.012 s | 10   |

#### Config: tasks=200, concurrency=20, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.006 s | 0.006 s | 0.011 s | 10   |
| processes  | 0.117 s | 0.107 s | 0.154 s | 10   |
| goroutines | 0.012 s | 0.011 s | 0.028 s | 10   |

#### Config: tasks=200, concurrency=30, repeats=10

| Model      | Median  | Min     | Max     | Runs |
|------------|---------|---------|---------|------|
| threads    | 0.006 s | 0.006 s | 0.006 s | 10   |
| processes  | 0.114 s | 0.113 s | 0.140 s | 10   |
| goroutines | 0.011 s | 0.011 s | 0.012 s | 10   |

## С++ Parallelism Benchmark

### CPU-bound benchmark results

#### Config: tasks=2000, concurrency=2, repeats=5

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.152 s | 0.152 s | 0.153 s | 5 |
| processes | 0.180 s | 0.178 s | 0.194 s | 5 |
| coroutines | 0.296 s | 0.296 s | 0.305 s | 5 |

#### Config: tasks=2000, concurrency=8, repeats=5

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.045 s | 0.045 s | 0.055 s | 5 |
| processes | 0.128 s | 0.127 s | 0.131 s | 5 |
| coroutines | 0.296 s | 0.296 s | 0.298 s | 5 |

#### Config: tasks=2000, concurrency=12, repeats=5

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.033 s | 0.033 s | 0.034 s | 5 |
| processes | 0.130 s | 0.126 s | 0.139 s | 5 |
| coroutines | 0.296 s | 0.296 s | 0.318 s | 5 |

#### Config: tasks=2000, concurrency=20, repeats=5

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.033 s | 0.033 s | 0.034 s | 5 |
| processes | 0.123 s | 0.121 s | 0.139 s | 5 |
| coroutines | 0.296 s | 0.296 s | 0.298 s | 5 |

#### Config: tasks=2000, concurrency=30, repeats=5

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.033 s | 0.033 s | 0.033 s | 5 |
| processes | 0.133 s | 0.128 s | 0.145 s | 5 |
| coroutines | 0.297 s | 0.296 s | 0.303 s | 5 |


### I/O-bound benchmark (local TCP echo; kernel I/O wait)

#### Config: tasks=200, concurrency=2, repeats=10

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.009 s | 0.008 s | 0.012 s | 5 |
| processes | 0.029 s | 0.028 s | 0.030 s | 5 |
| coroutines | 0.012 s | 0.011 s | 0.012 s | 5 |

#### Config: tasks=200, concurrency=8, repeats=10

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.010 s | 0.009 s | 0.015 s | 5 |
| processes | 0.025 s | 0.025 s | 0.028 s | 5 |
| coroutines | 0.011 s | 0.011 s | 0.012 s | 5 |

#### Config: tasks=200, concurrency=12, repeats=10

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.012 s | 0.011 s | 0.017 s | 5 |
| processes | 0.025 s | 0.024 s | 0.026 s | 5 |
| coroutines | 0.013 s | 0.013 s | 0.024 s | 5 |

#### Config: tasks=200, concurrency=20, repeats=10

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.012 s | 0.010 s | 0.014 s | 5 |
| processes | 0.024 s | 0.024 s | 0.024 s | 5 |
| coroutines | 0.013 s | 0.013 s | 0.023 s | 5 |

#### Config: tasks=200, concurrency=30, repeats=10

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.011 s | 0.011 s | 0.014 s | 5 |
| processes | 0.027 s | 0.026 s | 0.027 s | 5 |
| coroutines | 0.017 s | 0.013 s | 0.025 s | 5 |

#### Config: tasks=2000, concurrency=200, repeats=10

| Model | Median | Min | Max | Runs |
|------:|-------:|----:|----:|-----:|
| threads | 0.164 s | 0.135 s | 0.174 s | 5 |
| processes | 0.694 s | 0.371 s | 1.166 s | 5 |
| coroutines | 0.064 s | 0.063 s | 0.071 s | 5 |
