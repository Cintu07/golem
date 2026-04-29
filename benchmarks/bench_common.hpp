#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <string_view>

namespace bench {

using clock = std::chrono::steady_clock;

// Sink: prevent the compiler from optimizing away computed results.
// XOR the value into this before each benchmark completes.
inline volatile std::uint64_t sink = 0;

// Run f() `reps` times, each time calling f `inner` times.
// Return the median per-call time in nanoseconds.
template<typename F>
double time_ns(F&& f, int reps = 7, int inner = 1)
{
    std::array<double, 16> samples{};
    std::size_t n = (reps < 16) ? static_cast<std::size_t>(reps) : 16u;
    for (std::size_t i = 0; i < n; ++i) {
        auto t0 = clock::now();
        for (int j = 0; j < inner; ++j) f();
        auto t1 = clock::now();
        using ns = std::chrono::nanoseconds;
        samples[i] = static_cast<double>(
            std::chrono::duration_cast<ns>(t1 - t0).count()) / inner;
    }
    std::sort(samples.begin(), samples.begin() + n);
    return samples[n / 2];
}

// Print one CSV row: benchmark name, golem nanoseconds, std nanoseconds.
inline void row(std::string_view name, double golem_ns, double std_ns)
{
    std::printf("%s,%.0f,%.0f\n", name.data(), golem_ns, std_ns);
}

} // namespace bench
