/*
 * Copyright (C) 2007-2024 Apple Inc. All rights reserved.
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

#include <wtf/text/LChar.h>

namespace WTF {
namespace Unicode {

enum class ConversionResultCode : uint8_t {
    Success, // conversion successful
    SourceInvalid, // source sequence is invalid/malformed
    TargetExhausted // insufficient room in target for conversion
};

template<typename CharacterType> struct ConversionResult {
    ConversionResultCode code { };
    std::span<CharacterType> buffer { };
    bool isAllASCII { };
};

WTF_EXPORT_PRIVATE ConversionResult<char16_t> convert(std::span<const char8_t>, std::span<char16_t>);
WTF_EXPORT_PRIVATE ConversionResult<char8_t> convert(std::span<const char16_t>, std::span<char8_t>);
WTF_EXPORT_PRIVATE ConversionResult<char8_t> convert(std::span<const LChar>, std::span<char8_t>);

// Invalid sequences are converted to the replacement character.
WTF_EXPORT_PRIVATE ConversionResult<char16_t> convertReplacingInvalidSequences(std::span<const char8_t>, std::span<char16_t>);
WTF_EXPORT_PRIVATE ConversionResult<char8_t> convertReplacingInvalidSequences(std::span<const char16_t>, std::span<char8_t>);

WTF_EXPORT_PRIVATE bool equal(std::span<const char16_t>, std::span<const char8_t>);
WTF_EXPORT_PRIVATE bool equal(std::span<const LChar>, std::span<const char8_t>);

// The checkUTF8 function checks the UTF-8 and returns a span of the valid complete
// UTF-8 sequences at the start of the passed-in characters, along with the UTF-16
// length and an indication if all were ASCII.
struct CheckedUTF8 {
    std::span<const char8_t> characters { };
    size_t lengthUTF16 { };
    bool isAllASCII { };
};
WTF_EXPORT_PRIVATE CheckedUTF8 checkUTF8(std::span<const char8_t>);

// The computeUTF16LengthWithHash function returns a length and hash of 0 if the
// source is exhausted or invalid. The hash is of the computeHashAndMaskTop8Bits variety.
struct UTF16LengthWithHash {
    size_t lengthUTF16 { };
    unsigned hash { };
};
WTF_EXPORT_PRIVATE UTF16LengthWithHash computeUTF16LengthWithHash(std::span<const char8_t>);

} // namespace Unicode
} // namespace WTF
