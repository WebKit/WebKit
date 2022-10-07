/*
 * Copyright (C) 2003, 2006, 2017, 2022 Apple Inc.  All rights reserved.
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
#include <variant>
#include <vector>
#include <wtf/EnumTraits.h>
#include <wtf/Hasher.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

enum class TextRenderingMode : uint8_t {
    AutoTextRendering,
    OptimizeSpeed,
    OptimizeLegibility,
    GeometricPrecision
};

enum class FontSmoothingMode : uint8_t {
    AutoSmoothing,
    NoSmoothing,
    Antialiased,
    SubpixelAntialiased
};

enum class FontOrientation : uint8_t {
    Horizontal,
    Vertical
};

enum class NonCJKGlyphOrientation : uint8_t {
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

    bool operator==(const ExpansionBehavior& other) const
    {
        return left == other.left && right == other.right;
    }

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

struct FontVariantAlternatesNormal {
    bool operator==(const FontVariantAlternatesNormal&) const = default;
};

struct FontVariantAlternatesValues {
    bool operator==(const FontVariantAlternatesValues&) const = default;

    std::optional<String> stylistic;
    std::optional<String> styleset;
    std::optional<String> characterVariant;
    std::optional<String> swash;
    std::optional<String> ornaments;
    std::optional<String> annotation;
    bool historicalForms = false;

    friend void add(Hasher&, const FontVariantAlternatesValues&);
};

class FontVariantAlternates {
    using Values = FontVariantAlternatesValues;

public:
    bool operator==(const FontVariantAlternates&) const = default;

    bool isNormal() const
    {
        return std::holds_alternative<FontVariantAlternatesNormal>(m_val);
    }

    Values values() const
    {
        ASSERT(!isNormal());
        return *std::get_if<Values>(&m_val);
    }

    Values& valuesRef()
    {
        if (isNormal())
            setValues();

        return *std::get_if<Values>(&m_val);
    }

    void setValues()
    {
        m_val = Values { };
    }

    static FontVariantAlternates Normal()
    {
        FontVariantAlternates result;
        result.m_val = FontVariantAlternatesNormal { };
        return result;
    }

    friend void add(Hasher&, const FontVariantAlternates&);

private:
    std::variant<FontVariantAlternatesNormal, Values> m_val;
    FontVariantAlternates() = default;
};

WTF::TextStream& operator<<(WTF::TextStream&, FontVariantAlternates);

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

struct FontVariantSettings {
    FontVariantSettings()
        : commonLigatures(FontVariantLigatures::Normal)
        , discretionaryLigatures(FontVariantLigatures::Normal)
        , historicalLigatures(FontVariantLigatures::Normal)
        , contextualAlternates(FontVariantLigatures::Normal)
        , position(FontVariantPosition::Normal)
        , caps(FontVariantCaps::Normal)
        , numericFigure(FontVariantNumericFigure::Normal)
        , numericSpacing(FontVariantNumericSpacing::Normal)
        , numericFraction(FontVariantNumericFraction::Normal)
        , numericOrdinal(FontVariantNumericOrdinal::Normal)
        , numericSlashedZero(FontVariantNumericSlashedZero::Normal)
        , alternates(FontVariantAlternates::Normal())
        , eastAsianVariant(FontVariantEastAsianVariant::Normal)
        , eastAsianWidth(FontVariantEastAsianWidth::Normal)
        , eastAsianRuby(FontVariantEastAsianRuby::Normal)
    {
    }

    FontVariantSettings(
        FontVariantLigatures commonLigatures,
        FontVariantLigatures discretionaryLigatures,
        FontVariantLigatures historicalLigatures,
        FontVariantLigatures contextualAlternates,
        FontVariantPosition position,
        FontVariantCaps caps,
        FontVariantNumericFigure numericFigure,
        FontVariantNumericSpacing numericSpacing,
        FontVariantNumericFraction numericFraction,
        FontVariantNumericOrdinal numericOrdinal,
        FontVariantNumericSlashedZero numericSlashedZero,
        FontVariantAlternates alternates,
        FontVariantEastAsianVariant eastAsianVariant,
        FontVariantEastAsianWidth eastAsianWidth,
        FontVariantEastAsianRuby eastAsianRuby)
            : commonLigatures(commonLigatures)
            , discretionaryLigatures(discretionaryLigatures)
            , historicalLigatures(historicalLigatures)
            , contextualAlternates(contextualAlternates)
            , position(position)
            , caps(caps)
            , numericFigure(numericFigure)
            , numericSpacing(numericSpacing)
            , numericFraction(numericFraction)
            , numericOrdinal(numericOrdinal)
            , numericSlashedZero(numericSlashedZero)
            , alternates(alternates)
            , eastAsianVariant(eastAsianVariant)
            , eastAsianWidth(eastAsianWidth)
            , eastAsianRuby(eastAsianRuby)
    {
    }

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
            && eastAsianRuby == FontVariantEastAsianRuby::Normal;
    }

    bool operator==(const FontVariantSettings& other) const
    {
        return commonLigatures == other.commonLigatures
            && discretionaryLigatures == other.discretionaryLigatures
            && historicalLigatures == other.historicalLigatures
            && contextualAlternates == other.contextualAlternates
            && position == other.position
            && caps == other.caps
            && numericFigure == other.numericFigure
            && numericSpacing == other.numericSpacing
            && numericFraction == other.numericFraction
            && numericOrdinal == other.numericOrdinal
            && numericSlashedZero == other.numericSlashedZero
            && alternates == other.alternates
            && eastAsianVariant == other.eastAsianVariant
            && eastAsianWidth == other.eastAsianWidth
            && eastAsianRuby == other.eastAsianRuby;
    }

    bool operator!=(const FontVariantSettings& other) const { return !(*this == other); }

    FontVariantLigatures commonLigatures;
    FontVariantLigatures discretionaryLigatures;
    FontVariantLigatures historicalLigatures;
    FontVariantLigatures contextualAlternates;
    FontVariantPosition position;
    FontVariantCaps caps;
    FontVariantNumericFigure numericFigure;
    FontVariantNumericSpacing numericSpacing;
    FontVariantNumericFraction numericFraction;
    FontVariantNumericOrdinal numericOrdinal;
    FontVariantNumericSlashedZero numericSlashedZero;
    FontVariantAlternates alternates;
    FontVariantEastAsianVariant eastAsianVariant;
    FontVariantEastAsianWidth eastAsianWidth;
    FontVariantEastAsianRuby eastAsianRuby;
};

struct FontVariantLigaturesValues {
    FontVariantLigaturesValues(
        FontVariantLigatures commonLigatures,
        FontVariantLigatures discretionaryLigatures,
        FontVariantLigatures historicalLigatures,
        FontVariantLigatures contextualAlternates)
            : commonLigatures(commonLigatures)
            , discretionaryLigatures(discretionaryLigatures)
            , historicalLigatures(historicalLigatures)
            , contextualAlternates(contextualAlternates)
    {
    }

    FontVariantLigatures commonLigatures;
    FontVariantLigatures discretionaryLigatures;
    FontVariantLigatures historicalLigatures;
    FontVariantLigatures contextualAlternates;
};

struct FontVariantNumericValues {
    FontVariantNumericValues(
        FontVariantNumericFigure figure,
        FontVariantNumericSpacing spacing,
        FontVariantNumericFraction fraction,
        FontVariantNumericOrdinal ordinal,
        FontVariantNumericSlashedZero slashedZero)
            : figure(figure)
            , spacing(spacing)
            , fraction(fraction)
            , ordinal(ordinal)
            , slashedZero(slashedZero)
    {
    }

    FontVariantNumericFigure figure;
    FontVariantNumericSpacing spacing;
    FontVariantNumericFraction fraction;
    FontVariantNumericOrdinal ordinal;
    FontVariantNumericSlashedZero slashedZero;
};

struct FontVariantEastAsianValues {
    FontVariantEastAsianValues(
        FontVariantEastAsianVariant variant,
        FontVariantEastAsianWidth width,
        FontVariantEastAsianRuby ruby)
            : variant(variant)
            , width(width)
            , ruby(ruby)
    {
    }

    FontVariantEastAsianVariant variant;
    FontVariantEastAsianWidth width;
    FontVariantEastAsianRuby ruby;
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

enum class FontSmallCaps : bool {
    Off,
    On
};

enum class Kerning : uint8_t {
    Auto,
    Normal,
    NoShift
};

WTF::TextStream& operator<<(WTF::TextStream&, Kerning);

enum class FontOpticalSizing : uint8_t {
    Enabled,
    Disabled
};

// https://www.microsoft.com/typography/otspec/fvar.htm#VAT
enum class FontStyleAxis : uint8_t {
    slnt,
    ital
};

enum class AllowUserInstalledFonts : uint8_t {
    No,
    Yes
};

}
