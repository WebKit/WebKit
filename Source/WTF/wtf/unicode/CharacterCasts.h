/*
 * Copyright Red Hat
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

#include <unicode/utf16.h>

namespace WTF::Unicode {

// This only works if you're certain the input is in the Basic Multilingual Plane. If you need to
// handle arbitrary values, use U16_LEAD() and U16_TRAIL() instead.
inline constexpr char16_t castBMPUTF32CodeUnitToUTF16(char32_t c)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(U_IS_BMP(c));
    return static_cast<char16_t>(c);
}

// FIXME: Code using this is probably broken. Remove it.
inline constexpr char16_t deprecatedBrokenCastUTF32CodeUnitToUTF16IgnoringSurrogates(char32_t c)
{
    return static_cast<char16_t>(c);
}

// This only works if you're certain the input is not a surrogate. Be careful, because you're
// responsible for handling even *invalid* surrogates before calling this function, including
// unpaired surrogates or surrogate trail before the lead.
//
// To handle arbitrary values, which may be surrogates, use U16_GET_SUPPLEMENTARY(), or use
// u16Get from UnicodeExtras.h, or use SurrogatePairAwareTextIterator.
inline constexpr char32_t castNonSurrogateUTF16CodeUnitToUTF32(char16_t c)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(!U16_IS_SURROGATE(c));
    return static_cast<char32_t>(c);
}

// FIXME: Code using this is probably broken. Remove it.
inline constexpr char32_t deprecatedBrokenCastUTF16CodeUnitToUTF32IgnoringSurrogates(char16_t c)
{
    return static_cast<char32_t>(c);
}

// This only works if the input is ASCII (not Latin-1).
template <typename CharType>
constexpr char32_t castSingleByteCodeUnitToUTF32(CharType c)
{
    static_assert(sizeof(CharType) == 1);
    static_assert(!std::is_same_v<CharType, Latin1Character>);

    ASSERT_UNDER_CONSTEXPR_CONTEXT(U8_IS_SINGLE(c));
    return static_cast<char32_t>(c);
}

// FIXME: Code using this is probably broken for non-ASCII Latin-1 characters (0x80..0xff). Remove it.
constexpr char32_t deprecatedBrokenCastLatin1CharacterToUTF32WithoutConvertingToUnicode(Latin1Character c)
{
    return static_cast<char32_t>(c);
}

} // namespace WTF::Unicode

using WTF::Unicode::castBMPUTF32CodeUnitToUTF16;
using WTF::Unicode::deprecatedBrokenCastUTF32CodeUnitToUTF16IgnoringSurrogates;
using WTF::Unicode::castNonSurrogateUTF16CodeUnitToUTF32;
using WTF::Unicode::deprecatedBrokenCastUTF16CodeUnitToUTF32IgnoringSurrogates;
using WTF::Unicode::castSingleByteCodeUnitToUTF32;
using WTF::Unicode::deprecatedBrokenCastLatin1CharacterToUTF32WithoutConvertingToUnicode;
