//
// zug: transducers for C++
// Copyright (C) 2019 Juan Pedro Bolivar Puente
//
// This software is distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt
//

#include <catch2/catch_all.hpp>

#include <zug/compose.hpp>
#include <zug/into_vector.hpp>
#include <zug/reducing/first.hpp>
#include <zug/transducer/cat.hpp>
#include <zug/transducer/filter.hpp>
#include <zug/transducer/map.hpp>
#include <zug/transducer/partition_by.hpp>
#include <zug/transducer/take.hpp>
#include <zug/transducer/transducer.hpp>

#include "../spies.hpp"

using namespace zug;

TEST_CASE("partition_by, partition_by")
{
    auto v = std::vector<int>{1, 1, 2, 2, 2, 3};

    auto res = into_vector(partition_by(identity), v);
    CHECK(res == (decltype(res){{1, 1}, {2, 2, 2}, {3}}));
}

namespace {
int free_mod2(int x) { return x % 2; }
} // namespace

TEST_CASE("partition_by, invoke")
{
    auto v = std::vector<int>{1, 1, 2, 4, 2, 3};

    auto res = into_vector(partition_by(&free_mod2), v);
    CHECK(res == (decltype(res){{1, 1}, {2, 4, 2}, {3}}));
}

TEST_CASE("partition_by, example")
{
    // example1 {
    auto v   = std::vector<int>{1, 1, 2, 4, 2, 3};
    auto res = into_vector(partition_by([](int x) { return x % 2; }), v);
    using t  = std::vector<std::vector<int>>;
    CHECK(res == t{{1, 1}, {2, 4, 2}, {3}});
    // }
}

TEST_CASE("partition_by, more example")
{
    // example2 {
    auto v   = std::vector<int>{1, 1, 2, 2, 2, 3};
    auto res = into_vector(partition_by(identity), v);
    CHECK(res == decltype(res){{1, 1}, {2, 2, 2}, {3}});
    // }
}

TEST_CASE("partition_by, partition_by does not copy step function")
{
    auto step = testing::copy_spy<first_t>{};

    auto v = std::vector<int>{1, 2, 3, 4, 5, 7, 8, 9};
    reduce(partition_by(identity)(step), 0, v);
    CHECK(step.copied.count() == 2);
}

TEST_CASE("partition_by, partition by moves the state through")
{
    auto v   = std::vector<int>{1, 2, 3, 4, 5};
    auto spy = reduce(partition_by(identity)(first), testing::copy_spy<>{}, v);
    CHECK(spy.copied.count() == 0);
}

/*
TEST_CASE("partition_by, reduce nested deals with empty sequence properly")
{
    auto v    = std::vector<std::vector<int>>{{{}, {1, 1, 2}, {}}};
    auto part = transducer<int, std::vector<int>>{partition_by(identity)};
    auto res  = into_vector(comp(cat, part), v);
    CHECK(res == (std::vector<std::vector<int>>{{1, 1}, {2}}));
}
*/

TEST_CASE("partition_by, more examples 01")
{
    auto ts             = std::vector<int>{2, 4};
    auto partition_func = [ts](int x) {
        size_t idx = 0;
        for (const auto& t : ts) {
            if (x < t) {
                return idx;
            }
            idx++;
        }
        return ts.size();
    };
    auto v   = std::vector<int>{1, 1, 2, 2, 2, 3, 3, 4, 5, 6};
    auto res = into_vector(partition_by(partition_func), v);
    CHECK(res == decltype(res){{1, 1}, {2, 2, 2, 3, 3}, {4, 5, 6}});
}

struct a_t
{
    int t      = 0;
    double val = 0;
    bool operator==(const a_t& rhs) const { return t == rhs.t; }
};

struct b_t
{
    int t       = 0;
    double from = 0;
    double to   = 0;
    bool operator==(const b_t& rhs) const { return t == rhs.t; }
};

using meas_t = std::variant<a_t, b_t>;

auto get_t(meas_t s) -> int
{
    return ZUG_VISIT([](auto&& x) { return x.t; }, s);
};

TEST_CASE("partition_by, more examples 02")
{
    auto ts = std::vector<int>{2, 4};

    auto v = std::vector<meas_t>{a_t{.t = 1},
                                 a_t{.t = 1},
                                 b_t{.t = 2},
                                 a_t{.t = 2},
                                 b_t{.t = 2},
                                 a_t{.t = 3},
                                 b_t{.t = 3},
                                 a_t{.t = 4},
                                 b_t{.t = 5},
                                 a_t{.t = 6}};

    WHEN("v is a variant")
    {
        auto partition_func = [ts](meas_t x) {
            size_t idx = 0;
            for (const auto& t : ts) {
                if (get_t(x) < t) {
                    return idx;
                }
                idx++;
            }
            return ts.size();
        };

        auto res = into_vector(partition_by(partition_func), v);
        CHECK(res == decltype(res){{a_t{.t = 1}, a_t{.t = 1}},
                                   {b_t{.t = 2},
                                    a_t{.t = 2},
                                    b_t{.t = 2},
                                    a_t{.t = 3},
                                    b_t{.t = 3}},
                                   {a_t{.t = 4}, b_t{.t = 5}, a_t{.t = 6}}});
    }
    WHEN("v is a variant, and we filter to just get a_t types")
    {
        auto partition_func = [ts](a_t x) {
            size_t idx = 0;
            for (const auto& t : ts) {
                if (get_t(x) < t) {
                    return idx;
                }
                idx++;
            }
            return ts.size();
        };

        auto just_a =
            zug::filter([](auto x) { return std::holds_alternative<a_t>(x); }) |
            zug::map([](auto x) { return std::get<a_t>(x); }) |
            partition_by(partition_func);

        auto res = into_vector(just_a, v);

        CHECK(res == decltype(res){{a_t{.t = 1}, a_t{.t = 1}},
                                   {a_t{.t = 2}, a_t{.t = 3}},
                                   {a_t{.t = 4}, a_t{.t = 6}}});
    }
}

