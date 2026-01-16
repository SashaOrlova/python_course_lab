// bench_macos.cpp
// macOS-only: uses kqueue for coroutine-based I/O (asyncio analogue).
// C++20 benchmark: threads vs processes vs coroutines with Markdown output.
//
// Benchmarks:
//   CPU-bound: threads vs processes vs coroutines (cooperative; mirrors "asyncio is bad for CPU-bound")
//   I/O-bound: threads vs processes vs coroutines (kqueue + nonblocking sockets)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using steady_clock = std::chrono::steady_clock;

static double seconds_now() {
    return std::chrono::duration<double>(steady_clock::now().time_since_epoch()).count();
}

static std::string fmt_sec(double s) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << s << " s";
    return oss.str();
}

struct Config {
    int tasks = 2000;
    int concurrency = 200;
    int repeats = 5;
    int warmup = 1;
    int cpu_units = 200000;
    int payload_size = 256;
    int backlog = 4096;
    int timeout_ms = 20000;
};

static int to_int(const char* s, int def) {
    if (!s) return def;
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (!end || *end != '\0') return def;
    return (int)v;
}

static Config parse_args(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&](int def) {
            if (i + 1 >= argc) return def;
            return to_int(argv[++i], def);
        };

        if (a == "--tasks") cfg.tasks = next(cfg.tasks);
        else if (a == "--concurrency") cfg.concurrency = next(cfg.concurrency);
        else if (a == "--repeats") cfg.repeats = next(cfg.repeats);
        else if (a == "--warmup") cfg.warmup = next(cfg.warmup);
        else if (a == "--cpu-units") cfg.cpu_units = next(cfg.cpu_units);
        else if (a == "--payload-size") cfg.payload_size = next(cfg.payload_size);
        else if (a == "--backlog") cfg.backlog = next(cfg.backlog);
        else if (a == "--timeout-ms") cfg.timeout_ms = next(cfg.timeout_ms);
        else if (a == "--help" || a == "-h") {
            std::cout <<
                "Usage: ./bench [options]\n"
                "  --tasks N\n"
                "  --concurrency N\n"
                "  --repeats N\n"
                "  --warmup N\n"
                "  --cpu-units N\n"
                "  --payload-size N\n"
                "  --backlog N\n"
                "  --timeout-ms N\n";
            std::exit(0);
        }
    }

    if (cfg.tasks < 1) cfg.tasks = 1;
    if (cfg.concurrency < 1) cfg.concurrency = 1;
    if (cfg.repeats < 1) cfg.repeats = 1;
    if (cfg.warmup < 0) cfg.warmup = 0;
    return cfg;
}

struct Result {
    std::string model;
    std::vector<double> runs;
};

static double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}
static double minv(const std::vector<double>& v) {
    return *std::min_element(v.begin(), v.end());
}
static double maxv(const std::vector<double>& v) {
    return *std::max_element(v.begin(), v.end());
}

static void print_md_table(const std::string& title, const std::vector<Result>& results) {
    std::cout << "### " << title << "\n\n";
    std::cout << "| Model | Median | Min | Max | Runs |\n";
    std::cout << "|------:|-------:|----:|----:|-----:|\n";
    for (const auto& r : results) {
        std::cout << "| " << r.model
                  << " | " << fmt_sec(median(r.runs))
                  << " | " << fmt_sec(minv(r.runs))
                  << " | " << fmt_sec(maxv(r.runs))
                  << " | " << r.runs.size()
                  << " |\n";
    }
    std::cout << "\n";
}

template <class Fn>
static Result run_repeated(const Config& cfg, const std::string& label, Fn fn) {
    for (int i = 0; i < cfg.warmup; i++) fn();

    Result r;
    r.model = label;
    r.runs.reserve(cfg.repeats);

    for (int i = 0; i < cfg.repeats; i++) {
        double t0 = seconds_now();
        fn();
        r.runs.push_back(seconds_now() - t0);
    }
    return r;
}

static uint32_t cpu_work(int units) {
    uint32_t acc = 0;
    for (int i = 0; i < units; i++) {
        acc = acc * 1664525u + 1013904223u + (uint32_t)i;
    }
    return acc;
}

static void cpu_threads(const Config& cfg) {
    std::atomic<int> idx{0};
    std::vector<std::thread> workers;
    workers.reserve(cfg.concurrency);

    std::atomic<uint32_t> checksum{0};

    for (int t = 0; t < cfg.concurrency; t++) {
        workers.emplace_back([&] {
            uint32_t local = 0;
            while (true) {
                int i = idx.fetch_add(1, std::memory_order_relaxed);
                if (i >= cfg.tasks) break;
                local ^= cpu_work(cfg.cpu_units);
            }
            checksum.fetch_xor(local, std::memory_order_relaxed);
        });
    }
    for (auto& th : workers) th.join();
    (void)checksum.load();
}

static void cpu_processes(const Config& cfg) {
    int launched = 0;
    int completed = 0;

    while (completed < cfg.tasks) {
        while (launched - completed < cfg.concurrency && launched < cfg.tasks) {
            pid_t pid = ::fork();
            if (pid == 0) {
                (void)cpu_work(cfg.cpu_units);
                _exit(0);
            }
            launched++;
        }
        int status = 0;
        (void)::wait(&status);
        completed++;
    }
}

struct CpuTask {
    struct promise_type {
        CpuTask get_return_object() {
            return CpuTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> h{};

    CpuTask() = default;
    explicit CpuTask(std::coroutine_handle<promise_type> h_) : h(h_) {}

    CpuTask(const CpuTask&) = delete;
    CpuTask& operator=(const CpuTask&) = delete;

    CpuTask(CpuTask&& o) noexcept : h(o.h) { o.h = {}; }

    CpuTask& operator=(CpuTask&& o) noexcept {
        if (this != &o) {
            if (h) h.destroy();
            h = o.h;
            o.h = {};
        }
        return *this;
    }

    ~CpuTask() {
        if (h) h.destroy();
    }

    bool done() const noexcept { return !h || h.done(); }
    void resume() { if (h && !h.done()) h.resume(); }
};

static CpuTask cpu_coroutine_job(int units, int chunk, std::atomic<uint32_t>* out) {
    uint32_t acc = 0;
    int done = 0;
    while (done < units) {
        int step = std::min(chunk, units - done);
        for (int i = 0; i < step; i++) {
            acc = acc * 1664525u + 1013904223u + (uint32_t)(done + i);
        }
        done += step;
        co_await std::suspend_always{}; // cooperative yield
    }
    out->fetch_xor(acc, std::memory_order_relaxed);
    co_return;
}

static void cpu_coroutines(const Config& cfg) {
    const int chunk = 5000;
    std::atomic<uint32_t> checksum{0};

    int launched = 0;
    int finished = 0;

    std::vector<CpuTask> active;
    active.reserve((size_t)cfg.concurrency);

    auto launch_one = [&]() {
        active.emplace_back(cpu_coroutine_job(cfg.cpu_units, chunk, &checksum));
        active.back().resume();
        launched++;
    };

    while (launched < cfg.tasks && (int)active.size() < cfg.concurrency) {
        launch_one();
    }

    while (finished < cfg.tasks) {
        for (size_t i = 0; i < active.size();) {
            if (active[i].done()) {
                active[i] = std::move(active.back());
                active.pop_back();
                finished++;
                if (launched < cfg.tasks) launch_one();
            } else {
                active[i].resume();
                i++;
            }
        }
    }

    (void)checksum.load();
}

struct EchoServer {
    int listen_fd = -1;
    uint16_t port = 0;
    std::thread accept_thread;

    bool start(int backlog) {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) return false;

        int one = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        if (::bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) return false;
        if (::listen(listen_fd, backlog) < 0) return false;

        socklen_t len = sizeof(addr);
        if (::getsockname(listen_fd, (sockaddr*)&addr, &len) < 0) return false;
        port = ntohs(addr.sin_port);

        accept_thread = std::thread([this] {
            while (true) {
                int c = ::accept(listen_fd, nullptr, nullptr);
                if (c < 0) break;

                std::thread([c] {
                    char buf[4096];
                    while (true) {
                        ssize_t n = ::read(c, buf, sizeof(buf));
                        if (n <= 0) break;
                        ssize_t off = 0;
                        while (off < n) {
                            ssize_t w = ::write(c, buf + off, (size_t)(n - off));
                            if (w <= 0) break;
                            off += w;
                        }
                    }
                    ::close(c);
                }).detach();
            }
        });

        return true;
    }

    void stop() {
        if (listen_fd >= 0) {
            ::close(listen_fd);
            listen_fd = -1;
        }
        if (accept_thread.joinable()) accept_thread.join();
    }
};

static int set_timeouts(int fd, int timeout_ms) {
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) return -1;
    return 0;
}

static void io_one_blocking(uint16_t port, int payload_size, int timeout_ms) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return;
    (void)set_timeouts(s, timeout_ms);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (::connect(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(s);
        return;
    }

    std::vector<char> payload((size_t)payload_size, 'x');

    size_t off = 0;
    while (off < payload.size()) {
        ssize_t w = ::send(s, payload.data() + off, payload.size() - off, 0);
        if (w <= 0) { ::close(s); return; }
        off += (size_t)w;
    }

    std::vector<char> buf(payload.size());
    size_t got = 0;
    while (got < buf.size()) {
        ssize_t n = ::recv(s, buf.data() + got, buf.size() - got, 0);
        if (n <= 0) { ::close(s); return; }
        got += (size_t)n;
    }

    ::close(s);
}

static void io_threads(const Config& cfg, uint16_t port) {
    std::atomic<int> idx{0};
    std::vector<std::thread> workers;
    workers.reserve(cfg.concurrency);

    for (int t = 0; t < cfg.concurrency; t++) {
        workers.emplace_back([&] {
            while (true) {
                int i = idx.fetch_add(1, std::memory_order_relaxed);
                if (i >= cfg.tasks) break;
                io_one_blocking(port, cfg.payload_size, cfg.timeout_ms);
            }
        });
    }
    for (auto& th : workers) th.join();
}

static void io_processes(const Config& cfg, uint16_t port) {
    int launched = 0;
    int completed = 0;

    while (completed < cfg.tasks) {
        while (launched - completed < cfg.concurrency && launched < cfg.tasks) {
            pid_t pid = ::fork();
            if (pid == 0) {
                io_one_blocking(port, cfg.payload_size, cfg.timeout_ms);
                _exit(0);
            }
            launched++;
        }
        int status = 0;
        (void)::wait(&status);
        completed++;
    }
}


class KqueueLoop {
public:
    KqueueLoop() {
        kq_ = ::kqueue();
        if (kq_ < 0) {
            std::perror("kqueue");
            std::exit(1);
        }
    }
    ~KqueueLoop() { ::close(kq_); }

    void arm_read(int fd, std::coroutine_handle<> h) {
        struct kevent kev{};
        EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, (void*)h.address());
        if (::kevent(kq_, &kev, 1, nullptr, 0, nullptr) < 0) {
            std::perror("kevent arm_read");
            std::exit(1);
        }
    }
    void arm_write(int fd, std::coroutine_handle<> h) {
        struct kevent kev{};
        EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, (void*)h.address());
        if (::kevent(kq_, &kev, 1, nullptr, 0, nullptr) < 0) {
            std::perror("kevent arm_write");
            std::exit(1);
        }
    }

    void run_until(std::atomic<int>& pending) {
        constexpr int MAXEV = 256;
        struct kevent evs[MAXEV];

        while (pending.load(std::memory_order_acquire) > 0) {
            timespec ts{};
            ts.tv_sec = 1;
            ts.tv_nsec = 0;

            int n = ::kevent(kq_, nullptr, 0, evs, MAXEV, &ts);
            if (n < 0) {
                if (errno == EINTR) continue;
                std::perror("kevent wait");
                std::exit(1);
            }

            for (int i = 0; i < n; i++) {
                void* addr = evs[i].udata;
                if (!addr) continue;
                std::coroutine_handle<> h = std::coroutine_handle<>::from_address(addr);
                if (h) h.resume();
            }
        }
    }

private:
    int kq_;
};

static int set_nonblocking(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) return -1;
    return 0;
}

struct FdReadable {
    KqueueLoop* loop;
    int fd;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) { loop->arm_read(fd, h); }
    void await_resume() const noexcept {}
};

struct FdWritable {
    KqueueLoop* loop;
    int fd;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) { loop->arm_write(fd, h); }
    void await_resume() const noexcept {}
};

struct IoTask {
    struct promise_type {
        KqueueLoop* loop = nullptr;
        std::atomic<int>* pending = nullptr;

        IoTask get_return_object() { return IoTask{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }

        struct Final {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (h.promise().pending) h.promise().pending->fetch_sub(1, std::memory_order_release);
            }
            void await_resume() noexcept {}
        };

        Final final_suspend() noexcept { return {}; }

        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> h;
    explicit IoTask(std::coroutine_handle<promise_type> h_) : h(h_) {}
    IoTask(IoTask&& o) noexcept : h(o.h) { o.h = {}; }
    ~IoTask() { if (h) h.destroy(); }

    void start(KqueueLoop* loop, std::atomic<int>* pending) {
        h.promise().loop = loop;
        h.promise().pending = pending;
        h.resume();
    }
};

static IoTask io_client_task(KqueueLoop* loop, std::atomic<int>* pending,
                             uint16_t port, int payload_size) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) co_return;
    if (set_nonblocking(s) < 0) { ::close(s); co_return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    int rc = ::connect(s, (sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        if (errno != EINPROGRESS) { ::close(s); co_return; }
        co_await FdWritable{loop, s};
        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
            ::close(s);
            co_return;
        }
    }

    std::vector<char> payload((size_t)payload_size, 'x');

    size_t off = 0;
    while (off < payload.size()) {
        ssize_t w = ::send(s, payload.data() + off, payload.size() - off, 0);
        if (w > 0) { off += (size_t)w; continue; }
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            co_await FdWritable{loop, s};
            continue;
        }
        ::close(s);
        co_return;
    }

    std::vector<char> buf(payload.size());
    size_t got = 0;
    while (got < buf.size()) {
        ssize_t n = ::recv(s, buf.data() + got, buf.size() - got, 0);
        if (n > 0) { got += (size_t)n; continue; }
        if (n == 0) { ::close(s); co_return; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            co_await FdReadable{loop, s};
            continue;
        }
        ::close(s);
        co_return;
    }

    ::close(s);
    co_return;
}

static void io_coroutines_kqueue(const Config& cfg, uint16_t port) {
    KqueueLoop loop;
    std::atomic<int> pending{0};

    int launched = 0;
    int inflight = 0;

    std::vector<IoTask> active;
    active.reserve((size_t)cfg.concurrency);

    auto launch_one = [&]() {
        pending.fetch_add(1, std::memory_order_release);
        inflight++;

        IoTask t = io_client_task(&loop, &pending, port, cfg.payload_size);
        t.start(&loop, &pending);
        active.emplace_back(std::move(t));
        launched++;
    };

    while (launched < cfg.tasks && inflight < cfg.concurrency) launch_one();

    int last_pending = pending.load(std::memory_order_acquire);

    while (pending.load(std::memory_order_acquire) > 0) {
        loop.run_until(pending);

        int p = pending.load(std::memory_order_acquire);
        int finished_now = last_pending - p;
        if (finished_now > 0) {
            inflight -= finished_now;
            last_pending = p;
            while (launched < cfg.tasks && inflight < cfg.concurrency) {
                launch_one();
                last_pending = pending.load(std::memory_order_acquire);
            }
        }
    }
}

int main(int argc, char** argv) {
    Config cfg = parse_args(argc, argv);

    std::cout << "Config: tasks=" << cfg.tasks
              << ", concurrency=" << cfg.concurrency
              << ", repeats=" << cfg.repeats << "\n\n";

    std::cout << "CPU-bound benchmark (pure compute loop)\n\n";
    std::vector<Result> cpu_results;
    cpu_results.push_back(run_repeated(cfg, "threads", [&]{ cpu_threads(cfg); }));
    cpu_results.push_back(run_repeated(cfg, "processes", [&]{ cpu_processes(cfg); }));
    cpu_results.push_back(run_repeated(cfg, "coroutines", [&]{ cpu_coroutines(cfg); }));
    print_md_table("CPU-bound benchmark results", cpu_results);

    EchoServer server;
    if (!server.start(cfg.backlog)) {
        std::cerr << "Failed to start echo server\n";
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "I/O-bound benchmark (local TCP echo)\n\n";
    std::vector<Result> io_results;
    io_results.push_back(run_repeated(cfg, "threads", [&]{ io_threads(cfg, server.port); }));
    io_results.push_back(run_repeated(cfg, "processes", [&]{ io_processes(cfg, server.port); }));
    io_results.push_back(run_repeated(cfg, "coroutines", [&]{ io_coroutines_kqueue(cfg, server.port); }));

    print_md_table("I/O-bound benchmark results", io_results);

    server.stop();

    return 0;
}
