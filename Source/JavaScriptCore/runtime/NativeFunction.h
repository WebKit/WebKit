/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "JSCJSValue.h"
#include "JSCPtrTag.h"

namespace JSC {

class CallFrame;

typedef EncodedJSValue (JSC_HOST_CALL *RawNativeFunction)(JSGlobalObject*, CallFrame*);

class NativeFunction {
public:
    NativeFunction() = default;
    NativeFunction(std::nullptr_t) : m_ptr(nullptr) { }
    explicit NativeFunction(uintptr_t bits) : m_ptr(bitwise_cast<RawNativeFunction>(bits)) { }
    NativeFunction(RawNativeFunction other) : m_ptr(other) { }

    explicit operator intptr_t() const { return reinterpret_cast<intptr_t>(m_ptr); }
    explicit operator bool() const { return !!m_ptr; }
    bool operator!() const { return !m_ptr; }
    bool operator==(NativeFunction other) const { return m_ptr == other.m_ptr; }
    bool operator!=(NativeFunction other) const { return m_ptr == other.m_ptr; }

    EncodedJSValue operator()(JSGlobalObject* globalObject, CallFrame* callFrame) { return m_ptr(globalObject, callFrame); }

    void* rawPointer() const { return reinterpret_cast<void*>(m_ptr); }

private:
    RawNativeFunction m_ptr;

    friend class TaggedNativeFunction;
};

struct NativeFunctionHash {
    static unsigned hash(NativeFunction key) { return IntHash<uintptr_t>::hash(bitwise_cast<uintptr_t>(key)); }
    static bool equal(NativeFunction a, NativeFunction b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

class TaggedNativeFunction {
public:
    TaggedNativeFunction() = default;
    TaggedNativeFunction(std::nullptr_t) : m_ptr(nullptr) { }
    explicit TaggedNativeFunction(intptr_t bits) : m_ptr(bitwise_cast<void*>(bits)) { }

    TaggedNativeFunction(NativeFunction func)
        : m_ptr(tagCFunctionPtr<void*, JSEntryPtrTag>(func.m_ptr))
    { }
    TaggedNativeFunction(RawNativeFunction func)
        : m_ptr(tagCFunctionPtr<void*, JSEntryPtrTag>(func))
    { }

    explicit operator bool() const { return !!m_ptr; }
    bool operator!() const { return !m_ptr; }
    bool operator==(TaggedNativeFunction other) const { return m_ptr == other.m_ptr; }
    bool operator!=(TaggedNativeFunction other) const { return m_ptr != other.m_ptr; }

    EncodedJSValue operator()(JSGlobalObject* globalObject, CallFrame* callFrame) { return NativeFunction(*this)(globalObject, callFrame); }

    explicit operator NativeFunction()
    {
        ASSERT(m_ptr);
        return untagCFunctionPtr<NativeFunction, JSEntryPtrTag>(m_ptr);
    }

    void* rawPointer() const { return m_ptr; }

private:
    void* m_ptr;
};

struct TaggedNativeFunctionHash {
    static unsigned hash(TaggedNativeFunction key) { return IntHash<uintptr_t>::hash(bitwise_cast<uintptr_t>(key)); }
    static bool equal(TaggedNativeFunction a, TaggedNativeFunction b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

static_assert(sizeof(NativeFunction) == sizeof(void*), "");
static_assert(sizeof(TaggedNativeFunction) == sizeof(void*), "");

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::NativeFunction> {
    using Hash = JSC::NativeFunctionHash;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::TaggedNativeFunction> {
    using Hash = JSC::TaggedNativeFunctionHash;
};

} // namespace WTF

