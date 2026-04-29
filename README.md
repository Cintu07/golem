# golem

Four STL types rebuilt from scratch in C++23. No wrappers, no shortcuts. Raw storage, manual lifetime, real allocator support.

`optional` `vector` `variant` `unordered_map`

## what's in it

| type | what it does |
|---|---|
| `golem::optional<T>` | manual storage, all four assignment cases, `and_then` / `transform` / `or_else` |
| `golem::vector<T>` | geometric growth, `move_if_noexcept` relocation, strong exception guarantee on realloc |
| `golem::variant<Ts...>` | inline storage, `visit`, `get`, `get_if`, `valueless_by_exception` |
| `golem::unordered_map<K,V>` | flat Robin Hood table, backward-shift erase, no tombstones, hash mixing |

Internals are in `include/golem/detail/`. Lifetime primitives, allocator traits, compressed pair with EBO, and type pack utilities for variant.

## build

Needs CMake 3.25+, a C++23 compiler (GCC 13+, Clang 17+, MSVC 19.37+), and Ninja.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

ASan + UBSan (Linux/Clang):
```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

## benchmarks

Compiled with Clang 18, `-O2`, median of 7 runs on Linux/aarch64. Charts auto-generated on CI and uploaded as the `benchmark-charts` artifact on every push.

### optional

| benchmark | golem | std | ratio |
|---|---|---|---|
| construct 1M | 757 us | 730 us | 1.04x |
| access 1M | 988 ns | 988 ns | 1.00x |
| transform 1M | 988 ns | 890 ns | 1.11x |

![optional](results/bench_optional.png)

### variant

| benchmark | golem | std | ratio |
|---|---|---|---|
| construct int 1M | 802 us | 718 us | 1.12x |
| visit 1M | 755 us | 740 us | 1.02x |
| get 1M | 760 us | 780 us | 0.97x |

![variant](results/bench_variant.png)

### vector

| benchmark | golem | std | ratio |
|---|---|---|---|
| push_back 100k | 1.39 ms | 733 us | 1.90x |
| iterate 100k | 25.7 us | 25.9 us | 0.99x |
| copy construct 100k | 151 us | 27 us | 5.60x |
| move construct 100k | 113 us | 27 us | 4.20x |

Iteration is equal. The copy/move gap is element-wise construction vs memcpy. std uses __builtin_memmove for trivially copyable types.

![vector](results/bench_vector.png)

### unordered_map

| benchmark | golem | std | ratio |
|---|---|---|---|
| insert 100k | 24.9 ms | 13.3 ms | 1.87x |
| find 100k | 2.30 ms | 218 us | 10.5x |
| erase 50k | 10.3 ms | 8.2 ms | 1.26x |

The find gap is the honest cost of no SIMD probing. std uses chained buckets with decades of hardware tuning; a flat table without metadata bytes cannot match that yet.

![unordered_map](results/bench_unordered_map.png)

