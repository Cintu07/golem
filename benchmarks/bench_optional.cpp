#include "bench_common.hpp"
#include "golem/optional.hpp"
#include <optional>

static const int N = 1'000'000;

int main()
{
    bench::row("construct_1m",
        bench::time_ns([]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) {
                golem::optional<int> o(i);
                s ^= static_cast<std::uint64_t>(*o);
            }
            bench::sink ^= s;
        }),
        bench::time_ns([]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) {
                std::optional<int> o(i);
                s ^= static_cast<std::uint64_t>(*o);
            }
            bench::sink ^= s;
        }));

    golem::optional<int> go(42);
    std::optional<int>   so(42);

    bench::row("access_1m",
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) s += static_cast<std::uint64_t>(*go);
            bench::sink ^= s;
        }),
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) s += static_cast<std::uint64_t>(*so);
            bench::sink ^= s;
        }));

    bench::row("transform_1m",
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i)
                s += static_cast<std::uint64_t>(
                    *go.transform([](int x){ return x + 1; }));
            bench::sink ^= s;
        }),
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i)
                s += static_cast<std::uint64_t>(
                    *so.transform([](int x){ return x + 1; }));
            bench::sink ^= s;
        }));
}
