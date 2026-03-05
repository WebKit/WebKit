/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include <WebCore/FontTaggedSettings.h>
#include <optional>
#include <vector>
#include <wtf/Hasher.h>
#include <wtf/Markable.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class FontFeatureValues;

enum class TextRenderingMode : uint8_t {
    Auto,
    OptimizeSpeed,
    OptimizeLegibility,
    GeometricPrecision
};

WTF::TextStream& operator<<(WTF::TextStream&, TextRenderingMode);

enum class FontSmoothingMode : uint8_t {
    Auto,
    None,
    Antialiased,
    SubpixelAntialiased
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, FontSmoothingMode);

enum class FontOrientation : bool {
    Horizontal,
    Vertical
};

enum class NonCJKGlyphOrientation : bool {
    Mixed,
    Upright
};

struct ExpansionBehavior {
    enum class Behavior : uint8_t {
        Forbid,
        Allow,
        Force
    };

    ExpansionBehavior()
        : left(Behavior::Forbid)
        , right(Behavior::Allow)
    {

    }

    ExpansionBehavior(Behavior left, Behavior right)
        : left(left)
        , right(right)
    {
    }

    friend bool operator==(const ExpansionBehavior&, const ExpansionBehavior&) = default;

    static ExpansionBehavior defaultBehavior()
    {
        return { };
    }

    static ExpansionBehavior allowRightOnly()
    {
        return { Behavior::Forbid, Behavior::Allow };
    }

    static ExpansionBehavior allowLeftOnly()
    {
        return { Behavior::Allow, Behavior::Forbid };
    }

    static ExpansionBehavior forceLeftOnly()
    {
        return { Behavior::Force, Behavior::Forbid };
    }

    static ExpansionBehavior forbidAll()
    {
        return { Behavior::Forbid, Behavior::Forbid };
    }

    static constexpr unsigned bitsOfKind = 2;
    Behavior left : bitsOfKind;
    Behavior right : bitsOfKind;
};

WTF::TextStream& operator<<(WTF::TextStream&, ExpansionBehavior::Behavior);
WTF::TextStream& operator<<(WTF::TextStream&, ExpansionBehavior);

enum class FontSynthesisLonghandValue : bool {
    None,
    Auto
};

WTF::TextStream& operator<<(WTF::TextStream&, FontSynthesisLonghandValue);

enum class FontVariantLigatures : uint8_t { Normal, Yes, No };
enum class FontVariantPosition : uint8_t { Normal, Subscript, Superscript };

WTF::TextStream& operator<<(WTF::TextStream&, FontVariantPosition);

enum class FontVariantCaps : uint8_t {
    Normal,
    Small,
    AllSmall,
    Petite,
    AllPetite,
    Unicase,
    Titling
};

WTF::TextStream& operator<<(WTF::TextStream&, FontVariantCaps);

enum class FontVariantNumericFigure : uint8_t {
    Normal,
    LiningNumbers,
    OldStyleNumbers
};

enum class FontVariantNumericSpacing : uint8_t {
    Normal,
    ProportionalNumbers,
    TabularNumbers
};

enum class FontVariantNumericFraction : uint8_t {
    Normal,
    DiagonalFractions,
    StackedFractions
};

enum class FontVariantNumericOrdinal : bool { Normal, Yes };
enum class FontVariantNumericSlashedZero : bool { Normal, Yes };

struct FontVariantAlternatesValues {
    String stylistic;
    Vector<String> styleset;
    Vector<String> characterVariant;
    String swash;
    String ornaments;
    String annotation;
    bool historicalForms = false;

    friend void add(Hasher&, const FontVariantAlternatesValues&);

    bool operator==(const FontVariantAlternatesValues&) const = default;

private:
    friend struct MarkableTraits<FontVariantAlternatesValues>;

    bool m_isEmpty { false };
};

} // namespace WebCore

namespace WTF {

template<>
struct MarkableTraits<WebCore::FontVariantAlternatesValues> {
    static bool isEmptyValue(const WebCore::FontVariantAlternatesValues& value)
    {
        return value.m_isEmpty;
    }

    static WebCore::FontVariantAlternatesValues emptyValue()
    {
        WebCore::FontVariantAlternatesValues emptyValue;
        emptyValue.m_isEmpty = true;
        return emptyValue;
    }
};

} // namespace WTF

namespace WebCore {

class FontVariantAlternates {
    using Values = FontVariantAlternatesValues;

public:
    friend bool operator==(const FontVariantAlternates&, const FontVariantAlternates&) = default;

    bool isNormal() const
    {
        return !m_values;
    }

    const Values& values() const
    {
        ASSERT(!isNormal());
        return *m_values;
    }

    Values& valuesRef()
    {
        if (isNormal())
            setValues();

        return *m_values;
    }

    void setValues()
    {
        m_values = Values { };
    }

    static FontVariantAlternates Normal()
    {
        return { };
    }

    friend void add(Hasher&, const FontVariantAlternates&);

private:
    Markable<Values> m_values;
    FontVariantAlternates() = default;
};

WTF::TextStream& operator<<(WTF::TextStream&, const FontVariantAlternates&);

enum class FontVariantEastAsianVariant : uint8_t {
    Normal,
    Jis78,
    Jis83,
    Jis90,
    Jis04,
    Simplified,
    Traditional
};

enum class FontVariantEastAsianWidth : uint8_t {
    Normal,
    Full,
    Proportional
};

enum class FontVariantEastAsianRuby : uint8_t {
    Normal,
    Yes
};

enum class FontVariantEmoji : uint8_t {
    Normal,
    Text,
    Emoji,
    Unicode,
};

WTF::TextStream& operator<<(WTF::TextStream&, FontVariantEmoji);

struct FontVariantSettings {
    bool isAllNormal() const
    {
        return commonLigatures == FontVariantLigatures::Normal
            && discretionaryLigatures == FontVariantLigatures::Normal
            && historicalLigatures == FontVariantLigatures::Normal
            && contextualAlternates == FontVariantLigatures::Normal
            && position == FontVariantPosition::Normal
            && caps == FontVariantCaps::Normal
            && numericFigure == FontVariantNumericFigure::Normal
            && numericSpacing == FontVariantNumericSpacing::Normal
            && numericFraction == FontVariantNumericFraction::Normal
            && numericOrdinal == FontVariantNumericOrdinal::Normal
            && numericSlashedZero == FontVariantNumericSlashedZero::Normal
            && alternates.isNormal()
            && eastAsianVariant == FontVariantEastAsianVariant::Normal
            && eastAsianWidth == FontVariantEastAsianWidth::Normal
            && eastAsianRuby == FontVariantEastAsianRuby::Normal
            && emoji == FontVariantEmoji::Normal;
    }

    bool operator==(const FontVariantSettings&) const = default;

    FontVariantLigatures commonLigatures { FontVariantLigatures::Normal };
    FontVariantLigatures discretionaryLigatures { FontVariantLigatures::Normal };
    FontVariantLigatures historicalLigatures { FontVariantLigatures::Normal };
    FontVariantLigatures contextualAlternates { FontVariantLigatures::Normal };
    FontVariantPosition position { FontVariantPosition::Normal };
    FontVariantCaps caps { FontVariantCaps::Normal };
    FontVariantNumericFigure numericFigure { FontVariantNumericFigure::Normal };
    FontVariantNumericSpacing numericSpacing { FontVariantNumericSpacing::Normal };
    FontVariantNumericFraction numericFraction { FontVariantNumericFraction::Normal };
    FontVariantNumericOrdinal numericOrdinal { FontVariantNumericOrdinal::Normal };
    FontVariantNumericSlashedZero numericSlashedZero { FontVariantNumericSlashedZero::Normal };
    FontVariantAlternates alternates { FontVariantAlternates::Normal() };
    FontVariantEastAsianVariant eastAsianVariant { FontVariantEastAsianVariant::Normal };
    FontVariantEastAsianWidth eastAsianWidth { FontVariantEastAsianWidth::Normal };
    FontVariantEastAsianRuby eastAsianRuby { FontVariantEastAsianRuby::Normal };
    FontVariantEmoji emoji { FontVariantEmoji::Normal };
};

struct FontVariantLigaturesValues {
    FontVariantLigatures common { FontVariantLigatures::Normal };
    FontVariantLigatures discretionary { FontVariantLigatures::Normal };
    FontVariantLigatures historical { FontVariantLigatures::Normal };
    FontVariantLigatures contextual { FontVariantLigatures::Normal };

    constexpr bool operator==(const FontVariantLigaturesValues&) const = default;
};

struct FontVariantNumericValues {
    FontVariantNumericFigure figure { FontVariantNumericFigure::Normal };
    FontVariantNumericSpacing spacing { FontVariantNumericSpacing::Normal };
    FontVariantNumericFraction fraction { FontVariantNumericFraction::Normal };
    FontVariantNumericOrdinal ordinal { FontVariantNumericOrdinal::Normal };
    FontVariantNumericSlashedZero slashedZero { FontVariantNumericSlashedZero::Normal };

    constexpr bool operator==(const FontVariantNumericValues&) const = default;
};

struct FontVariantEastAsianValues {
    FontVariantEastAsianVariant variant { FontVariantEastAsianVariant::Normal };
    FontVariantEastAsianWidth width { FontVariantEastAsianWidth::Normal };
    FontVariantEastAsianRuby ruby { FontVariantEastAsianRuby::Normal };

    constexpr bool operator==(const FontVariantEastAsianValues&) const = default;
};

enum class FontWidthVariant : uint8_t {
    RegularWidth,
    HalfWidth,
    ThirdWidth,
    QuarterWidth,
    LastFontWidthVariant = QuarterWidth
};

const unsigned FontWidthVariantWidth = 2;

static_assert(!(static_cast<unsigned>(FontWidthVariant::LastFontWidthVariant) >> FontWidthVariantWidth), "FontWidthVariantWidth is correct");

enum class Kerning : uint8_t {
    Auto,
    Normal,
    NoShift
};

WTF::TextStream& operator<<(WTF::TextStream&, Kerning);

enum class FontOpticalSizing : bool {
    None,
    Auto
};

WTF::TextStream& operator<<(WTF::TextStream&, FontOpticalSizing);

// https://www.microsoft.com/typography/otspec/fvar.htm#VAT
enum class FontStyleAxis : bool {
    slnt,
    ital
};

enum class AllowUserInstalledFonts : bool { No, Yes };

using FeaturesMap = HashMap<FontTag, int, FourCharacterTagHash, FourCharacterTagHashTraits>;
FeaturesMap computeFeatureSettingsFromVariants(const FontVariantSettings&, RefPtr<FontFeatureValues>);

enum class ResolvedEmojiPolicy : uint8_t {
    NoPreference,
    RequireText,
    RequireEmoji,
};

enum class ColorGlyphType : uint8_t {
    Outline,
    Color,
};

} // namespace WebCore
