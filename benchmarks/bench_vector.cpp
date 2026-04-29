#include "bench_common.hpp"
#include "golem/vector.hpp"
#include <numeric>
#include <vector>

static const int N = 100'000;

static void push_back()
{
    {
        golem::vector<int> v;
        for (int i = 0; i < N; ++i) v.push_back(i);
        bench::sink ^= static_cast<std::uint64_t>(v.back());
    }
}

static void push_back_std()
{
    std::vector<int> v;
    for (int i = 0; i < N; ++i) v.push_back(i);
    bench::sink ^= static_cast<std::uint64_t>(v.back());
}

static void iterate(golem::vector<int>& v)
{
    std::uint64_t s = 0;
    for (int x : v) s += static_cast<std::uint64_t>(x);
    bench::sink ^= s;
}

static void iterate_std(std::vector<int>& v)
{
    std::uint64_t s = 0;
    for (int x : v) s += static_cast<std::uint64_t>(x);
    bench::sink ^= s;
}

static void copy_construct(golem::vector<int>& src)
{
    golem::vector<int> v(src);
    bench::sink ^= static_cast<std::uint64_t>(v.size());
}

static void copy_construct_std(std::vector<int>& src)
{
    std::vector<int> v(src);
    bench::sink ^= static_cast<std::uint64_t>(v.size());
}

int main()
{
    golem::vector<int> gv;
    std::vector<int>   sv;
    for (int i = 0; i < N; ++i) { gv.push_back(i); sv.push_back(i); }

    bench::row("push_back_100k",
        bench::time_ns(push_back),
        bench::time_ns(push_back_std));

    bench::row("iterate_100k",
        bench::time_ns([&]{ iterate(gv); }),
        bench::time_ns([&]{ iterate_std(sv); }));

    bench::row("copy_construct_100k",
        bench::time_ns([&]{ copy_construct(gv); }),
        bench::time_ns([&]{ copy_construct_std(sv); }));

    bench::row("move_construct_100k",
        bench::time_ns([&]{
            golem::vector<int> tmp(gv);
            golem::vector<int> v(std::move(tmp));
            bench::sink ^= static_cast<std::uint64_t>(v.size());
        }),
        bench::time_ns([&]{
            std::vector<int> tmp(sv);
            std::vector<int> v(std::move(tmp));
            bench::sink ^= static_cast<std::uint64_t>(v.size());
        }));
}
