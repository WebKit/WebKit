/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MathVariant.h"

#if ENABLE(MATHML)

#include <wtf/SortedArrayMap.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

// Performs the character mapping needed to implement MathML's mathvariant attribute.
// It takes a unicode character and maps it to its appropriate mathvariant counterpart specified by mathvariant.
// The mapped character is typically located within Unicode's mathematical blocks (0x1D***, 0x1EE**) but there are exceptions which this function accounts for.
// Characters without a valid mapping or valid aMathvar value are returned
// unaltered.
// Characters already in the mathematical blocks (or are one of the exceptions) are never transformed.
// Acceptable values for mathvariant are specified in MathMLElement.h
// The transformable characters can be found at:
// http://lists.w3.org/Archives/Public/www-math/2013Sep/0012.html and
// https://en.wikipedia.org/wiki/Mathematical_Alphanumeric_Symbols
char32_t mathVariantMapCodePoint(char32_t codePoint, MathVariant mathvariant)
{
    // Lookup tables for use with mathvariant mappings to transform a unicode character point to another unicode character that indicates the proper output.
    // key represents one of two concepts.
    // 1. In the Latin table it represents a hole in the mathematical alphanumeric block, where the character that should occupy that position is located elsewhere.
    // 2. It represents an Arabic letter.
    //  As a replacement, 0 is reserved to indicate no mapping was found.
    static constexpr SortedArrayMap arabicInitialMapTable = { std::to_array<std::pair<char32_t, char32_t>>({
        { 0x628, 0x1EE21 },
        { 0x62A, 0x1EE35 },
        { 0x62B, 0x1EE36 },
        { 0x62C, 0x1EE22 },
        { 0x62D, 0x1EE27 },
        { 0x62E, 0x1EE37 },
        { 0x633, 0x1EE2E },
        { 0x634, 0x1EE34 },
        { 0x635, 0x1EE31 },
        { 0x636, 0x1EE39 },
        { 0x639, 0x1EE2F },
        { 0x63A, 0x1EE3B },
        { 0x641, 0x1EE30 },
        { 0x642, 0x1EE32 },
        { 0x643, 0x1EE2A },
        { 0x644, 0x1EE2B },
        { 0x645, 0x1EE2C },
        { 0x646, 0x1EE2D },
        { 0x647, 0x1EE24 },
        { 0x64A, 0x1EE29 }
    }) };

    static constexpr SortedArrayMap arabicTailedMapTable = { std::to_array<std::pair<char32_t, char32_t>>({
        { 0x62C, 0x1EE42 },
        { 0x62D, 0x1EE47 },
        { 0x62E, 0x1EE57 },
        { 0x633, 0x1EE4E },
        { 0x634, 0x1EE54 },
        { 0x635, 0x1EE51 },
        { 0x636, 0x1EE59 },
        { 0x639, 0x1EE4F },
        { 0x63A, 0x1EE5B },
        { 0x642, 0x1EE52 },
        { 0x644, 0x1EE4B },
        { 0x646, 0x1EE4D },
        { 0x64A, 0x1EE49 },
        { 0x66F, 0x1EE5F },
        { 0x6BA, 0x1EE5D }
    }) };

    static constexpr SortedArrayMap arabicStretchedMapTable = { std::to_array<std::pair<char32_t, char32_t>>({
        { 0x628, 0x1EE61 },
        { 0x62A, 0x1EE75 },
        { 0x62B, 0x1EE76 },
        { 0x62C, 0x1EE62 },
        { 0x62D, 0x1EE67 },
        { 0x62E, 0x1EE77 },
        { 0x633, 0x1EE6E },
        { 0x634, 0x1EE74 },
        { 0x635, 0x1EE71 },
        { 0x636, 0x1EE79 },
        { 0x637, 0x1EE68 },
        { 0x638, 0x1EE7A },
        { 0x639, 0x1EE6F },
        { 0x63A, 0x1EE7B },
        { 0x641, 0x1EE70 },
        { 0x642, 0x1EE72 },
        { 0x643, 0x1EE6A },
        { 0x645, 0x1EE6C },
        { 0x646, 0x1EE6D },
        { 0x647, 0x1EE64 },
        { 0x64A, 0x1EE69 },
        { 0x66E, 0x1EE7C },
        { 0x6A1, 0x1EE7E }
    }) };

    static constexpr SortedArrayMap arabicLoopedMapTable = { std::to_array<std::pair<char32_t, char32_t>>({
        { 0x627, 0x1EE80 },
        { 0x628, 0x1EE81 },
        { 0x62A, 0x1EE95 },
        { 0x62B, 0x1EE96 },
        { 0x62C, 0x1EE82 },
        { 0x62D, 0x1EE87 },
        { 0x62E, 0x1EE97 },
        { 0x62F, 0x1EE83 },
        { 0x630, 0x1EE98 },
        { 0x631, 0x1EE93 },
        { 0x632, 0x1EE86 },
        { 0x633, 0x1EE8E },
        { 0x634, 0x1EE94 },
        { 0x635, 0x1EE91 },
        { 0x636, 0x1EE99 },
        { 0x637, 0x1EE88 },
        { 0x638, 0x1EE9A },
        { 0x639, 0x1EE8F },
        { 0x63A, 0x1EE9B },
        { 0x641, 0x1EE90 },
        { 0x642, 0x1EE92 },
        { 0x644, 0x1EE8B },
        { 0x645, 0x1EE8C },
        { 0x646, 0x1EE8D },
        { 0x647, 0x1EE84 },
        { 0x648, 0x1EE85 },
        { 0x64A, 0x1EE89 }
    }) };

    static constexpr SortedArrayMap arabicDoubleMapTable = { std::to_array<std::pair<char32_t, char32_t>>({
        { 0x628, 0x1EEA1 },
        { 0x62A, 0x1EEB5 },
        { 0x62B, 0x1EEB6 },
        { 0x62C, 0x1EEA2 },
        { 0x62D, 0x1EEA7 },
        { 0x62E, 0x1EEB7 },
        { 0x62F, 0x1EEA3 },
        { 0x630, 0x1EEB8 },
        { 0x631, 0x1EEB3 },
        { 0x632, 0x1EEA6 },
        { 0x633, 0x1EEAE },
        { 0x634, 0x1EEB4 },
        { 0x635, 0x1EEB1 },
        { 0x636, 0x1EEB9 },
        { 0x637, 0x1EEA8 },
        { 0x638, 0x1EEBA },
        { 0x639, 0x1EEAF },
        { 0x63A, 0x1EEBB },
        { 0x641, 0x1EEB0 },
        { 0x642, 0x1EEB2 },
        { 0x644, 0x1EEAB },
        { 0x645, 0x1EEAC },
        { 0x646, 0x1EEAD },
        { 0x648, 0x1EEA5 },
        { 0x64A, 0x1EEA9 }
    }) };

    static constexpr SortedArrayMap latinExceptionMapTable = { std::to_array<std::pair<char32_t, char32_t>>({
        { 0x1D455, 0x210E },
        { 0x1D49D, 0x212C },
        { 0x1D4A0, 0x2130 },
        { 0x1D4A1, 0x2131 },
        { 0x1D4A3, 0x210B },
        { 0x1D4A4, 0x2110 },
        { 0x1D4A7, 0x2112 },
        { 0x1D4A8, 0x2133 },
        { 0x1D4AD, 0x211B },
        { 0x1D4BA, 0x212F },
        { 0x1D4BC, 0x210A },
        { 0x1D4C4, 0x2134 },
        { 0x1D506, 0x212D },
        { 0x1D50B, 0x210C },
        { 0x1D50C, 0x2111 },
        { 0x1D515, 0x211C },
        { 0x1D51D, 0x2128 },
        { 0x1D53A, 0x2102 },
        { 0x1D53F, 0x210D },
        { 0x1D545, 0x2115 },
        { 0x1D547, 0x2119 },
        { 0x1D548, 0x211A },
        { 0x1D549, 0x211D },
        { 0x1D551, 0x2124 }
    }) };

    constexpr char16_t greekUpperTheta = 0x03F4;
    constexpr char16_t holeGreekUpperTheta = 0x03A2;
    constexpr char16_t nabla = 0x2207;
    constexpr char16_t partialDifferential = 0x2202;
    constexpr char16_t greekUpperAlpha = 0x0391;
    constexpr char16_t greekUpperOmega = 0x03A9;
    constexpr char16_t greekLowerAlpha = 0x03B1;
    constexpr char16_t greekLowerOmega = 0x03C9;
    constexpr char16_t greekLunateEpsilonSymbol = 0x03F5;
    constexpr char16_t greekThetaSymbol = 0x03D1;
    constexpr char16_t greekKappaSymbol = 0x03F0;
    constexpr char16_t greekPhiSymbol = 0x03D5;
    constexpr char16_t greekRhoSymbol = 0x03F1;
    constexpr char16_t greekPiSymbol = 0x03D6;
    constexpr char16_t greekLetterDigamma = 0x03DC;
    constexpr char32_t greekSmallLetterDigamma = 0x03DD;
    constexpr char32_t mathBoldCapitalDigamma = 0x1D7CA;
    constexpr char32_t mathBoldSmallDigamma = 0x1D7CB;

    constexpr char16_t latinSmallLetterDotlessI = 0x0131;
    constexpr char16_t latinSmallLetterDotlessJ = 0x0237;

    constexpr char32_t mathItalicSmallDotlessI = 0x1D6A4;
    constexpr char32_t mathItalicSmallDotlessJ = 0x1D6A5;

    constexpr char32_t mathBoldUpperA = 0x1D400;
    constexpr char32_t mathItalicUpperA = 0x1D434;
    constexpr char32_t mathBoldSmallA = 0x1D41A;
    constexpr char32_t mathBoldUpperAlpha = 0x1D6A8;
    constexpr char32_t mathBoldSmallAlpha = 0x1D6C2;
    constexpr char32_t mathItalicUpperAlpha = 0x1D6E2;
    constexpr char32_t mathBoldDigitZero = 0x1D7CE;
    constexpr char32_t mathDoubleStruckZero = 0x1D7D8;

    constexpr char32_t mathBoldUpperTheta = 0x1D6B9;
    constexpr char32_t mathBoldNabla = 0x1D6C1;
    constexpr char32_t mathBoldPartialDifferential = 0x1D6DB;
    constexpr char32_t mathBoldEpsilonSymbol = 0x1D6DC;
    constexpr char32_t mathBoldThetaSymbol = 0x1D6DD;
    constexpr char32_t mathBoldKappaSymbol = 0x1D6DE;
    constexpr char32_t mathBoldPhiSymbol = 0x1D6DF;
    constexpr char32_t mathBoldRhoSymbol = 0x1D6E0;
    constexpr char32_t mathBoldPiSymbol = 0x1D6E1;

    ASSERT(mathvariant >= MathVariant::Normal && mathvariant <= MathVariant::Stretched);

    if (mathvariant == MathVariant::Normal)
        return codePoint; // Nothing to do here.

    // Exceptional characters with at most one possible transformation.
    switch (codePoint) {
    case holeGreekUpperTheta:
        return codePoint; // Nothing at this code point is transformed
    case greekLetterDigamma:
        if (mathvariant == MathVariant::Bold)
            return mathBoldCapitalDigamma;
        return codePoint;
    case greekSmallLetterDigamma:
        if (mathvariant == MathVariant::Bold)
            return mathBoldSmallDigamma;
        return codePoint;
    case latinSmallLetterDotlessI:
        if (mathvariant == MathVariant::Italic)
            return mathItalicSmallDotlessI;
        return codePoint;
    case latinSmallLetterDotlessJ:
        if (mathvariant == MathVariant::Italic)
            return mathItalicSmallDotlessJ;
        return codePoint;
    }

    // The Unicode mathematical blocks are divided into four segments: Latin, Greek, numbers and Arabic.
    // In the case of the first three baseChar represents the relative order in which the characters are encoded in the Unicode mathematical block, normalised to the first character of that sequence.
    char32_t baseChar = 0;
    enum CharacterType {
        Latin,
        Greekish,
        Number,
        Arabic
    };
    CharacterType varType;
    if (isASCIIUpper(codePoint)) {
        baseChar = codePoint - 'A';
        varType = Latin;
    } else if (isASCIILower(codePoint)) {
        // Lowercase characters are placed immediately after the uppercase characters in the Unicode mathematical block.
        // The constant subtraction represents the number of characters between the start of the sequence (capital A) and the first lowercase letter.
        baseChar = mathBoldSmallA - mathBoldUpperA + codePoint - 'a';
        varType = Latin;
    } else if (isASCIIDigit(codePoint)) {
        baseChar = codePoint - '0';
        varType = Number;
    } else if (greekUpperAlpha <= codePoint && codePoint <= greekUpperOmega) {
        baseChar = codePoint - greekUpperAlpha;
        varType = Greekish;
    } else if (greekLowerAlpha <= codePoint && codePoint <= greekLowerOmega) {
        // Lowercase Greek comes after uppercase Greek.
        // Note in this instance the presence of an additional character (Nabla) between the end of the uppercase Greek characters and the lowercase ones.
        baseChar = mathBoldSmallAlpha - mathBoldUpperAlpha + codePoint - greekLowerAlpha;
        varType = Greekish;
    } else if (0x0600 <= codePoint && codePoint <= 0x06FF) {
        // Arabic characters are defined within this range
        varType = Arabic;
    } else {
        switch (codePoint) {
        case greekUpperTheta:
            baseChar = mathBoldUpperTheta - mathBoldUpperAlpha;
            break;
        case nabla:
            baseChar = mathBoldNabla - mathBoldUpperAlpha;
            break;
        case partialDifferential:
            baseChar = mathBoldPartialDifferential - mathBoldUpperAlpha;
            break;
        case greekLunateEpsilonSymbol:
            baseChar = mathBoldEpsilonSymbol - mathBoldUpperAlpha;
            break;
        case greekThetaSymbol:
            baseChar = mathBoldThetaSymbol - mathBoldUpperAlpha;
            break;
        case greekKappaSymbol:
            baseChar = mathBoldKappaSymbol - mathBoldUpperAlpha;
            break;
        case greekPhiSymbol:
            baseChar = mathBoldPhiSymbol - mathBoldUpperAlpha;
            break;
        case greekRhoSymbol:
            baseChar = mathBoldRhoSymbol - mathBoldUpperAlpha;
            break;
        case greekPiSymbol:
            baseChar = mathBoldPiSymbol - mathBoldUpperAlpha;
            break;
        default:
            return codePoint;
        }
        varType = Greekish;
    }

    int8_t multiplier;
    if (varType == Number) {
        // Each possible number mathvariant is encoded in a single, contiguous block.
        // For example the beginning of the double struck number range follows immediately after the end of the bold number range.
        // multiplier represents the order of the sequences relative to the first one.
        switch (mathvariant) {
        case MathVariant::Bold:
            multiplier = 0;
            break;
        case MathVariant::DoubleStruck:
            multiplier = 1;
            break;
        case MathVariant::SansSerif:
            multiplier = 2;
            break;
        case MathVariant::BoldSansSerif:
            multiplier = 3;
            break;
        case MathVariant::Monospace:
            multiplier = 4;
            break;
        default:
            // This mathvariant isn't defined for numbers or is otherwise normal.
            return codePoint;
        }
        // As the ranges are contiguous, to find the desired mathvariant range it is sufficient to
        // multiply the position within the sequence order (multiplier) with the period of the sequence (which is constant for all number sequences)
        // and to add the character point of the first character within the number mathvariant range.
        // To this the baseChar calculated earlier is added to obtain the final code point.
        return baseChar + multiplier * (mathDoubleStruckZero - mathBoldDigitZero) + mathBoldDigitZero;
    }
    if (varType == Greekish) {
        switch (mathvariant) {
        case MathVariant::Bold:
            multiplier = 0;
            break;
        case MathVariant::Italic:
            multiplier = 1;
            break;
        case MathVariant::BoldItalic:
            multiplier = 2;
            break;
        case MathVariant::BoldSansSerif:
            multiplier = 3;
            break;
        case MathVariant::SansSerifBoldItalic:
            multiplier = 4;
            break;
        default:
            // This mathvariant isn't defined for Greek or is otherwise normal.
            return codePoint;
        }
        // See the Number case for an explanation of the following calculation.
        return baseChar + mathBoldUpperAlpha + multiplier * (mathItalicUpperAlpha - mathBoldUpperAlpha);
    }

    char32_t latinChar = 0;
    char32_t newChar;
    if (varType == Arabic) {
        // The Arabic mathematical block is not continuous, nor does it have a monotonic mapping to the unencoded characters, requiring the use of a lookup table.
        switch (mathvariant) {
        case MathVariant::Initial:
            newChar = arabicInitialMapTable.get(codePoint);
            break;
        case MathVariant::Tailed:
            newChar = arabicTailedMapTable.get(codePoint);
            break;
        case MathVariant::Stretched:
            newChar = arabicStretchedMapTable.get(codePoint);
            break;
        case MathVariant::Looped:
            newChar = arabicLoopedMapTable.get(codePoint);
            break;
        case MathVariant::DoubleStruck:
            newChar = arabicDoubleMapTable.get(codePoint);
            break;
        default:
            return codePoint; // No valid transformations exist.
        }
    } else {
        // Must be Latin
        if (mathvariant > MathVariant::Monospace)
            return codePoint; // Latin doesn't support the Arabic mathvariants
        multiplier = static_cast<int>(mathvariant) - 2;
        // This is possible because the values for NS_MATHML_MATHVARIANT_* are chosen to coincide with the order in which the encoded mathvariant characters are located within their unicode block (less an offset to avoid None and Normal variants)
        // See the Number case for an explanation of the following calculation
        latinChar = baseChar + mathBoldUpperA + multiplier * (mathItalicUpperA - mathBoldUpperA);
        // There are roughly twenty characters that are located outside of the mathematical block, so the spaces where they ought to be are used as keys for a lookup table containing the correct character mappings.
        newChar = latinExceptionMapTable.get(latinChar);
    }

    if (newChar)
        return newChar;
    if (varType == Latin)
        return latinChar;
    return codePoint; // This is an Arabic character without a corresponding mapping.
}

} // namespace WebCore

#endif // ENABLE(MATHML)
