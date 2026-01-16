#!/usr/bin/env python3

from __future__ import annotations

import argparse
import asyncio
import os
import statistics
import sys
import time
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor
from dataclasses import dataclass
from typing import Callable, List, Tuple


def now() -> float:
    return time.perf_counter()

def median(xs: List[float]) -> float:
    return statistics.median(xs) if xs else float("nan")

def human_ms(sec: float) -> str:
    return f"{sec * 1000:.1f} ms"

def human_s(sec: float) -> str:
    return f"{sec:.3f} s"


@dataclass
class Result:
    name: str
    runs: List[float]

    @property
    def med(self) -> float:
        return median(self.runs)

    @property
    def min(self) -> float:
        return min(self.runs) if self.runs else float("nan")

    @property
    def max(self) -> float:
        return max(self.runs) if self.runs else float("nan")


def print_table(title: str, results: List[Result]) -> None:
    print("\n" + title)
    print("-" * len(title))
    header = f"{'Model':<18} {'median':>10} {'min':>10} {'max':>10} {'runs':>6}"
    print(header)
    print("-" * len(header))
    for r in results:
        print(f"{r.name:<18} {human_s(r.med):>10} {human_s(r.min):>10} {human_s(r.max):>10} {len(r.runs):>6}")
    print()


def cpu_work(units: int) -> int:
    acc = 0
    for i in range(units):
        acc = (acc * 1664525 + 1013904223 + i) & 0xFFFFFFFF
    return acc

def cpu_bench_threads(tasks: int, units: int, workers: int) -> None:
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futs = [ex.submit(cpu_work, units) for _ in range(tasks)]
        _ = sum(f.result() for f in futs)

def cpu_bench_processes(tasks: int, units: int, workers: int) -> None:
    with ProcessPoolExecutor(max_workers=workers) as ex:
        futs = [ex.submit(cpu_work, units) for _ in range(tasks)]
        _ = sum(f.result() for f in futs)

async def cpu_bench_asyncio(tasks: int, units: int, concurrency: int) -> None:
    sem = asyncio.Semaphore(concurrency)

    async def one() -> int:
        async with sem:
            return cpu_work(units)

    results = await asyncio.gather(*[one() for _ in range(tasks)])
    _ = sum(results)


def run_repeated(label: str, repeats: int, fn: Callable[[], None], warmup: int = 1) -> Result:
    for _ in range(warmup):
        fn()

    runs: List[float] = []
    for _ in range(repeats):
        t0 = now()
        fn()
        runs.append(now() - t0)
    return Result(label, runs)

async def run_repeated_async(label: str, repeats: int, coro_factory: Callable[[], "asyncio.Future[None]"], warmup: int = 1) -> Result:
    for _ in range(warmup):
        await coro_factory()

    runs: List[float] = []
    for _ in range(repeats):
        t0 = now()
        await coro_factory()
        runs.append(now() - t0)
    return Result(label, runs)


async def main_async(args: argparse.Namespace) -> int:
    print(f"Python: {sys.version.split()[0]} | PID: {os.getpid()} | CPU cores: {os.cpu_count()}")
    print(f"Config: tasks={args.tasks}, concurrency={args.concurrency}, repeats={args.repeats}")
    print()

    print("CPU-bound benchmark (pure Python loop; GIL-sensitive)")
    cpu_units = args.cpu_units

    cpu_results: List[Result] = []
    cpu_results.append(run_repeated(
        "threads",
        args.repeats,
        lambda: cpu_bench_threads(args.tasks, cpu_units, args.concurrency),
        warmup=args.warmup,
    ))
    cpu_results.append(run_repeated(
        "processes",
        args.repeats,
        lambda: cpu_bench_processes(args.tasks, cpu_units, args.concurrency),
        warmup=args.warmup,
    ))
    cpu_results.append(await run_repeated_async(
        "asyncio",
        args.repeats,
        lambda: cpu_bench_asyncio(args.tasks, cpu_units, args.concurrency),
        warmup=args.warmup,
    ))
    print_table("CPU-bound results (lower is better)", cpu_results)

    print("Interpretation hints:")
    print("- CPU-bound: processes should scale better; threads/asyncio are limited by the GIL.")
    print("- I/O-bound: asyncio and threads usually perform well; processes often add overhead.")
    return 0


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Benchmark threads vs processes vs asyncio (CPU-bound and I/O-bound).")
    p.add_argument("--tasks", type=int, default=20, help="Number of tasks/operations to run per benchmark.")
    p.add_argument("--concurrency", type=int, default=min(2, (os.cpu_count() or 8) * 25),
                   help="Concurrency level: thread/process workers, and asyncio semaphore limit.")
    p.add_argument("--repeats", type=int, default=5, help="Number of measured runs.")
    p.add_argument("--warmup", type=int, default=1, help="Warmup runs (not measured).")
    p.add_argument("--cpu-units", type=int, default=200_000, help="Work units per CPU task (increase for longer runs).")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    try:
        return asyncio.run(main_async(args))
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
