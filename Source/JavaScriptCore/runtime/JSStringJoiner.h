/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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

#include "ExceptionHelpers.h"
#include "JSCJSValue.h"
#include "JSGlobalObject.h"

namespace JSC {

class JSStringJoiner {
    WTF_FORBID_HEAP_ALLOCATION;
public:

    struct Entry {
        NO_UNIQUE_ADDRESS StringViewWithUnderlyingString m_view;
        NO_UNIQUE_ADDRESS uint16_t m_additional { 0 };
    };
    using Entries = Vector<Entry, 16>;

    JSStringJoiner(StringView separator);
    ~JSStringJoiner();

    void reserveCapacity(JSGlobalObject*, size_t);

    void append(JSGlobalObject*, JSValue);
    void appendNumber(VM&, int32_t);
    void appendNumber(VM&, double);
    bool appendWithoutSideEffects(JSGlobalObject*, JSValue);
    void appendEmptyString();

    JSValue join(JSGlobalObject*);

private:
    void append(JSString*, StringViewWithUnderlyingString&&);
    void append8Bit(const String&);
    unsigned joinedLength(JSGlobalObject*) const;
    JSValue joinSlow(JSGlobalObject*);

    StringView m_separator;
    Entries m_strings;
    CheckedUint32 m_accumulatedStringsLength;
    CheckedUint32 m_stringsCount;
    bool m_isAll8Bit { true };
    JSString* m_lastString { nullptr };
};

inline JSStringJoiner::JSStringJoiner(StringView separator)
    : m_separator(separator)
    , m_isAll8Bit(m_separator.is8Bit())
{
}

inline void JSStringJoiner::reserveCapacity(JSGlobalObject* globalObject, size_t count)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!m_strings.tryReserveCapacity(count)))
        throwOutOfMemoryError(globalObject, scope);
}

inline JSValue JSStringJoiner::join(JSGlobalObject* globalObject)
{
    if (m_stringsCount == 1) {
        if (m_lastString)
            return m_lastString;
        return jsString(globalObject->vm(), m_strings[0].m_view.toString());
    }
    return joinSlow(globalObject);
}

ALWAYS_INLINE void JSStringJoiner::append(JSString* jsString, StringViewWithUnderlyingString&& string)
{
    ++m_stringsCount;
    if (m_lastString == jsString) {
        auto& entry = m_strings.last();
        if (LIKELY(entry.m_additional < UINT16_MAX)) {
            ++entry.m_additional;
            m_accumulatedStringsLength += entry.m_view.view.length();
            return;
        }
    }
    m_accumulatedStringsLength += string.view.length();
    m_isAll8Bit = m_isAll8Bit && string.view.is8Bit();
    m_strings.append({ WTFMove(string), 0 });
    m_lastString = jsString;
}

ALWAYS_INLINE void JSStringJoiner::append8Bit(const String& string)
{
    ASSERT(string.is8Bit());
    ++m_stringsCount;
    m_accumulatedStringsLength += string.length();
    m_strings.append({ { string, string }, 0 });
    m_lastString = nullptr;
}

ALWAYS_INLINE void JSStringJoiner::appendEmptyString()
{
    ++m_stringsCount;
    m_strings.append({ { { }, { } }, 0 });
    m_lastString = nullptr;
}

ALWAYS_INLINE bool JSStringJoiner::appendWithoutSideEffects(JSGlobalObject* globalObject, JSValue value)
{
    // The following code differs from using the result of JSValue::toString in the following ways:
    // 1) It's inlined more than JSValue::toString is.
    // 2) It includes conversion to WTF::String in a way that avoids allocating copies of substrings.
    // 3) It doesn't create a JSString for numbers, true, or false.
    // 4) It turns undefined and null into the empty string instead of "undefined" and "null".
    // 5) It uses optimized code paths for all the cases known to be 8-bit and for the empty string.
    // If we might make an effectful calls, return false. Otherwise return true.

    if (value.isCell()) {
        JSString* jsString;
        // FIXME: Support JSBigInt in side-effect-free append.
        // https://bugs.webkit.org/show_bug.cgi?id=211173
        if (!value.asCell()->isString())
            return false;
        jsString = asString(value);
        append(jsString, jsString->viewWithUnderlyingString(globalObject));
        return true;
    }

    if (value.isInt32()) {
        appendNumber(globalObject->vm(), value.asInt32());
        return true;
    }
    if (value.isDouble()) {
        appendNumber(globalObject->vm(), value.asDouble());
        return true;
    }
    if (value.isTrue()) {
        append8Bit(globalObject->vm().propertyNames->trueKeyword.string());
        return true;
    }
    if (value.isFalse()) {
        append8Bit(globalObject->vm().propertyNames->falseKeyword.string());
        return true;
    }

#if USE(BIGINT32)
    if (value.isBigInt32()) {
        appendNumber(globalObject->vm(), value.bigInt32AsInt32());
        return true;
    }
#endif

    ASSERT(value.isUndefinedOrNull());
    appendEmptyString();
    return true;
}

ALWAYS_INLINE void JSStringJoiner::append(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool success = appendWithoutSideEffects(globalObject, value);
    RETURN_IF_EXCEPTION(scope, void());
    if (!success) {
        ASSERT(value.isCell());
        JSString* jsString = value.asCell()->toStringInline(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        RELEASE_AND_RETURN(scope, append(jsString, jsString->viewWithUnderlyingString(globalObject)));
    }
}

ALWAYS_INLINE void JSStringJoiner::appendNumber(VM& vm, int32_t value)
{
    append8Bit(vm.numericStrings.add(value));
}

ALWAYS_INLINE void JSStringJoiner::appendNumber(VM& vm, double value)
{
    if (canBeStrictInt32(value))
        appendNumber(vm, static_cast<int32_t>(value));
    else
        append8Bit(vm.numericStrings.add(value));
}

} // namespace JSC
