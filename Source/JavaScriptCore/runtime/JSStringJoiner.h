/*
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
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
#include "JSString.h"

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

    bool append(JSGlobalObject*, JSValue);
    void appendNumber(VM&, int32_t);
    void appendNumber(VM&, double);
    void appendEmptyString();

    JSString* join(JSGlobalObject*);

private:
    bool appendWithoutSideEffects(JSGlobalObject*, JSValue);
    void append(JSString*, StringViewWithUnderlyingString&&);
    void append8Bit(const String&);
    unsigned joinedLength(JSGlobalObject*) const;
    JSString* joinImpl(JSGlobalObject*);

    StringView m_separator;
    Entries m_strings;
    CheckedUint32 m_accumulatedStringsLength;
    CheckedUint32 m_stringsCount;
    bool m_hasOverflowed { false };
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
    if (!m_strings.tryReserveCapacity(count)) [[unlikely]]
        throwOutOfMemoryError(globalObject, scope);
}

inline JSString* JSStringJoiner::join(JSGlobalObject* globalObject)
{
    if (m_stringsCount == 1) {
        // If m_stringsCount is 1, then there's no chance of an overflow because m_strings
        // is a Vector<Entry, 16>, and has at least space for 16 entries.
        ASSERT(!m_hasOverflowed);
        if (m_lastString)
            return m_lastString;
        return jsString(globalObject->vm(), m_strings[0].m_view.toString());
    }
    return joinImpl(globalObject);
}

ALWAYS_INLINE void JSStringJoiner::append(JSString* jsString, StringViewWithUnderlyingString&& string)
{
    ++m_stringsCount;
    if (m_lastString == jsString) {
        auto& entry = m_strings.last();
        if (entry.m_additional < UINT16_MAX) [[likely]] {
            ++entry.m_additional;
            m_accumulatedStringsLength += entry.m_view.view.length();
            return;
        }
    }
    m_accumulatedStringsLength += string.view.length();
    m_isAll8Bit = m_isAll8Bit && string.view.is8Bit();
    m_hasOverflowed |= !m_strings.tryAppend({ WTFMove(string), 0 });
    m_lastString = jsString;
}

ALWAYS_INLINE void JSStringJoiner::append8Bit(const String& string)
{
    ASSERT(string.is8Bit());
    ++m_stringsCount;
    m_accumulatedStringsLength += string.length();
    m_hasOverflowed |= !m_strings.tryAppend({ { string, string }, 0 });
    m_lastString = nullptr;
}

ALWAYS_INLINE void JSStringJoiner::appendEmptyString()
{
    ++m_stringsCount;
    m_hasOverflowed |= !m_strings.tryAppend({ { { }, { } }, 0 });
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

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isCell()) {
        // FIXME: Support JSBigInt in side-effect-free append.
        // https://bugs.webkit.org/show_bug.cgi?id=211173
        if (JSString* jsString = jsDynamicCast<JSString*>(value)) {
            auto view = jsString->view(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
            // Since getting the view didn't OOM, we know that the underlying String exists and isn't
            // a rope. Thus, `tryGetValue` on the owner JSString will succeed. Since jsString could be
            // a substring we make sure to get the owner's String not jsString's.
            append(jsString, StringViewWithUnderlyingString(view, jsCast<const JSString*>(view.owner)->tryGetValue()));
            return true;
        }
        return false;
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

ALWAYS_INLINE bool JSStringJoiner::append(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool success = appendWithoutSideEffects(globalObject, value);
    RETURN_IF_EXCEPTION(scope, false);
    if (!success) {
        ASSERT(value.isCell());
        ASSERT(!value.isString());
        JSString* jsString = value.asCell()->toStringInline(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        auto view = jsString->view(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        scope.release();
        append(jsString, StringViewWithUnderlyingString(view, jsCast<const JSString*>(view.owner)->tryGetValue()));
        return false;
    }
    return true;
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

// Avoids the overhead of accumulating intermediate vectors of values when
// we're only joining existing strings.
class JSOnlyStringsAndInt32sJoiner {
public:
    JSOnlyStringsAndInt32sJoiner(StringView separator)
        : m_separator(separator)
        , m_isAll8Bit(m_separator.is8Bit())
    {
    }

    JSString* tryJoin(JSGlobalObject* globalObject, const WriteBarrier<Unknown>* data, unsigned length)
    {
        if (length == 1) {
            JSValue value = data[0].get();
            if (!value)
                return nullptr;
            if (value.isString())
                return asString(value);
            if (value.isInt32()) {
                m_accumulatedStringsLength = WTF::StringTypeAdapter<int32_t> { value.asInt32() }.length();
                return joinImpl(globalObject, data, length);
            }
            return nullptr;
        }

        for (size_t i = 0; i < length; ++i) {
            JSValue value = data[i].get();
            if (!value)
                return nullptr;

            if (value.isString()) {
                JSString* string = asString(value);
                m_accumulatedStringsLength += string->length();
                m_isAll8Bit &= string->is8Bit();
                continue;
            }

            if (value.isInt32()) {
                m_accumulatedStringsLength += WTF::StringTypeAdapter<int32_t> { value.asInt32() }.length();
                continue;
            }

            return nullptr;
        }

        return joinImpl(globalObject, data, length);
    }

private:
    JSString* joinImpl(JSGlobalObject*, const WriteBarrier<Unknown>*, unsigned);

    StringView m_separator;
    CheckedUint32 m_accumulatedStringsLength;
    bool m_isAll8Bit { true };
};

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

} // namespace JSC
