/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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

#include <optional>
#include <wtf/Forward.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/TextStream.h>


namespace WebCore {
struct ListStyleType {

    // The order of this enum must match the order of the list style types in CSSValueKeywords.in.
    enum class Type : uint8_t {
        Disc,
        Circle,
        Square,
        Decimal,
        DecimalLeadingZero,
        ArabicIndic,
        Binary,
        Bengali,
        Cambodian,
        Khmer,
        Devanagari,
        Gujarati,
        Gurmukhi,
        Kannada,
        LowerHexadecimal,
        Lao,
        Malayalam,
        Mongolian,
        Myanmar,
        Octal,
        Oriya,
        Persian,
        Urdu,
        Telugu,
        Tibetan,
        Thai,
        UpperHexadecimal,
        LowerRoman,
        UpperRoman,
        LowerGreek,
        LowerAlpha,
        LowerLatin,
        UpperAlpha,
        UpperLatin,
        Afar,
        EthiopicHalehameAaEt,
        EthiopicHalehameAaEr,
        Amharic,
        EthiopicHalehameAmEt,
        AmharicAbegede,
        EthiopicAbegedeAmEt,
        CJKEarthlyBranch,
        CJKHeavenlyStem,
        Ethiopic,
        EthiopicHalehameGez,
        EthiopicAbegede,
        EthiopicAbegedeGez,
        HangulConsonant,
        Hangul,
        LowerNorwegian,
        Oromo,
        EthiopicHalehameOmEt,
        Sidama,
        EthiopicHalehameSidEt,
        Somali,
        EthiopicHalehameSoEt,
        Tigre,
        EthiopicHalehameTig,
        TigrinyaEr,
        EthiopicHalehameTiEr,
        TigrinyaErAbegede,
        EthiopicAbegedeTiEr,
        TigrinyaEt,
        EthiopicHalehameTiEt,
        TigrinyaEtAbegede,
        EthiopicAbegedeTiEt,
        UpperGreek,
        UpperNorwegian,
        Asterisks,
        Footnotes,
        Hebrew,
        Armenian,
        LowerArmenian,
        UpperArmenian,
        Georgian,
        CJKIdeographic,
        Hiragana,
        Katakana,
        HiraganaIroha,
        KatakanaIroha,
        CJKDecimal,
        Tamil,
        DisclosureOpen,
        DisclosureClosed,
        JapaneseInformal,
        JapaneseFormal,
        KoreanHangulFormal,
        KoreanHanjaInformal,
        KoreanHanjaFormal,
        SimplifiedChineseInformal,
        SimplifiedChineseFormal,
        TraditionalChineseInformal,
        TraditionalChineseFormal,
        EthiopicNumeric,
        CounterStyle,
        String,
        None
    };

    bool isCircle() const;
    bool isSquare() const;
    bool isDisc() const;
    bool isDisclosureClosed() const;

    bool operator==(const ListStyleType& other) const { return type == other.type && identifier == other.identifier; }

    Type type { Type::None };
    // The identifier is the string when the type is String and is the @counter-style name when the type is CounterStyle.
    AtomString identifier;
};

TextStream& operator<<(TextStream&, ListStyleType::Type);
WTF::TextStream& operator<<(WTF::TextStream&, ListStyleType);


} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ListStyleType::Type> {
    using values = EnumValues<
        WebCore::ListStyleType::Type,
        WebCore::ListStyleType::Type::Disc,
        WebCore::ListStyleType::Type::Circle,
        WebCore::ListStyleType::Type::Square,
        WebCore::ListStyleType::Type::Decimal,
        WebCore::ListStyleType::Type::DecimalLeadingZero,
        WebCore::ListStyleType::Type::ArabicIndic,
        WebCore::ListStyleType::Type::Binary,
        WebCore::ListStyleType::Type::Bengali,
        WebCore::ListStyleType::Type::Cambodian,
        WebCore::ListStyleType::Type::Khmer,
        WebCore::ListStyleType::Type::Devanagari,
        WebCore::ListStyleType::Type::Gujarati,
        WebCore::ListStyleType::Type::Gurmukhi,
        WebCore::ListStyleType::Type::Kannada,
        WebCore::ListStyleType::Type::LowerHexadecimal,
        WebCore::ListStyleType::Type::Lao,
        WebCore::ListStyleType::Type::Malayalam,
        WebCore::ListStyleType::Type::Mongolian,
        WebCore::ListStyleType::Type::Myanmar,
        WebCore::ListStyleType::Type::Octal,
        WebCore::ListStyleType::Type::Oriya,
        WebCore::ListStyleType::Type::Persian,
        WebCore::ListStyleType::Type::Urdu,
        WebCore::ListStyleType::Type::Telugu,
        WebCore::ListStyleType::Type::Tibetan,
        WebCore::ListStyleType::Type::Thai,
        WebCore::ListStyleType::Type::UpperHexadecimal,
        WebCore::ListStyleType::Type::LowerRoman,
        WebCore::ListStyleType::Type::UpperRoman,
        WebCore::ListStyleType::Type::LowerGreek,
        WebCore::ListStyleType::Type::LowerAlpha,
        WebCore::ListStyleType::Type::LowerLatin,
        WebCore::ListStyleType::Type::UpperAlpha,
        WebCore::ListStyleType::Type::UpperLatin,
        WebCore::ListStyleType::Type::Afar,
        WebCore::ListStyleType::Type::EthiopicHalehameAaEt,
        WebCore::ListStyleType::Type::EthiopicHalehameAaEr,
        WebCore::ListStyleType::Type::Amharic,
        WebCore::ListStyleType::Type::EthiopicHalehameAmEt,
        WebCore::ListStyleType::Type::AmharicAbegede,
        WebCore::ListStyleType::Type::EthiopicAbegedeAmEt,
        WebCore::ListStyleType::Type::CJKEarthlyBranch,
        WebCore::ListStyleType::Type::CJKHeavenlyStem,
        WebCore::ListStyleType::Type::Ethiopic,
        WebCore::ListStyleType::Type::EthiopicHalehameGez,
        WebCore::ListStyleType::Type::EthiopicAbegede,
        WebCore::ListStyleType::Type::EthiopicAbegedeGez,
        WebCore::ListStyleType::Type::HangulConsonant,
        WebCore::ListStyleType::Type::Hangul,
        WebCore::ListStyleType::Type::LowerNorwegian,
        WebCore::ListStyleType::Type::Oromo,
        WebCore::ListStyleType::Type::EthiopicHalehameOmEt,
        WebCore::ListStyleType::Type::Sidama,
        WebCore::ListStyleType::Type::EthiopicHalehameSidEt,
        WebCore::ListStyleType::Type::Somali,
        WebCore::ListStyleType::Type::EthiopicHalehameSoEt,
        WebCore::ListStyleType::Type::Tigre,
        WebCore::ListStyleType::Type::EthiopicHalehameTig,
        WebCore::ListStyleType::Type::TigrinyaEr,
        WebCore::ListStyleType::Type::EthiopicHalehameTiEr,
        WebCore::ListStyleType::Type::TigrinyaErAbegede,
        WebCore::ListStyleType::Type::EthiopicAbegedeTiEr,
        WebCore::ListStyleType::Type::TigrinyaEt,
        WebCore::ListStyleType::Type::EthiopicHalehameTiEt,
        WebCore::ListStyleType::Type::TigrinyaEtAbegede,
        WebCore::ListStyleType::Type::EthiopicAbegedeTiEt,
        WebCore::ListStyleType::Type::UpperGreek,
        WebCore::ListStyleType::Type::UpperNorwegian,
        WebCore::ListStyleType::Type::Asterisks,
        WebCore::ListStyleType::Type::Footnotes,
        WebCore::ListStyleType::Type::Hebrew,
        WebCore::ListStyleType::Type::Armenian,
        WebCore::ListStyleType::Type::LowerArmenian,
        WebCore::ListStyleType::Type::UpperArmenian,
        WebCore::ListStyleType::Type::Georgian,
        WebCore::ListStyleType::Type::CJKIdeographic,
        WebCore::ListStyleType::Type::Hiragana,
        WebCore::ListStyleType::Type::Katakana,
        WebCore::ListStyleType::Type::HiraganaIroha,
        WebCore::ListStyleType::Type::KatakanaIroha,
        WebCore::ListStyleType::Type::CJKDecimal,
        WebCore::ListStyleType::Type::Tamil,
        WebCore::ListStyleType::Type::DisclosureOpen,
        WebCore::ListStyleType::Type::DisclosureClosed,
        WebCore::ListStyleType::Type::JapaneseInformal,
        WebCore::ListStyleType::Type::JapaneseFormal,
        WebCore::ListStyleType::Type::KoreanHangulFormal,
        WebCore::ListStyleType::Type::KoreanHanjaInformal,
        WebCore::ListStyleType::Type::KoreanHanjaFormal,
        WebCore::ListStyleType::Type::SimplifiedChineseInformal,
        WebCore::ListStyleType::Type::SimplifiedChineseFormal,
        WebCore::ListStyleType::Type::TraditionalChineseInformal,
        WebCore::ListStyleType::Type::TraditionalChineseFormal,
        WebCore::ListStyleType::Type::EthiopicNumeric,
        WebCore::ListStyleType::Type::CounterStyle,
        WebCore::ListStyleType::Type::String,
        WebCore::ListStyleType::Type::None
    >;
};

} // namespace WTF
