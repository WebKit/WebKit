/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <JavaScriptCore/InspectorValues.h>

using namespace Inspector;

namespace TestWebKitAPI {

TEST(InspectorValue, MemoryCostNull)
{
    Ref<InspectorValue> value = InspectorValue::null();
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 8U);
}

TEST(InspectorValue, MemoryCostBoolean)
{
    Ref<InspectorValue> value = InspectorValue::create(true);
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 8U);
}

TEST(InspectorValue, MemoryCostDouble)
{
    Ref<InspectorValue> value = InspectorValue::create(1.0);
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 8U);
}

TEST(InspectorValue, MemoryCostInteger)
{
    Ref<InspectorValue> value = InspectorValue::create(1);
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 8U);
}

TEST(InspectorValue, MemoryCostString)
{
    Ref<InspectorValue> value = InspectorValue::create("test");
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 36U);
}

TEST(InspectorValue, MemoryCostStringEmpty)
{
    Ref<InspectorValue> value = InspectorValue::create("");
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 32U);
}

TEST(InspectorValue, MemoryCostStringNull)
{
    Ref<InspectorValue> value = InspectorValue::create(String());
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 8U);
}

TEST(InspectorValue, MemoryCostStringGrowing)
{
    Ref<InspectorValue> valueA = InspectorValue::create("t");
    Ref<InspectorValue> valueB = InspectorValue::create("te");
    Ref<InspectorValue> valueC = InspectorValue::create("tes");
    Ref<InspectorValue> valueD = InspectorValue::create("test");
    EXPECT_LT(valueA->memoryCost(), valueB->memoryCost());
    EXPECT_LT(valueB->memoryCost(), valueC->memoryCost());
    EXPECT_LT(valueC->memoryCost(), valueD->memoryCost());
}

TEST(InspectorValue, MemoryCostStringUnicode)
{
    Ref<InspectorValue> valueA = InspectorValue::create("t");
    Ref<InspectorValue> valueB = InspectorValue::create("😀");
    EXPECT_LT(valueA->memoryCost(), valueB->memoryCost());
}

TEST(InspectorValue, MemoryCostObject)
{
    Ref<InspectorObject> value = InspectorObject::create();
    value->setValue("test", InspectorValue::null());
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 20U);
}

TEST(InspectorValue, MemoryCostObjectEmpty)
{
    Ref<InspectorObject> value = InspectorObject::create();
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 8U);
}

TEST(InspectorValue, MemoryCostObjectGrowing)
{
    Ref<InspectorObject> value = InspectorObject::create();

    value->setValue("1", InspectorValue::null());
    size_t memoryCost1 = value->memoryCost();

    value->setValue("2", InspectorValue::null());
    size_t memoryCost2 = value->memoryCost();

    value->setValue("3", InspectorValue::null());
    size_t memoryCost3 = value->memoryCost();

    value->setValue("4", InspectorValue::null());
    size_t memoryCost4 = value->memoryCost();

    EXPECT_LT(memoryCost1, memoryCost2);
    EXPECT_LT(memoryCost2, memoryCost3);
    EXPECT_LT(memoryCost3, memoryCost4);
}

TEST(InspectorValue, MemoryCostArray)
{
    Ref<InspectorArray> value = InspectorArray::create();
    value->pushValue(InspectorValue::null());
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 16U);
}

TEST(InspectorValue, MemoryCostArrayEmpty)
{
    Ref<InspectorArray> value = InspectorArray::create();
    size_t memoryCost = value->memoryCost();
    EXPECT_GT(memoryCost, 0U);
    EXPECT_LE(memoryCost, 8U);
}

TEST(InspectorValue, MemoryCostArrayGrowing)
{
    Ref<InspectorArray> value = InspectorArray::create();

    value->pushValue(InspectorValue::null());
    size_t memoryCost1 = value->memoryCost();

    value->pushValue(InspectorValue::null());
    size_t memoryCost2 = value->memoryCost();

    value->pushValue(InspectorValue::null());
    size_t memoryCost3 = value->memoryCost();

    value->pushValue(InspectorValue::null());
    size_t memoryCost4 = value->memoryCost();

    EXPECT_LT(memoryCost1, memoryCost2);
    EXPECT_LT(memoryCost2, memoryCost3);
    EXPECT_LT(memoryCost3, memoryCost4);
}

} // namespace TestWebKitAPI
