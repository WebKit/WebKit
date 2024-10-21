/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "FontCache.h"

#include "Font.h"
#include "FontDescription.h"
#include "StyleFontSizeFunctions.h"
#if defined(__ANDROID__) || defined(ANDROID)
#include <skia/ports/SkFontMgr_android.h>
#else
#include <skia/ports/SkFontMgr_fontconfig.h>
#endif
#include <wtf/Assertions.h>
#include <wtf/text/CString.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(GTK)
#include "GtkUtilities.h"
#endif

namespace WebCore {

void FontCache::platformInit()
{
}

SkFontMgr& FontCache::fontManager() const
{
    if (!m_fontManager) {
#if defined(__ANDROID__) || defined(ANDROID)
        m_fontManager = SkFontMgr_New_Android(nullptr);
#else
        m_fontManager = SkFontMgr_New_FontConfig(FcConfigReference(nullptr));
#endif
    }
    RELEASE_ASSERT(m_fontManager);
    return *m_fontManager.get();
}

static SkFontStyle skiaFontStyle(const FontDescription& fontDescription)
{
    int skWeight = SkFontStyle::kNormal_Weight;
    auto weight = fontDescription.weight();
    if (weight > FontSelectionValue(SkFontStyle::kInvisible_Weight) && weight <= FontSelectionValue(SkFontStyle::kExtraBlack_Weight))
        skWeight = static_cast<int>(weight);

    int skWidth = SkFontStyle::kNormal_Width;
    auto stretch = fontDescription.stretch();
    if (stretch <= ultraCondensedStretchValue())
        skWidth = SkFontStyle::kUltraCondensed_Width;
    else if (stretch <= extraCondensedStretchValue())
        skWidth = SkFontStyle::kExtraCondensed_Width;
    else if (stretch <= condensedStretchValue())
        skWidth = SkFontStyle::kCondensed_Width;
    else if (stretch <= semiCondensedStretchValue())
        skWidth = SkFontStyle::kSemiCondensed_Width;
    else if (stretch >= semiExpandedStretchValue())
        skWidth = SkFontStyle::kSemiExpanded_Width;
    else if (stretch >= expandedStretchValue())
        skWidth = SkFontStyle::kExpanded_Width;
    if (stretch >= extraExpandedStretchValue())
        skWidth = SkFontStyle::kExtraExpanded_Width;
    if (stretch >= ultraExpandedStretchValue())
        skWidth = SkFontStyle::kUltraExpanded_Width;

    SkFontStyle::Slant skSlant = SkFontStyle::kUpright_Slant;
    if (auto italic = fontDescription.italic()) {
        if (italic.value() > normalItalicValue() && italic.value() <= italicThreshold())
            skSlant = SkFontStyle::kItalic_Slant;
        else if (italic.value() > italicThreshold())
            skSlant = SkFontStyle::kOblique_Slant;
    }

    return SkFontStyle(skWeight, skWidth, skSlant);
}

RefPtr<Font> FontCache::systemFallbackForCharacterCluster(const FontDescription& description, const Font&, IsForPlatformFont, PreferColoredFont, StringView stringView)
{
    // FIXME: matchFamilyStyleCharacter is slow, we need a cache here, see https://bugs.webkit.org/show_bug.cgi?id=203544.
    auto codePoints = stringView.codePoints();
    auto codePointsIterator = codePoints.begin();
    char32_t baseCharacter = *codePointsIterator;
    ++codePointsIterator;
    if (isDefaultIgnorableCodePoint(baseCharacter) || isPrivateUseAreaCharacter(baseCharacter))
        return nullptr;

    bool isEmoji = codePointsIterator != codePoints.end() && *codePointsIterator == emojiVariationSelector;

    // FIXME: handle locale.
    Vector<const char*, 1> bcp47;
    if (isEmoji)
        bcp47.append("und-Zsye");

    // FIXME: handle synthetic properties.
    auto features = computeFeatures(description, { });
    auto typeface = fontManager().matchFamilyStyleCharacter(nullptr, skiaFontStyle(description), bcp47.data(), bcp47.size(), baseCharacter);
    FontPlatformData alternateFontData(WTFMove(typeface), description.computedSize(), false /* syntheticBold */, false /* syntheticOblique */, description.orientation(), description.widthVariant(), description.textRenderingMode(), WTFMove(features));
    return fontForPlatformData(alternateFontData);
}

Vector<String> FontCache::systemFontFamilies()
{
    auto& manager = fontManager();
    int count = manager.countFamilies();
    Vector<String> fontFamilies;
    fontFamilies.reserveInitialCapacity(count);
    for (int i = 0; i < count; ++i) {
        SkString familyName;
        manager.getFamilyName(i, &familyName);
        fontFamilies.append(String::fromUTF8(familyName.data()));
    }
    return fontFamilies;
}

bool FontCache::isSystemFontForbiddenForEditing(const String&)
{
    return false;
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    if (RefPtr<Font> font = fontForFamily(fontDescription, "serif"_s))
        return font.releaseNonNull();

    // Passing nullptr as family name makes Skia use a weak match.
    auto typeface = fontManager().matchFamilyStyle(nullptr, skiaFontStyle(fontDescription));
    if (!typeface) {
        // LastResort is guaranteed to be non-null, so fallback to empty font with not glyphs.
        typeface = SkTypeface::MakeEmpty();
    }

    FontPlatformData platformData(WTFMove(typeface), fontDescription.computedSize(), false /* syntheticBold */, false /* syntheticOblique */,
        fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode(), computeFeatures(fontDescription, { }));
    return fontForPlatformData(platformData);
}

Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomString&, AllowUserInstalledFonts)
{
    return { };
}

static String getFamilyNameStringFromFamily(const String& family)
{
    // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the name into
    // the fallback name (like "monospace") that fontconfig understands.
    if (family.length() && !family.startsWith("-webkit-"_s))
        return family;

    if (family == familyNamesData->at(FamilyNamesIndex::StandardFamily) || family == familyNamesData->at(FamilyNamesIndex::SerifFamily))
        return "serif"_s;
    if (family == familyNamesData->at(FamilyNamesIndex::SansSerifFamily))
        return "sans-serif"_s;
    if (family == familyNamesData->at(FamilyNamesIndex::MonospaceFamily))
        return "monospace"_s;
    if (family == familyNamesData->at(FamilyNamesIndex::CursiveFamily))
        return "cursive"_s;
    if (family == familyNamesData->at(FamilyNamesIndex::FantasyFamily))
        return "fantasy"_s;

#if PLATFORM(GTK)
    if (family == familyNamesData->at(FamilyNamesIndex::SystemUiFamily) || family == "-webkit-system-font"_s)
        return defaultGtkSystemFont();
#endif

    return emptyString();
}

Vector<hb_feature_t> FontCache::computeFeatures(const FontDescription& fontDescription, const FontCreationContext& fontCreationContext)
{
    FeaturesMap featuresToBeApplied;

    // 7.2. Feature precedence
    // https://www.w3.org/TR/css-fonts-3/#feature-precedence

    // 1. Font features enabled by default, including features required for a given script.

    // FIXME: optical sizing.

    // 2. If the font is defined via an @font-face rule, the font features implied by the
    //    font-feature-settings descriptor in the @font-face rule.
    if (fontCreationContext.fontFaceFeatures()) {
        for (auto& fontFaceFeature : *fontCreationContext.fontFaceFeatures())
            featuresToBeApplied.set(fontFaceFeature.tag(), fontFaceFeature.value());
    }

    // 3. Font features implied by the value of the ‘font-variant’ property, the related ‘font-variant’
    //    subproperties and any other CSS property that uses OpenType features.
    for (auto& newFeature : computeFeatureSettingsFromVariants(fontDescription.variantSettings(), fontCreationContext.fontFeatureValues()))
        featuresToBeApplied.set(newFeature.key, newFeature.value);

    // 4. Feature settings determined by properties other than ‘font-variant’ or ‘font-feature-settings’.
    bool optimizeSpeed = fontDescription.textRenderingMode() == TextRenderingMode::OptimizeSpeed;
    bool shouldDisableLigaturesForSpacing = fontDescription.shouldDisableLigaturesForSpacing();

    // clig and liga are on by default in HarfBuzz.
    auto commonLigatures = fontDescription.variantCommonLigatures();
    if (shouldDisableLigaturesForSpacing || (commonLigatures == FontVariantLigatures::No || (commonLigatures == FontVariantLigatures::Normal && optimizeSpeed))) {
        featuresToBeApplied.set(fontFeatureTag("liga"), 0);
        featuresToBeApplied.set(fontFeatureTag("clig"), 0);
    }

    // dlig is off by default in HarfBuzz.
    auto discretionaryLigatures = fontDescription.variantDiscretionaryLigatures();
    if (!shouldDisableLigaturesForSpacing && discretionaryLigatures == FontVariantLigatures::Yes)
        featuresToBeApplied.set(fontFeatureTag("dlig"), 1);

    // hlig is off by default in HarfBuzz.
    auto historicalLigatures = fontDescription.variantHistoricalLigatures();
    if (!shouldDisableLigaturesForSpacing && historicalLigatures == FontVariantLigatures::Yes)
        featuresToBeApplied.set(fontFeatureTag("hlig"), 1);

    // calt is on by default in HarfBuzz.
    auto contextualAlternates = fontDescription.variantContextualAlternates();
    if (shouldDisableLigaturesForSpacing || (contextualAlternates == FontVariantLigatures::No || (contextualAlternates == FontVariantLigatures::Normal && optimizeSpeed)))
        featuresToBeApplied.set(fontFeatureTag("calt"), 0);

    switch (fontDescription.widthVariant()) {
    case FontWidthVariant::RegularWidth:
        break;
    case FontWidthVariant::HalfWidth:
        featuresToBeApplied.set(fontFeatureTag("hwid"), 1);
        break;
    case FontWidthVariant::ThirdWidth:
        featuresToBeApplied.set(fontFeatureTag("twid"), 1);
        break;
    case FontWidthVariant::QuarterWidth:
        featuresToBeApplied.set(fontFeatureTag("qwid"), 1);
        break;
    }

    switch (fontDescription.variantEastAsianVariant()) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        featuresToBeApplied.set(fontFeatureTag("jp78"), 1);
        break;
    case FontVariantEastAsianVariant::Jis83:
        featuresToBeApplied.set(fontFeatureTag("jp83"), 1);
        break;
    case FontVariantEastAsianVariant::Jis90:
        featuresToBeApplied.set(fontFeatureTag("jp90"), 1);
        break;
    case FontVariantEastAsianVariant::Jis04:
        featuresToBeApplied.set(fontFeatureTag("jp04"), 1);
        break;
    case FontVariantEastAsianVariant::Simplified:
        featuresToBeApplied.set(fontFeatureTag("smpl"), 1);
        break;
    case FontVariantEastAsianVariant::Traditional:
        featuresToBeApplied.set(fontFeatureTag("trad"), 1);
        break;
    }

    switch (fontDescription.variantEastAsianWidth()) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        featuresToBeApplied.set(fontFeatureTag("fwid"), 1);
        break;
    case FontVariantEastAsianWidth::Proportional:
        featuresToBeApplied.set(fontFeatureTag("pwid"), 1);
        break;
    }

    switch (fontDescription.variantEastAsianRuby()) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        featuresToBeApplied.set(fontFeatureTag("ruby"), 1);
        break;
    }

    switch (fontDescription.variantNumericFigure()) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        featuresToBeApplied.set(fontFeatureTag("lnum"), 1);
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        featuresToBeApplied.set(fontFeatureTag("onum"), 1);
        break;
    }

    switch (fontDescription.variantNumericSpacing()) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        featuresToBeApplied.set(fontFeatureTag("pnum"), 1);
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        featuresToBeApplied.set(fontFeatureTag("tnum"), 1);
        break;
    }

    switch (fontDescription.variantNumericFraction()) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        featuresToBeApplied.set(fontFeatureTag("frac"), 1);
        break;
    case FontVariantNumericFraction::StackedFractions:
        featuresToBeApplied.set(fontFeatureTag("afrc"), 1);
        break;
    }

    if (fontDescription.variantNumericOrdinal() == FontVariantNumericOrdinal::Yes)
        featuresToBeApplied.set(fontFeatureTag("ordn"), 1);

    if (fontDescription.variantNumericSlashedZero() == FontVariantNumericSlashedZero::Yes)
        featuresToBeApplied.set(fontFeatureTag("zero"), 1);

    // 5. Font features implied by the value of ‘font-feature-settings’ property.
    for (auto& newFeature : fontDescription.featureSettings())
        featuresToBeApplied.set(newFeature.tag(), newFeature.value());

    if (featuresToBeApplied.isEmpty())
        return { };

    Vector<hb_feature_t> features;
    features.reserveInitialCapacity(featuresToBeApplied.size());
    for (const auto& iter : featuresToBeApplied)
        features.append({ HB_TAG(iter.key[0], iter.key[1], iter.key[2], iter.key[3]), static_cast<uint32_t>(iter.value), 0, static_cast<unsigned>(-1) });
    return features;
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomString& family, const FontCreationContext& fontCreationContext, OptionSet<FontLookupOptions> options)
{
    auto familyName = getFamilyNameStringFromFamily(family);
    auto typeface = fontManager().matchFamilyStyle(familyName.utf8().data(), skiaFontStyle(fontDescription));
    if (!typeface)
        return nullptr;

    auto size = fontDescription.adjustedSizeForFontFace(fontCreationContext.sizeAdjust());
    auto features = computeFeatures(fontDescription, fontCreationContext);
    UNUSED_PARAM(options);
    FontPlatformData platformData(WTFMove(typeface), size, false /* syntheticBold */, false /* syntheticOblique */, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode(), WTFMove(features));

    platformData.updateSizeWithFontSizeAdjust(fontDescription.fontSizeAdjust(), fontDescription.computedSize());
    auto platformDataUniquePtr = makeUnique<FontPlatformData>(platformData);

    return platformDataUniquePtr;
}

std::optional<ASCIILiteral> FontCache::platformAlternateFamilyName(const String&)
{
    return std::nullopt;
}

void FontCache::platformInvalidate()
{
}

void FontCache::platformPurgeInactiveFontData()
{
    m_harfBuzzFontCache.clear();
}

} // namespace WebCore
