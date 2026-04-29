#include "bench_common.hpp"
#include "golem/unordered_map.hpp"
#include <unordered_map>

static const int N = 100'000;

static void fill(golem::unordered_map<int,int>& m)
{
    for (int i = 0; i < N; ++i) m.insert({i, i * 2});
}

static void fill_std(std::unordered_map<int,int>& m)
{
    for (int i = 0; i < N; ++i) m.insert({i, i * 2});
}

int main()
{
    bench::row("insert_100k",
        bench::time_ns([]{
            golem::unordered_map<int,int> m;
            fill(m);
            bench::sink ^= static_cast<std::uint64_t>(m.size());
        }),
        bench::time_ns([]{
            std::unordered_map<int,int> m;
            fill_std(m);
            bench::sink ^= static_cast<std::uint64_t>(m.size());
        }));

    golem::unordered_map<int,int> gm;
    std::unordered_map<int,int>   sm;
    fill(gm);
    fill_std(sm);

    bench::row("find_100k",
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i)
                s ^= static_cast<std::uint64_t>(gm.find(i)->second);
            bench::sink ^= s;
        }),
        bench::time_ns([&]{
            std::uint64_t s = 0;
            for (int i = 0; i < N; ++i)
                s ^= static_cast<std::uint64_t>(sm.find(i)->second);
            bench::sink ^= s;
        }));

    bench::row("erase_50k",
        bench::time_ns([&]{
            golem::unordered_map<int,int> tmp(gm);
            for (int i = 0; i < N / 2; ++i) tmp.erase(i);
            bench::sink ^= static_cast<std::uint64_t>(tmp.size());
        }),
        bench::time_ns([&]{
            std::unordered_map<int,int> tmp(sm);
            for (int i = 0; i < N / 2; ++i) tmp.erase(i);
            bench::sink ^= static_cast<std::uint64_t>(tmp.size());
        }));
}
