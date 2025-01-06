/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "LifecycleLogger.h"
#include "MoveOnly.h"
#include "RefLogger.h"
#include <wtf/CompactVariant.h>
#include <wtf/GetPtr.h>

struct EmptyStruct {
    constexpr bool operator==(const EmptyStruct&) const = default;
};

struct SmallEnoughStruct {
    float value { 0 };

    constexpr bool operator==(const SmallEnoughStruct&) const = default;
    constexpr bool operator==(float other) const { return value == other; };
};

struct TooBigStruct {
    double value { 0 };

    constexpr bool operator==(const TooBigStruct&) const = default;
    constexpr bool operator==(double other) const { return value == other; };
};

// Treat LifecycleLogger as smart pointer to allow its use in CompactVariant.
template<> struct WTF::IsSmartPtr<TestWebKitAPI::LifecycleLogger> {
    static constexpr bool value = true;
};

template<> struct WTF::CompactVariantTraits<TooBigStruct> {
   static constexpr bool hasAlternativeRepresentation = true;

   static constexpr uint64_t encodeFromArguments(double value)
   {
       return static_cast<uint64_t>(std::bit_cast<uint32_t>(static_cast<float>(value)));
   }

   static constexpr uint64_t encode(const TooBigStruct& value)
   {
       return static_cast<uint64_t>(std::bit_cast<uint32_t>(static_cast<float>(value.value)));
   }

   static constexpr uint64_t encode(TooBigStruct&& value)
   {
       return static_cast<uint64_t>(std::bit_cast<uint32_t>(static_cast<float>(value.value)));
   }

   static constexpr TooBigStruct decode(uint64_t value)
   {
       return { std::bit_cast<float>(static_cast<uint32_t>(value)) };
   }
};

namespace TestWebKitAPI {

TEST(WTF_CompactVariant, Pointers)
{
    int testInt = 1;
    float testFloat = 2.0f;

    CompactVariant<int*, float*> variant = &testInt;
    EXPECT_TRUE(WTF::holdsAlternative<int*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)   { EXPECT_EQ(*value, 1); },
        [&](float* const& value) { FAIL(); }
    );

    variant = &testFloat;
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<float*>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)   { FAIL(); },
        [&](float* const& value) { EXPECT_EQ(*value, 2.0f); }
    );
}

TEST(WTF_CompactVariant, SmartPointers)
{
    {
        RefLogger testRefLogger("testRefLogger");
        Ref<RefLogger> ref(testRefLogger);

        CompactVariant<Ref<RefLogger>, std::unique_ptr<double>> variant { std::in_place_type<Ref<RefLogger>>, WTFMove(ref) };

        EXPECT_TRUE(WTF::holdsAlternative<Ref<RefLogger>>(variant));
        EXPECT_FALSE(WTF::holdsAlternative<std::unique_ptr<double>>(variant));

        WTF::switchOn(variant,
            [&](const Ref<RefLogger>&)          { SUCCEED(); },
            [&](const std::unique_ptr<double>&) { FAIL(); }
        );

        variant = std::make_unique<double>(2.0);
        EXPECT_FALSE(WTF::holdsAlternative<Ref<RefLogger>>(variant));
        EXPECT_TRUE(WTF::holdsAlternative<std::unique_ptr<double>>(variant));

        WTF::switchOn(variant,
            [&](const Ref<RefLogger>&)                { FAIL(); },
            [&](const std::unique_ptr<double>& value) { EXPECT_EQ(*value, 2.0);  }
        );
    }
    ASSERT_STREQ("ref(testRefLogger) deref(testRefLogger) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, SmallScalars)
{
    float testFloat = 2.0f;

    CompactVariant<int*, float*, float> variant = 3.0f;
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<float>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)   { FAIL(); },
        [&](float* const& value) { FAIL(); },
        [&](const float& value)  { EXPECT_EQ(value, 3.0f); }
    );

    variant = &testFloat;
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<float*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)   { FAIL(); },
        [&](float* const& value) { EXPECT_EQ(*value, 2.0f); },
        [&](const float& value)  { FAIL(); }
    );
}

TEST(WTF_CompactVariant, EmptyStruct)
{
    float testFloat = 2.0f;

    CompactVariant<int*, float*, EmptyStruct> variant = EmptyStruct { };
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<EmptyStruct>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)   { FAIL(); },
        [&](float* const& value) { FAIL(); },
        [&](const EmptyStruct&)  { SUCCEED(); }
    );

    variant = &testFloat;
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<float*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<EmptyStruct>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)   { FAIL(); },
        [&](float* const& value) { EXPECT_EQ(*value, 2.0f); },
        [&](const EmptyStruct&)  { FAIL(); }
    );
}

TEST(WTF_CompactVariant, SmallEnoughStruct)
{
    float testFloat = 2.0f;

    CompactVariant<int*, float*, SmallEnoughStruct> variant = SmallEnoughStruct { 3.0f };
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<SmallEnoughStruct>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)              { FAIL(); },
        [&](float* const& value)            { FAIL(); },
        [&](const SmallEnoughStruct& value) { EXPECT_EQ(value.value, 3.0f); }
    );

    variant = &testFloat;
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<float*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<SmallEnoughStruct>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)        { FAIL(); },
        [&](float* const& value)      { EXPECT_EQ(*value, 2.0f); },
        [&](const SmallEnoughStruct&) { FAIL(); }
    );
}

TEST(WTF_CompactVariant, TooBigStruct)
{
    float testFloat = 2.0f;

    CompactVariant<int*, float*, TooBigStruct> variant = TooBigStruct { 4.0 };
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<TooBigStruct>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)        { FAIL(); },
        [&](float* const& value)      { FAIL(); },
        [&](const TooBigStruct value) { EXPECT_EQ(value.value, 4.0); }
    );

    variant = &testFloat;
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<float*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<TooBigStruct>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)   { FAIL(); },
        [&](float* const& value) { EXPECT_EQ(*value, 2.0f); },
        [&](const TooBigStruct)  { FAIL(); }
    );

    variant = TooBigStruct { 5.0 };
    CompactVariant<int*, float*, TooBigStruct> movedToVariant = WTFMove(variant);

    EXPECT_TRUE(variant.valueless_by_move());

    EXPECT_FALSE(WTF::holdsAlternative<int*>(movedToVariant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(movedToVariant));
    EXPECT_TRUE(WTF::holdsAlternative<TooBigStruct>(movedToVariant));

    WTF::switchOn(movedToVariant,
        [&](int* const& value)        { FAIL(); },
        [&](float* const& value)      { FAIL(); },
        [&](const TooBigStruct value) { EXPECT_EQ(value.value, 5.0); }
    );

    variant = movedToVariant;

    EXPECT_FALSE(WTF::holdsAlternative<int*>(movedToVariant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(movedToVariant));
    EXPECT_TRUE(WTF::holdsAlternative<TooBigStruct>(movedToVariant));
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<TooBigStruct>(variant));

    WTF::switchOn(movedToVariant,
        [&](int* const& value)        { FAIL(); },
        [&](float* const& value)      { FAIL(); },
        [&](const TooBigStruct value) { EXPECT_EQ(value.value, 5.0); }
    );
    WTF::switchOn(variant,
        [&](int* const& value)        { FAIL(); },
        [&](float* const& value)      { FAIL(); },
        [&](const TooBigStruct value) { EXPECT_EQ(value.value, 5.0); }
    );
}

TEST(WTF_CompactVariant, MoveOnlyStruct)
{
    float testFloat = 2.0f;

    CompactVariant<int*, float*, MoveOnly> variant = MoveOnly { 5u };
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<MoveOnly>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)      { FAIL(); },
        [&](float* const& value)    { FAIL(); },
        [&](const MoveOnly& value)  { EXPECT_EQ(value.value(), 5u); }
    );

    variant = &testFloat;
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<float*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<MoveOnly>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)   { FAIL(); },
        [&](float* const& value) { EXPECT_EQ(*value, 2.0f); },
        [&](const MoveOnly&)     { FAIL(); }
    );

    variant = MoveOnly { 6u };
    EXPECT_FALSE(WTF::holdsAlternative<int*>(variant));
    EXPECT_FALSE(WTF::holdsAlternative<float*>(variant));
    EXPECT_TRUE(WTF::holdsAlternative<MoveOnly>(variant));

    WTF::switchOn(variant,
        [&](int* const& value)     { FAIL(); },
        [&](float* const& value)   { FAIL(); },
        [&](const MoveOnly& value) { EXPECT_EQ(value.value(), 6u); }
    );
}

TEST(WTF_CompactVariant, ValuelessByMove)
{
    int testInt = 1;
    CompactVariant<int*, float*> variant = &testInt;
    EXPECT_FALSE(variant.valueless_by_move());

    CompactVariant<int*, float*> other = WTFMove(variant);
    EXPECT_FALSE(other.valueless_by_move());
    EXPECT_TRUE(variant.valueless_by_move());

    // Test copying the "valueless_by_move" variant.
    CompactVariant<int*, float*> copy = variant;
    EXPECT_TRUE(variant.valueless_by_move());
    EXPECT_TRUE(copy.valueless_by_move());

    // Test re-moving the "valueless_by_move" variant.
    CompactVariant<int*, float*> moved = WTFMove(variant);
    EXPECT_TRUE(variant.valueless_by_move());
    EXPECT_TRUE(moved.valueless_by_move());
}

TEST(WTF_CompactVariant, ArgumentAssignment)
{
    {
        CompactVariant<float, LifecycleLogger> variant = LifecycleLogger("compact");

        ASSERT_STREQ("construct(compact) move-construct(compact) destruct(<default>) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}


TEST(WTF_CompactVariant, ArgumentConstruct)
{
    {
        CompactVariant<float, LifecycleLogger> variant { LifecycleLogger("compact") };

        ASSERT_STREQ("construct(compact) move-construct(compact) destruct(<default>) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentConstructInPlaceType)
{
    {
        CompactVariant<float, LifecycleLogger> variant { std::in_place_type<LifecycleLogger>, "compact" };

        ASSERT_STREQ("construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentConstructInPlaceIndex)
{
    {
        CompactVariant<float, LifecycleLogger> variant { std::in_place_index<1>, "compact" };

        ASSERT_STREQ("construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentMoveConstruct)
{
    {
        LifecycleLogger lifecycleLogger("compact");
        CompactVariant<float, LifecycleLogger> variant { WTFMove(lifecycleLogger) };

        ASSERT_STREQ("construct(compact) move-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) destruct(<default>) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentCopyConstruct)
{
    {
        LifecycleLogger lifecycleLogger("compact");
        CompactVariant<float, LifecycleLogger> variant { lifecycleLogger };

        ASSERT_STREQ("construct(compact) copy-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentMoveAssignment)
{
    {
        LifecycleLogger lifecycleLogger("compact");
        CompactVariant<float, LifecycleLogger> variant = WTFMove(lifecycleLogger);

        ASSERT_STREQ("construct(compact) move-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) destruct(<default>) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentCopyAssignment)
{
    {
        LifecycleLogger lifecycleLogger("compact");
        CompactVariant<float, LifecycleLogger> variant = lifecycleLogger;

        ASSERT_STREQ("construct(compact) copy-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, CopyConstruct)
{
    {
        CompactVariant<float, LifecycleLogger> variant { std::in_place_type<LifecycleLogger>, "compact" };

        CompactVariant<float, LifecycleLogger> other { variant };

        ASSERT_STREQ("construct(compact) copy-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, CopyAssignment)
{
    {
        CompactVariant<float, LifecycleLogger> variant { std::in_place_type<LifecycleLogger>, "compact" };

        CompactVariant<float, LifecycleLogger> other = variant;

        ASSERT_STREQ("construct(compact) copy-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, MoveConstruct)
{
    {
        CompactVariant<float, LifecycleLogger> variant { std::in_place_type<LifecycleLogger>, "compact" };

        CompactVariant<float, LifecycleLogger> other { WTFMove(variant) };

        ASSERT_STREQ("construct(compact) move-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, MoveAssignment)
{
    {
        CompactVariant<float, LifecycleLogger> variant { std::in_place_type<LifecycleLogger>, "compact" };

        CompactVariant<float, LifecycleLogger> other = WTFMove(variant);

        ASSERT_STREQ("construct(compact) move-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ConstructThenReassign)
{
    {
        CompactVariant<float, LifecycleLogger> variant { std::in_place_type<LifecycleLogger>, "compact" };

        variant = 1.0f;

        ASSERT_STREQ("construct(compact) destruct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentReassignment)
{
    {
        CompactVariant<float, LifecycleLogger> variant { 1.0f };

        variant = LifecycleLogger { "compact" };

        ASSERT_STREQ("construct(compact) move-construct(compact) destruct(<default>) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentCopyReassignment)
{
    {
        CompactVariant<float, LifecycleLogger> variant { 1.0f };

        LifecycleLogger lifecycleLogger { "compact" };
        variant = lifecycleLogger;

        ASSERT_STREQ("construct(compact) copy-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, ArgumentMoveReassignment)
{
    {
        CompactVariant<float, LifecycleLogger> variant { 1.0f };

        LifecycleLogger lifecycleLogger { "compact" };
        variant = WTFMove(lifecycleLogger);

        ASSERT_STREQ("construct(compact) move-construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(<default>) destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, EmplaceType)
{
    {
        CompactVariant<float, LifecycleLogger> variant { 1.0f };

        variant.emplace<LifecycleLogger>("compact");

        ASSERT_STREQ("construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, EmplaceIndex)
{
    {
        CompactVariant<float, LifecycleLogger> variant { 1.0f };

        variant.emplace<1>("compact");

        ASSERT_STREQ("construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

TEST(WTF_CompactVariant, SwitchOn)
{
    // `switchOn` should not cause any lifecycle events.
    {
        CompactVariant<float, LifecycleLogger> variant { std::in_place_type<LifecycleLogger>, "compact" };

        WTF::switchOn(variant,
            [&](const float&) { },
            [&](const LifecycleLogger&) { }
        );

        ASSERT_STREQ("construct(compact) ", takeLogStr().c_str());
    }
    ASSERT_STREQ("destruct(compact) ", takeLogStr().c_str());
}

} // namespace TestWebKitAPI
