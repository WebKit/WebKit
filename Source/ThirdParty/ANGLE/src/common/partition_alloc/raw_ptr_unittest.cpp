// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// raw_ptr_unittest.cpp: Tests for raw_ptr.
//
// These tests are run against both the no-op implementation and
// PartitionAlloc's one, so that the two do not diverge.

#include "common/partition_alloc/raw_ptr.h"
#include "common/angleutils.h"
#include "common/platform.h"
#include "common/unsafe_buffers.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>

#if defined(ANGLE_USE_PARTITION_ALLOC)
#    include <partition_alloc/buildflags.h>
#    if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#        include <partition_alloc/dangling_raw_ptr_checks.h>
#    endif
#endif

namespace
{
struct Base
{
    virtual ~Base() = default;
    int baseValue   = 1;
};

struct Derived : public Base
{
    int derivedValue = 2;
};

// Tests that default construction initializes to nullptr.
TEST(RawPtrTest, DefaultConstruction)
{
    raw_ptr<int> p;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(p);
}

// Tests that construction from nullptr works.
TEST(RawPtrTest, NullptrConstruction)
{
    raw_ptr<int> p = nullptr;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(p);
}

// Tests that construction from a raw pointer works.
TEST(RawPtrTest, RawPointerConstruction)
{
    int value      = 42;
    raw_ptr<int> p = &value;
    EXPECT_EQ(p.get(), &value);
    EXPECT_TRUE(p);
    EXPECT_EQ(*p, 42);
}

// Tests copy and move construction. Both the no-op implementation and
// PartitionAlloc's one zero the moved-from pointer.
TEST(RawPtrTest, CopyAndMoveConstruction)
{
    int value       = 42;
    raw_ptr<int> p1 = &value;

    raw_ptr<int> p2 = p1;
    EXPECT_EQ(p2.get(), &value);
    EXPECT_EQ(p1.get(), &value);

    raw_ptr<int> p3 = std::move(p2);
    EXPECT_EQ(p3.get(), &value);
    EXPECT_EQ(p2.get(), nullptr);  // NOLINT(bugprone-use-after-move)
}

// Tests assignment from raw_ptr, raw pointer and nullptr.
TEST(RawPtrTest, Assignment)
{
    int value1 = 42;
    int value2 = 84;

    raw_ptr<int> p1 = &value1;
    raw_ptr<int> p2 = &value2;

    p1 = p2;
    EXPECT_EQ(p1.get(), &value2);

    p1 = nullptr;
    EXPECT_EQ(p1.get(), nullptr);

    p1 = &value1;
    EXPECT_EQ(p1.get(), &value1);

    raw_ptr<int> p3;
    p3 = std::move(p1);
    EXPECT_EQ(p3.get(), &value1);
    EXPECT_EQ(p1.get(), nullptr);  // NOLINT(bugprone-use-after-move)
}

// Tests that self assignment does not clobber the pointer.
TEST(RawPtrTest, SelfAssignment)
{
    int value      = 42;
    raw_ptr<int> p = &value;

    // Going through a pointer avoids the compiler's self-assignment warning.
    raw_ptr<int> *alias = &p;

    p = *alias;
    EXPECT_EQ(p.get(), &value);

    p = std::move(*alias);
    EXPECT_EQ(p.get(), &value);
}

// Tests implicit conversion from raw_ptr<Derived> to raw_ptr<Base>.
TEST(RawPtrTest, Upcasting)
{
    Derived derived;
    raw_ptr<Derived> pDerived = &derived;

    raw_ptr<Base> pBase(pDerived);
    EXPECT_EQ(pBase.get(), &derived);
    EXPECT_EQ(pBase->baseValue, 1);

    raw_ptr<Base> pBaseAssigned;
    pBaseAssigned = pDerived;
    EXPECT_EQ(pBaseAssigned.get(), &derived);

    raw_ptr<Derived> pDerivedMoved = &derived;
    raw_ptr<Base> pBaseMoved(std::move(pDerivedMoved));
    EXPECT_EQ(pBaseMoved.get(), &derived);
    EXPECT_EQ(pDerivedMoved.get(), nullptr);  // NOLINT(bugprone-use-after-move)

    raw_ptr<Derived> pDerivedMoved2 = &derived;
    raw_ptr<Base> pBaseMoveAssigned;
    pBaseMoveAssigned = std::move(pDerivedMoved2);
    EXPECT_EQ(pBaseMoveAssigned.get(), &derived);
    EXPECT_EQ(pDerivedMoved2.get(), nullptr);  // NOLINT(bugprone-use-after-move)
}

// Tests dereferencing and member access.
TEST(RawPtrTest, DereferenceAndMemberAccess)
{
    struct S
    {
        int x = 42;
    };

    S s          = {};
    raw_ptr<S> p = &s;
    EXPECT_EQ(p->x, 42);
    EXPECT_EQ((*p).x, 42);
    EXPECT_EQ(&*p, &s);
}

// Tests pointer arithmetic, which requires the AllowPtrArithmetic trait.
TEST(RawPtrTest, PointerArithmetic)
{
    // SAFETY: Exercising pointer arithmetic is the point of this test. Every access below stays
    // within the bounds of `values`.
    ANGLE_UNSAFE_BUFFERS({
        int values[3] = {10, 20, 30};

        raw_ptr<int, AllowPtrArithmetic> p = &values[0];
        EXPECT_EQ(p, &values[0]);

        // Pre-increment returns a reference to the incremented pointer.
        raw_ptr<int, AllowPtrArithmetic> &preIncremented = ++p;
        EXPECT_EQ(&preIncremented, &p);
        EXPECT_EQ(p, &values[1]);

        // Post-increment returns the value from before the increment.
        raw_ptr<int, AllowPtrArithmetic> previous = p++;
        EXPECT_EQ(previous, &values[1]);
        EXPECT_EQ(p, &values[2]);

        // Pre-decrement returns a reference to the decremented pointer.
        raw_ptr<int, AllowPtrArithmetic> &preDecremented = --p;
        EXPECT_EQ(&preDecremented, &p);
        EXPECT_EQ(p, &values[1]);

        // Post-decrement returns the value from before the decrement.
        previous = p--;
        EXPECT_EQ(previous, &values[1]);
        EXPECT_EQ(p, &values[0]);

        static_assert(std::is_same_v<decltype(p + 1), raw_ptr<int, AllowPtrArithmetic>>);
        static_assert(std::is_same_v<decltype(1 + p), raw_ptr<int, AllowPtrArithmetic>>);
        static_assert(std::is_same_v<decltype(p - 1), raw_ptr<int, AllowPtrArithmetic>>);
        static_assert(std::is_same_v<decltype(p - p), ptrdiff_t>);

        EXPECT_EQ(p + 1, &values[1]);
        EXPECT_EQ(1 + p, &values[1]);
        EXPECT_EQ(p + 2, &values[2]);

        raw_ptr<int, AllowPtrArithmetic> last = &values[2];
        EXPECT_EQ(last - 1, &values[1]);
        EXPECT_EQ(last - 2, &values[0]);

        // Difference between two raw_ptr.
        EXPECT_EQ(last - p, 2);

        p += 2;
        EXPECT_EQ(p, &values[2]);

        p -= 2;
        EXPECT_EQ(p, &values[0]);

        EXPECT_EQ(p[1], 20);
    });
}

// Tests equality comparisons against raw_ptr, raw pointers and nullptr, in both
// operand orders.
TEST(RawPtrTest, EqualityComparisons)
{
    int value1 = 42;
    int value2 = 84;

    raw_ptr<int> p1   = &value1;
    raw_ptr<int> p2   = &value2;
    raw_ptr<int> null = nullptr;

    EXPECT_TRUE(p1 == p1);
    EXPECT_FALSE(p1 == p2);
    EXPECT_TRUE(p1 != p2);

    EXPECT_TRUE(p1 == &value1);
    EXPECT_TRUE(&value1 == p1);
    EXPECT_FALSE(p1 == &value2);
    EXPECT_FALSE(&value2 == p1);

    EXPECT_TRUE(null == nullptr);
    EXPECT_TRUE(nullptr == null);
    EXPECT_FALSE(p1 == nullptr);
    EXPECT_FALSE(nullptr == p1);
}

// Tests relational comparisons (<, <=, >, >=) in both operand orders.
TEST(RawPtrTest, RelationalComparisons)
{
    int values[2]   = {42, 84};
    raw_ptr<int> p0 = &values[0];
    raw_ptr<int> p1 = &values[1];

    EXPECT_TRUE(p0 < p1);
    EXPECT_TRUE(p0 <= p1);
    EXPECT_TRUE(p1 > p0);
    EXPECT_TRUE(p1 >= p0);

    EXPECT_FALSE(p1 < p0);
    EXPECT_FALSE(p1 <= p0);
    EXPECT_FALSE(p0 > p1);
    EXPECT_FALSE(p0 >= p1);

    EXPECT_TRUE(p0 <= p0);
    EXPECT_TRUE(p0 >= p0);

    EXPECT_TRUE(p0 < &values[1]);
    EXPECT_TRUE(p0 <= &values[1]);
    EXPECT_TRUE(&values[1] > p0);
    EXPECT_TRUE(&values[1] >= p0);
}

// Tests const pointee types.
TEST(RawPtrTest, ConstPointee)
{
    const int value      = 100;
    raw_ptr<const int> p = &value;
    EXPECT_EQ(p.get(), &value);
    EXPECT_EQ(*p, 100);
}

// Tests void pointee types, which do not support dereferencing.
TEST(RawPtrTest, Void)
{
    int value = 42;

    raw_ptr<void> p = &value;
    EXPECT_EQ(p.get(), &value);
    EXPECT_TRUE(p);

    raw_ptr<const void> constPointer = &value;
    EXPECT_EQ(constPointer.get(), &value);
}

// Tests interoperability with the standard library: std::less (through
// std::set), std::hash (through std::unordered_set) and std::to_address.
TEST(RawPtrTest, StandardLibrary)
{
    int value1 = 1;
    int value2 = 2;

    raw_ptr<int> p1 = &value1;
    raw_ptr<int> p2 = &value2;

    std::set<raw_ptr<int>> set;
    set.insert(p1);
    set.insert(p2);
    EXPECT_EQ(set.size(), 2u);
    EXPECT_NE(set.find(p1), set.end());
    // std::less is transparent, so lookup with a raw pointer creates no
    // temporary raw_ptr.
    EXPECT_NE(set.find(&value1), set.end());

    std::unordered_set<raw_ptr<int>> unorderedSet;
    unorderedSet.insert(p1);
    unorderedSet.insert(p2);
    EXPECT_EQ(unorderedSet.size(), 2u);
    EXPECT_NE(unorderedSet.find(p1), unorderedSet.end());

    EXPECT_EQ(std::to_address(p1), &value1);
}

// Tests that a raw_ptr marked with DisableDanglingPtrDetection is allowed to
// dangle.
TEST(RawPtrTest, DisableDanglingPtrDetection)
{
    std::unique_ptr<int> owner                       = std::make_unique<int>(42);
    raw_ptr<int, DisableDanglingPtrDetection> danger = owner.get();
    std::ignore                                      = danger;
    owner.reset();
}

// Tests ExtractAsDangling, which clears the pointer and returns a raw_ptr
// allowed to dangle.
TEST(RawPtrTest, ExtractAsDangling)
{
    int value                                          = 42;
    raw_ptr<int> p                                     = &value;
    raw_ptr<int, DisableDanglingPtrDetection> dangling = p.ExtractAsDangling();
    EXPECT_EQ(dangling.get(), &value);
    EXPECT_EQ(p.get(), nullptr);
}

#if defined(ANGLE_USE_PARTITION_ALLOC)
#    if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
class ScopedDanglingPointerTracker : angle::NonCopyable
{
  public:
    ScopedDanglingPointerTracker()
        : mOldDetectedFn(partition_alloc::GetDanglingRawPtrDetectedFn()),
          mOldReleasedFn(partition_alloc::GetDanglingRawPtrReleasedFn())
    {
        sDetected = false;
        sReleased = false;
        partition_alloc::SetDanglingRawPtrDetectedFn([](uintptr_t) { sDetected = true; });
        partition_alloc::SetDanglingRawPtrReleasedFn([](uintptr_t) { sReleased = true; });
    }

    ~ScopedDanglingPointerTracker()
    {
        partition_alloc::SetDanglingRawPtrDetectedFn(mOldDetectedFn);
        partition_alloc::SetDanglingRawPtrReleasedFn(mOldReleasedFn);
    }

    bool detected() const { return sDetected; }
    bool released() const { return sReleased; }

  private:
    static inline bool sDetected = false;
    static inline bool sReleased = false;

    partition_alloc::DanglingRawPtrDetectedFn *mOldDetectedFn;
    partition_alloc::DanglingRawPtrReleasedFn *mOldReleasedFn;
};

// Tests that dangling raw_ptr instances are detected and reported when
// ENABLE_DANGLING_RAW_PTR_CHECKS is enabled.
TEST(RawPtrTest, DanglingPointerDetected)
{
    ScopedDanglingPointerTracker tracker;

    std::unique_ptr<int> owner = std::make_unique<int>(42);
    raw_ptr<int> ptr           = owner.get();
    EXPECT_FALSE(tracker.detected());
    EXPECT_FALSE(tracker.released());

    owner.reset();
    EXPECT_TRUE(tracker.detected());
    EXPECT_FALSE(tracker.released());

    ptr = nullptr;
    EXPECT_TRUE(tracker.detected());
    EXPECT_TRUE(tracker.released());
}

#        if defined(GTEST_HAS_DEATH_TEST)
// Tests that releasing a dangling raw_ptr triggers FATAL crash when
// ENABLE_DANGLING_RAW_PTR_CHECKS is enabled.
TEST(RawPtrTest, DanglingPointerDetectedDeathTest)
{
    // PartitionAlloc-as-malloc is currently only enabled on Linux in ANGLE
    // standalone CQ, whereas it is also enabled on other platforms when
    // angle_unittests is built inside Chromium (where this test fails on macOS
    // and Windows). Suspected causes:
    // - On Apple, FATAL() logs via os_log_with_type() rather than stderr.
    // - On Windows, EXPECT_DEATH re-spawns the test binary, which may conflict
    //   with angle::TestSuite's --test-launcher-bot-mode flag on bots.
    // - On Android, stderr is buffered to a file and may not be flushed on
    //   trap.
    //
    // TODO(b/503180635): Investigate and enable this test on additional
    // platforms once we can run it on them in ANGLE standalone.
#            if !defined(ANGLE_PLATFORM_LINUX)
    GTEST_SKIP() << "Only supported on Linux for now.";
#            else
    EXPECT_DEATH(
        {
            std::unique_ptr<int> owner = std::make_unique<int>(42);
            raw_ptr<int> ptr           = owner.get();
            owner.reset();
            ptr = nullptr;
        },
        "DanglingPointerDetector: A pointer was dangling!");
#            endif
}
#        endif
#    endif  // PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) &&
            // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#endif      // defined(ANGLE_USE_PARTITION_ALLOC)

}  // namespace
