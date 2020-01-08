/*
 * Copyright (C) 2003, 2006, 2017 Apple Inc.  All rights reserved.
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

#include <wtf/EnumTraits.h>
#include <wtf/Optional.h>

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

// This setting is used to provide ways of switching between multiple rendering modes that may have different
// metrics. It is used to switch between CG and GDI text on Windows.
enum class FontRenderingMode : uint8_t {
    Normal,
    Alternate
};

enum class FontOrientation : uint8_t {
    Horizontal,
    Vertical
};

enum class NonCJKGlyphOrientation : uint8_t {
    Mixed,
    Upright
};

// Here, "Leading" and "Trailing" are relevant after the line has been rearranged for bidi.
// ("Leading" means "left" and "Trailing" means "right.")
enum ExpansionBehaviorFlags {
    ForbidTrailingExpansion = 0 << 0,
    AllowTrailingExpansion = 1 << 0,
    ForceTrailingExpansion = 2 << 0,
    TrailingExpansionMask = 3 << 0,

    ForbidLeadingExpansion = 0 << 2,
    AllowLeadingExpansion = 1 << 2,
    ForceLeadingExpansion = 2 << 2,
    LeadingExpansionMask = 3 << 2,

    DefaultExpansion = AllowTrailingExpansion | ForbidLeadingExpansion,
};
typedef unsigned ExpansionBehavior;

enum FontSynthesisValues {
    FontSynthesisNone = 0x0,
    FontSynthesisWeight = 0x1,
    FontSynthesisStyle = 0x2,
    FontSynthesisSmallCaps = 0x4
};
typedef unsigned FontSynthesis;
const unsigned FontSynthesisWidth = 3;

enum class FontVariantLigatures : uint8_t {
    Normal,
    Yes,
    No
};

enum class FontVariantPosition : uint8_t {
    Normal,
    Subscript,
    Superscript
};

enum class FontVariantCaps : uint8_t {
    Normal,
    Small,
    AllSmall,
    Petite,
    AllPetite,
    Unicase,
    Titling
};

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

enum class FontVariantNumericOrdinal : uint8_t {
    Normal,
    Yes
};

enum class FontVariantNumericSlashedZero : uint8_t {
    Normal,
    Yes
};

enum class FontVariantAlternates : uint8_t {
    Normal,
    HistoricalForms
};

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
        , alternates(FontVariantAlternates::Normal)
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
            && alternates == FontVariantAlternates::Normal
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

    unsigned uniqueValue() const
    {
        return static_cast<unsigned>(commonLigatures) << 26
            | static_cast<unsigned>(discretionaryLigatures) << 24
            | static_cast<unsigned>(historicalLigatures) << 22
            | static_cast<unsigned>(contextualAlternates) << 20
            | static_cast<unsigned>(position) << 18
            | static_cast<unsigned>(caps) << 15
            | static_cast<unsigned>(numericFigure) << 13
            | static_cast<unsigned>(numericSpacing) << 11
            | static_cast<unsigned>(numericFraction) << 9
            | static_cast<unsigned>(numericOrdinal) << 8
            | static_cast<unsigned>(numericSlashedZero) << 7
            | static_cast<unsigned>(alternates) << 6
            | static_cast<unsigned>(eastAsianVariant) << 3
            | static_cast<unsigned>(eastAsianWidth) << 1
            | static_cast<unsigned>(eastAsianRuby) << 0;
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<FontVariantSettings> decode(Decoder&);

    // FIXME: this would be much more compact with bitfields.
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

template<class Encoder>
void FontVariantSettings::encode(Encoder& encoder) const
{
    encoder << commonLigatures;
    encoder << discretionaryLigatures;
    encoder << historicalLigatures;
    encoder << contextualAlternates;
    encoder << position;
    encoder << caps;
    encoder << numericFigure;
    encoder << numericSpacing;
    encoder << numericFraction;
    encoder << numericOrdinal;
    encoder << numericSlashedZero;
    encoder << alternates;
    encoder << eastAsianVariant;
    encoder << eastAsianWidth;
    encoder << eastAsianRuby;
}

template<class Decoder>
Optional<FontVariantSettings> FontVariantSettings::decode(Decoder& decoder)
{
    Optional<FontVariantLigatures> commonLigatures;
    decoder >> commonLigatures;
    if (!commonLigatures)
        return WTF::nullopt;

    Optional<FontVariantLigatures> discretionaryLigatures;
    decoder >> discretionaryLigatures;
    if (!discretionaryLigatures)
        return WTF::nullopt;

    Optional<FontVariantLigatures> historicalLigatures;
    decoder >> historicalLigatures;
    if (!historicalLigatures)
        return WTF::nullopt;

    Optional<FontVariantLigatures> contextualAlternates;
    decoder >> contextualAlternates;
    if (!contextualAlternates)
        return WTF::nullopt;

    Optional<FontVariantPosition> position;
    decoder >> position;
    if (!position)
        return WTF::nullopt;

    Optional<FontVariantCaps> caps;
    decoder >> caps;
    if (!caps)
        return WTF::nullopt;

    Optional<FontVariantNumericFigure> numericFigure;
    decoder >> numericFigure;
    if (!numericFigure)
        return WTF::nullopt;

    Optional<FontVariantNumericSpacing> numericSpacing;
    decoder >> numericSpacing;
    if (!numericSpacing)
        return WTF::nullopt;

    Optional<FontVariantNumericFraction> numericFraction;
    decoder >> numericFraction;
    if (!numericFraction)
        return WTF::nullopt;

    Optional<FontVariantNumericOrdinal> numericOrdinal;
    decoder >> numericOrdinal;
    if (!numericOrdinal)
        return WTF::nullopt;

    Optional<FontVariantNumericSlashedZero> numericSlashedZero;
    decoder >> numericSlashedZero;
    if (!numericSlashedZero)
        return WTF::nullopt;

    Optional<FontVariantAlternates> alternates;
    decoder >> alternates;
    if (!alternates)
        return WTF::nullopt;

    Optional<FontVariantEastAsianVariant> eastAsianVariant;
    decoder >> eastAsianVariant;
    if (!eastAsianVariant)
        return WTF::nullopt;

    Optional<FontVariantEastAsianWidth> eastAsianWidth;
    decoder >> eastAsianWidth;
    if (!eastAsianWidth)
        return WTF::nullopt;

    Optional<FontVariantEastAsianRuby> eastAsianRuby;
    decoder >> eastAsianRuby;
    if (!eastAsianRuby)
        return WTF::nullopt;

    return {{
        *commonLigatures,
        *discretionaryLigatures,
        *historicalLigatures,
        *contextualAlternates,
        *position,
        *caps,
        *numericFigure,
        *numericSpacing,
        *numericFraction,
        *numericOrdinal,
        *numericSlashedZero,
        *alternates,
        *eastAsianVariant,
        *eastAsianWidth,
        *eastAsianRuby
    }};
}

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

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<FontVariantLigaturesValues> decode(Decoder&);

    FontVariantLigatures commonLigatures;
    FontVariantLigatures discretionaryLigatures;
    FontVariantLigatures historicalLigatures;
    FontVariantLigatures contextualAlternates;
};

template<class Encoder>
void FontVariantLigaturesValues::encode(Encoder& encoder) const
{
    encoder << commonLigatures;
    encoder << discretionaryLigatures;
    encoder << historicalLigatures;
    encoder << contextualAlternates;
}

template<class Decoder>
Optional<FontVariantLigaturesValues> FontVariantLigaturesValues::decode(Decoder& decoder)
{
    Optional<FontVariantLigatures> commonLigatures;
    decoder >> commonLigatures;
    if (!commonLigatures)
        return WTF::nullopt;

    Optional<FontVariantLigatures> discretionaryLigatures;
    decoder >> discretionaryLigatures;
    if (!discretionaryLigatures)
        return WTF::nullopt;

    Optional<FontVariantLigatures> historicalLigatures;
    decoder >> historicalLigatures;
    if (!historicalLigatures)
        return WTF::nullopt;

    Optional<FontVariantLigatures> contextualAlternates;
    decoder >> contextualAlternates;
    if (!contextualAlternates)
        return WTF::nullopt;

    return {{ *commonLigatures, *discretionaryLigatures, *historicalLigatures, *contextualAlternates }};
}

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

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<FontVariantNumericValues> decode(Decoder&);

    FontVariantNumericFigure figure;
    FontVariantNumericSpacing spacing;
    FontVariantNumericFraction fraction;
    FontVariantNumericOrdinal ordinal;
    FontVariantNumericSlashedZero slashedZero;
};

template<class Encoder>
void FontVariantNumericValues::encode(Encoder& encoder) const
{
    encoder << figure;
    encoder << spacing;
    encoder << fraction;
    encoder << ordinal;
    encoder << slashedZero;
}

template<class Decoder>
Optional<FontVariantNumericValues> FontVariantNumericValues::decode(Decoder& decoder)
{
    Optional<FontVariantNumericFigure> figure;
    decoder >> figure;
    if (!figure)
        return WTF::nullopt;

    Optional<FontVariantNumericSpacing> spacing;
    decoder >> spacing;
    if (!spacing)
        return WTF::nullopt;

    Optional<FontVariantNumericFraction> fraction;
    decoder >> fraction;
    if (!fraction)
        return WTF::nullopt;

    Optional<FontVariantNumericOrdinal> ordinal;
    decoder >> ordinal;
    if (!ordinal)
        return WTF::nullopt;

    Optional<FontVariantNumericSlashedZero> slashedZero;
    decoder >> slashedZero;
    if (!slashedZero)
        return WTF::nullopt;

    return {{ *figure, *spacing, *fraction, *ordinal, *slashedZero }};
}

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

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<FontVariantEastAsianValues> decode(Decoder&);

    FontVariantEastAsianVariant variant;
    FontVariantEastAsianWidth width;
    FontVariantEastAsianRuby ruby;
};

template<class Encoder>
void FontVariantEastAsianValues::encode(Encoder& encoder) const
{
    encoder << variant;
    encoder << width;
    encoder << ruby;
}

template<class Decoder>
Optional<FontVariantEastAsianValues> FontVariantEastAsianValues::decode(Decoder& decoder)
{
    Optional<FontVariantEastAsianVariant> variant;
    decoder >> variant;
    if (!variant)
        return WTF::nullopt;

    Optional<FontVariantEastAsianWidth> width;
    decoder >> width;
    if (!width)
        return WTF::nullopt;

    Optional<FontVariantEastAsianRuby> ruby;
    decoder >> ruby;
    if (!ruby)
        return WTF::nullopt;

    return {{ *variant, *width, *ruby }};
}

enum class FontWidthVariant : uint8_t {
    RegularWidth,
    HalfWidth,
    ThirdWidth,
    QuarterWidth,
    LastFontWidthVariant = QuarterWidth
};

const unsigned FontWidthVariantWidth = 2;

COMPILE_ASSERT(!(static_cast<unsigned>(FontWidthVariant::LastFontWidthVariant) >> FontWidthVariantWidth), FontWidthVariantWidth_is_correct);

enum class FontSmallCaps : uint8_t {
    Off = 0,
    On = 1
};

enum class Kerning : uint8_t {
    Auto,
    Normal,
    NoShift
};

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

namespace WTF {

template<> struct EnumTraits<WebCore::TextRenderingMode> {
    using values = EnumValues<
    WebCore::TextRenderingMode,
    WebCore::TextRenderingMode::AutoTextRendering,
    WebCore::TextRenderingMode::OptimizeSpeed,
    WebCore::TextRenderingMode::OptimizeLegibility,
    WebCore::TextRenderingMode::GeometricPrecision
    >;
};

template<> struct EnumTraits<WebCore::FontSmoothingMode> {
    using values = EnumValues<
    WebCore::FontSmoothingMode,
    WebCore::FontSmoothingMode::AutoSmoothing,
    WebCore::FontSmoothingMode::NoSmoothing,
    WebCore::FontSmoothingMode::Antialiased,
    WebCore::FontSmoothingMode::SubpixelAntialiased,
    WebCore::FontSmoothingMode::AutoSmoothing
    >;
};

template<> struct EnumTraits<WebCore::FontRenderingMode> {
    using values = EnumValues<
    WebCore::FontRenderingMode,
    WebCore::FontRenderingMode::Normal,
    WebCore::FontRenderingMode::Alternate
    >;
};

template<> struct EnumTraits<WebCore::FontOrientation> {
    using values = EnumValues<
    WebCore::FontOrientation,
    WebCore::FontOrientation::Horizontal,
    WebCore::FontOrientation::Vertical
    >;
};

template<> struct EnumTraits<WebCore::NonCJKGlyphOrientation> {
    using values = EnumValues<
    WebCore::NonCJKGlyphOrientation,
    WebCore::NonCJKGlyphOrientation::Mixed,
    WebCore::NonCJKGlyphOrientation::Upright
    >;
};

template<> struct EnumTraits<WebCore::FontVariantLigatures> {
    using values = EnumValues<
    WebCore::FontVariantLigatures,
    WebCore::FontVariantLigatures::Normal,
    WebCore::FontVariantLigatures::Yes,
    WebCore::FontVariantLigatures::No
    >;
};

template<> struct EnumTraits<WebCore::FontVariantPosition> {
    using values = EnumValues<
    WebCore::FontVariantPosition,
    WebCore::FontVariantPosition::Normal,
    WebCore::FontVariantPosition::Subscript,
    WebCore::FontVariantPosition::Superscript
    >;
};

template<> struct EnumTraits<WebCore::FontVariantCaps> {
    using values = EnumValues<
    WebCore::FontVariantCaps,
    WebCore::FontVariantCaps::Normal,
    WebCore::FontVariantCaps::Small,
    WebCore::FontVariantCaps::AllSmall,
    WebCore::FontVariantCaps::Petite,
    WebCore::FontVariantCaps::AllPetite,
    WebCore::FontVariantCaps::Unicase,
    WebCore::FontVariantCaps::Titling
    >;
};

template<> struct EnumTraits<WebCore::FontVariantNumericFigure> {
    using values = EnumValues<
    WebCore::FontVariantNumericFigure,
    WebCore::FontVariantNumericFigure::Normal,
    WebCore::FontVariantNumericFigure::LiningNumbers,
    WebCore::FontVariantNumericFigure::OldStyleNumbers
    >;
};

template<> struct EnumTraits<WebCore::FontVariantNumericSpacing> {
    using values = EnumValues<
    WebCore::FontVariantNumericSpacing,
    WebCore::FontVariantNumericSpacing::Normal,
    WebCore::FontVariantNumericSpacing::ProportionalNumbers,
    WebCore::FontVariantNumericSpacing::TabularNumbers
    >;
};

template<> struct EnumTraits<WebCore::FontVariantNumericFraction> {
    using values = EnumValues<
    WebCore::FontVariantNumericFraction,
    WebCore::FontVariantNumericFraction::Normal,
    WebCore::FontVariantNumericFraction::DiagonalFractions,
    WebCore::FontVariantNumericFraction::StackedFractions
    >;
};

template<> struct EnumTraits<WebCore::FontVariantNumericOrdinal> {
    using values = EnumValues<
    WebCore::FontVariantNumericOrdinal,
    WebCore::FontVariantNumericOrdinal::Normal,
    WebCore::FontVariantNumericOrdinal::Yes
    >;
};

template<> struct EnumTraits<WebCore::FontVariantNumericSlashedZero> {
    using values = EnumValues<
    WebCore::FontVariantNumericSlashedZero,
    WebCore::FontVariantNumericSlashedZero::Normal,
    WebCore::FontVariantNumericSlashedZero::Yes
    >;
};

template<> struct EnumTraits<WebCore::FontVariantAlternates> {
    using values = EnumValues<
    WebCore::FontVariantAlternates,
    WebCore::FontVariantAlternates::Normal,
    WebCore::FontVariantAlternates::HistoricalForms
    >;
};

template<> struct EnumTraits<WebCore::FontVariantEastAsianVariant> {
    using values = EnumValues<
    WebCore::FontVariantEastAsianVariant,
    WebCore::FontVariantEastAsianVariant::Normal,
    WebCore::FontVariantEastAsianVariant::Jis78,
    WebCore::FontVariantEastAsianVariant::Jis83,
    WebCore::FontVariantEastAsianVariant::Jis90,
    WebCore::FontVariantEastAsianVariant::Jis04,
    WebCore::FontVariantEastAsianVariant::Simplified,
    WebCore::FontVariantEastAsianVariant::Traditional,
    WebCore::FontVariantEastAsianVariant::Normal
    >;
};

template<> struct EnumTraits<WebCore::FontVariantEastAsianWidth> {
    using values = EnumValues<
    WebCore::FontVariantEastAsianWidth,
    WebCore::FontVariantEastAsianWidth::Normal,
    WebCore::FontVariantEastAsianWidth::Full,
    WebCore::FontVariantEastAsianWidth::Proportional
    >;
};

template<> struct EnumTraits<WebCore::FontVariantEastAsianRuby> {
    using values = EnumValues<
    WebCore::FontVariantEastAsianRuby,
    WebCore::FontVariantEastAsianRuby::Normal,
    WebCore::FontVariantEastAsianRuby::Yes
    >;
};

template<> struct EnumTraits<WebCore::FontWidthVariant> {
    using values = EnumValues<
    WebCore::FontWidthVariant,
    WebCore::FontWidthVariant::RegularWidth,
    WebCore::FontWidthVariant::HalfWidth,
    WebCore::FontWidthVariant::ThirdWidth,
    WebCore::FontWidthVariant::QuarterWidth
    >;
};

template<> struct EnumTraits<WebCore::FontSmallCaps> {
    using values = EnumValues<
    WebCore::FontSmallCaps,
    WebCore::FontSmallCaps::Off,
    WebCore::FontSmallCaps::On
    >;
};

template<> struct EnumTraits<WebCore::Kerning> {
    using values = EnumValues<
    WebCore::Kerning,
    WebCore::Kerning::Auto,
    WebCore::Kerning::Normal,
    WebCore::Kerning::NoShift
    >;
};

template<> struct EnumTraits<WebCore::FontOpticalSizing> {
    using values = EnumValues<
    WebCore::FontOpticalSizing,
    WebCore::FontOpticalSizing::Enabled,
    WebCore::FontOpticalSizing::Disabled
    >;
};

template<> struct EnumTraits<WebCore::FontStyleAxis> {
    using values = EnumValues<
    WebCore::FontStyleAxis,
    WebCore::FontStyleAxis::slnt,
    WebCore::FontStyleAxis::ital
    >;
};

template<> struct EnumTraits<WebCore::AllowUserInstalledFonts> {
    using values = EnumValues<
    WebCore::AllowUserInstalledFonts,
    WebCore::AllowUserInstalledFonts::No,
    WebCore::AllowUserInstalledFonts::Yes
    >;
};

} // namespace WTF
