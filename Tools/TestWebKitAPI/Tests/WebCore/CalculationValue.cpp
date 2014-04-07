/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/CalculationValue.h>

namespace TestWebKitAPI {

static unsigned deletionCount;

class CalculationDeletionTestNode : public WebCore::CalcExpressionNode {
public:
    virtual ~CalculationDeletionTestNode()
    {
        ++deletionCount;
    }

    virtual float evaluate(float) const override { return 0; }
    bool operator==(const CalcExpressionNode&) const { ASSERT_NOT_REACHED(); return false; }
};

static PassRef<WebCore::CalculationValue> createTestValue()
{
    auto node = std::make_unique<CalculationDeletionTestNode>();
    return WebCore::CalculationValue::create(std::move(node), WebCore::CalculationRangeAll);
}

TEST(CalculationValue, LengthConstruction)
{
    RefPtr<WebCore::CalculationValue> value = createTestValue();

    EXPECT_EQ(1U, value->refCount());

    {
        WebCore::Length length(*value);
        EXPECT_EQ(2U, value->refCount());
    }

    EXPECT_EQ(1U, value->refCount());

    {
        WebCore::Length lengthA(*value);
        EXPECT_EQ(2U, value->refCount());
        WebCore::Length lengthB(lengthA);
        EXPECT_EQ(2U, value->refCount());
    }

    EXPECT_EQ(1U, value->refCount());

    {
        WebCore::Length lengthC(*value);
        EXPECT_EQ(2U, value->refCount());
        WebCore::Length lengthD(std::move(lengthC));
        EXPECT_EQ(2U, value->refCount());
    }

    EXPECT_EQ(1U, value->refCount());

    EXPECT_EQ(0U, deletionCount);
    value = nullptr;
    EXPECT_EQ(1U, deletionCount);
    deletionCount = 0;
}

TEST(CalculationValue, LengthConstructionReleasedValue)
{
    RefPtr<WebCore::CalculationValue> value = createTestValue();

    EXPECT_EQ(1U, value->refCount());

    {
        auto* rawValue = value.get();
        WebCore::Length length(value.releaseNonNull());
        EXPECT_EQ(1U, rawValue->refCount());

        EXPECT_EQ(0U, deletionCount);
    }

    EXPECT_EQ(1U, deletionCount);
    deletionCount = 0;

    value = createTestValue();

    {
        auto* rawValue = value.get();
        WebCore::Length lengthA(value.releaseNonNull());
        EXPECT_EQ(1U, rawValue->refCount());
        WebCore::Length lengthB(lengthA);
        EXPECT_EQ(1U, rawValue->refCount());

        EXPECT_EQ(0U, deletionCount);
    }

    EXPECT_EQ(1U, deletionCount);
    deletionCount = 0;

    value = createTestValue();

    {
        auto* rawValue = value.get();
        WebCore::Length lengthC(value.releaseNonNull());
        EXPECT_EQ(1U, rawValue->refCount());
        WebCore::Length lengthD(std::move(lengthC));
        EXPECT_EQ(1U, rawValue->refCount());

        EXPECT_EQ(0U, deletionCount);
    }

    EXPECT_EQ(1U, deletionCount);
    deletionCount = 0;
}

TEST(CalculationValue, LengthAssignment)
{
    RefPtr<WebCore::CalculationValue> value = createTestValue();

    EXPECT_EQ(1U, value->refCount());

    {
        WebCore::Length lengthA(*value);
        EXPECT_EQ(2U, value->refCount());
        WebCore::Length lengthB;
        lengthB = lengthA;
        EXPECT_EQ(2U, value->refCount());
    }

    EXPECT_EQ(1U, value->refCount());

    {
        WebCore::Length lengthC(*value);
        EXPECT_EQ(2U, value->refCount());
        WebCore::Length lengthD;
        lengthD = std::move(lengthC);
        EXPECT_EQ(2U, value->refCount());
    }

    EXPECT_EQ(1U, value->refCount());

    EXPECT_EQ(0U, deletionCount);
    value = nullptr;
    EXPECT_EQ(1U, deletionCount);
    deletionCount = 0;

    value = createTestValue();
    RefPtr<WebCore::CalculationValue> value2 = createTestValue();

    EXPECT_EQ(1U, value->refCount());
    EXPECT_EQ(1U, value2->refCount());

    {
        WebCore::Length lengthE(*value);
        EXPECT_EQ(2U, value->refCount());
        WebCore::Length lengthF(*value2);
        EXPECT_EQ(2U, value2->refCount());
        lengthE = lengthF;
        EXPECT_EQ(1U, value->refCount());
        EXPECT_EQ(2U, value2->refCount());
    }

    EXPECT_EQ(1U, value->refCount());
    EXPECT_EQ(1U, value2->refCount());

    {
        WebCore::Length lengthG(*value);
        EXPECT_EQ(2U, value->refCount());
        WebCore::Length lengthH(*value2);
        EXPECT_EQ(2U, value2->refCount());
        lengthG = std::move(lengthH);
        EXPECT_EQ(1U, value->refCount());
        EXPECT_EQ(2U, value2->refCount());
    }

    EXPECT_EQ(0U, deletionCount);
    value = nullptr;
    EXPECT_EQ(1U, deletionCount);
    value2 = nullptr;
    EXPECT_EQ(2U, deletionCount);
    deletionCount = 0;
}

TEST(CalculationValue, LengthAssignmentReleasedValue)
{
    RefPtr<WebCore::CalculationValue> value = createTestValue();

    {
        auto* rawValue = value.get();
        WebCore::Length lengthA(value.releaseNonNull());
        EXPECT_EQ(1U, rawValue->refCount());
        WebCore::Length lengthB;
        lengthB = lengthA;
        EXPECT_EQ(1U, rawValue->refCount());

        EXPECT_EQ(0U, deletionCount);
    }

    EXPECT_EQ(1U, deletionCount);
    deletionCount = 0;

    value = createTestValue();

    {
        auto* rawValue = value.get();
        WebCore::Length lengthC(value.releaseNonNull());
        EXPECT_EQ(1U, rawValue->refCount());
        WebCore::Length lengthD;
        lengthD = std::move(lengthC);
        EXPECT_EQ(1U, rawValue->refCount());

        EXPECT_EQ(0U, deletionCount);
    }

    EXPECT_EQ(1U, deletionCount);
    deletionCount = 0;

    value = createTestValue();
    RefPtr<WebCore::CalculationValue> value2 = createTestValue();

    EXPECT_EQ(1U, value->refCount());
    EXPECT_EQ(1U, value2->refCount());

    {
        auto* rawValue = value.get();
        WebCore::Length lengthE(value.releaseNonNull());
        EXPECT_EQ(1U, rawValue->refCount());
        auto* rawValue2 = value2.get();
        WebCore::Length lengthF(value2.releaseNonNull());
        EXPECT_EQ(1U, rawValue2->refCount());

        lengthE = lengthF;
        EXPECT_EQ(1U, deletionCount);
        EXPECT_EQ(1U, rawValue2->refCount());
    }

    EXPECT_EQ(2U, deletionCount);
    deletionCount = 0;

    value = createTestValue();
    value2 = createTestValue();

    EXPECT_EQ(1U, value->refCount());
    EXPECT_EQ(1U, value2->refCount());

    {
        auto* rawValue = value.get();
        WebCore::Length lengthG(value.releaseNonNull());
        EXPECT_EQ(1U, rawValue->refCount());
        auto* rawValue2 = value2.get();
        WebCore::Length lengthH(value2.releaseNonNull());
        EXPECT_EQ(1U, rawValue2->refCount());

        lengthG = std::move(lengthH);
        EXPECT_EQ(1U, deletionCount);
        EXPECT_EQ(1U, rawValue2->refCount());
    }

    EXPECT_EQ(2U, deletionCount);
    deletionCount = 0;
}

}
