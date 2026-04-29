#include "bench_common.hpp"
#include "golem/variant.hpp"
#include <variant>

static const int N = 1'000'000;

int main()
{
    bench::row("construct_int_1m",
        bench::time_ns([]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) {
                golem::variant<int, double> v(i);
                s ^= static_cast<std::uint64_t>(golem::get<int>(v));
            }
            bench::sink ^= s;
        }),
        bench::time_ns([]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) {
                std::variant<int, double> v(i);
                s ^= static_cast<std::uint64_t>(std::get<int>(v));
            }
            bench::sink ^= s;
        }));

    bench::row("visit_1m",
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) {
                golem::variant<int, double> v(i);
                golem::visit([&](auto x){
                    s ^= static_cast<std::uint64_t>(x);
                }, v);
            }
            bench::sink ^= s;
        }),
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) {
                std::variant<int, double> v(i);
                std::visit([&](auto x){
                    s ^= static_cast<std::uint64_t>(x);
                }, v);
            }
            bench::sink ^= s;
        }));

    bench::row("get_1m",
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) {
                golem::variant<int, double> v(i);
                s ^= static_cast<std::uint64_t>(golem::get<int>(v));
            }
            bench::sink ^= s;
        }),
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i) {
                std::variant<int, double> v(i);
                s ^= static_cast<std::uint64_t>(std::get<int>(v));
            }
            bench::sink ^= s;
        }));
}
