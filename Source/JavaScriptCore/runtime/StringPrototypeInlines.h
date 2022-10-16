/*
 * Copyright (C) 2019 Apple, Inc. All rights reserved.
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

#include "CachedCall.h"
#include "ExceptionHelpers.h"
#include "StringPrototype.h"
#include <wtf/Range.h>
#include <wtf/text/StringSearch.h>

namespace JSC {

ALWAYS_INLINE std::tuple<int32_t, int32_t> extractSliceOffsets(int32_t length, int32_t startValue, std::optional<int32_t> endValue)
{
    int32_t from;
    if (startValue < 0)
        from = std::max<int32_t>(length + startValue, 0);
    else
        from = std::min<int32_t>(startValue, length);

    int32_t end = endValue.value_or(length);
    int32_t to;
    if (end < 0)
        to = std::max<int32_t>(length + end, 0);
    else
        to = std::min<int32_t>(end, length);

    if (from >= to)
        return { 0, 0 };
    return { from, to };
}

template<typename NumberType>
ALWAYS_INLINE JSString* stringSlice(JSGlobalObject* globalObject, VM& vm, JSString* string, int32_t length, NumberType start, std::optional<NumberType> endValue)
{
    if constexpr (std::is_same_v<NumberType, int32_t>) {
        auto [from, to] = extractSliceOffsets(length, start, endValue);
        return jsSubstring(vm, globalObject, string, from, to - from);
    } else {
        NumberType from = start < 0 ? length + start : start;
        NumberType end = endValue.value_or(length);
        NumberType to = end < 0 ? length + end : end;
        if (to > from && to > 0 && from < length) {
            if (from < 0)
                from = 0;
            if (to > length)
                to = length;
            return jsSubstring(vm, globalObject, string, static_cast<unsigned>(from), static_cast<unsigned>(to) - static_cast<unsigned>(from));
        }
        return jsEmptyString(vm);
    }
}

ALWAYS_INLINE std::tuple<int32_t, int32_t> extractSubstringOffsets(int32_t length, int32_t startValue, std::optional<int32_t> endValue)
{
    int32_t start = std::min<int32_t>(std::max<int32_t>(startValue, 0), length);
    int32_t end = length;
    if (endValue)
        end = std::min<int32_t>(std::max<int32_t>(endValue.value(), 0), length);

    ASSERT(start >= 0);
    ASSERT(end >= 0);
    if (start > end)
        std::swap(start, end);
    return { start, end };
}

ALWAYS_INLINE JSString* stringSubstring(JSGlobalObject* globalObject, JSString* string, int32_t startValue, std::optional<int32_t> endValue)
{
    int length = string->length();
    RELEASE_ASSERT(length >= 0);

    auto [start, end] = extractSubstringOffsets(length, startValue, endValue);

    return jsSubstring(globalObject, string, start, end - start);
}

ALWAYS_INLINE JSString* jsSpliceSubstringsWithSeparators(JSGlobalObject* globalObject, JSString* sourceVal, const String& source, const Range<int32_t>* substringRanges, int rangeCount, const String* separators, int separatorCount)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (rangeCount == 1 && separatorCount == 0) {
        int sourceSize = source.length();
        int position = substringRanges[0].begin();
        int length = substringRanges[0].distance();
        if (position <= 0 && length >= sourceSize)
            return sourceVal;
        // We could call String::substringSharingImpl(), but this would result in redundant checks.
        RELEASE_AND_RETURN(scope, jsString(vm, StringImpl::createSubstringSharingImpl(*source.impl(), std::max(0, position), std::min(sourceSize, length))));
    }

    if (rangeCount == 2 && separatorCount == 1) {
        String leftPart(StringImpl::createSubstringSharingImpl(*source.impl(), substringRanges[0].begin(), substringRanges[0].distance()));
        String rightPart(StringImpl::createSubstringSharingImpl(*source.impl(), substringRanges[1].begin(), substringRanges[1].distance()));
        RELEASE_AND_RETURN(scope, jsString(globalObject, leftPart, separators[0], rightPart));
    }

    CheckedInt32 totalLength = 0;
    bool allSeparators8Bit = true;
    for (int i = 0; i < rangeCount; i++)
        totalLength += substringRanges[i].distance();
    for (int i = 0; i < separatorCount; i++) {
        totalLength += separators[i].length();
        if (separators[i].length() && !separators[i].is8Bit())
            allSeparators8Bit = false;
    }
    if (totalLength.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    if (!totalLength)
        return jsEmptyString(vm);

    if (source.is8Bit() && allSeparators8Bit) {
        LChar* buffer;
        auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
        if (!impl) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        int maxCount = std::max(rangeCount, separatorCount);
        Checked<int, AssertNoOverflow> bufferPos = 0;
        for (int i = 0; i < maxCount; i++) {
            if (i < rangeCount) {
                auto substring = StringView { source }.substring(substringRanges[i].begin(), substringRanges[i].distance());
                substring.getCharacters8(buffer + bufferPos.value());
                bufferPos += substring.length();
            }
            if (i < separatorCount) {
                StringView separator = separators[i];
                separator.getCharacters8(buffer + bufferPos.value());
                bufferPos += separator.length();
            }
        }

        RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
    }

    UChar* buffer;
    auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
    if (!impl) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    int maxCount = std::max(rangeCount, separatorCount);
    Checked<int, AssertNoOverflow> bufferPos = 0;
    for (int i = 0; i < maxCount; i++) {
        if (i < rangeCount) {
            auto substring = StringView { source }.substring(substringRanges[i].begin(), substringRanges[i].distance());
            substring.getCharacters(buffer + bufferPos.value());
            bufferPos += substring.length();
        }
        if (i < separatorCount) {
            StringView separator = separators[i];
            separator.getCharacters(buffer + bufferPos.value());
            bufferPos += separator.length();
        }
    }

    RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
}

enum class StringReplaceSubstitutions : bool { Yes, No };
enum class StringReplaceUseTable : bool { Yes, No };
template<StringReplaceSubstitutions substitutions, StringReplaceUseTable useTable, typename TableType>
ALWAYS_INLINE JSString* stringReplaceStringString(JSGlobalObject* globalObject, JSString* stringCell, String string, String search, String replacement, const TableType* table)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t matchStart;
    if constexpr (useTable == StringReplaceUseTable::Yes)
        matchStart = table->find(string, search);
    else {
        UNUSED_PARAM(table);
        matchStart = string.find(search);
    }
    if (matchStart == notFound)
        return stringCell;

    size_t searchLength = search.length();
    size_t matchEnd = matchStart + searchLength;
    if constexpr (substitutions == StringReplaceSubstitutions::Yes) {
        size_t dollarSignPosition = replacement.find('$');
        if (dollarSignPosition != WTF::notFound) {
            StringBuilder builder(StringBuilder::OverflowHandler::RecordOverflow);
            int ovector[2] = { static_cast<int>(matchStart),  static_cast<int>(matchEnd) };
            substituteBackreferencesSlow(builder, replacement, string, ovector, nullptr, dollarSignPosition);
            if (UNLIKELY(builder.hasOverflowed())) {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }
            replacement = builder.toString();
        }
    }

    auto result = tryMakeString(StringView(string).substring(0, matchStart), replacement, StringView(string).substring(matchEnd, string.length() - matchEnd));
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    return jsString(vm, WTFMove(result));
}

enum class StringReplaceMode : bool { Single, Global };
inline JSString* replaceUsingStringSearch(VM& vm, JSGlobalObject* globalObject, JSString* jsString, String string, String searchString, JSValue replaceValue, StringReplaceMode mode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    CallData callData;
    std::optional<CachedCall> cachedCall;
    String replaceString;
    if (replaceValue.isString()) {
        replaceString = asString(replaceValue)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    } else {
        callData = JSC::getCallData(replaceValue);
        if (callData.type == CallData::Type::None) {
            replaceString = replaceValue.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
        } else if (callData.type == CallData::Type::JS) {
            cachedCall.emplace(globalObject, jsCast<JSFunction*>(replaceValue), 3);
            RETURN_IF_EXCEPTION(scope, nullptr);
            cachedCall->setThis(jsUndefined());
        }
    }

    if (mode == StringReplaceMode::Single) {
        if (!replaceString.isNull())
            RELEASE_AND_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::Yes, StringReplaceUseTable::No, BoyerMooreHorspoolTable<uint8_t>>(globalObject, jsString, WTFMove(string), WTFMove(searchString), WTFMove(replaceString), nullptr)));
    }

    size_t matchStart = string.find(searchString);
    if (matchStart == notFound)
        return jsString;

    size_t endOfLastMatch = 0;
    size_t searchStringLength = searchString.length();
    Vector<Range<int32_t>, 16> sourceRanges;
    Vector<String, 16> replacements;
    do {
        if (callData.type != CallData::Type::None) {
            JSValue replacement;
            if (cachedCall) {
                auto* substring = jsSubstring(vm, string, matchStart, searchStringLength);
                RETURN_IF_EXCEPTION(scope, nullptr);
                cachedCall->clearArguments();
                cachedCall->appendArgument(substring);
                cachedCall->appendArgument(jsNumber(matchStart));
                cachedCall->appendArgument(jsString);
                ASSERT(!cachedCall->hasOverflowedArguments());
                replacement = cachedCall->call();
            } else {
                MarkedArgumentBuffer args;
                auto* substring = jsSubstring(vm, string, matchStart, searchString.impl()->length());
                RETURN_IF_EXCEPTION(scope, nullptr);
                args.append(substring);
                args.append(jsNumber(matchStart));
                args.append(jsString);
                ASSERT(!args.hasOverflowed());
                replacement = call(globalObject, replaceValue, callData, jsUndefined(), args);
            }
            RETURN_IF_EXCEPTION(scope, nullptr);
            replaceString = replacement.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }

        if (UNLIKELY(!sourceRanges.tryConstructAndAppend(endOfLastMatch, matchStart))) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        size_t matchEnd = matchStart + searchStringLength;
        if (callData.type != CallData::Type::None)
            replacements.append(replaceString);
        else {
            StringBuilder replacement(StringBuilder::OverflowHandler::RecordOverflow);
            int ovector[2] = { static_cast<int>(matchStart),  static_cast<int>(matchEnd) };
            substituteBackreferences(replacement, replaceString, string, ovector, nullptr);
            if (UNLIKELY(replacement.hasOverflowed())) {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }
            replacements.append(replacement.toString());
        }

        endOfLastMatch = matchEnd;
        if (mode == StringReplaceMode::Single)
            break;
        matchStart = string.find(searchString, !searchStringLength ? endOfLastMatch + 1 : endOfLastMatch);
    } while (matchStart != notFound);

    if (UNLIKELY(!sourceRanges.tryConstructAndAppend(endOfLastMatch, string.length()))) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, jsString, string, sourceRanges.data(), sourceRanges.size(), replacements.data(), replacements.size()));
}

} // namespace JSC
