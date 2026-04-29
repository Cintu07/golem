#pragma once
#include <stdexcept>
#include <utility>

// Instrumented type that tracks live object count and can throw on demand.

struct tracked
{
    static int live;
    static int throw_after;  // throw on the Nth construction (-1 = never)
    static int ctor_count;

    int value;

    tracked() : value(0)  { inc(); }
    explicit tracked(int v) : value(v) { inc(); }

    tracked(const tracked& o) : value(o.value) { inc(); }
    tracked(tracked&& o) noexcept : value(o.value) { ++live; ++ctor_count; }

    tracked& operator=(const tracked& o) { value = o.value; return *this; }
    tracked& operator=(tracked&& o) noexcept { value = o.value; return *this; }

    ~tracked() noexcept { --live; }

    bool operator==(const tracked& o) const noexcept { return value == o.value; }

private:
    void inc()
    {
        ++ctor_count;
        if (throw_after >= 0 && ctor_count > throw_after)
            throw std::runtime_error("tracked: forced throw");
        ++live;
    }
};

inline int tracked::live         = 0;
inline int tracked::throw_after  = -1;
inline int tracked::ctor_count   = 0;

inline void tracked_reset() noexcept
{
    tracked::live         = 0;
    tracked::throw_after  = -1;
    tracked::ctor_count   = 0;
}

// Move-only type.
struct move_only
{
    int value;
    explicit move_only(int v) : value(v) {}
    move_only(move_only&&) noexcept = default;
    move_only& operator=(move_only&&) noexcept = default;
    move_only(const move_only&) = delete;
    move_only& operator=(const move_only&) = delete;
};

// Non-trivial type with throwing copy constructor.
struct throwing_copy
{
    int value;
    explicit throwing_copy(int v) : value(v) {}
    throwing_copy(const throwing_copy&) { throw std::runtime_error("copy throws"); }
    throwing_copy(throwing_copy&&) noexcept = default;
    throwing_copy& operator=(const throwing_copy&) = delete;
    throwing_copy& operator=(throwing_copy&&) noexcept = default;
};
