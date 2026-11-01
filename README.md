# C++ Concurrent LRU Cache

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

Requires CMake 3.20+ and a C++20 compiler (tested with GCC 11.5). GoogleTest is downloaded automatically by CMake's FetchContent.

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
./scripts/run_benchmark.sh            # builds Release, runs, saves to results/
./build/cache_benchmark --ops 2000000 --capacity 10000 --keys 20000 --max-threads 8
```

Setup: 1,000,000 operations per run, split evenly across threads. Capacity 10,000, 20,000 distinct keys chosen uniformly at random (so about 50% of gets hit), 32-byte values. The cache is filled to capacity before timing starts. Every operation is timed individually with `std::chrono::steady_clock`. Percentiles use the nearest-rank method on the sorted latencies.

Machine: AMD EPYC-Milan, 8 hardware threads, GCC 11.5.0, `-O3` (Release). Raw output is in [`results/benchmark_20261101_143351.txt`](results/benchmark_20261101_143351.txt).

**Read-heavy (80% get / 20% put)**

| Threads | ops/sec | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|---:|---:|---:|---:|---:|---:|
| 1 | 4,894,059 | 171 | 149 | 290 | 510 |
| 2 | 1,605,573 | 1,203 | 380 | 4,400 | 20,421 |
| 4 | 897,251 | 4,372 | 820 | 18,762 | 36,210 |
| 8 | 844,362 | 9,270 | 3,209 | 37,051 | 74,121 |

**Balanced (50% get / 50% put)**

| Threads | ops/sec | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|---:|---:|---:|---:|---:|---:|
| 1 | 4,570,787 | 186 | 160 | 291 | 390 |
| 2 | 1,442,379 | 1,327 | 280 | 3,780 | 23,291 |
| 4 | 652,937 | 5,837 | 980 | 26,251 | 64,800 |
| 8 | 579,992 | 13,428 | 4,849 | 51,370 | 104,951 |

**Write-heavy (20% get / 80% put)**

| Threads | ops/sec | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|---:|---:|---:|---:|---:|---:|
| 1 | 4,515,588 | 190 | 169 | 291 | 380 |
| 2 | 1,106,064 | 1,767 | 591 | 4,940 | 29,600 |
| 4 | 590,739 | 6,600 | 1,260 | 29,091 | 60,210 |
| 8 | 513,751 | 15,307 | 5,131 | 58,860 | 112,662 |

What the numbers show:
- **More threads made it slower.** Total throughput dropped from about 4.5–4.9M ops/s on one thread to about 0.5–0.8M ops/s on eight. Every operation, `get` included, takes the same lock, so threads mostly wait on each other. Contention also moves the mutex's cache line between cores and puts threads to sleep and wakes them in the kernel, which costs more than the cache work itself.
- **Tail latency grows much faster than the median.** At 8 threads the p50 is a few microseconds, but the p99 is 74–113 µs. Those slow operations are the ones that waited behind several others for the lock.
- **Read-heavy suffers least, but not by much.** `put` does a bit more work than `get` (it may allocate a node and evict one), so its critical section is longer. Even so, read-heavy workloads still serialise, because `get` also writes to the list.

Caveats:
- These runs were on a shared HPC login node with a load average of about 4 during the run, so the multithreaded numbers are noisy. Rerun on an idle machine before quoting exact figures.
- The per-operation timer adds its own overhead, roughly a few tens of nanoseconds, which inflates the single-thread latencies a little.
- Workloads are uniform-random. Real access patterns are usually skewed toward a few popular keys, which changes the hit rate.

## 9. Limitations

- One global lock: correct, but it does not scale with threads. Splitting the cache into several independently locked shards is the usual next step.
- Keys and values are fixed to `std::string`. The class is not a template.
- No TTL or expiration yet.
- `get` returns a copy of the value, so very large values are expensive to read.
- Single process, in memory only: no persistence, networking or distribution, by design.

See [`docs/design.md`](docs/design.md) for design notes and trade-offs.
