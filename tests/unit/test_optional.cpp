#include "golem/optional.hpp"
#include "tests/support/tracked.hpp"
#include <cassert>
#include <stdexcept>
#include <string>

using golem::optional;
using golem::nullopt;
using golem::in_place;

static void test_default_construct()
{
    optional<int> o;
    assert(!o.has_value());
    assert(!o);
}

static void test_value_construct()
{
    optional<int> o(42);
    assert(o.has_value());
    assert(*o == 42);
}

static void test_nullopt_construct()
{
    optional<int> o(nullopt);
    assert(!o.has_value());
}

static void test_in_place()
{
    optional<std::string> o(in_place, 3, 'x');
    assert(o.has_value());
    assert(*o == "xxx");
}

static void test_copy_ctor()
{
    optional<int> a(7);
    optional<int> b(a);
    assert(b.has_value() && *b == 7);
}

static void test_move_ctor()
{
    optional<std::string> a(in_place, "hello");
    optional<std::string> b(std::move(a));
    assert(b.has_value() && *b == "hello");
}

static void test_copy_assign_four_cases()
{
    // empty <- empty
    optional<int> a, b;
    a = b;
    assert(!a);

    // full <- empty
    optional<int> c(1), d;
    c = d;
    assert(!c);

    // empty <- full
    optional<int> e, f(2);
    e = f;
    assert(e && *e == 2);

    // full <- full
    optional<int> g(3), h(4);
    g = h;
    assert(g && *g == 4);
}

static void test_emplace()
{
    optional<std::string> o;
    o.emplace(3, 'z');
    assert(o && *o == "zzz");

    o.emplace(2, 'a');
    assert(o && *o == "aa");
}

static void test_reset()
{
    optional<int> o(5);
    o.reset();
    assert(!o);
}

static void test_value_throws()
{
    optional<int> o;
    bool threw = false;
    try { o.value(); }
    catch (const golem::bad_optional_access&) { threw = true; }
    assert(threw);
}

static void test_value_or()
{
    optional<int> o;
    assert(o.value_or(99) == 99);
    o = 1;
    assert(o.value_or(99) == 1);
}

static void test_and_then()
{
    optional<int> o(4);
    auto result = o.and_then([](int x) { return optional<int>(x * 2); });
    assert(result && *result == 8);

    optional<int> empty;
    auto result2 = empty.and_then([](int x) { return optional<int>(x * 2); });
    assert(!result2);
}

static void test_transform()
{
    optional<int> o(3);
    auto result = o.transform([](int x) { return x * x; });
    assert(result && *result == 9);

    optional<int> empty;
    auto result2 = empty.transform([](int x) { return x * x; });
    assert(!result2);
}

static void test_or_else()
{
    optional<int> o;
    auto result = o.or_else([] { return optional<int>(42); });
    assert(result && *result == 42);

    optional<int> full(1);
    auto result2 = full.or_else([] { return optional<int>(42); });
    assert(result2 && *result2 == 1);
}

static void test_tracked_no_leak()
{
    tracked_reset();
    {
        optional<tracked> o(in_place, 5);
        assert(tracked::live == 1);
        o.reset();
        assert(tracked::live == 0);
    }
    assert(tracked::live == 0);
}

static void test_swap()
{
    optional<int> a(1), b(2);
    a.swap(b);
    assert(*a == 2 && *b == 1);

    optional<int> c(3), d;
    c.swap(d);
    assert(!c && *d == 3);
}

int main()
{
    test_default_construct();
    test_value_construct();
    test_nullopt_construct();
    test_in_place();
    test_copy_ctor();
    test_move_ctor();
    test_copy_assign_four_cases();
    test_emplace();
    test_reset();
    test_value_throws();
    test_value_or();
    test_and_then();
    test_transform();
    test_or_else();
    test_tracked_no_leak();
    test_swap();
    return 0;
}
