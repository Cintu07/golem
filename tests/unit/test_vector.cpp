#include "golem/vector.hpp"
#include "tests/support/tracked.hpp"
#include <cassert>
#include <string>
#include <vector>

using golem::vector;

static void test_default_empty()
{
    vector<int> v;
    assert(v.empty());
    assert(v.size() == 0);
    assert(v.capacity() == 0);
}

static void test_push_back_growth()
{
    vector<int> v;
    for (int i = 0; i < 100; ++i)
        v.push_back(i);
    assert(v.size() == 100);
    for (int i = 0; i < 100; ++i)
        assert(v[i] == i);
}

static void test_reserve()
{
    vector<int> v;
    v.reserve(64);
    assert(v.capacity() >= 64);
    assert(v.empty());
}

static void test_emplace_back()
{
    vector<std::string> v;
    v.emplace_back(3, 'a');
    assert(v.size() == 1 && v[0] == "aaa");
}

static void test_pop_back()
{
    vector<int> v = {1, 2, 3};
    v.pop_back();
    assert(v.size() == 2 && v.back() == 2);
}

static void test_initializer_list()
{
    vector<int> v = {10, 20, 30};
    assert(v.size() == 3);
    assert(v[0] == 10 && v[1] == 20 && v[2] == 30);
}

static void test_insert_erase()
{
    vector<int> v = {1, 3, 4};
    auto it = v.insert(v.begin() + 1, 2);
    assert(v.size() == 4);
    assert(*it == 2);
    assert(v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 4);

    v.erase(v.begin() + 2);
    assert(v.size() == 3);
    assert(v[0] == 1 && v[1] == 2 && v[2] == 4);
}

static void test_copy_ctor()
{
    vector<int> a = {1, 2, 3};
    vector<int> b = a;
    assert(b.size() == 3 && b[0] == 1);
    a[0] = 99;
    assert(b[0] == 1);
}

static void test_move_ctor()
{
    vector<std::string> a = {"x", "y"};
    vector<std::string> b(std::move(a));
    assert(b.size() == 2 && b[0] == "x");
    assert(a.empty());
}

static void test_clear()
{
    vector<int> v = {1, 2, 3};
    v.clear();
    assert(v.empty());
    assert(v.capacity() > 0);
}

static void test_resize()
{
    vector<int> v = {1, 2};
    v.resize(5, 9);
    assert(v.size() == 5 && v[4] == 9);
    v.resize(2);
    assert(v.size() == 2 && v[1] == 2);
}

static void test_tracked_no_leak()
{
    tracked_reset();
    {
        vector<tracked> v;
        for (int i = 0; i < 10; ++i)
            v.emplace_back(i);
        assert(tracked::live == 10);
        v.erase(v.begin());
        assert(tracked::live == 9);
    }
    assert(tracked::live == 0);
}

static void test_swap()
{
    vector<int> a = {1, 2}, b = {3, 4, 5};
    a.swap(b);
    assert(a.size() == 3 && b.size() == 2);
    assert(a[0] == 3 && b[0] == 1);
}

static void test_move_only()
{
    vector<move_only> v;
    v.emplace_back(1);
    v.emplace_back(2);
    assert(v.size() == 2 && v[0].value == 1);
    vector<move_only> w(std::move(v));
    assert(w.size() == 2 && v.empty());
}

static void test_differential_push_back()
{
    vector<int> ours;
    std::vector<int> theirs;
    for (int i = 0; i < 500; ++i) {
        ours.push_back(i);
        theirs.push_back(i);
    }
    assert(ours.size() == theirs.size());
    for (std::size_t i = 0; i < ours.size(); ++i)
        assert(ours[i] == theirs[i]);
}

int main()
{
    test_default_empty();
    test_push_back_growth();
    test_reserve();
    test_emplace_back();
    test_pop_back();
    test_initializer_list();
    test_insert_erase();
    test_copy_ctor();
    test_move_ctor();
    test_clear();
    test_resize();
    test_tracked_no_leak();
    test_swap();
    test_move_only();
    test_differential_push_back();
    return 0;
}
