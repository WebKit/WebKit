/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <thread>
#include <wtf/Atomics.h>

template <typename T>
NEVER_INLINE auto testConsume(const T* location)
{
    WTF::compilerFence(); // Paranoid testing.
    auto ret = WTF::consumeLoad(location);
    WTF::compilerFence(); // Paranoid testing.
    return ret;
}

namespace TestWebKitAPI {

TEST(WTF, Consumei8)
{
    uint8_t i8 = 42;
    auto i8_consumed = testConsume(&i8);
    ASSERT_EQ(i8_consumed.value, 42u);
    ASSERT_EQ(i8_consumed.dependency, 0u);
}

TEST(WTF, Consumei16)
{
    uint16_t i16 = 42;
    auto i16_consumed = testConsume(&i16);
    ASSERT_EQ(i16_consumed.value, 42u);
    ASSERT_EQ(i16_consumed.dependency, 0u);
}

TEST(WTF, Consumei32)
{
    uint32_t i32 = 42;
    auto i32_consumed = testConsume(&i32);
    ASSERT_EQ(i32_consumed.value, 42u);
    ASSERT_EQ(i32_consumed.dependency, 0u);
}

TEST(WTF, Consumei64)
{
    uint64_t i64 = 42;
    auto i64_consumed = testConsume(&i64);
    ASSERT_EQ(i64_consumed.value, 42u);
    ASSERT_EQ(i64_consumed.dependency, 0u);
}

TEST(WTF, Consumef32)
{
    float f32 = 42.f;
    auto f32_consumed = testConsume(&f32);
    ASSERT_EQ(f32_consumed.value, 42.f);
    ASSERT_EQ(f32_consumed.dependency, 0u);
}

TEST(WTF, Consumef64)
{
    double f64 = 42.;
    auto f64_consumed = testConsume(&f64);
    ASSERT_EQ(f64_consumed.value, 42.);
    ASSERT_EQ(f64_consumed.dependency, 0u);
}

static int* global;

TEST(WTF, ConsumeGlobalPtr)
{
    auto* ptr = &global;
    auto ptr_consumed = testConsume(&ptr);
    ASSERT_EQ(ptr_consumed.value, &global);
    ASSERT_EQ(ptr_consumed.dependency, 0u);
}

static int* globalArray[128];

TEST(WTF, ConsumeGlobalArrayPtr)
{
    auto* ptr = &globalArray[64];
    auto ptr_consumed = testConsume(&ptr);
    ASSERT_EQ(ptr_consumed.value, &globalArray[64]);
    ASSERT_EQ(ptr_consumed.dependency, 0u);
}

TEST(WTF, ConsumeStackPtr)
{
    char* hello = nullptr;
    auto* stack = &hello;
    auto stack_consumed = testConsume(&stack);
    ASSERT_EQ(stack_consumed.value, &hello);
    ASSERT_EQ(stack_consumed.dependency, 0u);
}

TEST(WTF, ConsumeWithThread)
{
    bool ready = false;
    constexpr size_t num = 1024;
    uint32_t* vec = new uint32_t[num];
    std::thread t([&]() {
        for (size_t i = 0; i != num; ++i)
            vec[i] = i * 2;
        WTF::storeStoreFence();
        ready = true;
    });
    do {
        auto stack_consumed = testConsume(&ready);
        if (stack_consumed.value) {
            for (size_t i = 0; i != num; ++i)
                ASSERT_EQ(vec[i + stack_consumed.dependency], i * 2);
            break;
        }
    } while (true);
    t.join();
    delete[] vec;
}
}
