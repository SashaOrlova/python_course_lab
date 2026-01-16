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
    runs: List[float]  # seconds

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

async def start_echo_server() -> Tuple[asyncio.base_events.Server, int]:
    async def handle(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        try:
            while True:
                data = await reader.read(1024)
                if not data:
                    break
                writer.write(data)
                await writer.drain()
        finally:
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass

    server = await asyncio.start_server(handle, host="127.0.0.1", port=0, backlog=4096)
    sock = next(iter(server.sockets))
    port = sock.getsockname()[1]
    return server, port

def io_process_worker(args):
    port, payload = args
    import socket

    with socket.create_connection(("127.0.0.1", port), timeout=20.0) as s:
        s.sendall(payload)
        recvd = b""
        while len(recvd) < len(payload):
            chunk = s.recv(len(payload) - len(recvd))
            if not chunk:
                raise RuntimeError("Socket closed early")
            recvd += chunk
        if recvd != payload:
            raise RuntimeError("Echo mismatch")
    return 1

async def io_client_once(port: int, payload: bytes) -> None:
    reader, writer = await asyncio.open_connection("127.0.0.1", port)
    try:
        writer.write(payload)
        await writer.drain()
        data = await reader.readexactly(len(payload))
        if data != payload:
            raise RuntimeError("Echo mismatch")
    finally:
        writer.close()
        await writer.wait_closed()

async def io_bench_asyncio(tasks: int, concurrency: int, port: int, payload: bytes) -> None:
    sem = asyncio.Semaphore(concurrency)

    async def one():
        async with sem:
            await io_client_once(port, payload)

    await asyncio.gather(*[one() for _ in range(tasks)])

def io_bench_threads(tasks: int, concurrency: int, port: int, payload: bytes) -> None:
    import socket

    def one():
        with socket.create_connection(("127.0.0.1", port), timeout=20.0) as s:
            s.sendall(payload)
            recvd = b""
            while len(recvd) < len(payload):
                chunk = s.recv(len(payload) - len(recvd))
                if not chunk:
                    raise RuntimeError("Socket closed early")
                recvd += chunk
            if recvd != payload:
                raise RuntimeError("Echo mismatch")

    with ThreadPoolExecutor(max_workers=concurrency) as ex:
        futs = [ex.submit(one) for _ in range(tasks)]
        for f in futs:
            f.result()

def io_bench_processes(tasks: int, concurrency: int, port: int, payload: bytes) -> None:
    from concurrent.futures import ProcessPoolExecutor

    proc_workers = min(concurrency, 64)
    job_args = [(port, payload) for _ in range(tasks)]

    with ProcessPoolExecutor(max_workers=proc_workers) as ex:
        _ = sum(ex.map(io_process_worker, job_args))

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

async def run_repeated_blocking(
    label: str,
    repeats: int,
    fn,               # callable with no args
    warmup: int = 1
) -> Result:
    # Warmup (not measured)
    for _ in range(warmup):
        await asyncio.to_thread(fn)

    runs: List[float] = []
    for _ in range(repeats):
        t0 = now()
        await asyncio.to_thread(fn)
        runs.append(now() - t0)

    return Result(label, runs)

async def main_async(args: argparse.Namespace) -> int:
    print(f"Python: {sys.version.split()[0]} | PID: {os.getpid()} | CPU cores: {os.cpu_count()}")
    print(f"Config: tasks={args.tasks}, concurrency={args.concurrency}, repeats={args.repeats}")
    print()

    print("I/O-bound benchmark (local TCP echo; kernel I/O wait)")
    server, port = await start_echo_server()
    payload = b"x" * args.payload_size

    try:
        io_results: List[Result] = []

        io_results.append(await run_repeated_blocking(
            "threads",
            args.repeats,
            lambda: io_bench_threads(args.tasks, args.concurrency, port, payload),
            warmup=args.warmup,
        ))

        io_results.append(await run_repeated_blocking(
            "processes",
            args.repeats,
            lambda: io_bench_processes(args.tasks, args.concurrency, port, payload),
            warmup=args.warmup,
        ))

        io_results.append(await run_repeated_async(
            "asyncio",
            args.repeats,
            lambda: io_bench_asyncio(args.tasks, args.concurrency, port, payload),
            warmup=args.warmup,
        ))

        print_table("I/O-bound results (lower is better)", io_results)

    finally:
        server.close()
        await server.wait_closed()

    return 0


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Benchmark threads vs processes vs asyncio (CPU-bound and I/O-bound).")
    p.add_argument("--tasks", type=int, default=200, help="Number of tasks/operations to run per benchmark.")
    p.add_argument("--concurrency", type=int, default=min(20, (os.cpu_count() or 8) * 25),
                   help="Concurrency level: thread/process workers, and asyncio semaphore limit.")
    p.add_argument("--repeats", type=int, default=10, help="Number of measured runs.")
    p.add_argument("--warmup", type=int, default=1, help="Warmup runs (not measured).")
    p.add_argument("--payload-size", type=int, default=256, help="Bytes per I/O request.")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    try:
        return asyncio.run(main_async(args))
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
