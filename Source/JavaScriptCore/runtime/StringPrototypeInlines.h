/*
 * Copyright (C) 2019-2024 Apple, Inc. All rights reserved.
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

#include "CachedCallInlines.h"
#include "ExceptionHelpers.h"
#include "JSCellButterfly.h"
#include "ObjectConstructor.h"
#include "ParseInt.h"
#include "StringPrototype.h"
#include "StringReplaceCacheInlines.h"
#include "RegExpGlobalData.h"
#include "RegExpGlobalDataInlines.h"
#include "RegExpObject.h"
#include <wtf/Range.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringSearch.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

inline Structure* StringPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(DerivedStringObjectType, StructureFlags), info());
}

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

template<typename T> concept Arithmetic = std::is_arithmetic_v<T>;

template<Arithmetic NumberType>
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
    int32_t start = std::clamp(startValue, 0, length);
    int32_t end = std::clamp(endValue.value_or(length), 0, length);

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
        std::span<Latin1Character> buffer;
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
                substring.getCharacters8(buffer.subspan(bufferPos.value()));
                bufferPos += substring.length();
            }
            if (i < separatorCount) {
                StringView separator = separators[i];
                separator.getCharacters8(buffer.subspan(bufferPos.value()));
                bufferPos += separator.length();
            }
        }

        RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
    }

    std::span<char16_t> buffer;
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
            substring.getCharacters(buffer.subspan(bufferPos.value()));
            bufferPos += substring.length();
        }
        if (i < separatorCount) {
            StringView separator = separators[i];
            separator.getCharacters(buffer.subspan(bufferPos.value()));
            bufferPos += separator.length();
        }
    }

    RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
}

ALWAYS_INLINE JSString* jsSpliceSubstringsWithSeparator(JSGlobalObject* globalObject, JSString* sourceVal, const String& source, const Range<int32_t>* substringRanges, int rangeCount, const String& separator, int separatorCount)
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
        RELEASE_AND_RETURN(scope, jsString(globalObject, leftPart, separator, rightPart));
    }

    CheckedInt32 totalLength = 0;
    bool allSeparators8Bit = !separator.length() || separator.is8Bit();
    for (int i = 0; i < rangeCount; i++)
        totalLength += substringRanges[i].distance();
    totalLength += CheckedInt32(separatorCount) * separator.length();
    if (totalLength.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    if (!totalLength)
        return jsEmptyString(vm);

    if (source.is8Bit() && allSeparators8Bit) {
        std::span<Latin1Character> buffer;
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
                substring.getCharacters8(buffer.subspan(bufferPos.value()));
                bufferPos += substring.length();
            }
            if (i < separatorCount) {
                StringView { separator }.getCharacters8(buffer.subspan(bufferPos.value()));
                bufferPos += separator.length();
            }
        }

        RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
    }

    std::span<char16_t> buffer;
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
            substring.getCharacters(buffer.subspan(bufferPos.value()));
            bufferPos += substring.length();
        }
        if (i < separatorCount) {
            StringView { separator }.getCharacters(buffer.subspan(bufferPos.value()));
            bufferPos += separator.length();
        }
    }

    RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
}

enum class StringReplaceSubstitutions : bool { No, Yes };
template<StringReplaceSubstitutions substitutions>
ALWAYS_INLINE String tryMakeReplacedString(const String& string, const String& replacement, size_t matchStart, size_t matchEnd)
{
    if constexpr (substitutions == StringReplaceSubstitutions::Yes) {
        size_t dollarPos = replacement.find('$');
        if (dollarPos != WTF::notFound) {
            StringBuilder builder(OverflowPolicy::RecordOverflow);
            int ovector[2] = { static_cast<int>(matchStart), static_cast<int>(matchEnd) };
            substituteBackreferencesSlow(builder, replacement, string, ovector, nullptr, dollarPos);
            if (builder.hasOverflowed()) [[unlikely]]
                return { };
            if (auto result = tryMakeString(StringView(string).substring(0, matchStart), builder.toString(), StringView(string).substring(matchEnd, string.length() - matchEnd)); !result.isNull()) [[likely]]
                return result;
        }
    }
    return tryMakeString(StringView(string).substring(0, matchStart), replacement, StringView(string).substring(matchEnd, string.length() - matchEnd));
}

enum class StringReplaceUseTable : bool { No, Yes };
template<StringReplaceSubstitutions substitutions, StringReplaceUseTable useTable, typename TableType>
ALWAYS_INLINE JSString* stringReplaceStringString(JSGlobalObject* globalObject, JSString* stringCell, const String& string, const String& search, const String& replacement, const TableType* table)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t matchStart;
    if constexpr (useTable == StringReplaceUseTable::Yes)
        matchStart = table->find(string, search);
    else {
        UNUSED_PARAM(table);
        matchStart = StringView(string).find(vm.adaptiveStringSearcherTables(), StringView(search));
    }
    if (matchStart == notFound)
        return stringCell;

    size_t searchLength = search.length();
    size_t matchEnd = matchStart + searchLength;
    auto result = tryMakeReplacedString<substitutions>(string, replacement, matchStart, matchEnd);
    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    return jsString(vm, WTFMove(result));
}

template<StringReplaceSubstitutions substitutions, StringReplaceUseTable useTable, typename TableType>
ALWAYS_INLINE JSString* stringReplaceAllStringString(JSGlobalObject* globalObject, JSString* stringCell, const String& string, const String& search, const String& replacement, const TableType* table)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<size_t, 16> matchStarts;
    size_t searchLength = search.length();

    size_t matchStart = 0;
    while (true) {
        if constexpr (useTable == StringReplaceUseTable::Yes)
            matchStart = table->find(StringView(string).substring(matchStart), search);
        else
            matchStart = StringView(string).find(vm.adaptiveStringSearcherTables(), StringView(search), matchStart);

        if (matchStart == notFound)
            break;

        matchStarts.append(matchStart);
        matchStart += searchLength;
        if (search.isEmpty())
            ++matchStart;
    }

    if (matchStarts.isEmpty())
        return stringCell;

    auto resultLength = CheckedSize { string.length() + matchStarts.size() * (replacement.length() - searchLength) };

    StringBuilder resultBuilder(OverflowPolicy::RecordOverflow);
    resultBuilder.reserveCapacity(resultLength);

    size_t lastMatchEnd = 0;
    for (auto start : matchStarts) {
        resultBuilder.append(StringView(string).substring(lastMatchEnd, start - lastMatchEnd));
        if constexpr (substitutions == StringReplaceSubstitutions::Yes) {
            int ovector[2] = { static_cast<int>(start), static_cast<int>(start + searchLength) };
            substituteBackreferences(resultBuilder, replacement, string, ovector, nullptr);
        } else
            resultBuilder.append(replacement);
        lastMatchEnd = start + searchLength;
    }
    resultBuilder.append(StringView(string).substring(lastMatchEnd));

    if (resultBuilder.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    return jsString(vm, resultBuilder.toString());
}

enum class StringReplaceMode : bool { Single, Global };
template <StringReplaceMode mode>
inline JSString* replaceUsingStringSearch(VM& vm, JSGlobalObject* globalObject, JSString* jsString, const String& string, const String& searchString, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    CallData callData;
    String replaceString;
    if (replaceValue.isString()) {
        replaceString = asString(replaceValue)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    } else {
        callData = JSC::getCallDataInline(replaceValue);
        if (callData.type == CallData::Type::None) {
            replaceString = replaceValue.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    if (!replaceString.isNull()) {
        if constexpr (mode == StringReplaceMode::Single)
            RELEASE_AND_RETURN(scope, (stringReplaceStringString<StringReplaceSubstitutions::Yes, StringReplaceUseTable::No, BoyerMooreHorspoolTable<uint8_t>>(globalObject, jsString, string, searchString, replaceString, nullptr)));
        else {
            ASSERT(mode == StringReplaceMode::Global);
            RELEASE_AND_RETURN(scope, (stringReplaceAllStringString<StringReplaceSubstitutions::Yes, StringReplaceUseTable::No, BoyerMooreHorspoolTable<uint8_t>>(globalObject, jsString, string, searchString, replaceString, nullptr)));
        }
    }

    ASSERT(callData.type != CallData::Type::None);
    size_t matchStart = StringView(string).find(vm.adaptiveStringSearcherTables(), StringView(searchString));
    if (matchStart == notFound)
        return jsString;

    std::optional<CachedCall> cachedCall;
    if (callData.type == CallData::Type::JS) {
        cachedCall.emplace(globalObject, jsCast<JSFunction*>(replaceValue), 3);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    size_t endOfLastMatch = 0;
    size_t searchStringLength = searchString.length();
    Vector<Range<int32_t>, 16> sourceRanges;
    Vector<String, 16> replacements;
    do {
        JSValue replacement;
        if (cachedCall) {
            auto* substring = jsSubstring(vm, globalObject, jsString, matchStart, searchStringLength);
            RETURN_IF_EXCEPTION(scope, nullptr);
            replacement = cachedCall->callWithArguments(globalObject, jsUndefined(), substring, jsNumber(matchStart), jsString);
            RETURN_IF_EXCEPTION(scope, nullptr);
        } else {
            MarkedArgumentBuffer args;
            auto* substring = jsSubstring(vm, globalObject, jsString, matchStart, searchString.impl()->length());
            RETURN_IF_EXCEPTION(scope, nullptr);
            args.append(substring);
            args.append(jsNumber(matchStart));
            args.append(jsString);
            ASSERT(!args.hasOverflowed());
            replacement = call(globalObject, replaceValue, callData, jsUndefined(), args);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        replaceString = replacement.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);

        if (!sourceRanges.tryConstructAndAppend(endOfLastMatch, matchStart)) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        size_t matchEnd = matchStart + searchStringLength;
        replacements.append(replaceString);

        endOfLastMatch = matchEnd;
        if constexpr (mode == StringReplaceMode::Single)
            break;
        matchStart = StringView(string).find(vm.adaptiveStringSearcherTables(), StringView(searchString), !searchStringLength ? endOfLastMatch + 1 : endOfLastMatch);
    } while (matchStart != notFound);

    if (!sourceRanges.tryConstructAndAppend(endOfLastMatch, string.length())) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, jsString, string, sourceRanges.span().data(), sourceRanges.size(), replacements.span().data(), replacements.size()));
}

enum class DollarCheck : uint8_t { Yes, No };
template<DollarCheck check = DollarCheck::Yes>
inline JSString* tryReplaceOneCharUsingString(JSGlobalObject* globalObject, JSString* string, JSString* search, JSString* replacement)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: Substring should be supported. However, substring of substring is not allowed at the moment.
    if (!string->isNonSubstringRope() || string->length() < 0x128)
        return nullptr;

    RETURN_IF_EXCEPTION(scope, nullptr);
    auto searchString = search->value(globalObject);
    if (searchString->length() != 1)
        return nullptr;

    auto replaceString = replacement->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if constexpr (check == DollarCheck::Yes) {
        if (replaceString->find('$') != notFound)
            return nullptr;
    }

    RELEASE_AND_RETURN(scope, string->tryReplaceOneChar(globalObject, searchString[0], replacement));
}

static ALWAYS_INLINE JSString* jsSpliceSubstrings(JSGlobalObject* globalObject, JSString* sourceVal, const String& source, std::span<const Range<int32_t>> substringRanges)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (substringRanges.size() == 1) {
        int sourceSize = source.length();
        int position = substringRanges.front().begin();
        int length = substringRanges.front().distance();
        if (position <= 0 && length >= sourceSize)
            return sourceVal;
        // We could call String::substringSharingImpl(), but this would result in redundant checks.
        RELEASE_AND_RETURN(scope, jsString(vm, StringImpl::createSubstringSharingImpl(*source.impl(), std::max(0, position), std::min(sourceSize, length))));
    }

    // We know that the sum of substringRanges lengths cannot exceed length of
    // source because the substringRanges were computed from the source string
    // in removeAllUsingRegExpSearch(). Hence, totalLength cannot exceed
    // String::MaxLength, and therefore, cannot overflow.
    Checked<int, AssertNoOverflow> totalLength = 0;
    for (auto& range : substringRanges)
        totalLength += range.distance();
    ASSERT(totalLength <= static_cast<int>(String::MaxLength));

    if (!totalLength)
        return jsEmptyString(vm);

    if (source.is8Bit()) {
        std::span<Latin1Character> buffer;
        auto sourceData = source.span8();
        auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
        if (!impl) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        Checked<size_t, AssertNoOverflow> bufferPos = 0;
        for (auto range : substringRanges) {
            size_t srcLen = range.distance();
            StringImpl::copyCharacters(buffer.subspan(bufferPos.value()), sourceData.subspan(range.begin(), srcLen));
            bufferPos += srcLen;
        }

        RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
    }

    std::span<char16_t> buffer;
    auto sourceData = source.span16();

    auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
    if (!impl) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    Checked<size_t, AssertNoOverflow> bufferPos = 0;
    for (auto& range : substringRanges) {
        size_t srcLen = range.distance();
        StringImpl::copyCharacters(buffer.subspan(bufferPos.value()), sourceData.subspan(range.begin(), srcLen));
        bufferPos += srcLen;
    }

    RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
}

#define OUT_OF_MEMORY(exec__, scope__) \
    do { \
        throwOutOfMemoryError(exec__, scope__); \
        return nullptr; \
    } while (false)

static ALWAYS_INLINE JSString* removeAllUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    SuperSamplerScope superSamplerScope(false);

    size_t lastIndex = 0;
    unsigned startPosition = 0;
    Vector<Range<int32_t>, 64> sourceRanges;
    unsigned sourceLen = source.length();

    auto genericMatches = [&](VM& vm, auto input, auto pattern) ALWAYS_INLINE_LAMBDA -> size_t {
        ASSERT(!pattern.empty());
        unsigned startIndex = 0;
        if (pattern.size() == 1) {
            size_t lastFound = notFound;
            auto patternCharacter = pattern[0];
            for (size_t i = 0; i < input.size(); ++i) {
                if (input[i] != patternCharacter)
                    continue;
                if (startIndex < i) {
                    if (!sourceRanges.tryConstructAndAppend(startIndex, i)) [[unlikely]] {
                        throwOutOfMemoryError(globalObject, scope);
                        return notFound;
                    }
                }
                lastFound = i;
                startIndex = i + 1;
            }
            if (lastFound == notFound)
                return lastFound;

            if (startIndex < sourceLen) {
                if (!sourceRanges.tryConstructAndAppend(startIndex, sourceLen)) [[unlikely]] {
                    throwOutOfMemoryError(globalObject, scope);
                    return notFound;
                }
            }
            return lastFound;
        }

        AdaptiveStringSearcher<typename decltype(pattern)::value_type, typename decltype(input)::value_type> search(vm.adaptiveStringSearcherTables(), pattern);
        size_t found = search.search(input, startIndex);
        if (found == notFound)
            return notFound;

        size_t lastFound = notFound;
        do {
            if (startIndex < found) {
                if (!sourceRanges.tryConstructAndAppend(startIndex, found)) [[unlikely]] {
                    throwOutOfMemoryError(globalObject, scope);
                    return notFound;
                }
            }
            startIndex = found + pattern.size();
            lastFound = found;
            found = search.search(input, startIndex);
        } while (found != notFound);

        if (startIndex < sourceLen) {
            if (!sourceRanges.tryConstructAndAppend(startIndex, sourceLen)) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return notFound;
            }
        }
        return lastFound;
    };

    if (regExp->hasValidAtom()) {
        const String& pattern = regExp->atom();
        ASSERT(!pattern.isEmpty());
        size_t lastIndex = 0;
        if (pattern.is8Bit()) {
            if (source.is8Bit())
                lastIndex = genericMatches(vm, source.span8(), pattern.span8());
            else
                lastIndex = genericMatches(vm, source.span16(), pattern.span8());
        } else {
            if (source.is8Bit())
                lastIndex = genericMatches(vm, source.span8(), pattern.span16());
            else
                lastIndex = genericMatches(vm, source.span16(), pattern.span16());
        }
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (lastIndex == notFound)
            return string;

        // Record the last matching.
        globalObject->regExpGlobalData().recordMatch(vm, globalObject, regExp, string, MatchResult { static_cast<unsigned>(lastIndex), static_cast<unsigned>(lastIndex + pattern.length()) }, /* oneCharacterMatch */ false);
        RELEASE_AND_RETURN(scope, jsSpliceSubstrings(globalObject, string, source, sourceRanges.span()));
    }

    while (true) {
        MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!result)
            break;

        if (lastIndex < result.start) {
            if (!sourceRanges.tryConstructAndAppend(lastIndex, result.start)) [[unlikely]]
                OUT_OF_MEMORY(globalObject, scope);
        }
        lastIndex = result.end;
        startPosition = lastIndex;

        // special case of empty match
        if (result.empty()) {
            startPosition++;
            if (startPosition > sourceLen)
                break;
        }
    }

    if (!lastIndex)
        return string;

    if (static_cast<unsigned>(lastIndex) < sourceLen) {
        if (!sourceRanges.tryConstructAndAppend(lastIndex, sourceLen)) [[unlikely]]
            OUT_OF_MEMORY(globalObject, scope);
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstrings(globalObject, string, source, sourceRanges.span()));
}

ALWAYS_INLINE JSCellButterfly* addToRegExpSearchCache(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto* entry = vm.stringReplaceCache.get(source, regExp)) {
        auto lastMatch = entry->m_lastMatch;
        auto matchResult = entry->m_matchResult;
        globalObject->regExpGlobalData().resetResultFromCache(globalObject, regExp, string, matchResult, WTFMove(lastMatch));
        RELEASE_AND_RETURN(scope, entry->m_result);
    }

    size_t lastIndex = 0;
    unsigned startPosition = 0;
    MarkedArgumentBuffer results;
    while (true) {
        int* ovector;
        MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition, &ovector);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!result)
            break;

        for (unsigned i = 0; i < regExp->numSubpatterns() + 1; ++i) {
            int matchStart = ovector[i * 2];
            int matchLen = ovector[i * 2 + 1] - matchStart;

            JSValue patternValue;

            if (matchStart < 0)
                patternValue = jsUndefined();
            else {
                patternValue = jsSubstringOfResolved(vm, string, matchStart, matchLen);
                RETURN_IF_EXCEPTION(scope, { });
            }

            results.append(patternValue);
        }

        results.append(jsNumber(result.start));

        lastIndex = result.end;
        startPosition = lastIndex;

        // special case of empty match
        if (result.empty()) {
            startPosition++;
            if (startPosition > source.length())
                break;
            if (U16_IS_LEAD(source[startPosition - 1]) && U16_IS_TRAIL(source[startPosition])) {
                startPosition++;
                if (startPosition > source.length())
                    break;
            }
        }
    }

    // Nothing matches.
    if (results.isEmpty())
        RELEASE_AND_RETURN(scope, nullptr);

    JSCellButterfly* result = JSCellButterfly::tryCreateFromArgList(vm, results);
    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    vm.stringReplaceCache.set(source, regExp, result, globalObject->regExpGlobalData().matchResult(), globalObject->regExpGlobalData().ovector());
    RELEASE_AND_RETURN(scope, result);
}

static ALWAYS_INLINE JSString* replaceAllWithCacheUsingRegExpSearchThreeArguments(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp, JSFunction* replaceFunction, JSCellButterfly* result)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned sourceLen = source.length();
    unsigned cachedCount = regExp->numSubpatterns() + 2;

    // regExp->numSubpatterns() + 1 for pattern args, + 2 for match start and string
    unsigned length = result->length();
    unsigned items = length / cachedCount;

    MarkedArgumentBuffer replacements;
    replacements.fill(vm, items, [](JSValue*) { });
    if (replacements.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    CachedCall cachedCall(globalObject, replaceFunction, 3);
    RETURN_IF_EXCEPTION(scope, nullptr);

    ASSERT(!regExp->numSubpatterns());
    ASSERT(cachedCount == 2);
    uint64_t totalLength = 0;
    bool replacementsAre8Bit = true;
    {
        size_t lastIndex = 0;
        size_t index = 0;
        for (auto& slot : replacements) {
            JSString* matchedString = asString(result->get(index * 2));
            JSValue startOffset = result->get(index * 2 + 1);

            int32_t start = startOffset.asInt32();
            int32_t end = start + matchedString->length();

            JSValue jsResult = cachedCall.callWithArguments(globalObject, jsUndefined(), matchedString, startOffset, string);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, nullptr);

            auto jsString = jsResult.toString(globalObject);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, nullptr);

            auto string = jsString->value(globalObject);
            replacementsAre8Bit &= string->is8Bit();
            totalLength += string->length();
            totalLength += (start - lastIndex);
            slot = JSValue::encode(jsString);

            lastIndex = end;
            ++index;
        }
        if (static_cast<unsigned>(lastIndex) < sourceLen)
            totalLength += (sourceLen - lastIndex);
    }

    StringView sourceView { source };
    if (sourceView.is8Bit() && replacementsAre8Bit) {
        std::span<Latin1Character> buffer;
        auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
        if (!impl) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        size_t lastIndex = 0;
        unsigned index = 0;
        size_t bufferPos = 0;
        for (auto& slot : replacements) {
            int32_t length = asString(result->get(index * 2))->length();
            int32_t start = result->get(index * 2 + 1).asInt32();
            int32_t end = start + length;

            auto substring = sourceView.substring(lastIndex, start - lastIndex);
            substring.getCharacters8(buffer.subspan(bufferPos));
            bufferPos += substring.length();

            auto replacement = asString(JSValue::decode(slot))->value(globalObject);
            StringView { replacement }.getCharacters8(buffer.subspan(bufferPos));
            bufferPos += replacement->length();

            ++index;
            lastIndex = end;
        }
        if (static_cast<unsigned>(lastIndex) < sourceLen) {
            auto substring = sourceView.substring(lastIndex, sourceLen - lastIndex);
            substring.getCharacters8(buffer.subspan(bufferPos));
        }

        ASSERT(lastIndex <= sourceLen && (bufferPos + (sourceLen - lastIndex)) == totalLength);
        RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
    }

    std::span<char16_t> buffer;
    auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
    if (!impl) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    size_t lastIndex = 0;
    unsigned index = 0;
    size_t bufferPos = 0;
    for (auto& slot : replacements) {
        int32_t length = asString(result->get(index * 2))->length();
        int32_t start = result->get(index * 2 + 1).asInt32();
        int32_t end = start + length;

        auto substring = sourceView.substring(lastIndex, start - lastIndex);
        substring.getCharacters(buffer.subspan(bufferPos));
        bufferPos += substring.length();

        auto replacement = asString(JSValue::decode(slot))->value(globalObject);
        StringView { replacement }.getCharacters(buffer.subspan(bufferPos));
        bufferPos += replacement->length();

        ++index;
        lastIndex = end;
    }
    if (static_cast<unsigned>(lastIndex) < sourceLen) {
        auto substring = sourceView.substring(lastIndex, sourceLen - lastIndex);
        substring.getCharacters(buffer.subspan(bufferPos));
    }

    ASSERT(lastIndex <= sourceLen && (bufferPos + (sourceLen - lastIndex)) == totalLength);
    RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
}

static ALWAYS_INLINE JSString* replaceAllWithCacheUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp, JSFunction* replaceFunction)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    SuperSamplerScope superSamplerScope(true);

    ASSERT(!string->isRope()); // This is already resolved.
    ASSERT(source.length() >= Options::thresholdForStringReplaceCache());
    // Currently not caching results when named captures are specified.
    ASSERT(!regExp->hasNamedCaptures());

    unsigned sourceLen = source.length();
    unsigned cachedCount = regExp->numSubpatterns() + 2;
    unsigned argCount = cachedCount + 1;

    JSCellButterfly* result = addToRegExpSearchCache(vm, globalObject, string, source, regExp);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!result)
        return string;

    if (argCount == 3)
        RELEASE_AND_RETURN(scope, replaceAllWithCacheUsingRegExpSearchThreeArguments(vm, globalObject, string, source, regExp, replaceFunction, result));

    // regExp->numSubpatterns() + 1 for pattern args, + 2 for match start and string
    unsigned length = result->length();
    unsigned items = length / cachedCount;
    Vector<String, 16> replacements;

    if (!replacements.tryReserveCapacity(items)) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    replacements.grow(items);

    CachedCall cachedCall(globalObject, replaceFunction, argCount);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (argCount == 3) {
        ASSERT(!regExp->numSubpatterns());
        ASSERT(cachedCount == 2);
        uint64_t totalLength = 0;
        bool replacementsAre8Bit = true;
        {
            size_t lastIndex = 0;
            size_t index = 0;
            for (auto& slot : replacements) {
                JSString* matchedString = asString(result->get(index * 2));
                JSValue startOffset = result->get(index * 2 + 1);

                int32_t start = startOffset.asInt32();
                int32_t end = start + matchedString->length();

                JSValue jsResult = cachedCall.callWithArguments(globalObject, jsUndefined(), matchedString, startOffset, string);
                RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, nullptr);

                auto string = jsResult.toWTFString(globalObject);
                RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, nullptr);

                replacementsAre8Bit &= string.is8Bit();
                totalLength += string.length();
                totalLength += (start - lastIndex);
                slot = WTFMove(string);

                lastIndex = end;
                ++index;
            }
            if (static_cast<unsigned>(lastIndex) < sourceLen)
                totalLength += (sourceLen - lastIndex);
        }

        StringView sourceView { source };
        if (sourceView.is8Bit() && replacementsAre8Bit) {
            std::span<Latin1Character> buffer;
            auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
            if (!impl) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }

            size_t lastIndex = 0;
            unsigned index = 0;
            size_t bufferPos = 0;
            for (auto& replacement : replacements) {
                int32_t length = asString(result->get(index * 2))->length();
                int32_t start = result->get(index * 2 + 1).asInt32();
                int32_t end = start + length;

                auto substring = sourceView.substring(lastIndex, start - lastIndex);
                substring.getCharacters8(buffer.subspan(bufferPos));
                bufferPos += substring.length();

                StringView { replacement }.getCharacters8(buffer.subspan(bufferPos));
                bufferPos += replacement.length();

                ++index;
                lastIndex = end;
            }
            if (static_cast<unsigned>(lastIndex) < sourceLen) {
                auto substring = sourceView.substring(lastIndex, sourceLen - lastIndex);
                substring.getCharacters8(buffer.subspan(bufferPos));
            }

            ASSERT(lastIndex <= sourceLen && (bufferPos + (sourceLen - lastIndex)) == totalLength);
            RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
        }

        std::span<char16_t> buffer;
        auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
        if (!impl) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        size_t lastIndex = 0;
        unsigned index = 0;
        size_t bufferPos = 0;
        for (auto& replacement : replacements) {
            int32_t length = asString(result->get(index * 2))->length();
            int32_t start = result->get(index * 2 + 1).asInt32();
            int32_t end = start + length;

            auto substring = sourceView.substring(lastIndex, start - lastIndex);
            substring.getCharacters(buffer.subspan(bufferPos));
            bufferPos += substring.length();

            StringView { replacement }.getCharacters(buffer.subspan(bufferPos));
            bufferPos += replacement.length();

            ++index;
            lastIndex = end;
        }
        if (static_cast<unsigned>(lastIndex) < sourceLen) {
            auto substring = sourceView.substring(lastIndex, sourceLen - lastIndex);
            substring.getCharacters(buffer.subspan(bufferPos));
        }

        ASSERT(lastIndex <= sourceLen && (bufferPos + (sourceLen - lastIndex)) == totalLength);
        RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
    }

    Vector<Range<int32_t>, 16> sourceRanges;
    if (!sourceRanges.tryReserveCapacity(items + 1)) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    size_t lastIndex = 0;
    size_t cursor = 0;
    for (auto& slot : replacements) {
        cachedCall.clearArguments();
        for (unsigned i = 0; i < cachedCount; ++i)
            cachedCall.appendArgument(result->get(cursor + i));
        cachedCall.appendArgument(string);

        int32_t start = result->get(cursor + cachedCount - 1).asInt32();
        int32_t end = start + asString(result->get(cursor))->length();

        sourceRanges.constructAndAppend(lastIndex, start);

        cachedCall.setThis(jsUndefined());
        if (cachedCall.hasOverflowedArguments()) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        JSValue jsResult = cachedCall.call();
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, nullptr);

        auto string = jsResult.toWTFString(globalObject);
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, nullptr);

        slot = WTFMove(string);

        lastIndex = end;
        cursor += cachedCount;
    }

    if (static_cast<unsigned>(lastIndex) < sourceLen)
        sourceRanges.constructAndAppend(lastIndex, sourceLen);
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, string, source, sourceRanges.span().data(), sourceRanges.size(), replacements.span().data(), replacements.size()));
}

static ALWAYS_INLINE JSString* tryTrimSpaces(VM& vm, JSGlobalObject* globalObject, const String& source, JSString* string, RegExp* regExp)
{
    unsigned sourceLen = source.length();
    unsigned left = 0;
    unsigned right = sourceLen;
    switch (regExp->specificPattern()) {
    case Yarr::SpecificPattern::TrailingSpacesPlus: {
        while (right > 0 && isStrWhiteSpace(source[right - 1]))
            right--;

        if (right == sourceLen) {
            // Not found.
            return string;
        }

        if (!right) {
            // Everything is spaces.
            globalObject->regExpGlobalData().resetResultFromCache(globalObject, regExp, string, MatchResult { 0, sourceLen }, { });
            return jsEmptyString(vm);
        }

        globalObject->regExpGlobalData().resetResultFromCache(globalObject, regExp, string, MatchResult { right, sourceLen }, { });
        return jsString(vm, source.substringSharingImpl(0, right));
    }
    case Yarr::SpecificPattern::LeadingSpacesPlus: {
        while (left < sourceLen && isStrWhiteSpace(source[left]))
            left++;

        if (!left)
            return string;

        if (left == sourceLen) {
            // Everything is spaces.
            globalObject->regExpGlobalData().resetResultFromCache(globalObject, regExp, string, MatchResult { 0, sourceLen }, { });
            return jsEmptyString(vm);
        }

        globalObject->regExpGlobalData().resetResultFromCache(globalObject, regExp, string, MatchResult { 0, left }, { });
        return jsString(vm, source.substringSharingImpl(left, sourceLen));
    }
    case Yarr::SpecificPattern::TrailingSpacesStar:
    case Yarr::SpecificPattern::LeadingSpacesStar:
    case Yarr::SpecificPattern::Atom:
    case Yarr::SpecificPattern::Newlines:
    case Yarr::SpecificPattern::None:
        break;
    }
    return nullptr;
}

ALWAYS_INLINE JSString* replaceAllWithStringUsingRegExpSearchNoBackreferences(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp, const String& replacementString)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t lastIndex = 0;
    unsigned startPosition = 0;
    unsigned sourceLen = source.length();

    Vector<Range<int32_t>, 16> sourceRanges;
    size_t replacementCount = 0;

    while (1) {
        int* ovector;
        MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition, &ovector);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!result)
            break;

        if (!sourceRanges.tryConstructAndAppend(lastIndex, result.start)) [[unlikely]]
            OUT_OF_MEMORY(globalObject, scope);

        ++replacementCount;

        lastIndex = result.end;
        startPosition = lastIndex;

        // special case of empty match
        if (result.empty()) {
            startPosition++;
            if (startPosition > sourceLen)
                break;
            if (U16_IS_LEAD(source[startPosition - 1]) && U16_IS_TRAIL(source[startPosition])) {
                startPosition++;
                if (startPosition > sourceLen)
                    break;
            }
        }
    }

    if (!lastIndex && !replacementCount)
        return string;

    if (static_cast<unsigned>(lastIndex) < sourceLen) {
        if (!sourceRanges.tryConstructAndAppend(lastIndex, sourceLen)) [[unlikely]]
            OUT_OF_MEMORY(globalObject, scope);
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparator(globalObject, string, source, sourceRanges.span().data(), sourceRanges.size(), replacementString, replacementCount));
}

ALWAYS_INLINE JSString* replaceAllWithStringUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp, const String& replacementString)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t dollarPos = replacementString.find('$');
    if (dollarPos == notFound)
        RELEASE_AND_RETURN(scope, replaceAllWithStringUsingRegExpSearchNoBackreferences(vm, globalObject, string, source, regExp, replacementString));

    size_t lastIndex = 0;
    unsigned startPosition = 0;
    unsigned sourceLen = source.length();

    Vector<Range<int32_t>, 16> sourceRanges;
    Vector<String, 16> replacements;

    while (1) {
        int* ovector;
        MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition, &ovector);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!result)
            break;

        if (!sourceRanges.tryConstructAndAppend(lastIndex, result.start)) [[unlikely]]
            OUT_OF_MEMORY(globalObject, scope);

        StringBuilder replacement(OverflowPolicy::RecordOverflow);
        substituteBackreferencesSlow(replacement, replacementString, source, ovector, regExp, dollarPos);
        if (replacement.hasOverflowed()) [[unlikely]]
            OUT_OF_MEMORY(globalObject, scope);
        replacements.append(replacement.toString());

        lastIndex = result.end;
        startPosition = lastIndex;

        // special case of empty match
        if (result.empty()) {
            startPosition++;
            if (startPosition > sourceLen)
                break;
            if (U16_IS_LEAD(source[startPosition - 1]) && U16_IS_TRAIL(source[startPosition])) {
                startPosition++;
                if (startPosition > sourceLen)
                    break;
            }
        }
    }

    if (!lastIndex && replacements.isEmpty())
        return string;

    if (static_cast<unsigned>(lastIndex) < sourceLen) {
        if (!sourceRanges.tryConstructAndAppend(lastIndex, sourceLen)) [[unlikely]]
            OUT_OF_MEMORY(globalObject, scope);
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, string, source, sourceRanges.span().data(), sourceRanges.size(), replacements.span().data(), replacements.size()));
}

ALWAYS_INLINE JSString* replaceOneWithStringUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp, const String& replacementString)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    int* ovector;
    MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, 0, &ovector);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!result)
        return string;

    auto before = StringView { source }.substring(0, result.start);
    auto after = StringView { source }.substring(result.end, source.length() - result.end);

    size_t dollarPos = replacementString.find('$');
    if (dollarPos == WTF::notFound) [[likely]] {
        auto concatenated = tryMakeString(before, StringView { replacementString }, after);
        if (!concatenated) [[unlikely]]
            OUT_OF_MEMORY(globalObject, scope);
        RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(concatenated)));
    }

    StringBuilder replacement(OverflowPolicy::RecordOverflow);
    substituteBackreferencesSlow(replacement, replacementString, source, ovector, regExp, dollarPos);
    if (replacement.hasOverflowed()) [[unlikely]]
        OUT_OF_MEMORY(globalObject, scope);
    auto concatenated = tryMakeString(before, StringView { replacement }, after);
    if (!concatenated) [[unlikely]]
        OUT_OF_MEMORY(globalObject, scope);
    RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(concatenated)));
}

ALWAYS_INLINE JSString* replaceUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, JSString* string, JSValue searchValue, const CallData& callData, const String& replacementString, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto source = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned sourceLen = source->length();
    RegExpObject* regExpObject = jsCast<RegExpObject*>(searchValue);
    RegExp* regExp = regExpObject->regExp();
    bool global = regExp->global();
    bool hasNamedCaptures = regExp->hasNamedCaptures();

    if (global) {
        // ES5.1 15.5.4.10 step 8.a.
        regExpObject->setLastIndex(globalObject, 0);
        RETURN_IF_EXCEPTION(scope, nullptr);

        if (callData.type == CallData::Type::None && !replacementString.length())
            RELEASE_AND_RETURN(scope, removeAllUsingRegExpSearch(vm, globalObject, string, source, regExp));

        if (callData.type == CallData::Type::JS && !hasNamedCaptures && sourceLen >= Options::thresholdForStringReplaceCache())
            RELEASE_AND_RETURN(scope, replaceAllWithCacheUsingRegExpSearch(vm, globalObject, string, source, regExp, jsCast<JSFunction*>(replaceValue)));
    }

    if (callData.type == CallData::Type::None) {
        switch (regExp->specificPattern()) {
        case Yarr::SpecificPattern::TrailingSpacesPlus:
        case Yarr::SpecificPattern::LeadingSpacesPlus:
        case Yarr::SpecificPattern::TrailingSpacesStar:
        case Yarr::SpecificPattern::LeadingSpacesStar: {
            if (!replacementString.isEmpty())
                break;

            if (auto* result = tryTrimSpaces(vm, globalObject, source, string, regExp))
                return result;

            break;
        }
        case Yarr::SpecificPattern::Atom:
        case Yarr::SpecificPattern::Newlines:
        case Yarr::SpecificPattern::None:
            break;
        }

        if (global)
            RELEASE_AND_RETURN(scope, replaceAllWithStringUsingRegExpSearch(vm, globalObject, string, source, regExp, replacementString));
        RELEASE_AND_RETURN(scope, replaceOneWithStringUsingRegExpSearch(vm, globalObject, string, source, regExp, replacementString));
    }

    size_t lastIndex = 0;
    unsigned startPosition = 0;

    Vector<Range<int32_t>, 16> sourceRanges;
    Vector<String, 16> replacements;

    // This is either a loop (if global is set) or a one-way (if not).
    if (global && callData.type == CallData::Type::JS) {
        // regExp->numSubpatterns() + 1 for pattern args, + 2 for match start and string
        int argCount = regExp->numSubpatterns() + 1 + 2;
        if (hasNamedCaptures)
            ++argCount;
        JSFunction* func = jsCast<JSFunction*>(replaceValue);
        std::optional<CachedCall> cachedCallHolder;
        CachedCall* cachedCall = nullptr;
        while (true) {
            int* ovector;
            MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition, &ovector);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!result)
                break;

            if (!sourceRanges.tryConstructAndAppend(lastIndex, result.start)) [[unlikely]]
                OUT_OF_MEMORY(globalObject, scope);

            if (!cachedCall) {
                cachedCallHolder.emplace(globalObject, func, argCount);
                RETURN_IF_EXCEPTION(scope, nullptr);
                cachedCall = &cachedCallHolder.value();
            }
            cachedCall->clearArguments();
            JSObject* groups = hasNamedCaptures ? constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure()) : nullptr;

            for (unsigned i = 0; i < regExp->numSubpatterns() + 1; ++i) {
                int matchStart = ovector[i * 2];
                int matchLen = ovector[i * 2 + 1] - matchStart;

                JSValue patternValue;

                if (matchStart < 0)
                    patternValue = jsUndefined();
                else {
                    patternValue = jsSubstring(vm, globalObject, string, matchStart, matchLen);
                    RETURN_IF_EXCEPTION(scope, nullptr);
                }

                cachedCall->appendArgument(patternValue);

                if (i && hasNamedCaptures) {
                    String groupName = regExp->getCaptureGroupNameForSubpatternId(i);
                    if (!groupName.isEmpty()) {
                        auto captureIndex = regExp->subpatternIdForGroupName(groupName, ovector);

                        if (captureIndex == i)
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), patternValue);
                        else if (captureIndex > 0) {
                            int captureStart = ovector[captureIndex * 2];
                            int captureLen = ovector[captureIndex * 2 + 1] - captureStart;
                            JSValue captureValue;
                            if (captureStart < 0)
                                captureValue = jsUndefined();
                            else {
                                captureValue = jsSubstring(vm, globalObject, string, captureStart, captureLen);
                                RETURN_IF_EXCEPTION(scope, nullptr);
                            }
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), captureValue);
                        } else
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), jsUndefined());
                    }
                }
            }

            cachedCall->appendArgument(jsNumber(result.start));
            cachedCall->appendArgument(string);
            if (hasNamedCaptures)
                cachedCall->appendArgument(groups);

            cachedCall->setThis(jsUndefined());
            if (cachedCall->hasOverflowedArguments()) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }

            JSValue jsResult = cachedCall->call();
            RETURN_IF_EXCEPTION(scope, nullptr);
            replacements.append(jsResult.toWTFString(globalObject));
            RETURN_IF_EXCEPTION(scope, nullptr);

            lastIndex = result.end;
            startPosition = lastIndex;

            // special case of empty match
            if (result.empty()) {
                startPosition++;
                if (startPosition > sourceLen)
                    break;
                if (U16_IS_LEAD(source[startPosition - 1]) && U16_IS_TRAIL(source[startPosition])) {
                    startPosition++;
                    if (startPosition > sourceLen)
                        break;
                }
            }
        }
    } else {
        ASSERT(callData.type != CallData::Type::None);
        do {
            int* ovector;
            MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition, &ovector);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!result)
                break;

            if (!sourceRanges.tryConstructAndAppend(lastIndex, result.start)) [[unlikely]]
                OUT_OF_MEMORY(globalObject, scope);

            MarkedArgumentBuffer args;
            JSObject* groups = hasNamedCaptures ? constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure()) : nullptr;

            for (unsigned i = 0; i < regExp->numSubpatterns() + 1; ++i) {
                int matchStart = ovector[i * 2];
                int matchLen = ovector[i * 2 + 1] - matchStart;

                JSValue patternValue;

                if (matchStart < 0)
                    patternValue = jsUndefined();
                else {
                    patternValue = jsSubstring(vm, globalObject, string, matchStart, matchLen);
                    RETURN_IF_EXCEPTION(scope, nullptr);
                }

                args.append(patternValue);

                if (i && hasNamedCaptures) {
                    String groupName = regExp->getCaptureGroupNameForSubpatternId(i);
                    if (!groupName.isEmpty()) {
                        auto captureIndex = regExp->subpatternIdForGroupName(groupName, ovector);

                        if (captureIndex == i)
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), patternValue);
                        else if (captureIndex > 0) {
                            int captureStart = ovector[captureIndex * 2];
                            int captureLen = ovector[captureIndex * 2 + 1] - captureStart;
                            JSValue captureValue;
                            if (captureStart < 0)
                                captureValue = jsUndefined();
                            else {
                                captureValue = jsSubstring(vm, globalObject, string, captureStart, captureLen);
                                RETURN_IF_EXCEPTION(scope, nullptr);
                            }
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), captureValue);
                        } else
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), jsUndefined());
                    }
                }
            }

            args.append(jsNumber(result.start));
            args.append(string);
            if (hasNamedCaptures)
                args.append(groups);
            if (args.hasOverflowed()) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }

            JSValue replacement = call(globalObject, replaceValue, callData, jsUndefined(), args);
            RETURN_IF_EXCEPTION(scope, nullptr);
            String replacementString = replacement.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            replacements.append(replacementString);
            RETURN_IF_EXCEPTION(scope, nullptr);

            lastIndex = result.end;
            startPosition = lastIndex;

            // special case of empty match
            if (result.empty()) {
                startPosition++;
                if (startPosition > sourceLen)
                    break;
                if (U16_IS_LEAD(source[startPosition - 1]) && U16_IS_TRAIL(source[startPosition])) {
                    startPosition++;
                    if (startPosition > sourceLen)
                        break;
                }
            }
        } while (global);
    }

    if (!lastIndex && replacements.isEmpty())
        return string;

    if (static_cast<unsigned>(lastIndex) < sourceLen) {
        if (!sourceRanges.tryConstructAndAppend(lastIndex, sourceLen)) [[unlikely]]
            OUT_OF_MEMORY(globalObject, scope);
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, string, source, sourceRanges.span().data(), sourceRanges.size(), replacements.span().data(), replacements.size()));
}

ALWAYS_INLINE JSString* replaceUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, JSString* string, JSValue searchValue, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    String replacementString;
    auto callData = JSC::getCallDataInline(replaceValue);
    if (callData.type == CallData::Type::None) {
        replacementString = replaceValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(
        vm, globalObject, string, searchValue, callData, replacementString, replaceValue));
}

inline bool checkObjectCoercible(JSValue thisValue)
{
    if (thisValue.isString())
        return true;

    if (thisValue.isUndefinedOrNull())
        return false;

    if (thisValue.isObject() && asObject(thisValue)->isEnvironment())
        return false;

    return true;
}

template<StringReplaceMode replaceMode>
ALWAYS_INLINE JSString* replace(VM& vm, JSGlobalObject* globalObject, JSValue thisValue, JSValue searchValue, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!checkObjectCoercible(thisValue)) [[unlikely]] {
        throwVMTypeError(globalObject, scope);
        return nullptr;
    }
    JSString* string = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSString* searchJSString = searchValue.isString() ? asString(searchValue) : nullptr;
    JSString* replaceJSString = replaceValue.isString() ? asString(replaceValue) : nullptr;

    if (searchValue.inherits<RegExpObject>())
        RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(vm, globalObject, string, searchValue, replaceValue));

    if constexpr (replaceMode == StringReplaceMode::Single) {
        if (searchJSString && replaceJSString) {
            if (JSString* result = tryReplaceOneCharUsingString<DollarCheck::Yes>(globalObject, string, searchJSString, replaceJSString))
                return result;
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    auto thisString = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // This path avoids an extra ref count churn for the most likely case that the search value is a string.
    if (searchJSString) [[likely]] {
        auto searchString = searchJSString->value(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);

        RELEASE_AND_RETURN(scope, replaceUsingStringSearch<replaceMode>(vm, globalObject, string, thisString, WTFMove(searchString), replaceValue));
    }

    String searchString = searchValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, replaceUsingStringSearch<replaceMode>(vm, globalObject, string, thisString, WTFMove(searchString), replaceValue));
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
