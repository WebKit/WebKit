/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

// This header serves two purposes:
// 1. It allows clients to access these inlined methods without causing
//    circular references in headers caused by JSCJSValueInlines.h
// 2. It enables build speed reductions when clients which only need to
//    definitions for a subset of JSValue's inlined methods include this
//    file rather than all of JSCJSValueInlines.h

#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCell.h>

namespace JSC {

inline bool JSValue::isString() const
{
    return isCell() && asCell()->isString();
}

inline bool JSValue::isBigInt() const
{
    return isBigInt32() || isHeapBigInt();
}

inline bool JSValue::isHeapBigInt() const
{
    return isCell() && asCell()->isHeapBigInt();
}

inline bool JSValue::isSymbol() const
{
    return isCell() && asCell()->isSymbol();
}

inline bool JSValue::isPrimitive() const
{
    return !isCell() || asCell()->isString() || asCell()->isSymbol() || asCell()->isHeapBigInt();
}

inline bool JSValue::isGetterSetter() const
{
    return isCell() && asCell()->isGetterSetter();
}

inline bool JSValue::isCustomGetterSetter() const
{
    return isCell() && asCell()->isCustomGetterSetter();
}

inline bool JSValue::isObject() const
{
    return isCell() && asCell()->isObject();
}

inline bool JSValue::getString(JSGlobalObject* globalObject, String& s) const
{
    return isCell() && asCell()->getString(globalObject, s);
}

inline String JSValue::getString(JSGlobalObject* globalObject) const
{
    return isCell() ? asCell()->getString(globalObject) : String();
}

inline JSObject* JSValue::getObject() const
{
    return isCell() ? asCell()->getObject() : nullptr;
}

inline JSValue JSValue::toPrimitive(JSGlobalObject* globalObject, PreferredPrimitiveType preferredType) const
{
    return isCell() ? asCell()->toPrimitive(globalObject, preferredType) : asValue();
}

inline bool JSValue::toBoolean(JSGlobalObject* globalObject) const
{
    if (isInt32())
        return asInt32();
    if (isDouble())
        return asDouble() > 0.0 || asDouble() < 0.0; // false for NaN
    if (isCell())
        return asCell()->toBoolean(globalObject);
#if USE(BIGINT32)
    if (isBigInt32())
        return !!bigInt32AsInt32();
#endif
    return isTrue(); // false, null, and undefined all convert to false.
}

inline JSObject* JSValue::toObject(JSGlobalObject* globalObject) const
{
    return isCell() ? asCell()->toObject(globalObject) : toObjectSlowCase(globalObject);
}

inline bool JSValue::isCallable() const
{
    return isCell() && asCell()->isCallable();
}

template<Concurrency concurrency>
inline TriState JSValue::isCallableWithConcurrency() const
{
    if (!isCell())
        return TriState::False;
    return asCell()->isCallableWithConcurrency<concurrency>();
}

inline bool JSValue::isConstructor() const
{
    return isCell() && asCell()->isConstructor();
}

template<Concurrency concurrency>
inline TriState JSValue::isConstructorWithConcurrency() const
{
    if (!isCell())
        return TriState::False;
    return asCell()->isConstructorWithConcurrency<concurrency>();
}

// JSValue::inherits, classInfoOrNull, and structureOrNull need
// JSCell::inherits/classInfo which are defined in Structure.h.
// They remain in JSCJSValueStructure.h for now.

} // namespace JSC
