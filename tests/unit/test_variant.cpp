#include "golem/variant.hpp"
#include "tests/support/tracked.hpp"
#include <cassert>
#include <string>

using golem::variant;
using golem::monostate;

static void test_default_ctor()
{
    variant<int, float> v;
    assert(v.index() == 0);
    assert(golem::holds_alternative<int>(v));
}

static void test_converting_ctor()
{
    variant<int, std::string> v(std::string("hi"));
    assert(v.index() == 1);
    assert(golem::get<std::string>(v) == "hi");
}

static void test_in_place_type()
{
    variant<int, std::string> v(golem::in_place_type<std::string>, 3, 'z');
    assert(v.index() == 1);
    assert(golem::get<std::string>(v) == "zzz");
}

static void test_in_place_index()
{
    variant<int, std::string> v(golem::in_place_index<0>, 42);
    assert(v.index() == 0);
    assert(golem::get<0>(v) == 42);
}

static void test_copy_ctor()
{
    variant<int, std::string> a(std::string("copy"));
    variant<int, std::string> b(a);
    assert(b.index() == 1 && golem::get<std::string>(b) == "copy");
}

static void test_move_ctor()
{
    variant<int, std::string> a(std::string("move"));
    variant<int, std::string> b(std::move(a));
    assert(b.index() == 1 && golem::get<std::string>(b) == "move");
}

static void test_copy_assign_same_alt()
{
    variant<int, std::string> a(1), b(2);
    a = b;
    assert(golem::get<int>(a) == 2);
}

static void test_copy_assign_diff_alt()
{
    variant<int, std::string> a(1);
    variant<int, std::string> b(std::string("hello"));
    a = b;
    assert(a.index() == 1 && golem::get<std::string>(a) == "hello");
}

static void test_emplace()
{
    variant<int, std::string> v(0);
    v.emplace<std::string>(4, 'x');
    assert(v.index() == 1 && golem::get<std::string>(v) == "xxxx");
}

static void test_visit()
{
    variant<int, std::string> v(42);
    int saw = 0;
    golem::visit([&](auto& val) {
        if constexpr (std::is_same_v<std::remove_reference_t<decltype(val)>, int>)
            saw = val;
    }, v);
    assert(saw == 42);
}

static void test_get_throws()
{
    variant<int, std::string> v(1);
    bool threw = false;
    try { golem::get<std::string>(v); }
    catch (const golem::bad_variant_access&) { threw = true; }
    assert(threw);
}

static void test_get_if()
{
    variant<int, std::string> v(99);
    assert(golem::get_if<int>(&v) != nullptr);
    assert(*golem::get_if<int>(&v) == 99);
    assert(golem::get_if<std::string>(&v) == nullptr);
}

static void test_monostate()
{
    variant<monostate, int> v;
    assert(v.index() == 0);
    assert(golem::holds_alternative<monostate>(v));
}

static void test_tracked_no_leak()
{
    tracked_reset();
    {
        variant<tracked, int> v(golem::in_place_type<tracked>, 5);
        assert(tracked::live == 1);
        v.emplace<int>(0);
        assert(tracked::live == 0);
    }
    assert(tracked::live == 0);
}

int main()
{
    test_default_ctor();
    test_converting_ctor();
    test_in_place_type();
    test_in_place_index();
    test_copy_ctor();
    test_move_ctor();
    test_copy_assign_same_alt();
    test_copy_assign_diff_alt();
    test_emplace();
    test_visit();
    test_get_throws();
    test_get_if();
    test_monostate();
    test_tracked_no_leak();
    return 0;
}
