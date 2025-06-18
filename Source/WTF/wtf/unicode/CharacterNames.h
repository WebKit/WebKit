/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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

#include <unicode/utypes.h>

namespace WTF::Unicode {

// Names here are taken from the Unicode standard.

// Most of these are char16_t constants, not char32_t, which makes them
// more convenient for WebCore code that mostly uses UTF-16.

constexpr char16_t HiraganaLetterSmallA = 0x3041;
constexpr char16_t activateArabicFormShaping = 0x206D;
constexpr char16_t activateSymmetricSwapping = 0x206B;
constexpr char32_t aegeanWordSeparatorDot = 0x10101;
constexpr char32_t aegeanWordSeparatorLine = 0x10100;
constexpr char16_t apostrophe = 0x0027;
constexpr char16_t arabicIndicPerMilleSign = 0x0609;
constexpr char16_t arabicIndicPerTenThousandSign = 0x060A;
constexpr char16_t arabicPercentSign = 0x066A;
constexpr char16_t blackCircle = 0x25CF;
constexpr char16_t blackDownPointingSmallTriangle = 0x25BE;
constexpr char16_t blackLeftPointingSmallTriangle = 0x25C2;
constexpr char16_t blackRightPointingSmallTriangle = 0x25B8;
constexpr char16_t blackSquare = 0x25A0;
constexpr char16_t blackUpPointingSmallTriangle = 0x25B4;
constexpr char16_t blackUpPointingTriangle = 0x25B2;
constexpr char16_t bullet = 0x2022;
constexpr char16_t bullseye = 0x25CE;
constexpr char16_t byteOrderMark = 0xFEFF;
constexpr char16_t carriageReturn = 0x000D;
constexpr char16_t cjkWater = 0x6C34;
constexpr char16_t combiningEnclosingKeycap = 0x20E3;
constexpr char16_t deleteCharacter = 0x007F;
constexpr char16_t doubleHighReversed9QuotationMark = 0x201F;
constexpr char16_t doubleLowReversed9QuotationMark = 0x2E42;
constexpr char16_t doublePrimeQuotationMark = 0x301E;
constexpr char16_t emSpace = 0x2003;
constexpr char16_t emojiVariationSelector = 0xFE0F; // Technical name is "VARIATION SELECTOR-16"
constexpr char16_t enDash = 0x2013;
constexpr char16_t ethiopicPrefaceColon = 0x1366;
constexpr char16_t ethiopicWordspace = 0x1361;
constexpr char16_t firstStrongIsolate = 0x2068;
constexpr char16_t fisheye = 0x25C9;
constexpr char16_t formFeed = 0x000C;
constexpr char16_t fullwidthAmpersand = 0xFF06;
constexpr char16_t fullwidthApostrophe = 0xFF07;
constexpr char16_t fullwidthCommercialAt = 0xFF20;
constexpr char16_t fullwidthNumberSign = 0xFF03;
constexpr char16_t fullwidthPercentSign = 0xFF05;
constexpr char16_t fullwidthQuotationMark = 0xFF02;
constexpr char16_t functionApplication = 0x2061;
constexpr char16_t halfwidthLeftCornerBracket = 0xFF62;
constexpr char16_t halfwidthRightCornerBracket = 0xFF63;
constexpr char16_t hebrewPunctuationGeresh = 0x05F3;
constexpr char16_t hebrewPunctuationGershayim = 0x05F4;
constexpr char16_t horizontalEllipsis = 0x2026;
constexpr char16_t hyphen = 0x2010;
constexpr char16_t hyphenMinus = 0x002D;
constexpr char16_t ideographicComma = 0x3001;
constexpr char16_t ideographicFullStop = 0x3002;
constexpr char16_t ideographicSpace = 0x3000;
constexpr char16_t inhibitArabicFormShaping = 0x206C;
constexpr char16_t inhibitSymmetricSwapping = 0x206A;
constexpr char16_t invisibleSeparator = 0x2063;
constexpr char16_t invisibleTimes = 0x2062;
constexpr char16_t leftCornerBracket = 0x300C;
constexpr char16_t leftDoubleQuotationMark = 0x201C;
constexpr char16_t leftLowDoubleQuotationMark = 0x201E;
constexpr char16_t leftLowSingleQuotationMark = 0x201A;
constexpr char16_t leftPointingDoubleAngleQuotationMark = 0x00AB;
constexpr char16_t leftSingleQuotationMark = 0x2018;
constexpr char16_t leftToRightEmbed = 0x202A;
constexpr char16_t leftToRightIsolate = 0x2066;
constexpr char16_t leftToRightMark = 0x200E;
constexpr char16_t leftToRightOverride = 0x202D;
constexpr char16_t leftWhiteCornerBracket = 0x300E;
constexpr char16_t lineSeparator = 0x2028;
constexpr char16_t lowDoublePrimeQuotationMark = 0x301F;
constexpr char16_t lowLine = 0x005F;
constexpr char16_t mediumShade = 0x2592;
constexpr char16_t minusSign = 0x2212;
constexpr char16_t multiplicationSign = 0x00D7;
constexpr char16_t narrowNoBreakSpace = 0x202F;
constexpr char16_t nationalDigitShapes = 0x206E;
constexpr char16_t newlineCharacter = 0x000A;
constexpr char16_t noBreakSpace = 0x00A0;
constexpr char16_t nominalDigitShapes = 0x206F;
constexpr char16_t nullCharacter = 0x0;
constexpr char16_t objectReplacementCharacter = 0xFFFC;
constexpr char16_t optionKey = 0x2325;
constexpr char16_t paragraphSeparator = 0x2029;
constexpr char16_t partAlternationMark = 0x303D;
constexpr char16_t perMilleSign = 0x2030;
constexpr char16_t perTenThousandSign = 0x2031;
constexpr char16_t pilcrowSign = 0x00B6;
constexpr char16_t popDirectionalFormatting = 0x202C;
constexpr char16_t popDirectionalIsolate = 0x2069;
constexpr char16_t presentationFormForVerticalLeftCornerBracket = 0xFE41;
constexpr char16_t presentationFormForVerticalLeftWhiteCornerBracket = 0xFE43;
constexpr char16_t presentationFormForVerticalRightCornerBracket = 0xFE42;
constexpr char16_t presentationFormForVerticalRightWhiteCornerBracket = 0xFE44;
constexpr char16_t quotationMark = 0x0022;
constexpr char16_t replacementCharacter = 0xFFFD;
constexpr char16_t reverseSolidus = 0x005C;
constexpr char16_t reversedDoublePrimeQuotationMark = 0x301D;
constexpr char16_t reversedPilcrowSign = 0x204B;
constexpr char16_t rightCornerBracket = 0x300D;
constexpr char16_t rightDoubleQuotationMark = 0x201D;
constexpr char16_t rightPointingDoubleAngleQuotationMark = 0x00BB;
constexpr char16_t rightSingleQuotationMark = 0x2019;
constexpr char16_t rightToLeftEmbed = 0x202B;
constexpr char16_t rightToLeftIsolate = 0x2067;
constexpr char16_t rightToLeftMark = 0x200F;
constexpr char16_t rightToLeftOverride = 0x202E;
constexpr char16_t rightWhiteCornerBracket = 0x300F;
constexpr char16_t sectionSign = 0x00A7;
constexpr char16_t sesameDot = 0xFE45;
constexpr char16_t singleLeftPointingAngleQuotationMark = 0x2039;
constexpr char16_t singleLow9QuotationMark = 0x201B;
constexpr char16_t singleRightPointingAngleQuotationMark = 0x203A;
constexpr char16_t smallAmpersand = 0xFE60;
constexpr char16_t smallCommercialAt = 0xFE6B;
constexpr char16_t smallLetterSharpS = 0x00DF;
constexpr char16_t smallNumberSign = 0xFE5F;
constexpr char16_t smallPercentSign = 0xFE6A;
constexpr char16_t softHyphen = 0x00AD;
constexpr char16_t space = 0x0020;
constexpr char16_t swungDash = 0x2053;
constexpr char16_t tabCharacter = 0x0009;
constexpr char16_t textVariationSelector = 0xFE0E; // Technical name is "VARIATION SELECTOR-15"
constexpr char16_t thinSpace = 0x2009;
constexpr char16_t tibetanMarkDelimiterTshegBstar = 0x0F0C;
constexpr char16_t tibetanMarkIntersyllabicTsheg = 0x0F0B;
constexpr char16_t tironianSignEt = 0x204A;
constexpr char32_t ugariticWordDivider = 0x1039F;
constexpr char16_t upArrowhead = 0x2303;
constexpr char16_t verticalEllipsis = 0x22EE;
constexpr char16_t verticalTabulation = 0x000b;
constexpr char16_t whiteBullet = 0x25E6;
constexpr char16_t whiteCircle = 0x25CB;
constexpr char16_t whiteSesameDot = 0xFE46;
constexpr char16_t whiteUpPointingTriangle = 0x25B3;
constexpr char16_t wordJoiner = 0x2060;
constexpr char16_t yenSign = 0x00A5;
constexpr char16_t zeroWidthJoiner = 0x200D;
constexpr char16_t zeroWidthNoBreakSpace = 0xFEFF;
constexpr char16_t zeroWidthNonJoiner = 0x200C;
constexpr char16_t zeroWidthSpace = 0x200B;

} // namespace WTF::Unicode

using WTF::Unicode::HiraganaLetterSmallA;
using WTF::Unicode::activateArabicFormShaping;
using WTF::Unicode::activateSymmetricSwapping;
using WTF::Unicode::aegeanWordSeparatorDot;
using WTF::Unicode::aegeanWordSeparatorLine;
using WTF::Unicode::arabicIndicPerMilleSign;
using WTF::Unicode::arabicIndicPerTenThousandSign;
using WTF::Unicode::arabicPercentSign;
using WTF::Unicode::blackCircle;
using WTF::Unicode::blackDownPointingSmallTriangle;
using WTF::Unicode::blackLeftPointingSmallTriangle;
using WTF::Unicode::blackRightPointingSmallTriangle;
using WTF::Unicode::blackSquare;
using WTF::Unicode::blackUpPointingSmallTriangle;
using WTF::Unicode::blackUpPointingTriangle;
using WTF::Unicode::bullet;
using WTF::Unicode::bullseye;
using WTF::Unicode::byteOrderMark;
using WTF::Unicode::carriageReturn;
using WTF::Unicode::cjkWater;
using WTF::Unicode::combiningEnclosingKeycap;
using WTF::Unicode::deleteCharacter;
using WTF::Unicode::doubleHighReversed9QuotationMark;
using WTF::Unicode::doubleLowReversed9QuotationMark;
using WTF::Unicode::doublePrimeQuotationMark;
using WTF::Unicode::emSpace;
using WTF::Unicode::emojiVariationSelector;
using WTF::Unicode::enDash;
using WTF::Unicode::ethiopicPrefaceColon;
using WTF::Unicode::ethiopicWordspace;
using WTF::Unicode::firstStrongIsolate;
using WTF::Unicode::fisheye;
using WTF::Unicode::formFeed;
using WTF::Unicode::fullwidthAmpersand;
using WTF::Unicode::fullwidthApostrophe;
using WTF::Unicode::fullwidthCommercialAt;
using WTF::Unicode::fullwidthNumberSign;
using WTF::Unicode::fullwidthPercentSign;
using WTF::Unicode::fullwidthQuotationMark;
using WTF::Unicode::functionApplication;
using WTF::Unicode::halfwidthLeftCornerBracket;
using WTF::Unicode::halfwidthRightCornerBracket;
using WTF::Unicode::hebrewPunctuationGeresh;
using WTF::Unicode::hebrewPunctuationGershayim;
using WTF::Unicode::horizontalEllipsis;
using WTF::Unicode::hyphen;
using WTF::Unicode::hyphenMinus;
using WTF::Unicode::ideographicComma;
using WTF::Unicode::ideographicFullStop;
using WTF::Unicode::ideographicSpace;
using WTF::Unicode::inhibitArabicFormShaping;
using WTF::Unicode::inhibitSymmetricSwapping;
using WTF::Unicode::invisibleSeparator;
using WTF::Unicode::invisibleTimes;
using WTF::Unicode::leftCornerBracket;
using WTF::Unicode::leftDoubleQuotationMark;
using WTF::Unicode::leftLowDoubleQuotationMark;
using WTF::Unicode::leftLowSingleQuotationMark;
using WTF::Unicode::leftPointingDoubleAngleQuotationMark;
using WTF::Unicode::leftSingleQuotationMark;
using WTF::Unicode::leftToRightEmbed;
using WTF::Unicode::leftToRightIsolate;
using WTF::Unicode::leftToRightMark;
using WTF::Unicode::leftToRightOverride;
using WTF::Unicode::leftWhiteCornerBracket;
using WTF::Unicode::lineSeparator;
using WTF::Unicode::lowDoublePrimeQuotationMark;
using WTF::Unicode::lowLine;
using WTF::Unicode::mediumShade;
using WTF::Unicode::minusSign;
using WTF::Unicode::multiplicationSign;
using WTF::Unicode::narrowNoBreakSpace;
using WTF::Unicode::nationalDigitShapes;
using WTF::Unicode::newlineCharacter;
using WTF::Unicode::noBreakSpace;
using WTF::Unicode::nominalDigitShapes;
using WTF::Unicode::nullCharacter;
using WTF::Unicode::objectReplacementCharacter;
using WTF::Unicode::paragraphSeparator;
using WTF::Unicode::partAlternationMark;
using WTF::Unicode::perMilleSign;
using WTF::Unicode::perTenThousandSign;
using WTF::Unicode::pilcrowSign;
using WTF::Unicode::popDirectionalFormatting;
using WTF::Unicode::popDirectionalIsolate;
using WTF::Unicode::presentationFormForVerticalLeftCornerBracket;
using WTF::Unicode::presentationFormForVerticalLeftWhiteCornerBracket;
using WTF::Unicode::presentationFormForVerticalRightCornerBracket;
using WTF::Unicode::presentationFormForVerticalRightWhiteCornerBracket;
using WTF::Unicode::quotationMark;
using WTF::Unicode::replacementCharacter;
using WTF::Unicode::reverseSolidus;
using WTF::Unicode::reversedDoublePrimeQuotationMark;
using WTF::Unicode::reversedPilcrowSign;
using WTF::Unicode::rightCornerBracket;
using WTF::Unicode::rightDoubleQuotationMark;
using WTF::Unicode::rightPointingDoubleAngleQuotationMark;
using WTF::Unicode::rightSingleQuotationMark;
using WTF::Unicode::rightToLeftEmbed;
using WTF::Unicode::rightToLeftIsolate;
using WTF::Unicode::rightToLeftMark;
using WTF::Unicode::rightToLeftOverride;
using WTF::Unicode::rightWhiteCornerBracket;
using WTF::Unicode::sectionSign;
using WTF::Unicode::sesameDot;
using WTF::Unicode::singleLeftPointingAngleQuotationMark;
using WTF::Unicode::singleLow9QuotationMark;
using WTF::Unicode::singleRightPointingAngleQuotationMark;
using WTF::Unicode::smallAmpersand;
using WTF::Unicode::smallCommercialAt;
using WTF::Unicode::smallNumberSign;
using WTF::Unicode::smallPercentSign;
using WTF::Unicode::softHyphen;
using WTF::Unicode::space;
using WTF::Unicode::swungDash;
using WTF::Unicode::tabCharacter;
using WTF::Unicode::textVariationSelector;
using WTF::Unicode::thinSpace;
using WTF::Unicode::tibetanMarkDelimiterTshegBstar;
using WTF::Unicode::tibetanMarkIntersyllabicTsheg;
using WTF::Unicode::tironianSignEt;
using WTF::Unicode::ugariticWordDivider;
using WTF::Unicode::upArrowhead;
using WTF::Unicode::verticalEllipsis;
using WTF::Unicode::verticalTabulation;
using WTF::Unicode::whiteBullet;
using WTF::Unicode::whiteCircle;
using WTF::Unicode::whiteSesameDot;
using WTF::Unicode::whiteUpPointingTriangle;
using WTF::Unicode::wordJoiner;
using WTF::Unicode::yenSign;
using WTF::Unicode::zeroWidthJoiner;
using WTF::Unicode::zeroWidthNoBreakSpace;
using WTF::Unicode::zeroWidthNonJoiner;
using WTF::Unicode::zeroWidthSpace;
