/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UnrealizedCoreTextFont.h"

#include "FontCacheCoreText.h"
#include "FontCreationContext.h"
#include "FontDescription.h"
#include "FontInterrogation.h"
#include "FontMetricsNormalization.h"
#include <CoreFoundation/CoreFoundation.h>
#include <optional>
#include <pal/spi/cf/CoreTextSPI.h>

namespace WebCore {

static std::optional<CGFloat> getCGFloatValue(CFNumberRef number)
{
    ASSERT(number);
    ASSERT(CFGetTypeID(number) == CFNumberGetTypeID());
    CGFloat value = 0;
    auto success = CFNumberGetValue(number, kCFNumberCGFloatType, &value);
    if (success && value)
        return value;
    return std::nullopt;
}

CGFloat UnrealizedCoreTextFont::getSize() const
{
    if (auto sizeAttribute = static_cast<CFNumberRef>(CFDictionaryGetValue(m_attributes.get(), kCTFontSizeAttribute))) {
        if (auto result = getCGFloatValue(sizeAttribute))
            return *result;
    }

    auto sizeAttribute = WTF::switchOn(m_baseFont, [](const RetainPtr<CTFontRef>& font) {
        return adoptCF(static_cast<CFNumberRef>(CTFontCopyAttribute(font.get(), kCTFontSizeAttribute)));
    }, [](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) {
        return adoptCF(static_cast<CFNumberRef>(CTFontDescriptorCopyAttribute(fontDescriptor.get(), kCTFontSizeAttribute)));
    });
    if (sizeAttribute) {
        if (auto result = getCGFloatValue(sizeAttribute.get()))
            return *result;
    }

    return 0;
}

UnrealizedCoreTextFont::operator bool() const
{
    return WTF::switchOn(m_baseFont, [](const RetainPtr<CTFontRef>& font) -> bool {
        return font;
    }, [](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) -> bool {
        return fontDescriptor;
    });
}

void UnrealizedCoreTextFont::addAttributesForOpticalSizing(CFMutableDictionaryRef attributes, VariationsMap& variationsToBeApplied, const OpticalSizingType& opticalSizingType, CGFloat size)
{
    WTF::switchOn(opticalSizingType, [&](OpticalSizingTypes::None) {
        CFDictionarySetValue(attributes, kCTFontOpticalSizeAttribute, CFSTR("none"));
    }, [&](OpticalSizingTypes::JustVariation) {
#if USE(VARIABLE_OPTICAL_SIZING)
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=252592 We should never be enabling just the opsz variation without also enabling trak.
        // We should delete this and use the OpticalSizingType::Everything path instead.
        variationsToBeApplied.set({ { 'o', 'p', 's', 'z' } }, size);
#else
        UNUSED_PARAM(variationsToBeApplied);
        UNUSED_PARAM(size);
#endif
    }, [&](const OpticalSizingTypes::Everything& everything) {
        if (everything.opticalSizingValue) {
            auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &everything.opticalSizingValue.value()));
            CFDictionarySetValue(attributes, kCTFontOpticalSizeAttribute, number.get());
        } else
            CFDictionarySetValue(attributes, kCTFontOpticalSizeAttribute, CFSTR("auto"));
    });
}

static inline void appendOpenTypeFeature(CFMutableArrayRef features, const FontFeature& feature)
{
    auto featureKey = adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(feature.tag().data()), feature.tag().size() * sizeof(FontTag::value_type), kCFStringEncodingASCII, false));
    int rawFeatureValue = feature.value();
    auto featureValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawFeatureValue));
    CFTypeRef featureDictionaryKeys[] = { kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue };
    CFTypeRef featureDictionaryValues[] = { featureKey.get(), featureValue.get() };
    auto featureDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureDictionaryKeys, featureDictionaryValues, std::size(featureDictionaryValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFArrayAppendValue(features, featureDictionary.get());
}

static void addLightPalette(CFMutableDictionaryRef attributes)
{
    CFIndex light = kCTFontPaletteLight;
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &light));
    CFDictionarySetValue(attributes, kCTFontPaletteAttribute, number.get());
}

static void addDarkPalette(CFMutableDictionaryRef attributes)
{
    CFIndex dark = kCTFontPaletteDark;
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &dark));
    CFDictionarySetValue(attributes, kCTFontPaletteAttribute, number.get());
}

static void addAttributesForCustomFontPalettes(CFMutableDictionaryRef attributes, std::optional<FontPaletteIndex> basePalette, const Vector<FontPaletteValues::OverriddenColor>& overrideColors)
{
    if (basePalette) {
        switch (basePalette->type) {
        case FontPaletteIndex::Type::Light:
            addLightPalette(attributes);
            break;
        case FontPaletteIndex::Type::Dark:
            addDarkPalette(attributes);
            break;
        case FontPaletteIndex::Type::Integer: {
            int64_t rawIndex = basePalette->integer; // There is no kCFNumberUIntType.
            auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &rawIndex));
            CFDictionarySetValue(attributes, kCTFontPaletteAttribute, number.get());
            break;
        }
        }
    }

    if (!overrideColors.isEmpty()) {
        auto overrideDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        for (const auto& pair : overrideColors) {
            const auto& color = pair.second;
            int64_t rawIndex = pair.first; // There is no kCFNumberUIntType.
            auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &rawIndex));
            auto colorObject = cachedCGColor(color);
            CFDictionarySetValue(overrideDictionary.get(), number.get(), colorObject.get());
        }
        if (CFDictionaryGetCount(overrideDictionary.get()))
            CFDictionarySetValue(attributes, kCTFontPaletteColorsAttribute, overrideDictionary.get());
    }
}

static void addAttributesForFontPalettes(CFMutableDictionaryRef attributes, const FontPalette& fontPalette, const FontPaletteValues* fontPaletteValues)
{
    switch (fontPalette.type) {
    case FontPalette::Type::Normal:
        break;
    case FontPalette::Type::Light:
        addLightPalette(attributes);
        break;
    case FontPalette::Type::Dark:
        addDarkPalette(attributes);
        break;
    case FontPalette::Type::Custom: {
        if (fontPaletteValues)
            addAttributesForCustomFontPalettes(attributes, fontPaletteValues->basePalette(), fontPaletteValues->overrideColors());
        break;
    }
    }
}

static void applyFeatures(CFMutableDictionaryRef attributes, const FeaturesMap& featuresToBeApplied)
{
    if (featuresToBeApplied.isEmpty())
        return;

    RetainPtr<CFMutableArrayRef> featureArray;
    if (auto fontFeatureSettings = static_cast<CFArrayRef>(CFDictionaryGetValue(attributes, kCTFontFeatureSettingsAttribute)))
        featureArray = adoptCF(CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, fontFeatureSettings));
    else
        featureArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));

    for (auto& p : featuresToBeApplied) {
        auto feature = FontFeature(p.key, p.value);
        appendOpenTypeFeature(featureArray.get(), feature);
    }

    CFDictionarySetValue(attributes, kCTFontFeatureSettingsAttribute, featureArray.get());
}

void UnrealizedCoreTextFont::applyVariations(CFMutableDictionaryRef attributes, const VariationsMap& variationsToBeApplied)
{
    if (variationsToBeApplied.isEmpty())
        return;

    RetainPtr<CFMutableDictionaryRef> variationDictionary;
    if (auto fontVariations = static_cast<CFDictionaryRef>(CFDictionaryGetValue(attributes, kCTFontVariationAttribute)))
        variationDictionary = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, fontVariations));
    else
        variationDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    for (auto& p : variationsToBeApplied) {
        long long bitwiseTag = p.key[0] << 24 | p.key[1] << 16 | p.key[2] << 8 | p.key[3];
        auto tagNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &bitwiseTag));
        auto valueNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &p.value));
        CFDictionarySetValue(variationDictionary.get(), tagNumber.get(), valueNumber.get());
    }

    CFDictionarySetValue(attributes, kCTFontVariationAttribute, variationDictionary.get());
}

void UnrealizedCoreTextFont::modifyFromContext(CFMutableDictionaryRef attributes, const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, ApplyTraitsVariations applyTraitsVariations, float weight, float width, float slope, CGFloat size, const OpticalSizingType& opticalSizingType)
{
    auto fontStyleAxis = fontDescription.fontStyleAxis();
    const auto& variations = fontDescription.variationSettings();
    const auto& features = fontDescription.featureSettings();
    const auto& variantSettings = fontDescription.variantSettings();
    auto textRenderingMode = fontDescription.textRenderingMode();
    auto shouldDisableLigaturesForSpacing = fontDescription.shouldDisableLigaturesForSpacing();

    // This algorithm is described at https://drafts.csswg.org/css-fonts-4/#feature-variation-precedence
    FeaturesMap featuresToBeApplied;
    VariationsMap variationsToBeApplied;

    // The system font is somewhat magical. Don't mess with its variations.
    if (applyTraitsVariations == ApplyTraitsVariations::Yes) {
        variationsToBeApplied.set({ { 'w', 'g', 'h', 't' } }, weight);
        variationsToBeApplied.set({ { 'w', 'd', 't', 'h' } }, width);
        if (fontStyleAxis == FontStyleAxis::ital)
            variationsToBeApplied.set({ { 'i', 't', 'a', 'l' } }, 1);
        else
            variationsToBeApplied.set({ { 's', 'l', 'n', 't' } }, slope);
    }

    // FIXME: Implement font-named-instance

    // FIXME: Implement the font-variation-settings descriptor inside @font-face

    if (fontCreationContext.fontFaceFeatures()) {
        for (auto& fontFaceFeature : *fontCreationContext.fontFaceFeatures())
            featuresToBeApplied.set(fontFaceFeature.tag(), fontFaceFeature.value());
    }

    addAttributesForOpticalSizing(attributes, variationsToBeApplied, opticalSizingType, size);

    for (auto& newFeature : computeFeatureSettingsFromVariants(variantSettings, fontCreationContext.fontFeatureValues()))
        featuresToBeApplied.set(newFeature.key, newFeature.value);

    if (textRenderingMode == TextRenderingMode::OptimizeSpeed) {
        featuresToBeApplied.set(fontFeatureTag("liga"), 0);
        featuresToBeApplied.set(fontFeatureTag("clig"), 0);
        featuresToBeApplied.set(fontFeatureTag("dlig"), 0);
        featuresToBeApplied.set(fontFeatureTag("hlig"), 0);
        featuresToBeApplied.set(fontFeatureTag("calt"), 0);
    }
    if (shouldDisableLigaturesForSpacing) {
        featuresToBeApplied.set(fontFeatureTag("liga"), 0);
        featuresToBeApplied.set(fontFeatureTag("clig"), 0);
        featuresToBeApplied.set(fontFeatureTag("dlig"), 0);
        featuresToBeApplied.set(fontFeatureTag("hlig"), 0);
        // Core Text doesn't disable calt when letter-spacing is applied, so we won't either.
    }

    for (auto& newFeature : features)
        featuresToBeApplied.set(newFeature.tag(), newFeature.value());

    for (auto& newVariation : variations)
        variationsToBeApplied.set(newVariation.tag(), newVariation.value());

    applyFeatures(attributes, featuresToBeApplied);
    applyVariations(attributes, variationsToBeApplied);

    addAttributesForFontPalettes(attributes, fontDescription.fontPalette(), fontCreationContext.fontPaletteValues());

    addAttributesForInstalledFonts(attributes, fontDescription.shouldAllowUserInstalledFonts());
}

void UnrealizedCoreTextFont::modifyFromContext(const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, FontTypeForPreparation fontTypeForPreparation, ApplyTraitsVariations applyTraitsVariations, bool shouldEnhanceTextLegibility)
{
    m_applyTraitsVariations = applyTraitsVariations;

    if (m_applyTraitsVariations == ApplyTraitsVariations::Yes) {
        auto fontSelectionRequest = fontDescription.fontSelectionRequest();
        m_weight = fontSelectionRequest.weight;
        m_width = fontSelectionRequest.width;
        m_slope = fontSelectionRequest.slope.value_or(normalItalicValue());
        m_fontStyleAxis = fontDescription.fontStyleAxis();
        if (auto weightValue = fontCreationContext.fontFaceCapabilities().weight)
            m_weight = std::max(std::min(m_weight, static_cast<float>(weightValue->maximum)), static_cast<float>(weightValue->minimum));
        if (auto widthValue = fontCreationContext.fontFaceCapabilities().width)
            m_width = std::max(std::min(m_width, static_cast<float>(widthValue->maximum)), static_cast<float>(widthValue->minimum));
        if (auto slopeValue = fontCreationContext.fontFaceCapabilities().weight)
            m_slope = std::max(std::min(m_slope, static_cast<float>(slopeValue->maximum)), static_cast<float>(slopeValue->minimum));
        if (shouldEnhanceTextLegibility && fontTypeForPreparation == FontTypeForPreparation::SystemFont) {
            auto ctWeight = denormalizeCTWeight(m_weight);
            ctWeight = CTFontGetAccessibilityBoldWeightOfWeight(ctWeight);
            m_weight = normalizeCTWeight(ctWeight);
        }
    }

    m_size = getSize();

    m_variationSettings = fontDescription.variationSettings();

    if (fontDescription.textRenderingMode() == TextRenderingMode::OptimizeLegibility) {
        std::optional<float> opticalSizingValue;
#if USE(CORE_TEXT_OPTICAL_SIZING_WORKAROUND)
        // We can delete this when rdar://problem/104370451 is fixed.
        // And then we can change OpticalSizingType back to an enum instead of a variant.
        auto iterator = std::find_if(std::begin(m_variationSettings), std::end(m_variationSettings), [opsz = fontFeatureTag("opsz")](const FontVariationSettings::Setting& setting) {
            return setting.tag() == opsz;
        });
        if (iterator != m_variationSettings.end()) {
            // Core Text's auto optical sizing clobbers whatever font-variation-settings says, which is not how the
            // web is supposed to work, so we can explicitly specify a value for Core Text to use.
            opticalSizingValue = iterator->value();
        }
#endif
        m_opticalSizingType = OpticalSizingTypes::Everything { WTFMove(opticalSizingValue) };
    } else if (fontDescription.opticalSizing() == FontOpticalSizing::Disabled)
        m_opticalSizingType = OpticalSizingTypes::None { };
    else
        m_opticalSizingType = OpticalSizingTypes::JustVariation { };

    modify([&](CFMutableDictionaryRef attributes) {
        modifyFromContext(attributes, fontDescription, fontCreationContext, m_applyTraitsVariations, m_weight, m_width, m_slope, m_size, m_opticalSizingType);
    });
}

// The purpose of this function is to determine whether we need to rebuild the font.
// We need to rebuild the font if naively creating a big dictionary of attributes and giving it to Core Text
// doesn't end up creating the font that CSS requires to be created.
// The common case will be that we usually don't have to rebuild fonts.
// Once rdar://problem/105483251 and rdar://problem/108877507 are solved, we can delete this function and the whole font rebuilding codepath.
auto UnrealizedCoreTextFont::rebuildReason(CTFontRef font) const -> RebuildReason
{
    RebuildReason rebuildReason;
    std::optional<VariationDefaultsMap> lazyDefaultVariations;

    // Both OpenType 1.8 and TrueType GX support font variations, using the same named keys and values.
    // However, the scale is different between the two: OpenType uses the CSS scale, whereas TrueType
    // uses a 0-1-2 scale.
    //
    // However, we don't actually know whether the font is a TrueType GX font or not until we create a
    // CTFont. Now, after we've created the font, we can check to see whether or not our guess was correct.
    // If it wasn't, we have to recreate the font with the normalized values.
    if (m_applyTraitsVariations == ApplyTraitsVariations::Yes && FontInterrogation(font).variationType == FontInterrogation::VariationType::TrueTypeGX) {
        lazyDefaultVariations = defaultVariationValues(font, ShouldLocalizeAxisNames::No);
        if (lazyDefaultVariations->contains({ { 'w', 'g', 'h', 't' } })
            || lazyDefaultVariations->contains({ { 'w', 'd', 't', 'h' } })
            || lazyDefaultVariations->contains({ { 'i', 't', 'a', 'l' } })
            || lazyDefaultVariations->contains({ { 's', 'l', 'n', 't' } }))
            rebuildReason.gxVariations = true;
    }

    // On some OSes, all out-of-bounds variations need to be manually clamped.
    // Here, our job is just to detect if an out of bounds value exists.
    // If one does, we'll set rebuildReason.variationDefaults, which realize() will use to actually do the clamping.
    // When the fix for rdar://problem/108877507 is deployed everywhere, we can delete this.
#if USE(CORE_TEXT_VARIATIONS_CLAMPING_WORKAROUND)
    if (!lazyDefaultVariations)
        lazyDefaultVariations = defaultVariationValues(font, ShouldLocalizeAxisNames::No);

    auto isOutOfBounds = [&lazyDefaultVariations](FontTag fontTag, float value) {
        auto iterator = lazyDefaultVariations->find(fontTag);
        if (iterator == lazyDefaultVariations->end()) {
            // We can't be out of bounds if there is no range.
            return false;
        }
        return !iterator->value.contains(value);
    };

    auto isOpticalSizeOutOfBounds = [&isOutOfBounds](float value) {
        return isOutOfBounds({ { 'o', 'p', 's', 'z' } }, value);
    };

    WTF::switchOn(m_opticalSizingType, [&](OpticalSizingTypes::None) {
        // We can't be out of bounds if there is no value.
    }, [&](OpticalSizingTypes::JustVariation) {
        if (isOpticalSizeOutOfBounds(m_size))
            rebuildReason.variationDefaults = lazyDefaultVariations;
    }, [&](const OpticalSizingTypes::Everything& everything) {
        if (!everything.opticalSizingValue) {
            // We can't be out of bounds if there is no value.
            return;
        }
        if (isOpticalSizeOutOfBounds(*everything.opticalSizingValue))
            rebuildReason.variationDefaults = lazyDefaultVariations;
    });

    if (!rebuildReason.variationDefaults) {
        for (const auto& variationSetting : m_variationSettings) {
            if (isOutOfBounds(variationSetting.tag(), variationSetting.value())) {
                rebuildReason.variationDefaults = lazyDefaultVariations;
                break;
            }
        }
    }
#endif

    return rebuildReason;
}

RetainPtr<CTFontRef> UnrealizedCoreTextFont::realize() const
{
    if (!static_cast<bool>(*this))
        return nullptr;

    auto font = WTF::switchOn(m_baseFont, [this](const RetainPtr<CTFontRef>& font) -> RetainPtr<CTFontRef> {
        if (!font)
            return nullptr;
        if (!CFDictionaryGetCount(m_attributes.get()))
            return font;
        auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(m_attributes.get()));
        return adoptCF(CTFontCreateCopyWithAttributes(font.get(), m_size, nullptr, modification.get()));
    }, [this](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) -> RetainPtr<CTFontRef> {
        if (!fontDescriptor)
            return nullptr;
        if (!CFDictionaryGetCount(m_attributes.get()))
            return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), m_size, nullptr));
        auto updatedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), m_attributes.get()));
        return adoptCF(CTFontCreateWithFontDescriptor(updatedFontDescriptor.get(), m_size, nullptr));
    });
    ASSERT(font);

    if (auto rebuildReason = this->rebuildReason(font.get()); rebuildReason.hasEffect()) {
        VariationsMap variationsToBeApplied;
        std::optional<VariationDefaultsMap> variationDefaults = WTFMove(rebuildReason.variationDefaults);
#if USE(CORE_TEXT_VARIATIONS_CLAMPING_WORKAROUND)
        // Even if everything was in-bounds, we still have to clamp,
        // because normalization might put us out-of-bounds.
        if (!variationDefaults)
            variationDefaults = defaultVariationValues(font.get(), ShouldLocalizeAxisNames::No);
#endif

        auto maybeClampToRange = [&variationDefaults](FontTag fontTag, float value) -> std::optional<float> {
#if USE(CORE_TEXT_VARIATIONS_CLAMPING_WORKAROUND)
            ASSERT(variationDefaults);
            auto iterator = variationDefaults->find(fontTag);
            if (iterator == variationDefaults->end()) {
                // The font doesn't support this variation axis.
                return { };
            }
            return iterator->value.clamp(value);
#else
            UNUSED_PARAM(variationDefaults);
            UNUSED_PARAM(fontTag);
            return value;
#endif
        };

        auto setVariation = [&variationsToBeApplied, &maybeClampToRange](FontTag fontTag, float value) {
            if (auto clamped = maybeClampToRange(fontTag, value))
                variationsToBeApplied.set(fontTag, value);
        };

        auto weight = m_weight;
        auto width = m_width;
        auto slope = m_slope;

        if (rebuildReason.gxVariations) {
            weight = denormalizeGXWeight(m_weight);
            width = denormalizeVariationWidth(m_width);
            slope = denormalizeSlope(m_slope);
        }

        {
            setVariation({ { 'w', 'g', 'h', 't' } }, weight);
            setVariation({ { 'w', 'd', 't', 'h' } }, width);
            if (m_fontStyleAxis == FontStyleAxis::ital)
                setVariation({ { 'i', 't', 'a', 'l' } }, 1);
            else
                setVariation({ { 's', 'l', 'n', 't' } }, slope);
        }

        auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        {
            auto maybeClampToOpticalSizingRange = [&maybeClampToRange](float value) {
                return maybeClampToRange({ { 'o', 'p', 's', 'z' } }, value);
            };

            OpticalSizingType usedOpticalSizingType = m_opticalSizingType;
            CGFloat usedSize = m_size;

            WTF::switchOn(m_opticalSizingType, [&](OpticalSizingTypes::None) {
                // No optical sizing means there's no value that needs to be clamped.
            }, [&](OpticalSizingTypes::JustVariation) {
                if (auto clampedValue = maybeClampToOpticalSizingRange(m_size))
                    usedSize = *clampedValue;
            }, [&](const OpticalSizingTypes::Everything& everything) {
                if (!everything.opticalSizingValue) {
                    // Auto optical sizing means there's no value that needs to be clamped.
                    return;
                }
                if (auto clampedValue = maybeClampToOpticalSizingRange(*everything.opticalSizingValue))
                    usedOpticalSizingType = OpticalSizingTypes::Everything { *clampedValue };
            });
            addAttributesForOpticalSizing(attributes.get(), variationsToBeApplied, usedOpticalSizingType, usedSize);
        }

        {
            for (auto& variationSetting : m_variationSettings)
                setVariation(variationSetting.tag(), variationSetting.value());

            applyVariations(attributes.get(), variationsToBeApplied);
        }

        auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
        return adoptCF(CTFontCreateCopyWithAttributes(font.get(), m_size, nullptr, modification.get()));
    }

    return font;
}

} // namespace WebCore
