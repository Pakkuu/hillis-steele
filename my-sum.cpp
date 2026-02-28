//EPV220001 ETHAN VARGHESE
//AXJ.     AJAY ALLURI

/*
 * my-sum.cpp
 * Implements the Hillis & Steele parallel prefix-sum algorithm using
 * UNIX fork(), shmget() shared memory, and a reusable barrier.
 *
 * Space: O(n)  – two ping-pong arrays of size n plus O(m) barrier state.
 * Time:  O(n log n / m + m log n)
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <cerrno>
#include <cstring>
#include <climits>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

/* ── Barrier ─────────────────────────────────────────────────────────────── */

/*
 * Reusable spinlock barrier backed by atomic counters in shmget() shared
 * memory.  Works across fork()ed processes on Linux and macOS.
 *
 * Fields:
 *   count    – total number of processes
 *   arrived  – processes that have called arriveAndWait() this generation
 *   released – processes that have been woken this generation
 *   gen      – generation counter; prevents spurious wake-ups after reset
 *   buf_idx  – index of the "current" ping-pong buffer (0 or 1); flipped
 *              atomically by the last arriving process each round
 */
struct Barrier {
    int count;
    volatile int arrived;
    volatile int released;
    volatile int gen;
    volatile int buf_idx;
};

/* barrier_init – call before fork(); buf_idx starts at 0. */
static void barrier_init(Barrier* b, int count) {
    b->count    = count;
    b->arrived  = 0;
    b->released = 0;
    b->gen      = 0;
    b->buf_idx  = 0;
}

/* barrier_destroy – no-op (no OS resources to release). */
static void barrier_destroy(Barrier*) {}

/*
 * arriveAndWait – spin until all `count` processes arrive.
 * The last to arrive flips buf_idx and advances the generation to unblock
 * everyone.  Reusable across rounds.
 */
static void arriveAndWait(Barrier* b) {
    int my_gen = __atomic_load_n(&b->gen, __ATOMIC_ACQUIRE);

    int prev = __atomic_fetch_add(&b->arrived, 1, __ATOMIC_ACQ_REL);
    if (prev + 1 == b->count) {
        /* Last to arrive: flip buffer, reset arrived, advance generation. */
        __atomic_fetch_xor(&b->buf_idx, 1, __ATOMIC_ACQ_REL);
        __atomic_store_n(&b->arrived,   0, __ATOMIC_RELEASE);
        __atomic_fetch_add(&b->gen,     1, __ATOMIC_ACQ_REL);
    } else {
        /* Spin until the generation advances. */
        while (__atomic_load_n(&b->gen, __ATOMIC_ACQUIRE) == my_gen)
            ; /* busy-wait */
    }
}

/* ── Shared-memory helpers ───────────────────────────────────────────────── */

/* shm_alloc – allocate a private shared-memory segment of `bytes` bytes. */
static int shm_alloc(size_t bytes) {
    int id = shmget(IPC_PRIVATE, bytes, IPC_CREAT | 0600);
    if (id < 0) {
        std::cerr << "shmget failed: " << strerror(errno) << "\n";
        exit(1);
    }
    return id;
}

/* shm_attach – attach and return a typed pointer to the segment. */
static void* shm_attach(int id) {
    void* p = shmat(id, nullptr, 0);
    if (p == reinterpret_cast<void*>(-1)) {
        std::cerr << "shmat failed: " << strerror(errno) << "\n";
        exit(1);
    }
    return p;
}

/* shm_free – detach and schedule segment removal. */
static void shm_free(int id, void* p) {
    shmdt(p);
    shmctl(id, IPC_RMID, nullptr);
}

/* ── I/O helpers ─────────────────────────────────────────────────────────── */

/*
 * read_and_validate_input – reads tokens from `in`, converts each to a long,
 * and appends to `out`.  Returns false with an error message if any token is
 * not a valid integer or if fewer than `expected_count` are present.
 */
static bool read_and_validate_input(std::istream& in, int expected_count,
                                    std::vector<long>& out) {
    out.clear();
    std::string token;
    while (in >> token) {
        std::istringstream iss(token);
        long v;
        if (!(iss >> v) || !iss.eof()) {
            std::cerr << "\"" << token << "\" is not a valid integer\n";
            return false;
        }
        out.push_back(v);
    }
    if (static_cast<int>(out.size()) < expected_count) {
        std::cerr << "Input has " << out.size()
                  << " element(s); expected at least " << expected_count << "\n";
        return false;
    }
    out.resize(expected_count);    /* keep only the first n */
    return true;
}

/*
 * write_output – writes `n` elements of `arr` space-separated to `out`,
 * followed by a newline.
 */
static void write_output(std::ostream& out, const long* arr, int n) {
    for (int i = 0; i < n; i++) {
        if (i > 0) out << ' ';
        out << arr[i];
    }
    out << '\n';
}

/* ── Worker ──────────────────────────────────────────────────────────────── */

/*
 * worker – executed by each child process.
 *
 * Parameters:
 *   id      – worker index in [0, m)
 *   n       – total number of elements
 *   m       – total number of workers
 *   rounds  – ceil(log2(n)), number of algorithm iterations
 *   bufs    – two shared ping-pong arrays; bufs[0] and bufs[1] each have n longs
 *   barrier – shared reusable barrier; barrier->buf_idx indicates current read buffer
 */
static void worker(int id, int n, int m, int rounds,
                   long* bufs[2], Barrier* barrier) {
    /* Divide [0, n) into m contiguous chunks; last chunk may be one smaller. */
    int chunk = (n + m - 1) / m;
    int lo    = id * chunk;
    int hi    = std::min(lo + chunk, n);   /* exclusive */

    for (int p = 0; p < rounds; p++) {
        int stride = 1 << p;               /* 2^p (rounds are 0-indexed here) */
        int r      = barrier->buf_idx;     /* read from bufs[r] */
        int w      = r ^ 1;               /* write to bufs[w] */

        for (int i = lo; i < hi; i++) {
            if (i < stride)
                bufs[w][i] = bufs[r][i];
            else
                bufs[w][i] = bufs[r][i - stride] + bufs[r][i];
        }

        /*
         * Wait for all workers to finish writing bufs[w].
         * The last to arrive flips barrier->buf_idx so the next round
         * reads from bufs[w].
         */
        arriveAndWait(barrier);
    }
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char* argv[]) {

    /* ---- Argument validation ---- */
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <n> <m> <input_file> <output_file>\n";
        return 1;
    }

    char* end;
    long n_long = strtol(argv[1], &end, 10);
    if (*end != '\0' || n_long <= 0 || n_long > INT_MAX) {
        std::cerr << "n must be a positive integer\n";
        return 1;
    }
    int n = static_cast<int>(n_long);

    long m_long = strtol(argv[2], &end, 10);
    if (*end != '\0' || m_long <= 0 || m_long > INT_MAX) {
        std::cerr << "m must be a positive integer\n";
        return 1;
    }
    int m = static_cast<int>(m_long);

    if (m > n) {
        std::cerr << "m (" << m << ") must not exceed n (" << n << ")\n";
        return 1;
    }

    std::ifstream in_file(argv[3]);
    if (!in_file) {
        std::cerr << "Cannot open input file \"" << argv[3] << "\"\n";
        return 1;
    }

    std::vector<long> input;
    if (!read_and_validate_input(in_file, n, input))
        return 1;
    in_file.close();

    std::ofstream out_file(argv[4]);
    if (!out_file) {
        std::cerr << "Cannot open output file \"" << argv[4] << "\"\n";
        return 1;
    }

    /* ---- Allocate shared memory ---- */

    /* Two ping-pong arrays of n longs. */
    int   buf_id[2];
    long* bufs[2];
    for (int i = 0; i < 2; i++) {
        buf_id[i] = shm_alloc(n * sizeof(long));
        bufs[i]   = static_cast<long*>(shm_attach(buf_id[i]));
    }

    /* Barrier. */
    int      bar_id  = shm_alloc(sizeof(Barrier));
    Barrier* barrier = static_cast<Barrier*>(shm_attach(bar_id));

    /* Copy input into bufs[0] (the initial "current" buffer). */
    for (int i = 0; i < n; i++)
        bufs[0][i] = input[i];

    int rounds = (n == 1) ? 0 : static_cast<int>(std::ceil(std::log2(n)));
    barrier_init(barrier, m);

    /* ---- Fork m worker processes ---- */
    std::vector<pid_t> pids(m);
    for (int id = 0; id < m; id++) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "fork failed: " << strerror(errno) << "\n";
            for (int j = 0; j < id; j++) kill(pids[j], SIGTERM);
            goto cleanup;
        }
        if (pid == 0) {
            /* Child: run then exit; detach shared memory on exit. */
            worker(id, n, m, rounds, bufs, barrier);
            for (int i = 0; i < 2; i++) shmdt(bufs[i]);
            shmdt(barrier);
            exit(0);
        }
        pids[id] = pid;
    }

    /* ---- Wait for all workers ---- */
    for (int id = 0; id < m; id++) {
        int status;
        waitpid(pids[id], &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            std::cerr << "Worker " << id << " exited abnormally\n";
    }

    /* ---- Write result ---- */
    /* After `rounds` flips, barrier->buf_idx points to the output buffer. */
    write_output(out_file, bufs[barrier->buf_idx], n);
    out_file.close();

cleanup:
    barrier_destroy(barrier);
    for (int i = 0; i < 2; i++) shm_free(buf_id[i], bufs[i]);
    shm_free(bar_id, barrier);

    return 0;
}