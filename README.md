# golem

Custom STL core from scratch in C++23: `vector`, `optional`, `variant`, and a flat `unordered_map`.

This is a learning project. The goal is not to clone the standard library but to reimplement the four types that teach the most about modern C++: manual lifetime, allocator-aware storage, exception guarantees, active-state management, visitation, and Robin Hood hashing.

---

## What is in here

| Type | File | Notes |
|---|---|---|
| `golem::optional<T>` | `include/golem/optional.hpp` | Manual storage, all four assignment cases, `and_then`, `transform`, `or_else` |
| `golem::vector<T, Alloc>` | `include/golem/vector.hpp` | Geometric growth, `move_if_noexcept` relocation, allocator traits, rollback guards |
| `golem::variant<Ts...>` | `include/golem/variant.hpp` | Inline storage, active-index tracking, `visit`, `get`, `get_if`, `valueless_by_exception` |
| `golem::unordered_map<K,V,...>` | `include/golem/unordered_map.hpp` | Flat open-addressed Robin Hood table, backward-shift erase, hash mixing |

Shared internals live in `include/golem/detail/`:
- `lifetime.hpp` -- `construct_at`, `destroy_at`, `destroy_range`, `raw_storage`, `scope_fail`
- `allocator.hpp` -- `alloc_traits`, `compressed_pair`
- `type_traits.hpp` -- `nth_type_t`, `index_of_v`, `all_unique_v`

---

## Design decisions

**C++23.** No artificial language-version restrictions. Uses concepts for cleaner constraint syntax, `std::bit_ceil` for power-of-two rounding, and structured bindings throughout.

**optional and variant are not allocator-aware.** This matches standard design. Only `vector` and `unordered_map` take allocator template parameters.

**vector strong exception guarantee on reallocation.** During `reallocate`, if `T`'s move constructor is `noexcept`, elements are moved. Otherwise they are copied. If a copy throws midway, a rollback guard destroys already-constructed elements in the new buffer and frees it, leaving the original unchanged.

**Middle insert gives basic guarantee.** Only reallocation paths guarantee strong behavior. Single-element `insert` at a position uses `push_back` plus `std::rotate`, which is basic guarantee if moves can throw.

**unordered_map does not promise std::unordered_map mutation stability.** This is a flat table. Any successful `insert`, `erase`, or `rehash` invalidates all iterators, pointers, and references. This is documented, not a bug.

**Robin Hood backward-shift erase.** No tombstones. When a slot is erased, subsequent occupied slots with probe distance > 0 are shifted back one position, preserving the Robin Hood invariant and keeping clusters compact.

**Hash mixing.** User hashes go through a two-multiply Murmur3-style finalizer before masking to bucket index. This avoids catastrophic collision patterns from bad hash functions.

---

## Invariants

**vector:**
- `size <= capacity`
- `[0, size)` holds exactly `size` live objects
- `[size, capacity)` is raw uninitialized storage
- `data()` is contiguous
- allocation and deallocation counts balance at destruction

**unordered_map:**
- `size` equals the number of occupied slots
- every occupied slot is reachable from its home bucket by forward probing
- probe distances are non-decreasing within a cluster from home
- backward-shift erase leaves no occupied slot behind an empty slot in a cluster
- cached hash in each slot stays consistent with the stored key

**variant:**
- exactly one alternative is live at all times (or `valueless_by_exception` if index is `variant_npos`)
- destructor runs exactly once on the active object
- after failed alternative-change assignment, state becomes `valueless_by_exception`

---

## Build

Requires CMake 3.25+, a C++23 compiler (GCC 13+, Clang 17+, MSVC 19.37+), and Ninja.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

ASan + UBSan (Linux/Clang only):
```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

---

## Benchmarks

All benchmarks run on Linux/aarch64, compiled with Clang 18, `-O2`, median of 7 runs.
Charts are generated automatically on every CI push and available as a downloadable artifact named `benchmark-charts`.

### optional

| Benchmark | golem | std | ratio |
|---|---|---|---|
| construct 1M optionals | 757 µs | 730 µs | 1.04× |
| access value 1M times | 988 ns | 988 ns | 1.00× |
| transform value 1M times | 988 ns | 890 ns | 1.11× |

![optional benchmarks](results/bench_optional.png)

### variant

| Benchmark | golem | std | ratio |
|---|---|---|---|
| construct int variant 1M | 802 µs | 718 µs | 1.12× |
| visit 1M variants | 755 µs | 740 µs | 1.02× |
| get value 1M times | 760 µs | 780 µs | 0.97× |

![variant benchmarks](results/bench_variant.png)

### vector

| Benchmark | golem | std | ratio |
|---|---|---|---|
| push_back 100k ints | 1.39 ms | 733 µs | 1.90× |
| iterate 100k elements | 25.7 µs | 25.9 µs | 0.99× |
| copy construct 100k | 151 µs | 27 µs | 5.60× |
| move construct 100k | 113 µs | 27 µs | 4.20× |

Iteration is equal. The copy/move gap is element-wise construction vs `memcpy` — `std::vector` uses `__builtin_memmove` for trivially copyable types, golem does not yet.

![vector benchmarks](results/bench_vector.png)

### unordered_map

| Benchmark | golem | std | ratio |
|---|---|---|---|
| insert 100k entries | 24.9 ms | 13.3 ms | 1.87× |
| find 100k keys | 2.30 ms | 218 µs | 10.5× |
| erase 50k entries | 10.3 ms | 8.2 ms | 1.26× |

The find gap is expected. `std::unordered_map` uses chained buckets with pointer-stable nodes; the flat Robin Hood table here has better cache behavior in theory but lacks the SIMD metadata probing that makes modern flat maps fast. This is the main target for v2.

![unordered_map benchmarks](results/bench_unordered_map.png)

---

## Out of scope for v1

- PMR, scoped allocators, fancy pointers
- Full `constexpr` parity with the standard
- Bucket API on `unordered_map` (it fits chaining, not flat tables)
- Node handles, transparent lookup, `insert_or_assign`, `try_emplace`
- SIMD probing or SwissTable-style control bytes
- Full standard-library ABI or conformance on every defect report

---

## Previous project

This follows a callable wrapper built in C++23 with inline storage, per-type static vtables, and all eight const/noexcept/ref qualifier combinations. That project is separate.
