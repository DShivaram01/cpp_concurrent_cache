# Design Notes

## Layout in memory

```text
index_ (unordered_map)                usageOrder_ (list, MRU -> LRU)
+-----------+----------+
| "key:7"   |  iter ---+---------->  [ "key:7" | v ]  <- most recently used
| "key:3"   |  iter ---+------+      [ "key:9" | v ]
| "key:9"   |  iter ---+---+  +--->  [ "key:3" | v ]  <- evicted next
+-----------+----------+   +-------> (points at "key:9")
```

The key is stored twice: once as the map key and once inside the list node. Eviction needs the key stored in the list, because it starts from `list.back()` and must erase the matching map entry. Storing the key only once (for example, a `string_view` into the node) would save memory but tie the two containers' lifetimes together. That is not worth the risk at this size.

## Invariants

These hold whenever the mutex is not held:

1. `index_.size() == usageOrder_.size()`
2. `usageOrder_.size() <= capacity_`
3. For every `(k, it)` in `index_`, `it->first == k`.
4. Every list node is pointed to by exactly one map entry.

Every mutating method preserves them by changing both containers inside one critical section. The concurrent tests check invariants 1–3 from the outside after the threads join.

## Why `get` is not `const`

In an LRU cache a read counts as a use. Making `get` `const` would require marking the list `mutable` and would hide the fact that `get` writes to shared state. Keeping it non-`const` makes the side effect obvious. It is also why a plain `std::shared_mutex` does not help directly: `get` would still need the exclusive lock.

`contains` is deliberately different. It is a pure lookup that does not change the order, so it really is `const`.

## Why one `std::mutex`

Alternatives considered:

| Option | Why not (for now) |
|---|---|
| `std::shared_mutex` | `get` changes the list, so it needs the exclusive lock anyway. Only `contains` and `size` would run in parallel. Not worth the extra complexity without measurements showing a benefit. |
| Sharding (N caches, each with its own lock, chosen by `hash(key) % N`) | Fixes the scaling problem the benchmark shows. The cost is that LRU becomes approximate (per shard), and `size`/`clear` must visit every shard. This is the natural next step. |
| Lock-free structures | Much harder to get right and outside this project's scope. |

## Race conditions without the lock

Examples of what goes wrong when two threads run at the same time:

- **Thread A `get(k)` splices node X to the front while thread B evicts X.** A ends up using a freed node. This is the heap corruption seen in the unlocked test run.
- **Two `put` calls with new keys when the cache has one free slot.** Both see `size < capacity`, both insert, and the cache goes over capacity.
- **`unordered_map` rehash during a concurrent `find`.** The bucket array is reallocated while another thread is reading it.

## Benchmark method

- Operations (`get` or `put`, plus a key index) are generated before timing with a fixed-seed `std::mt19937` per thread. Random number generation and key formatting are therefore not part of the measured time.
- A `std::latch` holds all threads until they are ready, then releases them together. Wall time is measured from the release until the last thread joins.
- Each thread writes its latencies into a pre-sized vector, so recording does not allocate. Afterwards all latencies are merged and sorted.
- p50/p95/p99 use nearest rank: `sorted[ceil(p/100 * n) - 1]`.
