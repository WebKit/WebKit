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

#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/AccessibleAddress.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class JSCell;
class NativeCallee;

class CalleeBits {
public:
    constexpr CalleeBits() = default;
    CalleeBits(int64_t value)
        : m_ptr { reinterpret_cast<void*>(value) }
    { }
    CalleeBits(NativeCallee* nativeCallee) { *this = nativeCallee; }

    CalleeBits& operator=(JSCell* cell)
    {
        m_ptr = cell;
        ASSERT(isCell());
        return *this;
    }

    CalleeBits& operator=(NativeCallee* nativeCallee)
    {
        m_ptr = boxNativeCalleeIfExists(nativeCallee);
        ASSERT_IMPLIES(nativeCallee, isNativeCallee());
        return *this;
    }

    static constexpr CalleeBits nullCallee()
    {
        return CalleeBits();
    }

    static EncodedJSValue encodeJSCallee(const JSCell* cell)
    {
        if (!cell)
            return nullCallee().encodedBits();
        return reinterpret_cast<EncodedJSValue>(cell);
    }

    static EncodedJSValue encodeBoxedNativeCallee(void* boxedCallee)
    {
        return reinterpret_cast<EncodedJSValue>(boxedCallee);
    }

    EncodedJSValue encodedBits() const { return reinterpret_cast<EncodedJSValue>(m_ptr); }

    static EncodedJSValue encodeNativeCallee(NativeCallee* callee)
    {
        if (!callee)
            return nullCallee().encodedBits();
        return encodeBoxedNativeCallee(boxNativeCallee(callee));
    }

    static void* boxNativeCalleeIfExists(NativeCallee* callee)
    {
        if (callee)
            return boxNativeCallee(callee);
        return nullptr;
    }

    static void* boxNativeCallee(NativeCallee* callee)
    {
        auto bits = std::bit_cast<uintptr_t>(callee);
        CalleeBits result { static_cast<int64_t>((bits - lowestAccessibleAddress()) | JSValue::NativeCalleeTag) };
        ASSERT(result.isNativeCallee());
        return result.rawPtr();
    }

    bool isNativeCallee() const
    {
        return (reinterpret_cast<uintptr_t>(m_ptr) & JSValue::NativeCalleeMask) == JSValue::NativeCalleeTag;
    }
    bool isCell() const { return !isNativeCallee(); }

    JSCell* asCell() const
    {
        ASSERT(!isNativeCallee());
        return static_cast<JSCell*>(m_ptr);
    }

    NativeCallee* asNativeCallee() const
    {
        ASSERT(isNativeCallee());
        return std::bit_cast<NativeCallee*>(static_cast<uintptr_t>(std::bit_cast<uintptr_t>(m_ptr) & ~JSValue::NativeCalleeTag) + lowestAccessibleAddress());
    }

    void* rawPtr() const { return m_ptr; }
    // For Ref/RefPtr support.
    explicit operator bool() const { return m_ptr; }
    bool operator==(const CalleeBits&) const = default;

private:
    void* m_ptr { nullptr };
};

} // namespace JSC
