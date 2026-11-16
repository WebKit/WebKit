// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// raw_ref_unittest.cpp: Tests for raw_ref.
//
// These tests are run against both the no-op implementation and
// PartitionAlloc's one, so that the two do not diverge.

#include "common/partition_alloc/raw_ref.h"

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{
// raw_ref is not default constructible: it always refers to an object.
static_assert(!std::is_default_constructible_v<raw_ref<int>>);

struct Base
{
    virtual ~Base() = default;
    int baseValue   = 1;
};

struct Derived : public Base
{
    int derivedValue = 2;
};

// Tests construction from a reference and from a pointer.
TEST(RawRefTest, Construction)
{
    int value = 42;

    raw_ref<int> r(value);
    EXPECT_EQ(&r.get(), &value);
    EXPECT_EQ(&*r, &value);
    EXPECT_EQ(*r, 42);

    raw_ref<int> fromPointer = raw_ref<int>::from_ptr(&value);
    EXPECT_EQ(&fromPointer.get(), &value);
}

// Tests copy and move construction.
TEST(RawRefTest, CopyAndMoveConstruction)
{
    int value = 42;

    raw_ref<int> r1(value);

    raw_ref<int> r2(r1);
    EXPECT_EQ(&r2.get(), &value);
    EXPECT_EQ(&r1.get(), &value);

    raw_ref<int> r3(std::move(r1));
    EXPECT_EQ(&r3.get(), &value);
}

// Tests assignment from a raw_ref and from a reference.
TEST(RawRefTest, Assignment)
{
    int value1 = 42;
    int value2 = 84;

    raw_ref<int> r1(value1);
    raw_ref<int> r2(value2);

    r1 = r2;
    EXPECT_EQ(&r1.get(), &value2);

    r1 = value1;
    EXPECT_EQ(&r1.get(), &value1);

    raw_ref<int> r3(value2);
    r3 = std::move(r1);
    EXPECT_EQ(&r3.get(), &value1);

    // Self-assignment and self-move assignment must preserve the referent.
    raw_ref<int> *alias = &r3;
    r3                  = *alias;
    EXPECT_EQ(&r3.get(), &value1);

    r3 = std::move(*alias);
    EXPECT_EQ(&r3.get(), &value1);
}

// Tests implicit conversion from raw_ref<Derived> to raw_ref<Base>.
TEST(RawRefTest, Upcasting)
{
    Derived derived;
    raw_ref<Derived> rDerived(derived);

    raw_ref<Base> rBase(rDerived);
    EXPECT_EQ(&rBase.get(), static_cast<Base *>(&derived));
    EXPECT_EQ(rBase->baseValue, 1);

    raw_ref<Derived> rDerivedMoved(derived);
    raw_ref<Base> rBaseMoved(std::move(rDerivedMoved));
    EXPECT_EQ(&rBaseMoved.get(), static_cast<Base *>(&derived));
}

// Tests dereferencing and member access.
TEST(RawRefTest, DereferenceAndMemberAccess)
{
    struct S
    {
        int x = 42;
    };

    S s = {};
    raw_ref<S> r(s);
    EXPECT_EQ(r->x, 42);
    EXPECT_EQ((*r).x, 42);
    EXPECT_EQ(r.get().x, 42);
}

// Tests that mutations through a raw_ref are visible on the referent.
TEST(RawRefTest, Mutation)
{
    int value = 42;
    raw_ref<int> r(value);

    *r = 84;
    EXPECT_EQ(value, 84);
}

// Tests equality comparisons against raw_ref and references, in both operand
// orders.
TEST(RawRefTest, EqualityComparisons)
{
    int value1 = 1;
    int value2 = 2;

    raw_ref<int> r1(value1);
    raw_ref<int> r1Alias(value1);
    raw_ref<int> r2(value2);

    EXPECT_TRUE(r1 == r1Alias);
    EXPECT_FALSE(r1 == r2);
    EXPECT_TRUE(r1 != r2);

    EXPECT_TRUE(r1 == value1);
    EXPECT_TRUE(value1 == r1);
    EXPECT_TRUE(r1 != value2);
    EXPECT_TRUE(value2 != r1);
}

// Tests relational comparisons (<, <=, >, >=) in both operand orders.
TEST(RawRefTest, RelationalComparisons)
{
    int values[2] = {42, 84};

    raw_ref<int> r0(values[0]);
    raw_ref<int> r1(values[1]);

    EXPECT_TRUE(r0 < r1);
    EXPECT_TRUE(r0 <= r1);
    EXPECT_TRUE(r1 > r0);
    EXPECT_TRUE(r1 >= r0);

    EXPECT_FALSE(r1 < r0);
    EXPECT_FALSE(r0 > r1);

    EXPECT_TRUE(r0 < values[1]);
    EXPECT_TRUE(values[1] > r0);
}

// Tests swapping two raw_ref.
TEST(RawRefTest, Swap)
{
    int value1 = 1;
    int value2 = 2;

    raw_ref<int> r1(value1);
    raw_ref<int> r2(value2);

    swap(r1, r2);
    EXPECT_EQ(&r1.get(), &value2);
    EXPECT_EQ(&r2.get(), &value1);
}

// Tests const referent types.
TEST(RawRefTest, ConstReferent)
{
    const int value = 100;
    raw_ref<const int> r(value);
    EXPECT_EQ(&r.get(), &value);
    EXPECT_EQ(*r, 100);
}

// Tests the std::less and std::pointer_traits specializations.
TEST(RawRefTest, StandardLibrary)
{
    int value1 = 42;
    int value2 = 84;

    std::set<raw_ref<int>> set;
    set.emplace(value1);
    set.emplace(value2);
    EXPECT_EQ(set.size(), 2u);
    EXPECT_NE(set.find(raw_ref<int>(value1)), set.end());

    // std::less is transparent: a reference can be looked up directly.
    EXPECT_NE(set.find(value1), set.end());

    raw_ref<int> r(value1);
    EXPECT_EQ(std::to_address(r), &value1);
    EXPECT_EQ(&std::pointer_traits<raw_ref<int>>::pointer_to(value2).get(), &value2);
}

// Tests that a raw_ref marked with DisableDanglingPtrDetection is allowed to
// dangle.
TEST(RawRefTest, DisableDanglingPtrDetection)
{
    std::unique_ptr<int> owner = std::make_unique<int>(42);
    raw_ref<int, DisableDanglingPtrDetection> danger(*owner);
    std::ignore = danger;
    owner.reset();
}

// Tests that raw_ref can be constructed from a type that overloads unary operator&.
TEST(RawRefTest, OverloadedAddressOf)
{
    struct CustomAddressOf
    {
        int value;
        void operator&() const = delete;
    };

    CustomAddressOf instance{42};
    raw_ref<CustomAddressOf> ref(instance);
    EXPECT_EQ(ref->value, 42);
    EXPECT_EQ((*ref).value, 42);
    EXPECT_EQ(std::addressof(ref.get()), std::addressof(instance));
}

}  // namespace
