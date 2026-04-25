/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/MarkedVector.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <JavaScriptCore/VM.h>

namespace TestWebKitAPI {

using JSC::HeapType;
using JSC::JSGlobalObject;
using JSC::JSLockHolder;
using JSC::JSString;
using JSC::JSValue;
using JSC::MarkedArgumentBuffer;
using JSC::MarkedVector;
using JSC::VM;
using JSC::jsNull;
using JSC::jsNumber;
using JSC::jsString;

// --- Test helper: creates distinct verifiable values of type T ---

static constexpr size_t testInlineCapacity = 10;
static constexpr size_t testMallocedCapacity = 20;

static_assert(testMallocedCapacity > testInlineCapacity);

template<typename T> struct MarkedVectorTestHelper;

template<>
struct MarkedVectorTestHelper<JSValue> {
    static JSValue make(VM&, JSGlobalObject*, int i) { return jsNumber(i); }
    static void verify(VM&, JSValue value, int i) { EXPECT_EQ(value, jsNumber(i)); }

    NEVER_INLINE static void populateForGC(VM& vm, JSGlobalObject*, MarkedVector<JSValue, testInlineCapacity>& vec, int count)
    {
        for (int i = 0; i < count; ++i)
            vec.append(jsString(vm, String(String::number(i))));
    }
    static void createNoise(VM& vm, JSGlobalObject*)
    {
        for (int i = 0; i < 50; ++i)
            jsString(vm, String(makeString("noise"_s, i)));
    }
    static void verifyAfterGC(VM&, MarkedVector<JSValue, testInlineCapacity>& vec, int count)
    {
        EXPECT_EQ(vec.size(), static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            EXPECT_TRUE(vec[i].isString());
            auto result = asString(vec[i])->tryGetValue();
            EXPECT_EQ(static_cast<const String&>(result), String::number(i));
        }
    }
};

template<>
struct MarkedVectorTestHelper<JSC::JSObject*> {
    static JSC::JSObject* make(VM& vm, JSGlobalObject* globalObject, int i)
    {
        auto* obj = constructEmptyObject(globalObject);
        obj->putDirect(vm, JSC::Identifier::fromString(vm, "idx"_s), jsNumber(i));
        return obj;
    }
    static void verify(VM& vm, JSC::JSObject* value, int i)
    {
        EXPECT_TRUE(value != nullptr);
        JSValue val = value->getDirect(vm, JSC::Identifier::fromString(vm, "idx"_s));
        EXPECT_EQ(val, jsNumber(i));
    }

    NEVER_INLINE static void populateForGC(VM& vm, JSGlobalObject* globalObject, MarkedVector<JSC::JSObject*, testInlineCapacity>& vec, int count)
    {
        for (int i = 0; i < count; ++i)
            vec.append(make(vm, globalObject, i));
    }
    static void createNoise(VM& vm, JSGlobalObject* globalObject)
    {
        for (int i = 0; i < 50; ++i)
            make(vm, globalObject, 1000 + i);
    }
    static void verifyAfterGC(VM& vm, MarkedVector<JSC::JSObject*, testInlineCapacity>& vec, int count)
    {
        EXPECT_EQ(vec.size(), static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            verify(vm, vec[i], i);
    }
};

template<>
struct MarkedVectorTestHelper<JSValueRef> {
    static JSValueRef make(VM& vm, JSGlobalObject*, int i) { return toRef(vm, jsNumber(i)); }
    static void verify(VM&, JSValueRef value, int i)
    {
        JSValue v = toJS(value);
        EXPECT_EQ(v, jsNumber(i));
    }

    NEVER_INLINE static void populateForGC(VM& vm, JSGlobalObject*, MarkedVector<JSValueRef, testInlineCapacity>& vec, int count)
    {
        for (int i = 0; i < count; ++i)
            vec.append(toRef(vm, static_cast<JSValue>(jsString(vm, String(String::number(i))))));
    }
    static void createNoise(VM& vm, JSGlobalObject*)
    {
        for (int i = 0; i < 50; ++i)
            jsString(vm, String(makeString("noise"_s, i)));
    }
    static void verifyAfterGC(VM&, MarkedVector<JSValueRef, testInlineCapacity>& vec, int count)
    {
        EXPECT_EQ(vec.size(), static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            EXPECT_TRUE(vec[i] != nullptr);
            JSValue v = toJS(vec[i]);
            EXPECT_TRUE(v.isString());
            auto result = asString(v)->tryGetValue();
            EXPECT_EQ(static_cast<const String&>(result), String::number(i));
        }
    }
};

template<>
struct MarkedVectorTestHelper<JSObjectRef> {
    static JSObjectRef make(VM& vm, JSGlobalObject* globalObject, int i)
    {
        return toRef(MarkedVectorTestHelper<JSC::JSObject*>::make(vm, globalObject, i));
    }
    static void verify(VM& vm, JSObjectRef value, int i)
    {
        EXPECT_TRUE(value != nullptr);
        MarkedVectorTestHelper<JSC::JSObject*>::verify(vm, toJS(value), i);
    }

    NEVER_INLINE static void populateForGC(VM& vm, JSGlobalObject* globalObject, MarkedVector<JSObjectRef, testInlineCapacity>& vec, int count)
    {
        for (int i = 0; i < count; ++i)
            vec.append(make(vm, globalObject, i));
    }
    static void createNoise(VM& vm, JSGlobalObject* globalObject)
    {
        MarkedVectorTestHelper<JSC::JSObject*>::createNoise(vm, globalObject);
    }
    static void verifyAfterGC(VM& vm, MarkedVector<JSObjectRef, testInlineCapacity>& vec, int count)
    {
        EXPECT_EQ(vec.size(), static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            verify(vm, vec[i], i);
    }
};

// --- Templatized test bodies ---

template<typename T>
void testDefaultConstructor(VM&, JSGlobalObject*)
{
    MarkedVector<T> vec;
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.isEmpty());
}

template<typename T>
void testInitialCapacityConstructor(VM&, JSGlobalObject*)
{
    MarkedVector<T> vec(32);
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.isEmpty());
}

template<typename T>
void testAppend(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 0);
    T v1 = MarkedVectorTestHelper<T>::make(vm, globalObject, 1);
    T v2 = MarkedVectorTestHelper<T>::make(vm, globalObject, 2);
    vec.append(v0);
    vec.append(v1);
    vec.append(v2);

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_FALSE(vec.isEmpty());
    EXPECT_EQ(vec.at(0), v0);
    EXPECT_EQ(vec[1], v1);
    EXPECT_EQ(vec[2], v2);
}

template<typename T>
void testAppendBeyondInlineCapacity(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    T values[testMallocedCapacity];

    for (size_t i = 0; i < testMallocedCapacity; ++i) {
        values[i] = MarkedVectorTestHelper<T>::make(vm, globalObject, i);
        vec.append(values[i]);
    }

    EXPECT_EQ(vec.size(), testMallocedCapacity);
    for (size_t i = 0; i < testMallocedCapacity; ++i)
        EXPECT_EQ(vec[i], values[i]);
}

template<typename T>
void testLast(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 0);
    T v1 = MarkedVectorTestHelper<T>::make(vm, globalObject, 1);
    T v2 = MarkedVectorTestHelper<T>::make(vm, globalObject, 2);
    vec.append(v0);
    vec.append(v1);
    vec.append(v2);

    EXPECT_EQ(vec.last(), v2);
}

template<typename T>
void testTakeLast(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 0);
    T v1 = MarkedVectorTestHelper<T>::make(vm, globalObject, 1);
    T v2 = MarkedVectorTestHelper<T>::make(vm, globalObject, 2);
    vec.append(v0);
    vec.append(v1);
    vec.append(v2);

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec.last(), v2);

    T taken = vec.takeLast();
    EXPECT_EQ(taken, v2);
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec.last(), v1);
}

template<typename T>
void testRemoveLast(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 0);
    T v1 = MarkedVectorTestHelper<T>::make(vm, globalObject, 1);
    vec.append(v0);
    vec.append(v1);

    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec.last(), v1);

    vec.removeLast();
    EXPECT_EQ(vec.size(), 1u);
    EXPECT_EQ(vec[0], v0);
    EXPECT_EQ(vec.last(), v0);
}

template<typename T>
void testBeginEnd(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    T values[3];
    for (int i = 0; i < 3; ++i) {
        values[i] = MarkedVectorTestHelper<T>::make(vm, globalObject, i);
        vec.append(values[i]);
    }

    int idx = 0;
    for (auto* it = vec.begin(); it != vec.end(); ++it) {
        EXPECT_EQ(*it, values[idx]);
        ++idx;
    }
    EXPECT_EQ(idx, 3);
}

template<typename T>
void testAutoIteration(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    T values[3];
    for (int i = 0; i < 3; ++i) {
        values[i] = MarkedVectorTestHelper<T>::make(vm, globalObject, i);
        vec.append(values[i]);
    }

    int idx = 0;
    for (auto it : vec) {
        EXPECT_EQ(it, values[idx]);
        ++idx;
    }
    EXPECT_EQ(idx, 3);
}

template<typename T>
void testSpan(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 0);
    T v1 = MarkedVectorTestHelper<T>::make(vm, globalObject, 1);
    T v2 = MarkedVectorTestHelper<T>::make(vm, globalObject, 2);
    T v3 = MarkedVectorTestHelper<T>::make(vm, globalObject, 3);
    vec.append(v0);
    vec.append(v1);
    vec.append(v2);

    auto s = vec.span();
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s[0], v0);
    EXPECT_EQ(s[1], v1);
    EXPECT_EQ(s[2], v2);

    auto ms = vec.mutableSpan();
    EXPECT_EQ(ms.size(), 3u);
    ms[1] = v3;
    EXPECT_EQ(vec[0], v0);
    EXPECT_EQ(vec[1], v3);
    EXPECT_EQ(vec[2], v2);

    auto s2 = vec.span();
    EXPECT_EQ(s2.size(), 3u);
    EXPECT_EQ(s2[0], v0);
    EXPECT_EQ(s2[1], v3);
    EXPECT_EQ(s2[2], v2);
}

template<typename T>
void testClear(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T> vec;
    for (int i = 0; i < 3; ++i)
        vec.append(MarkedVectorTestHelper<T>::make(vm, globalObject, i));

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_TRUE(!vec.isEmpty());

    vec.clear();
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.isEmpty());

    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 10);
    vec.append(v0);
    EXPECT_EQ(vec.size(), 1u);
    EXPECT_EQ(vec[0], v0);
}

template<typename T>
void testClearMallocBuffer(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, testInlineCapacity> vec;
    T values[testMallocedCapacity];
    for (size_t i = 0; i < testMallocedCapacity; ++i) {
        values[i] = MarkedVectorTestHelper<T>::make(vm, globalObject, i);
        vec.append(values[i]);
    }

    EXPECT_EQ(vec.size(), testMallocedCapacity);
    vec.clear();
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.isEmpty());

    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 10);
    vec.append(v0);
    EXPECT_EQ(vec.size(), 1u);
    EXPECT_EQ(vec[0], v0);
}

template<typename T>
void testMoveConstructor(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, testInlineCapacity> src;
    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 0);
    T v1 = MarkedVectorTestHelper<T>::make(vm, globalObject, 1);
    src.append(v0);
    src.append(v1);

    MarkedVector<T, testInlineCapacity> dst(WTF::move(src));
    EXPECT_EQ(dst.size(), 2u);
    EXPECT_EQ(dst[0], v0);
    EXPECT_EQ(dst[1], v1);

    EXPECT_EQ(src.size(), 0u);
    EXPECT_TRUE(src.isEmpty());
}

template<typename T>
void testMoveConstructorMallocBuffer(VM& vm, JSGlobalObject* globalObject)
{
    constexpr size_t localInlineCapacity = 8;
    MarkedVector<T, localInlineCapacity> src;
    T values[testMallocedCapacity];
    for (size_t i = 0; i < testMallocedCapacity; ++i) {
        values[i] = MarkedVectorTestHelper<T>::make(vm, globalObject, i);
        src.append(values[i]);
    }

    MarkedVector<T, localInlineCapacity> dst(WTF::move(src));
    EXPECT_EQ(dst.size(), testMallocedCapacity);
    for (size_t i = 0; i < testMallocedCapacity; ++i)
        EXPECT_EQ(dst[i], values[i]);

    EXPECT_EQ(src.size(), 0u);
    EXPECT_TRUE(src.isEmpty());
}

template<typename T>
void testMoveAssignment(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, testInlineCapacity> src;
    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 0);
    T v1 = MarkedVectorTestHelper<T>::make(vm, globalObject, 1);
    src.append(v0);
    src.append(v1);
    EXPECT_EQ(src.size(), 2u);

    MarkedVector<T, testInlineCapacity> dst;
    dst.append(MarkedVectorTestHelper<T>::make(vm, globalObject, 99));
    EXPECT_EQ(dst.size(), 1u);

    dst = WTF::move(src);

    EXPECT_EQ(dst.size(), 2u);
    EXPECT_EQ(dst[0], v0);
    EXPECT_EQ(dst[1], v1);
    EXPECT_EQ(src.size(), 0u);
}

template<typename T>
void testMoveAssignmentMallocBuffer(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, testInlineCapacity> src;
    T values[testMallocedCapacity];
    for (size_t i = 0; i < testMallocedCapacity; ++i) {
        values[i] = MarkedVectorTestHelper<T>::make(vm, globalObject, i);
        src.append(values[i]);
    }
    EXPECT_EQ(src.size(), testMallocedCapacity);

    MarkedVector<T, testInlineCapacity> dst;
    dst.append(MarkedVectorTestHelper<T>::make(vm, globalObject, 99));
    EXPECT_EQ(dst.size(), 1u);

    dst = WTF::move(src);

    EXPECT_EQ(dst.size(), testMallocedCapacity);
    for (size_t i = 0; i < testMallocedCapacity; ++i)
        EXPECT_EQ(dst[i], values[i]);
    EXPECT_EQ(src.size(), 0u);
}

template<typename T>
void testGCLivenessInlineBuffer(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, testInlineCapacity> vec;

    MarkedVectorTestHelper<T>::populateForGC(vm, globalObject, vec, 5);
    sanitizeStackForVM(vm);
    vm.heap.collectSync();
    MarkedVectorTestHelper<T>::createNoise(vm, globalObject);
    MarkedVectorTestHelper<T>::verifyAfterGC(vm, vec, 5);
}

template<typename T>
void testGCLivenessMallocBuffer(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, testInlineCapacity> vec;

    MarkedVectorTestHelper<T>::populateForGC(vm, globalObject, vec, testMallocedCapacity);
    sanitizeStackForVM(vm);
    vm.heap.collectSync();
    MarkedVectorTestHelper<T>::createNoise(vm, globalObject);
    MarkedVectorTestHelper<T>::verifyAfterGC(vm, vec, testMallocedCapacity);
}

template<typename T>
void testFillWith(VM& vm, JSGlobalObject* globalObject)
{
    Vector<int> source { 0, 1, 2, 3, 4 };

    MarkedVector<T> vec;
    vec.fillWith(vm, source, [&](int i) -> T {
        return MarkedVectorTestHelper<T>::make(vm, globalObject, i);
    });

    EXPECT_EQ(vec.size(), 5u);
    for (int i = 0; i < 5; ++i)
        MarkedVectorTestHelper<T>::verify(vm, vec[i], i);
}

template<typename T>
void testFillWithMallocBuffer(VM& vm, JSGlobalObject* globalObject)
{
    Vector<int> source;
    for (size_t i = 0; i < testMallocedCapacity; ++i)
        source.append(i);

    MarkedVector<T, testInlineCapacity> vec;
    vec.fillWith(vm, source, [&](int i) -> T {
        return MarkedVectorTestHelper<T>::make(vm, globalObject, i);
    });

    EXPECT_EQ(vec.size(), testMallocedCapacity);
    for (size_t i = 0; i < testMallocedCapacity; ++i)
        MarkedVectorTestHelper<T>::verify(vm, vec[i], i);
}

template<typename T>
void testFill(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, testInlineCapacity> vec;
    constexpr int count = 5;
    T values[count];
    for (int i = 0; i < count; ++i)
        values[i] = MarkedVectorTestHelper<T>::make(vm, globalObject, i);

    EXPECT_EQ(vec.size(), 0u);
    EXPECT_TRUE(vec.isEmpty());

    vec.fill(vm, count, [&](T* buffer) {
        for (int i = 0; i < count; ++i)
            buffer[i] = values[i];
    });

    EXPECT_EQ(vec.size(), static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        EXPECT_EQ(vec[i], values[i]);
}

template<typename T>
void testFillMallocBuffer(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, testInlineCapacity> vec;
    constexpr int count = testMallocedCapacity;
    T values[count];
    for (int i = 0; i < count; ++i)
        values[i] = MarkedVectorTestHelper<T>::make(vm, globalObject, i);

    vec.fill(vm, count, [&](T* buffer) {
        for (int i = 0; i < count; ++i)
            buffer[i] = values[i];
    });

    EXPECT_EQ(vec.size(), static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        EXPECT_EQ(vec[i], values[i]);
}

template<typename T>
void testAppendWithCrashOnOverflow(VM& vm, JSGlobalObject* globalObject)
{
    MarkedVector<T, 8, RecordOverflow> vec;
    T v0 = MarkedVectorTestHelper<T>::make(vm, globalObject, 0);
    T v1 = MarkedVectorTestHelper<T>::make(vm, globalObject, 1);
    T v2 = MarkedVectorTestHelper<T>::make(vm, globalObject, 2);
    vec.appendWithCrashOnOverflow(v0);
    vec.appendWithCrashOnOverflow(v1);
    vec.appendWithCrashOnOverflow(v2);
    ASSERT_FALSE(vec.hasOverflowed());

    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], v0);
    EXPECT_EQ(vec[1], v1);
    EXPECT_EQ(vec[2], v2);
}

// --- Macro to stamp out TEST() for all four types ---

#define MARKEDVECTOR_TEST(TestName, TypeSuffix, Type)                       \
    TEST(JavaScriptCore_MarkedVector, TestName##_##TypeSuffix)              \
    {                                                                       \
        WTF::initializeMainThread();                                        \
        JSC::initialize();                                                  \
        RefPtr<VM> vm = VM::create(HeapType::Large);                        \
        {                                                                   \
            JSLockHolder locker(*vm);                                       \
            auto* globalObject = JSGlobalObject::create(                    \
                *vm, JSGlobalObject::createStructure(*vm, jsNull()));       \
            test##TestName<Type>(*vm, globalObject);                        \
            vm = nullptr;                                                   \
        }                                                                   \
    }

#define MARKEDVECTOR_TYPED_TESTS(TestName)                                  \
    MARKEDVECTOR_TEST(TestName, JSValue, JSValue)                           \
    MARKEDVECTOR_TEST(TestName, JSValueRef, JSValueRef)                     \
    MARKEDVECTOR_TEST(TestName, JSObjectRef, JSObjectRef)                   \
    MARKEDVECTOR_TEST(TestName, JSObjectPtr, JSC::JSObject*)

MARKEDVECTOR_TYPED_TESTS(DefaultConstructor)
MARKEDVECTOR_TYPED_TESTS(InitialCapacityConstructor)
MARKEDVECTOR_TYPED_TESTS(Append)
MARKEDVECTOR_TYPED_TESTS(AppendBeyondInlineCapacity)
MARKEDVECTOR_TYPED_TESTS(Last)
MARKEDVECTOR_TYPED_TESTS(TakeLast)
MARKEDVECTOR_TYPED_TESTS(RemoveLast)
MARKEDVECTOR_TYPED_TESTS(BeginEnd)
MARKEDVECTOR_TYPED_TESTS(AutoIteration)
MARKEDVECTOR_TYPED_TESTS(Span)
MARKEDVECTOR_TYPED_TESTS(Clear)
MARKEDVECTOR_TYPED_TESTS(ClearMallocBuffer)
MARKEDVECTOR_TYPED_TESTS(MoveConstructor)
MARKEDVECTOR_TYPED_TESTS(MoveConstructorMallocBuffer)
MARKEDVECTOR_TYPED_TESTS(MoveAssignment)
MARKEDVECTOR_TYPED_TESTS(MoveAssignmentMallocBuffer)

MARKEDVECTOR_TYPED_TESTS(GCLivenessInlineBuffer)
MARKEDVECTOR_TYPED_TESTS(GCLivenessMallocBuffer)
MARKEDVECTOR_TYPED_TESTS(FillWith)
MARKEDVECTOR_TYPED_TESTS(FillWithMallocBuffer)
MARKEDVECTOR_TYPED_TESTS(Fill)
MARKEDVECTOR_TYPED_TESTS(FillMallocBuffer)
MARKEDVECTOR_TYPED_TESTS(AppendWithCrashOnOverflow)

// --- MarkedArgumentBuffer-specific tests ---

TEST(JavaScriptCore_MarkedVector, ArgumentBufferAppendAndAccess)
{
    WTF::initializeMainThread();
    JSC::initialize();

    RefPtr<VM> vm = VM::create(HeapType::Large);
    {
        JSLockHolder locker(*vm);
        MarkedArgumentBuffer args;
        args.append(jsNumber(1));
        args.append(jsNumber(2));
        args.append(jsNumber(3));
        ASSERT_FALSE(args.hasOverflowed());

        EXPECT_EQ(args.size(), 3u);
        EXPECT_EQ(args[0], jsNumber(1));
        EXPECT_EQ(args[1], jsNumber(2));
        EXPECT_EQ(args[2], jsNumber(3));
        vm = nullptr;
    }
}

TEST(JavaScriptCore_MarkedVector, ArgumentBufferOOBReadRecordsOverflow)
{
    WTF::initializeMainThread();
    JSC::initialize();

    RefPtr<VM> vm = VM::create(HeapType::Large);
    {
        JSLockHolder locker(*vm);
        MarkedArgumentBuffer args;
        args.append(jsNumber(42));
        ASSERT_FALSE(args.hasOverflowed());

        JSValue overflowedValue = args.at(100);
        EXPECT_EQ(overflowedValue, JSC::jsUndefined());
        EXPECT_TRUE(args.hasOverflowed());
        vm = nullptr;
    }
}

TEST(JavaScriptCore_MarkedVector, ArgumentBufferOOBWriteRecordsOverflow)
{
    WTF::initializeMainThread();
    JSC::initialize();

    RefPtr<VM> vm = VM::create(HeapType::Large);
    {
        JSLockHolder locker(*vm);
        MarkedArgumentBuffer args;
        args.append(jsNumber(42));
        ASSERT_FALSE(args.hasOverflowed());

        args.at(100) = jsNumber(100);
        EXPECT_TRUE(args.hasOverflowed());

        JSValue overflowedValue = args.at(100);
        EXPECT_EQ(overflowedValue, JSC::jsUndefined());
        EXPECT_TRUE(args.hasOverflowed());
        vm = nullptr;
    }
}

TEST(JavaScriptCore_MarkedVector, ArgumentBufferData)
{
    WTF::initializeMainThread();
    JSC::initialize();

    RefPtr<VM> vm = VM::create(HeapType::Large);
    {
        JSLockHolder locker(*vm);
        MarkedArgumentBuffer args;
        args.append(jsNumber(7));
        args.append(jsNumber(8));
        ASSERT_FALSE(args.hasOverflowed());

        const JSC::EncodedJSValue* raw = args.data();
        EXPECT_EQ(JSValue::decode(raw[0]), jsNumber(7));
        EXPECT_EQ(JSValue::decode(raw[1]), jsNumber(8));
        ASSERT_FALSE(args.hasOverflowed());
        vm = nullptr;
    }
}

} // namespace TestWebKitAPI
