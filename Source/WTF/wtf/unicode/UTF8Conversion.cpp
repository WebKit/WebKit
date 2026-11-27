/*
 * Copyright (C) 2007-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "config.h"
#include <wtf/unicode/UTF8Conversion.h>

#include <unicode/uchar.h>
#include <wtf/SIMDUTF.h>
#include <wtf/text/icu/UnicodeExtras.h>
#include <wtf/unicode/CharacterNames.h>

namespace WTF::Unicode {

static constexpr char32_t sentinelCodePoint = U_SENTINEL;

enum class Replacement : bool { None, ReplaceInvalidSequences };

template<Replacement = Replacement::None, typename CharacterType> static char32_t next(std::span<const CharacterType>, size_t& offset);
template<typename CharacterType> static bool append(std::span<CharacterType>, size_t& offset, char32_t character);

template<> NODELETE char32_t next<Replacement::None, Latin1Character>(std::span<const Latin1Character> characters, size_t& offset)
{
    return characters[offset++];
}

template<> char32_t next<Replacement::None, char8_t>(std::span<const char8_t> characters, size_t& offset)
{
    char32_t character;
    U8_NEXT_SPAN(characters, offset, character);
    return U_IS_SURROGATE(character) ? sentinelCodePoint : character;
}

template<> char32_t next<Replacement::ReplaceInvalidSequences, char8_t>(std::span<const char8_t> characters, size_t& offset)
{
    char32_t character;
    U8_NEXT_OR_FFFD_SPAN(characters, offset, character);
    return character;
}

template<> NODELETE char32_t next<Replacement::None, char16_t>(std::span<const char16_t> characters, size_t& offset)
{
    char32_t character;
    U16_NEXT(characters, offset, characters.size(), character);
    return U_IS_SURROGATE(character) ? sentinelCodePoint : character;
}

template<> NODELETE bool append<char8_t>(std::span<char8_t> characters, size_t& offset, char32_t character)
{
    UBool sawError = false;
    U8_APPEND(characters, offset, characters.size(), character, sawError);
    return sawError;
}

template<> NODELETE bool append<char16_t>(std::span<char16_t> characters, size_t& offset, char32_t character)
{
    UBool sawError = false;
    U16_APPEND(characters, offset, characters.size(), character, sawError);
    return sawError;
}

template<Replacement replacement = Replacement::None, typename SourceCharacterType, typename BufferCharacterType> static ConversionResult<BufferCharacterType> convertInternal(std::span<const SourceCharacterType> source, std::span<BufferCharacterType> buffer)
{
    auto resultCode = ConversionResultCode::Success;
    size_t sourceOffset = 0;
    size_t bufferOffset = 0;
    while (sourceOffset < source.size()) {
        size_t nextSourceOffset = sourceOffset;
        char32_t character = next<replacement>(source, nextSourceOffset);
        if (character == sentinelCodePoint) {
            resultCode = ConversionResultCode::SourceInvalid;
            break;
        }
        if (bufferOffset == buffer.size()) {
            resultCode = ConversionResultCode::TargetExhausted;
            break;
        }
        bool sawError = append(buffer, bufferOffset, character);
        if (sawError) {
            resultCode = ConversionResultCode::TargetExhausted;
            break;
        }
        sourceOffset = nextSourceOffset;
    }
    return { resultCode, buffer.first(bufferOffset), sourceOffset };
}

ConversionResult<char8_t> convert(std::span<const char16_t> source, std::span<char8_t> buffer)
{
    size_t requiredLength = simdutf::utf8_length_from_utf16le(source);

    if (buffer.size() < requiredLength)
        return convertInternal(source, buffer);

    auto result = simdutf::convert_utf16le_to_utf8_with_errors(source, buffer);

    if (result.error == simdutf::error_code::SUCCESS)
        return { ConversionResultCode::Success, buffer.first(result.count), source.size() };

    return convertInternal(source, buffer);
}

ConversionResult<char16_t> convert(std::span<const char8_t> source, std::span<char16_t> buffer)
{
    return convertInternal(source, buffer);
}

SUPPRESS_NODELETE ConversionResult<char8_t> convert(std::span<const Latin1Character> source, std::span<char8_t> buffer)
{
    // Each Latin-1 character takes at most two bytes in UTF-8.
    if (buffer.size() / 2 >= source.size()) {
        size_t length = simdutf::convert_latin1_to_utf8(byteCast<char>(source), buffer);
        return { ConversionResultCode::Success, buffer.first(length), source.size() };
    }

    auto result = simdutf::convert_latin1_to_utf8_safe_with_details(byteCast<char>(source), buffer);
    auto code = result.input_count == source.size() ? ConversionResultCode::Success : ConversionResultCode::TargetExhausted;
    return { code, buffer.first(result.output_count), result.input_count };
}

SUPPRESS_NODELETE ConversionResult<char8_t> convertReplacingUnpairedSurrogates(std::span<const char16_t> source, std::span<char8_t> buffer)
{
    auto result = simdutf::convert_utf16_to_utf8_with_replacement_safe(source, buffer);
    auto code = result.input_count == source.size() ? ConversionResultCode::Success : ConversionResultCode::TargetExhausted;
    return { code, buffer.first(result.output_count), result.input_count };
}

ConversionResult<char16_t> convertReplacingInvalidSequences(std::span<const char8_t> source, std::span<char16_t> buffer)
{
    return convertInternal<Replacement::ReplaceInvalidSequences>(source, buffer);
}

size_t lengthUTF8(std::span<const Latin1Character> source)
{
    return simdutf::utf8_length_from_latin1(byteCast<uint8_t>(source));
}

size_t lengthUTF8(std::span<const char16_t> source)
{
    return simdutf::utf8_length_from_utf16le_with_replacement(source).count;
}

size_t lengthUTF8AssumingWellFormed(std::span<const char16_t> source)
{
    return simdutf::utf8_length_from_utf16le(source);
}

// simdutf only reads the source and writes to the buffer, but the static analyzer cannot follow its dispatch.
SUPPRESS_NODELETE void convertIntoBuffer(std::span<const Latin1Character> source, std::span<char8_t> buffer)
{
    ASSERT(buffer.size() == lengthUTF8(source));
    auto written = simdutf::convert_latin1_to_utf8(byteCast<char>(source), buffer);
    ASSERT_UNUSED(written, written == buffer.size());
}

SUPPRESS_NODELETE bool convertWellFormedIntoBuffer(std::span<const char16_t> source, std::span<char8_t> buffer)
{
    ASSERT(buffer.size() >= lengthUTF8AssumingWellFormed(source));
    return simdutf::convert_utf16le_to_utf8_with_errors(source, buffer).error == simdutf::error_code::SUCCESS;
}

SUPPRESS_NODELETE void convertReplacingUnpairedSurrogatesIntoBuffer(std::span<const char16_t> source, std::span<char8_t> buffer)
{
    ASSERT_WITH_SECURITY_IMPLICATION(buffer.size() == lengthUTF8(source));
    auto written = simdutf::convert_utf16le_to_utf8_with_replacement(source, buffer);
    ASSERT_UNUSED(written, written == buffer.size());
}

std::span<const char8_t> checkUTF8WithoutUTF16Length(std::span<const char8_t> source)
{
    auto result = simdutf::validate_utf8_with_errors(source);
    size_t validLength = result.error == simdutf::error_code::SUCCESS ? source.size() : result.count;
    return source.first(validLength);
}

CheckedUTF8 checkUTF8(std::span<const char8_t> source)
{
    auto result = simdutf::validate_utf8_with_errors(source);
    size_t validLength = result.error == simdutf::error_code::SUCCESS ? source.size() : result.count;
    auto validSpan = source.first(validLength);
    size_t lengthUTF16 = simdutf::utf16_length_from_utf8(validSpan);
    return { validSpan, lengthUTF16, validLength == lengthUTF16 };
}

template<typename CharacterTypeA, typename CharacterTypeB>
NODELETE SUPPRESS_NODELETE bool equalInternal(std::span<CharacterTypeA> a, std::span<CharacterTypeB> b)
{
    size_t offsetA = 0;
    size_t offsetB = 0;
    while (offsetA < a.size() && offsetB < b.size()) {
        auto characterA = next(a, offsetA);
        // sentinelCodePoint is U_SENTINEL (not U+FFFD), meaning decoding failed
        // without producing any code point. Two unrelated decoding failures
        // should not compare as equal.
        if (characterA == sentinelCodePoint || characterA != next(b, offsetB))
            return false;
    }
    return offsetA == a.size() && offsetB == b.size();
}

bool equal(std::span<const char16_t> a, std::span<const char8_t> b)
{
    return equalInternal(a, b);
}

bool equal(std::span<const Latin1Character> a, std::span<const char8_t> b)
{
    return equalInternal(a, b);
}

} // namespace WTF::Unicode
