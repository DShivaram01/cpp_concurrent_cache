# C++ Concurrent LRU Cache

[![CI](https://github.com/DShivaram01/cpp_concurrent_cache/actions/workflows/ci.yml/badge.svg)](https://github.com/DShivaram01/cpp_concurrent_cache/actions/workflows/ci.yml)

A small, thread-safe, in-memory key-value cache written in C++20. It has a fixed capacity and evicts the least recently used entry when full. The project comes with unit tests, multithreaded stress tests and a benchmark program.

## 1. What it does

```cpp
LRUCache cache(1000);

cache.put("user:42", "alice");
std::optional<std::string> name = cache.get("user:42");  // "alice"
cache.contains("user:42");                               // true, does not change LRU order
cache.remove("user:42");                                 // true
cache.size();                                            // 0
cache.clear();
```

Keys and values are `std::string`. When the cache is full, `put` with a new key evicts the least recently used entry. A cache with capacity 0 stores nothing.

## 2. Data structures

| Structure | Role |
|---|---|
| `std::list<std::pair<std::string, std::string>>` | Entries in usage order. Front is most recently used, back is least recently used. |
| `std::unordered_map<std::string, std::list<...>::iterator>` | Key to that entry's position in the list. |

The map gives O(1) average lookup. The list gives O(1) reordering and removal once you have an iterator, and `std::list` iterators stay valid when other elements are inserted, moved or erased. Neither structure is enough on its own: a map has no ordering, and finding a key in a list is O(n).

## 3. How LRU works here

- **`get(key)`**: look up the iterator in the map and move that node to the front with `list::splice`. Splicing relinks the node without copying it or allocating.
- **`put(key, value)` for an existing key**: overwrite the value and move it to the front.
- **`put(key, value)` for a new key when full**: remove `list.back()` and its map entry, then insert the new entry at the front.
- **`remove(key)`**: erase the node from the list and the key from the map.
- **`contains(key)`**: map lookup only. It does not count as a use, so the order does not change.

Because `get` moves entries, it changes the cache's state. That is why `get` is not `const`, and why it needs the same exclusive lock as `put`.

## 4. Complexity

| Operation | Average | Notes |
|---|---:|---|
| `get` | O(1) | hash lookup + splice |
| `put` | O(1) | hash lookup + splice or push_front |
| `remove` | O(1) | hash erase + list erase |
| `contains` | O(1) | hash lookup |
| eviction | O(1) | `list.back()` + hash erase |
| `clear` | O(n) | frees every node |

Hash operations are O(n) in the worst case, when many keys collide. The figures above are average-case, which is the standard claim for `unordered_map`.

## 5. Thread safety

One `std::mutex` protects both containers. Every public method except `capacity()` takes a `std::lock_guard` before it touches the containers. `capacity_` is `const` after construction, so reading it needs no lock.

Why a single plain mutex:
- The map and list have to change together. A thread must never see a map entry that points to a list node another thread has just erased.
- `get` writes to the list, so a reader/writer lock (`std::shared_mutex`) would not let `get` calls run in parallel without extra design work.
- It is easy to reason about. Every operation is atomic with respect to every other.

Before the lock was added, the concurrent test suite crashed on its first run with `malloc(): unsorted double linked list corrupted`: two threads were splicing and erasing list nodes at the same time. With the lock, the suite passed 200 repeated runs (`--gtest_repeat=200`) with no failures.

The concurrent tests check that the final state is consistent: `size() <= capacity`, the number of keys reporting `contains() == true` equals `size()`, and every stored value belongs to its key.

## 6. Build

Requires CMake 3.20+ and a C++20 compiler (tested with GCC 11.5 and 13.3). GoogleTest is downloaded automatically by CMake's FetchContent.

```bash
cmake -S . -B build
cmake --build build -j
```

## 7. Tests

```bash
ctest --test-dir build --output-on-failure

# Stress the concurrent tests
./build/concurrent_cache_tests --gtest_repeat=200 --gtest_brief=1
```

- `cache_tests`: 28 single-threaded tests covering eviction order, capacity 0 and 1, updates, remove, clear and 10,000 inserts.
- `concurrent_cache_tests`: 5 tests, each running 8 threads: disjoint inserts, eviction under contention, mixed operations, readers alongside a writer on one key, and `clear` during writes.

## 8. Benchmark results

```bash
./scripts/run_benchmark.sh --ops 4000000 --max-threads 32   # builds Release, runs, saves to results/
```

Setup: 4,000,000 operations per run, split evenly across threads. Capacity 10,000, 20,000 distinct keys chosen uniformly at random (so about 50% of gets hit), 32-byte values. The cache is filled to capacity before timing starts. Every operation is timed individually with `std::chrono::steady_clock`. Percentiles use the nearest-rank method on the sorted latencies.

Machine: AMD Ryzen 9 9950X (16 cores / 32 threads, two 8-core CCDs), Ubuntu, GCC 13.3.0, `-O3` (Release), default CPU frequency governor. Load average was about 0.3 with no other users logged in. I ran the benchmark three times back to back. The tables show the median of the three runs for each row. Raw output: [`run 1`](results/benchmark_20261104_175247.txt), [`run 2`](results/benchmark_20261104_175317.txt), [`run 3`](results/benchmark_20261104_175349.txt).

**Read-heavy (80% get / 20% put)**

| Threads | ops/sec | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|---:|---:|---:|---:|---:|---:|
| 1 | 8,978,083 | 92 | 90 | 160 | 210 |
| 2 | 4,997,949 | 380 | 150 | 1,730 | 3,360 |
| 4 | 4,836,343 | 803 | 280 | 3,100 | 5,720 |
| 8 | 4,074,591 | 1,931 | 950 | 6,430 | 10,830 |
| 16 | 2,144,657 | 7,422 | 5,750 | 20,520 | 31,060 |
| 32 | 1,987,396 | 16,042 | 13,400 | 43,650 | 68,500 |

**Balanced (50% get / 50% put)**

| Threads | ops/sec | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|---:|---:|---:|---:|---:|---:|
| 1 | 9,415,095 | 90 | 80 | 150 | 180 |
| 2 | 4,594,810 | 414 | 140 | 1,820 | 3,690 |
| 4 | 4,168,236 | 936 | 360 | 3,650 | 6,460 |
| 8 | 3,393,261 | 2,315 | 1,060 | 7,910 | 13,110 |
| 16 | 1,964,151 | 8,104 | 5,260 | 23,910 | 36,170 |
| 32 | 1,690,581 | 18,866 | 14,780 | 50,950 | 78,220 |

**Write-heavy (20% get / 80% put)**

| Threads | ops/sec | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|---:|---:|---:|---:|---:|---:|
| 1 | 8,907,234 | 96 | 90 | 150 | 180 |
| 2 | 3,758,910 | 510 | 160 | 2,090 | 4,830 |
| 4 | 3,646,455 | 1,070 | 470 | 4,140 | 7,460 |
| 8 | 2,943,807 | 2,677 | 1,040 | 9,520 | 16,420 |
| 16 | 2,513,764 | 6,331 | 1,950 | 23,950 | 42,320 |
| 32 | 1,273,463 | 25,038 | 9,850 | 90,060 | 147,231 |

What the numbers show:
- **More threads made it slower.** One thread does about 9M ops/s. At 32 threads total throughput is 1.3–2.0M ops/s, so the whole machine gets 14–22% of what a single thread gets alone. Every operation, `get` included, takes the same lock, so only one thread does useful work at a time. Handing the lock over costs more than the work itself, which is about 90 ns: the mutex's cache line has to move to another core, and threads that lose the race sleep and wake through the kernel.
- **Most of the time is spent waiting.** At 32 threads the median operation takes 10–15 µs. The cache work inside it is still under 100 ns.
- **The tail grows much faster than the median.** p99 goes from about 200 ns on one thread to 69 µs (read-heavy) and 147 µs (write-heavy) at 32 threads. Those are the operations that queued behind many others.
- **Writes hurt more than reads, but reads still serialise.** `put` may allocate a node and evict one, so it holds the lock longer. `get` also moves its entry to the front of the list, so it needs the same exclusive lock and can't run in parallel either.

Caveats:
- The 1-, 16- and 32-thread rows are steady, within about 5% across runs (write-heavy at 16 threads is the exception). The 2–16 thread rows moved by 20–50% between runs. Write-heavy at 16 threads ranged from 1.76M to 3.03M ops/s. On this CPU it matters whether the contending threads land on one CCD or are split across both, and the threads were not pinned. Treat the mid-range rows as approximate.
- I didn't have root access, so the frequency governor was left at its default.
- The per-operation timer adds some overhead of its own, a few tens of nanoseconds, which inflates the single-thread latencies a little.
- Workloads are uniform-random. Real access patterns usually favour a few popular keys, which changes the hit rate and concentrates contention.

## 9. Limitations

- One global lock: correct, but throughput drops as threads are added (see section 8). Splitting the cache into several independently locked shards is the usual next step.
- Keys and values are fixed to `std::string`. The class is not a template.
- No TTL or expiration yet.
- `get` returns a copy of the value, so very large values are expensive to read.
- Single process, in memory only: no persistence, networking or distribution, by design.

See [`docs/design.md`](docs/design.md) for design notes and trade-offs.
