/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSScope.h>
#include <JavaScriptCore/Register.h>

namespace JSC {

class CodeBlock;

ALWAYS_INLINE CallFrame* Register::callFrame() const
{
    return u.callFrame;
}

ALWAYS_INLINE CodeBlock* Register::codeBlock() const
{
    return u.codeBlock;
}

SUPPRESS_ASAN ALWAYS_INLINE CodeBlock* Register::asanUnsafeCodeBlock() const
{
    return u.codeBlock;
}

ALWAYS_INLINE JSObject* Register::object() const
{
    return asObject(jsValue());
}

ALWAYS_INLINE Register& Register::operator=(CallFrame* callFrame)
{
    u.callFrame = callFrame;
    return *this;
}

ALWAYS_INLINE Register& Register::operator=(CodeBlock* codeBlock)
{
    u.codeBlock = codeBlock;
    return *this;
}

ALWAYS_INLINE Register& Register::operator=(JSCell* object)
{
    u.value = JSValue::encode(JSValue(object));
    return *this;
}

ALWAYS_INLINE Register& Register::operator=(JSScope* scope)
{
    *this = JSValue(scope);
    return *this;
}

ALWAYS_INLINE Register& Register::operator=(EncodedJSValue encodedJSValue)
{
    u.value = encodedJSValue;
    return *this;
}

ALWAYS_INLINE JSScope* Register::scope() const
{
    return uncheckedDowncast<JSScope>(unboxedCell());
}

ALWAYS_INLINE Register::Register()
{
#ifndef NDEBUG
    *this = JSValue();
#endif
}

ALWAYS_INLINE Register::Register(const JSValue& v)
{
    u.value = JSValue::encode(v);
}

SUPPRESS_ASAN ALWAYS_INLINE JSValue Register::asanUnsafeJSValue() const
{
    return JSValue::decode(u.value);
}

ALWAYS_INLINE JSValue Register::jsValue() const
{
    return JSValue::decode(u.value);
}

ALWAYS_INLINE EncodedJSValue Register::encodedJSValue() const
{
    return u.value;
}

ALWAYS_INLINE int32_t Register::i() const
{
    return jsValue().asInt32();
}

ALWAYS_INLINE int64_t Register::unboxedInt52() const
{
    return u.integer >> JSValue::int52ShiftAmount;
}

SUPPRESS_ASAN ALWAYS_INLINE int64_t Register::asanUnsafeUnboxedInt52() const
{
    return u.integer >> JSValue::int52ShiftAmount;
}

inline Register Register::withInt(int32_t i)
{
    Register r = jsNumber(i);
    return r;
}

} // namespace JSC
