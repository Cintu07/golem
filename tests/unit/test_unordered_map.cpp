#include "golem/unordered_map.hpp"
#include <cassert>
#include <string>
#include <unordered_map>

using golem::unordered_map;

static void test_empty()
{
    unordered_map<int, int> m;
    assert(m.empty());
    assert(m.size() == 0);
}

static void test_insert_and_find()
{
    unordered_map<int, std::string> m;
    auto [it, inserted] = m.insert({1, "one"});
    assert(inserted);
    assert(it->second == "one");
    assert(m.size() == 1);
    assert(m.contains(1));
    assert(!m.contains(2));
}

static void test_duplicate_insert()
{
    unordered_map<int, int> m;
    m.insert({1, 10});
    auto [it, inserted] = m.insert({1, 20});
    assert(!inserted);
    assert(it->second == 10);
    assert(m.size() == 1);
}

static void test_operator_bracket()
{
    unordered_map<int, int> m;
    m[1] = 100;
    assert(m[1] == 100);
    m[1] = 200;
    assert(m[1] == 200);
    m[2];
    assert(m.size() == 2);
}

static void test_at()
{
    unordered_map<int, int> m;
    m.insert({5, 50});
    assert(m.at(5) == 50);
    bool threw = false;
    try { m.at(99); }
    catch (const std::out_of_range&) { threw = true; }
    assert(threw);
}

static void test_erase_key()
{
    unordered_map<int, int> m;
    m.insert({1, 1});
    m.insert({2, 2});
    std::size_t removed = m.erase(1);
    assert(removed == 1);
    assert(!m.contains(1));
    assert(m.contains(2));
    assert(m.size() == 1);
}

static void test_clear()
{
    unordered_map<int, int> m;
    for (int i = 0; i < 20; ++i)
        m.insert({i, i * 2});
    m.clear();
    assert(m.empty());
}

static void test_rehash_preserves_contents()
{
    unordered_map<int, int> m;
    for (int i = 0; i < 50; ++i)
        m.insert({i, i});
    m.rehash(256);
    for (int i = 0; i < 50; ++i)
        assert(m.at(i) == i);
}

static void test_iterators()
{
    unordered_map<int, int> m;
    for (int i = 0; i < 10; ++i)
        m.insert({i, i * 3});
    int count = 0;
    for (auto& kv : m) {
        assert(kv.second == kv.first * 3);
        ++count;
    }
    assert(count == 10);
}

static void test_copy_and_move()
{
    unordered_map<int, int> a;
    for (int i = 0; i < 20; ++i) a.insert({i, i});

    unordered_map<int, int> b(a);
    assert(b.size() == 20);
    for (int i = 0; i < 20; ++i) assert(b.at(i) == i);

    unordered_map<int, int> c(std::move(a));
    assert(c.size() == 20);
    assert(a.empty());
}

static void test_differential_vs_std()
{
    unordered_map<std::string, int> ours;
    std::unordered_map<std::string, int> theirs;

    for (int i = 0; i < 200; ++i) {
        std::string k = std::to_string(i);
        ours[k]   = i * 7;
        theirs[k] = i * 7;
    }

    assert(ours.size() == theirs.size());
    for (auto& [k, v] : theirs) {
        assert(ours.contains(k));
        assert(ours.at(k) == v);
    }

    // Erase half.
    for (int i = 0; i < 100; ++i) {
        std::string k = std::to_string(i);
        ours.erase(k);
        theirs.erase(k);
    }

    assert(ours.size() == theirs.size());
    for (auto& [k, v] : theirs) {
        assert(ours.contains(k));
        assert(ours.at(k) == v);
    }
}

int main()
{
    test_empty();
    test_insert_and_find();
    test_duplicate_insert();
    test_operator_bracket();
    test_at();
    test_erase_key();
    test_clear();
    test_rehash_preserves_contents();
    test_iterators();
    test_copy_and_move();
    test_differential_vs_std();
    return 0;
}
