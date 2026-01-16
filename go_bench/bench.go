package main

import (
	"bufio"
	"flag"
	"fmt"
	"net"
	"os"
	"os/exec"
	"runtime"
	"sort"
	"strconv"
	"sync"
	"time"
)

type Config struct {
	Tasks       int
	Concurrency int
	Repeats     int
	Warmup      int
	CpuUnits    int
	PayloadSize int
	BacklogHint int
	TimeoutMs   int
}

func parseArgs() Config {
	var c Config
	flag.IntVar(&c.Tasks, "tasks", 200, "Number of tasks/operations per benchmark.")
	flag.IntVar(&c.Concurrency, "concurrency", min(30, runtime.NumCPU()*25), "Concurrency level (workers / in-flight ops).")
	flag.IntVar(&c.Repeats, "repeats", 10, "Number of measured runs.")
	flag.IntVar(&c.Warmup, "warmup", 1, "Warmup runs (not measured).")
	flag.IntVar(&c.CpuUnits, "cpu-units", 200000, "Work units per CPU task.")
	flag.IntVar(&c.PayloadSize, "payload-size", 256, "Bytes per I/O request.")
	flag.IntVar(&c.BacklogHint, "backlog", 4096, "Server accept backlog hint (best effort, OS dependent).")
	flag.IntVar(&c.TimeoutMs, "timeout-ms", 20000, "Dial/read/write timeout in milliseconds.")
	flag.Parse()
	return c
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

type Result struct {
	Model string
	Runs  []time.Duration
}

func (r Result) Median() time.Duration { return median(r.Runs) }
func (r Result) Min() time.Duration    { return minDur(r.Runs) }
func (r Result) Max() time.Duration    { return maxDur(r.Runs) }

func median(xs []time.Duration) time.Duration {
	tmp := append([]time.Duration(nil), xs...)
	sort.Slice(tmp, func(i, j int) bool { return tmp[i] < tmp[j] })
	return tmp[len(tmp)/2]
}

func minDur(xs []time.Duration) time.Duration {
	m := xs[0]
	for _, x := range xs[1:] {
		if x < m {
			m = x
		}
	}
	return m
}

func maxDur(xs []time.Duration) time.Duration {
	m := xs[0]
	for _, x := range xs[1:] {
		if x > m {
			m = x
		}
	}
	return m
}

func fmtSec(d time.Duration) string {
	return fmt.Sprintf("%.3f s", d.Seconds())
}

func printTable(title string, results []Result) {
	fmt.Println()
	fmt.Println(title)
	fmt.Println("-----------------------------------")
	fmt.Printf("%-12s %10s %10s %10s %6s\n", "Model", "median", "min", "max", "runs")
	fmt.Println("----------------------------------------------------------")
	for _, r := range results {
		fmt.Printf("%-12s %10s %10s %10s %6d\n",
			r.Model,
			fmtSec(r.Median()),
			fmtSec(r.Min()),
			fmtSec(r.Max()),
			len(r.Runs),
		)
	}
}

func cpuWork(units int) uint32 {
	var acc uint32 = 0
	for i := 0; i < units; i++ {
		acc = acc*1664525 + 1013904223 + uint32(i)
	}
	return acc
}

func cpuGoroutines(cfg Config) {
	sem := make(chan struct{}, cfg.Concurrency)
	var wg sync.WaitGroup
	var sum uint32

	for i := 0; i < cfg.Tasks; i++ {
		wg.Add(1)
		sem <- struct{}{}
		go func() {
			defer wg.Done()
			sum ^= cpuWork(cfg.CpuUnits)
			<-sem
		}()
	}
	wg.Wait()
	_ = sum
}

func cpuThreads(cfg Config) {
	jobs := make(chan struct{}, cfg.Tasks)
	for i := 0; i < cfg.Tasks; i++ {
		jobs <- struct{}{}
	}
	close(jobs)

	var wg sync.WaitGroup
	var sum uint32
	var mu sync.Mutex

	workers := cfg.Concurrency
	if workers < 1 {
		workers = 1
	}

	wg.Add(workers)
	for w := 0; w < workers; w++ {
		go func() {
			runtime.LockOSThread()
			defer runtime.UnlockOSThread()
			defer wg.Done()

			var local uint32
			for range jobs {
				local ^= cpuWork(cfg.CpuUnits)
			}
			mu.Lock()
			sum ^= local
			mu.Unlock()
		}()
	}
	wg.Wait()
	_ = sum
}

func cpuProcesses(cfg Config) {
	sem := make(chan struct{}, cfg.Concurrency)
	var wg sync.WaitGroup

	for i := 0; i < cfg.Tasks; i++ {
		wg.Add(1)
		sem <- struct{}{}
		go func() {
			defer wg.Done()
			cmd := exec.Command(os.Args[0], "cpu-child", strconv.Itoa(cfg.CpuUnits))
			_ = cmd.Run()
			<-sem
		}()
	}
	wg.Wait()
}

func cpuChild(units int) {
	_ = cpuWork(units)
}

type EchoServer struct {
	ln   net.Listener
	addr string
	done chan struct{}
}

func startEchoServer(cfg Config) (*EchoServer, error) {
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		return nil, err
	}
	s := &EchoServer{
		ln:   ln,
		addr: ln.Addr().String(),
		done: make(chan struct{}),
	}

	go func() {
		defer close(s.done)
		for {
			conn, err := ln.Accept()
			if err != nil {
				return
			}
			go handleConn(conn)
		}
	}()

	return s, nil
}

func (s *EchoServer) Close() {
	_ = s.ln.Close()
	<-s.done
}

func handleConn(conn net.Conn) {
	defer conn.Close()
	r := bufio.NewReader(conn)
	w := bufio.NewWriter(conn)
	buf := make([]byte, 4096)

	for {
		n, err := r.Read(buf)
		if n > 0 {
			_, _ = w.Write(buf[:n])
			_ = w.Flush()
		}
		if err != nil {
			return
		}
	}
}

func ioOne(addr string, payload []byte, timeout time.Duration) error {
	d := net.Dialer{Timeout: timeout}
	conn, err := d.Dial("tcp", addr)
	if err != nil {
		return err
	}
	defer conn.Close()

	_ = conn.SetDeadline(time.Now().Add(timeout))

	if _, err := conn.Write(payload); err != nil {
		return err
	}

	want := len(payload)
	got := 0
	buf := make([]byte, want)
	for got < want {
		n, err := conn.Read(buf[got:])
		if n > 0 {
			got += n
		}
		if err != nil {
			return err
		}
	}
	return nil
}

func ioGoroutines(cfg Config, addr string, payload []byte) {
	sem := make(chan struct{}, cfg.Concurrency)
	var wg sync.WaitGroup
	timeout := time.Duration(cfg.TimeoutMs) * time.Millisecond

	for i := 0; i < cfg.Tasks; i++ {
		wg.Add(1)
		sem <- struct{}{}
		go func() {
			defer wg.Done()
			_ = ioOne(addr, payload, timeout)
			<-sem
		}()
	}
	wg.Wait()
}

func ioThreads(cfg Config, addr string, payload []byte) {
	jobs := make(chan struct{}, cfg.Tasks)
	for i := 0; i < cfg.Tasks; i++ {
		jobs <- struct{}{}
	}
	close(jobs)

	timeout := time.Duration(cfg.TimeoutMs) * time.Millisecond

	workers := cfg.Concurrency
	if workers < 1 {
		workers = 1
	}

	var wg sync.WaitGroup
	wg.Add(workers)
	for w := 0; w < workers; w++ {
		go func() {
			runtime.LockOSThread()
			defer runtime.UnlockOSThread()
			defer wg.Done()
			for range jobs {
				_ = ioOne(addr, payload, timeout)
			}
		}()
	}
	wg.Wait()
}

func ioProcesses(cfg Config, addr string) {
	sem := make(chan struct{}, cfg.Concurrency)
	var wg sync.WaitGroup

	for i := 0; i < cfg.Tasks; i++ {
		wg.Add(1)
		sem <- struct{}{}
		go func() {
			defer wg.Done()
			cmd := exec.Command(os.Args[0], "io-child", addr, strconv.Itoa(cfg.PayloadSize), strconv.Itoa(cfg.TimeoutMs))
			_ = cmd.Run()
			<-sem
		}()
	}
	wg.Wait()
}

func ioChild(addr string, payloadSize int, timeoutMs int) {
	payload := make([]byte, payloadSize)
	timeout := time.Duration(timeoutMs) * time.Millisecond
	_ = ioOne(addr, payload, timeout)
}

func runRepeated(cfg Config, label string, fn func()) Result {
	for i := 0; i < cfg.Warmup; i++ {
		fn()
	}
	runs := make([]time.Duration, 0, cfg.Repeats)
	for i := 0; i < cfg.Repeats; i++ {
		t0 := time.Now()
		fn()
		runs = append(runs, time.Since(t0))
	}
	return Result{Model: label, Runs: runs}
}

func main() {
	if len(os.Args) > 1 {
		switch os.Args[1] {
		case "cpu-child":
			if len(os.Args) != 3 {
				os.Exit(2)
			}
			units, _ := strconv.Atoi(os.Args[2])
			cpuChild(units)
			return
		case "io-child":
			if len(os.Args) != 5 {
				os.Exit(2)
			}
			addr := os.Args[2]
			ps, _ := strconv.Atoi(os.Args[3])
			tm, _ := strconv.Atoi(os.Args[4])
			ioChild(addr, ps, tm)
			return
		}
	}

	cfg := parseArgs()

	fmt.Printf("Config: tasks=%d, concurrency=%d, repeats=%d\n\n", cfg.Tasks, cfg.Concurrency, cfg.Repeats)

	fmt.Println("CPU-bound benchmark (pure loop; Go has no GIL)")
	cpuResults := []Result{
		runRepeated(cfg, "threads", func() { cpuThreads(cfg) }),
		runRepeated(cfg, "processes", func() { cpuProcesses(cfg) }),
		runRepeated(cfg, "goroutines", func() { cpuGoroutines(cfg) }),
	}
	printTable("CPU-bound results (lower is better)", cpuResults)

	fmt.Println("\nI/O-bound benchmark (local TCP echo)")
	srv, err := startEchoServer(cfg)
	if err != nil {
		fmt.Println("Failed to start echo server:", err)
		os.Exit(1)
	}
	defer srv.Close()

	payload := make([]byte, cfg.PayloadSize)
	ioResults := []Result{
		runRepeated(cfg, "threads", func() { ioThreads(cfg, srv.addr, payload) }),
		runRepeated(cfg, "processes", func() { ioProcesses(cfg, srv.addr) }),
		runRepeated(cfg, "goroutines", func() { ioGoroutines(cfg, srv.addr, payload) }),
	}
	printTable("I/O-bound results (lower is better)", ioResults)
}
