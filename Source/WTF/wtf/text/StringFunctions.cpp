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
#include <wtf/text/StringFunctions.h>

#include <wtf/ASCIICType.h>
#include <wtf/Borrow.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DataLog.h>
#include <wtf/Function.h>
#include <wtf/StdLibExtras.h>
#include <wtf/dtoa.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/UTF8Conversion.h>

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

#ifndef NDEBUG
// Defined in WTFString.cpp; for use in the debugger.
Vector<char> asciiDebug(WTF::StringImpl*);
#endif

namespace WTF {

// StringImpl factories with the semantics of the String static functions of the same names.

RefPtr<StringImpl> StringImpl::fromUTF8ReplacingInvalidSequences(std::span<const char8_t> string)
{
    if (!string.data())
        return nullptr;

    RELEASE_ASSERT(string.size() <= MaxLength);

    if (string.empty())
        return empty();

    if (charactersAreAllASCII(string))
        return create(byteCast<Latin1Character>(string));

    Vector<char16_t, 1024> buffer(string.size());

    auto result = Unicode::convertReplacingInvalidSequences(string, borrow(buffer)->mutableSpan());
    if (result.code != Unicode::ConversionResultCode::Success)
        return nullptr;

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(result.buffer.size() <= string.size());
    return create(result.buffer);
}

RefPtr<StringImpl> StringImpl::fromUTF8WithLatin1Fallback(std::span<const char8_t> string)
{
    if (RefPtr utf8 = create(string))
        return utf8;
    // Do this assertion before chopping the size_t down to unsigned.
    RELEASE_ASSERT(string.size() <= MaxLength);
    auto latin1 = byteCast<Latin1Character>(string);
    if (!latin1.data())
        return nullptr;
    return create(latin1);
}

RefPtr<StringImpl> StringImpl::fromCodePoint(char32_t codePoint)
{
    std::array<char16_t, 2> buffer;
    uint8_t length = 0;
    UBool error = false;
    U16_APPEND(buffer, length, 2, codePoint, error);
    if (error)
        return nullptr;
    return create(std::span { buffer }.first(length));
}

RefPtr<StringImpl> StringImpl::make8Bit(std::span<const char16_t> source)
{
    std::span<Latin1Character> destination;
    auto result = createUninitialized(source.size(), destination);
    copyCharacters(destination, source);
    return result;
}

RefPtr<StringImpl> StringImpl::numberToStringFixedPrecision(float number, unsigned precision, TrailingZerosPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    return create(numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TrailingZerosPolicy::Truncate));
}

RefPtr<StringImpl> StringImpl::numberToStringFixedPrecision(double number, unsigned precision, TrailingZerosPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    return create(numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TrailingZerosPolicy::Truncate));
}

RefPtr<StringImpl> StringImpl::numberToStringFixedWidth(double number, unsigned decimalPlaces)
{
    NumberToStringBuffer buffer;
    return create(numberToFixedWidthString(number, decimalPlaces, buffer));
}

RefPtr<StringImpl> makeStringByJoining(std::span<const RefPtr<StringImpl>> strings, StringImpl* separator)
{
    CheckedInt32 totalLength = 0;
    bool is8Bit = WTF::is8Bit(separator);
    for (size_t i = 0; i < strings.size(); ++i) {
        if (i)
            totalLength += WTF::length(separator);
        totalLength += WTF::length(strings[i]);
        is8Bit = is8Bit && WTF::is8Bit(strings[i]);
    }
    if (totalLength.hasOverflowed())
        CRASH();

    auto append = [&](auto& destination, StringImpl* string) {
        if (!string)
            return;
        StringView { string }.getCharacters(destination);
        skip(destination, string->length());
    };

    auto join = [&](auto destination) {
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i)
                append(destination, separator);
            append(destination, strings[i].get());
        }
    };

    if (is8Bit) {
        std::span<Latin1Character> buffer;
        RefPtr result = StringImpl::tryCreateUninitialized(totalLength.value(), buffer);
        if (!result)
            CRASH();
        join(buffer);
        return result;
    }

    std::span<char16_t> buffer;
    RefPtr result = StringImpl::tryCreateUninitialized(totalLength.value(), buffer);
    if (!result)
        CRASH();
    join(buffer);
    return result;
}

namespace Detail {

UTF8CString utf8(StringImpl* string, ConversionMode mode)
{
    auto expectedString = WTF::tryGetUTF8(string, mode);
    RELEASE_ASSERT(expectedString);
    return expectedString.value();
}

Latin1CString latin1(StringImpl* string)
{
    // Basic Latin1 (ISO) encoding - Unicode characters 0..255 are
    // preserved, characters outside of this range are converted to '?'.

    if (isEmpty(string))
        return Latin1CString { ""_span8 };

    if (string->is8Bit())
        return Latin1CString { string->span8() };

    auto characters = string->span16();
    std::span<Latin1Character> characterBuffer;
    auto result = Latin1CString::newUninitialized(characters.size(), characterBuffer);

    size_t characterBufferIndex = 0;
    for (auto character : characters)
        characterBuffer[characterBufferIndex++] = !isLatin1(character) ? Latin1Character { '?' } : static_cast<Latin1Character>(character);

    return result;
}

std::expected<Vector<char16_t>, UTF8ConversionError> charactersWithoutNullTermination(StringImpl* string)
{
    Vector<char16_t> result;
    if (!string)
        return result;

    if (!result.tryReserveInitialCapacity(string->length() + 1))
        return makeUnexpected(UTF8ConversionError::OutOfMemory);

    if (string->is8Bit())
        result.append(string->span8());
    else
        result.append(string->span16());

    return result;
}

double toDouble(StringImpl* string, bool* ok)
{
    if (!string) {
        if (ok)
            *ok = false;
        return 0.0;
    }
    return string->toDouble(ok);
}

float toFloat(StringImpl* string, bool* ok)
{
    if (!string) {
        if (ok)
            *ok = false;
        return 0.0f;
    }
    return string->toFloat(ok);
}

RefPtr<StringImpl> foldCase(StringImpl* string)
{
    if (!string)
        return nullptr;
    return string->foldCase();
}

void split(StringImpl* string, char16_t separator, NOESCAPE const Function<void(StringView)>& functor, bool allowEmptyEntries)
{
    StringView view(string);

    unsigned startPos = 0;
    size_t endPos;
    while ((endPos = find(string, separator, startPos)) != notFound) {
        if (allowEmptyEntries || startPos != endPos)
            functor(view.substring(startPos, endPos - startPos));
        startPos = endPos + 1;
    }
    if (allowEmptyEntries || startPos != length(string))
        functor(view.substring(startPos));
}

Vector<RefPtr<StringImpl>> split(StringImpl* string, char16_t separator, bool allowEmptyEntries)
{
    Vector<RefPtr<StringImpl>> result;
    split(string, separator, [&result](StringView item) {
        result.append(item.toString());
    }, allowEmptyEntries);
    return result;
}

Vector<RefPtr<StringImpl>> split(StringImpl* string, StringView separator, bool allowEmptyEntries)
{
    Vector<RefPtr<StringImpl>> result;

    unsigned startPos = 0;
    size_t endPos;
    while ((endPos = find(string, separator, startPos)) != notFound) {
        if (allowEmptyEntries || startPos != endPos)
            result.append(substring(string, startPos, endPos - startPos));
        startPos = endPos + separator.length();
    }
    if (allowEmptyEntries || startPos != length(string))
        result.append(substring(string, startPos));

    return result;
}

RefPtr<StringImpl> makeStringByRemoving(StringImpl* string, unsigned position, unsigned lengthToRemove)
{
    if (!lengthToRemove)
        return string;
    auto length = WTF::length(string);
    if (position >= length)
        return string;
    lengthToRemove = std::min(lengthToRemove, length - position);
    StringView view { string };
    auto left = view.left(position);
    auto right = view.substring(position + lengthToRemove);
    RefPtr result = tryMakeStringImplFromAdaptersInternal(left.length() + right.length(), left.is8Bit() && right.is8Bit(), StringTypeAdapter<StringView> { left }, StringTypeAdapter<StringView> { right });
    if (!result)
        CRASH();
    return result;
}

template<bool isLowercase>
static ALWAYS_INLINE RefPtr<AtomStringImpl> convertAtomToASCIICase(AtomStringImpl* atom)
{
    StringImpl* impl = atom;
    if (!impl) [[unlikely]]
        return nullptr;

    // Convert short strings without allocating a new StringImpl, since
    // there's a good chance these strings are already in the atom
    // string table and so no memory allocation will be required.
    unsigned length;
    const unsigned localBufferSize = 100;
    if (impl->is8Bit() && (length = impl->length()) <= localBufferSize) {
        auto characters = impl->span8();
        unsigned failingIndex;
        for (unsigned i = 0; i < length; ++i) {
            if constexpr (isLowercase) {
                if (isASCIIUpper(characters[i])) [[unlikely]] {
                    failingIndex = i;
                    goto SlowPath;
                }
            } else {
                if (isASCIILower(characters[i])) [[likely]] {
                    failingIndex = i;
                    goto SlowPath;
                }
            }
        }
        return atom;
SlowPath:
        std::array<Latin1Character, localBufferSize> localBuffer;
        std::span<Latin1Character> localSpan { localBuffer };
        StringImpl::copyCharacters(localSpan, characters.first(failingIndex));
        for (unsigned i = failingIndex; i < length; ++i)
            localBuffer[i] = isLowercase ? toASCIILower(characters[i]) : toASCIIUpper(characters[i]);
        return AtomStringImpl::add(std::span<const Latin1Character> { localBuffer }.first(length));
    }

    Ref<StringImpl> convertedString = isLowercase ? impl->convertToASCIILowercase() : impl->convertToASCIIUppercase();
    if (convertedString.ptr() == impl) [[likely]]
        return atom;

    return AtomStringImpl::add(convertedString.ptr());
}

RefPtr<AtomStringImpl> convertToASCIILowercase(AtomStringImpl* atom)
{
    return convertAtomToASCIICase<true>(atom);
}

RefPtr<AtomStringImpl> convertToASCIIUppercase(AtomStringImpl* atom)
{
    return convertAtomToASCIICase<false>(atom);
}

#if USE(CF)
RetainPtr<CFStringRef> createCFString(StringImpl* string)
{
    if (RefPtr protectedString = string)
        return protectedString->createCFString();
    return CFSTR("");
}
#endif

#if OS(WINDOWS)
Vector<wchar_t> wideCharacters(StringImpl* string)
{
    Vector<wchar_t> result;
    if (!string)
        return result;

    result.reserveInitialCapacity(string->length() + 1);

    if (string->is8Bit())
        result.append(string->span8());
    else
        result.append(string->span16());

    result.append(0);

    return result;
}
#endif

#ifndef NDEBUG
void show(StringImpl* string)
{
    auto ascii = asciiDebug(string);
    auto span = ascii.span();
    dataLogF("%.*s\n", static_cast<int>(span.size()), span.data());
}
#endif

} // namespace Detail

} // namespace WTF
