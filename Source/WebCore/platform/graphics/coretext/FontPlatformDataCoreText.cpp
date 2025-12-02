/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "FontPlatformData.h"

#include "Font.h"
#include "FontCustomPlatformData.h"
#include "SharedBuffer.h"
#include <CoreText/CoreText.h>
#include <WebCore/Font.h>
#include <pal/spi/cf/CoreTextSPI.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

inline int mapFontWidthVariantToCTFeatureSelector(FontWidthVariant variant)
{
    switch (variant) {
    case FontWidthVariant::RegularWidth:
        return kProportionalTextSelector;

    case FontWidthVariant::HalfWidth:
        return kHalfWidthTextSelector;

    case FontWidthVariant::ThirdWidth:
        return kThirdWidthTextSelector;

    case FontWidthVariant::QuarterWidth:
        return kQuarterWidthTextSelector;
    }

    ASSERT_NOT_REACHED();
    return kProportionalTextSelector;
}

FontPlatformData::FontPlatformData(RetainPtr<CTFontRef>&& font, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode, const FontCustomPlatformData* customPlatformData)
    : FontPlatformData(size, syntheticBold, syntheticOblique, orientation, widthVariant, textRenderingMode, customPlatformData)
{
    ASSERT_ARG(font, font);
    m_font = WTFMove(font);
    m_isColorBitmapFont = CTFontGetSymbolicTraits(m_font.get()) & kCTFontColorGlyphsTrait;
    m_isSystemFont = WebCore::isSystemFont(m_font.get());
    auto variations = adoptCF(checked_cf_cast<CFDictionaryRef>(CTFontCopyAttribute(m_font.get(), kCTFontVariationAttribute)));
    m_hasVariations = variations && CFDictionaryGetCount(variations.get());

#if PLATFORM(IOS_FAMILY)
    m_isEmoji = CTFontIsAppleColorEmoji(m_font.get());
#endif

    if (m_widthVariant != FontWidthVariant::RegularWidth) {
        // FIXME: Do something smarter than creating the CTFontRef twice <webkit.org/b/276635>
        int featureTypeValue = kTextSpacingType;
        int featureSelectorValue = mapFontWidthVariantToCTFeatureSelector(m_widthVariant);
        RetainPtr<CTFontDescriptorRef> sourceDescriptor = adoptCF(CTFontCopyFontDescriptor(m_font.get()));
        RetainPtr<CFNumberRef> featureType = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureTypeValue));
        RetainPtr<CFNumberRef> featureSelector = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureSelectorValue));
        RetainPtr<CTFontDescriptorRef> newDescriptor = adoptCF(CTFontDescriptorCreateCopyWithFeature(sourceDescriptor.get(), featureType.get(), featureSelector.get()));
        RetainPtr<CTFontRef> newFont = adoptCF(CTFontCreateWithFontDescriptor(newDescriptor.get(), m_size, 0));

        if (newFont)
            m_font = WTFMove(newFont);
    }
}

static RetainPtr<CTFontDescriptorRef> findFontDescriptor(CFURLRef url, CFStringRef postScriptName)
{
    if (!url)
        return nullptr;
    auto fontDescriptors = adoptCF(CTFontManagerCreateFontDescriptorsFromURL(url));
    if (!fontDescriptors || !CFArrayGetCount(fontDescriptors.get()))
        return nullptr;
    if (CFArrayGetCount(fontDescriptors.get()) == 1)
        return checked_cf_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors.get(), 0));

    for (CFIndex i = 0; i < CFArrayGetCount(fontDescriptors.get()); ++i) {
        RetainPtr fontDescriptor = checked_cf_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors.get(), i));
        RetainPtr currentPostScriptName = adoptCF(CTFontDescriptorCopyAttribute(fontDescriptor.get(), kCTFontNameAttribute));
        if (CFEqual(currentPostScriptName.get(), postScriptName))
            return fontDescriptor;
    }
    return nullptr;
}

RetainPtr<CTFontRef> createCTFont(CFDictionaryRef attributes, float size, CTFontDescriptorOptions options, CFStringRef referenceURL, CFStringRef desiredPostScriptName)
{
    auto desiredReferenceURL = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, referenceURL, nullptr));

    auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithAttributesAndOptions(attributes, options));
    if (fontDescriptor) {
        RetainPtr font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));
        RetainPtr actualPostScriptName = adoptCF(CTFontCopyPostScriptName(font.get()));
        RetainPtr actualReferenceURL = adoptCF(CTFontCopyAttribute(font.get(), kCTFontReferenceURLAttribute));
        if (safeCFEqual(actualPostScriptName.get(), desiredPostScriptName) && safeCFEqual(desiredReferenceURL.get(), actualReferenceURL.get()))
            return font;
    }

    // CoreText couldn't round-trip the font.
    // We can fall back to doing our best to find it ourself.
    fontDescriptor = findFontDescriptor(desiredReferenceURL.get(), desiredPostScriptName);
    if (fontDescriptor)
        fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), attributes));
    else
        fontDescriptor = adoptCF(CTFontDescriptorCreateLastResort());
    ASSERT(fontDescriptor);
    return adoptCF(CTFontCreateWithFontDescriptorAndOptions(fontDescriptor.get(), size, nullptr, options));
}

FontPlatformData FontPlatformData::create(const Attributes& data, const FontCustomPlatformData* custom)
{
    RetainPtr<CTFontRef> ctFont;
    if (custom) {
        RetainPtr baseFontDescriptor = custom->fontDescriptor.get();
        RELEASE_ASSERT(baseFontDescriptor);
        RetainPtr fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(baseFontDescriptor.get(), data.m_attributes.get()));
        ctFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), data.m_size, nullptr));
    } else
        ctFont = createCTFont(data.m_attributes.get(), data.m_size, data.m_options, data.m_url.get(), data.m_psName.get());

    return WebCore::FontPlatformData(ctFont.get(), data.m_size, data.m_syntheticBold, data.m_syntheticOblique, data.m_orientation, data.m_widthVariant, data.m_textRenderingMode, custom);
}

bool isSystemFont(CTFontRef font)
{
    return CTFontIsSystemUIFont(font);
}

RetainPtr<CTFontRef> FontPlatformData::registeredFont() const
{
    RetainPtr platformFont = ctFont();
    ASSERT(platformFont);
    if (platformFont && adoptCF(CTFontCopyAttribute(platformFont.get(), kCTFontURLAttribute)))
        return platformFont;
    return nullptr;
}

RetainPtr<CFTypeRef> FontPlatformData::objectForEqualityCheck(CTFontRef ctFont)
{
    auto fontDescriptor = adoptCF(CTFontCopyFontDescriptor(ctFont));
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=138683 This is a shallow pointer compare for web fonts
    // because the URL contains the address of the font. This means we might erroneously get false negatives.
    auto object = adoptCF(CTFontDescriptorCopyAttribute(fontDescriptor.get(), kCTFontReferenceURLAttribute));
    ASSERT(!object || CFGetTypeID(object.get()) == CFURLGetTypeID());
    return object;
}

RetainPtr<CFTypeRef> FontPlatformData::objectForEqualityCheck() const
{
    return objectForEqualityCheck(protectedCTFont().get());
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    if (RetainPtr<CFDataRef> data = adoptCF(CTFontCopyTable(protectedCTFont().get(), table, kCTFontTableOptionNoOptions)))
        return SharedBuffer::create(data.get());

    return platformOpenTypeTable(table);
}

#if !LOG_DISABLED

String FontPlatformData::description() const
{
    String fontDescription { adoptCF(CFCopyDescription(ctFont())).get() };
    return makeString(fontDescription, ' ', m_size,
        (m_syntheticBold ? " synthetic bold"_s : ""_s),
        (m_syntheticOblique ? " synthetic oblique"_s : ""_s),
        (m_orientation == FontOrientation::Vertical ? " vertical orientation"_s : ""_s));
}

#endif

String FontPlatformData::familyName() const
{
    if (RetainPtr platformFont = ctFont())
        return adoptCF(CTFontCopyFamilyName(platformFont.get())).get();
    return { };
}

FontPlatformData FontPlatformData::cloneWithSize(const FontPlatformData& source, float size)
{
    FontPlatformData copy(source);
    copy.updateSize(size);
    return copy;
}

void FontPlatformData::updateSize(float size)
{
    m_size = size;
    ASSERT(m_font.get());
    m_font = adoptCF(CTFontCreateCopyWithAttributes(m_font.get(), m_size, nullptr, nullptr));
}

FontPlatformData::Attributes FontPlatformData::attributes() const
{
    Attributes result(m_size, m_orientation, m_widthVariant, m_textRenderingMode, m_syntheticBold, m_syntheticOblique);

    auto fontDescriptor = adoptCF(CTFontCopyFontDescriptor(m_font.get()));
    result.m_attributes = adoptCF(CTFontDescriptorCopyAttributes(fontDescriptor.get()));

    if (!m_customPlatformData) {
        result.m_options = CTFontDescriptorGetOptions(fontDescriptor.get());
        auto referenceURL = adoptCF(checked_cf_cast<CFURLRef>(CTFontCopyAttribute(m_font.get(), kCTFontReferenceURLAttribute)));
        result.m_url = CFURLGetString(referenceURL.get());
        result.m_psName = adoptCF(CTFontCopyPostScriptName(m_font.get()));
    }

    return result;
}

FontPlatformData::FontPlatformData(float size, WebCore::FontOrientation&& orientation, WebCore::FontWidthVariant&& widthVariant, WebCore::TextRenderingMode&& textRenderingMode, bool syntheticBold, bool syntheticOblique, RetainPtr<CTFontRef>&& font, RefPtr<FontCustomPlatformData>&& customPlatformData)
    : m_font(font)
    , m_size(size)
    , m_orientation(orientation)
    , m_widthVariant(widthVariant)
    , m_textRenderingMode(textRenderingMode)
    , m_customPlatformData(customPlatformData)
    , m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
{
    m_isColorBitmapFont = CTFontGetSymbolicTraits(m_font.get()) & kCTFontColorGlyphsTrait;
    m_isSystemFont = WebCore::isSystemFont(m_font.get());
    auto variations = adoptCF(checked_cf_cast<CFDictionaryRef>(CTFontCopyAttribute(m_font.get(), kCTFontVariationAttribute)));
    m_hasVariations = variations && CFDictionaryGetCount(variations.get());
#if PLATFORM(IOS_FAMILY)
    m_isEmoji = CTFontIsAppleColorEmoji(m_font.get());
#endif
}

FontPlatformData::IPCData FontPlatformData::toIPCData() const
{
    RetainPtr font = ctFont();
    RetainPtr fontDescriptor = adoptCF(CTFontCopyFontDescriptor(font.get()));
    RetainPtr attributes = adoptCF(CTFontDescriptorCopyAttributes(fontDescriptor.get()));

    const auto& data = creationData();
    if (data) {
        FontMetadata fontData = {
            CTFontGetSize(font.get()),
            orientation(),
            widthVariant(),
            textRenderingMode(),
            syntheticBold(),
            syntheticOblique()
        };

        return CustomFontCreationData { fontData, { data->fontFaceData->span() }, FontPlatformSerializedAttributes::fromCF(attributes.get()), data->itemInCollection };
    }

    auto options = CTFontDescriptorGetOptions(fontDescriptor.get());
    RetainPtr referenceURL = adoptCF(checked_cf_cast<CFURLRef>(CTFontCopyAttribute(font.get(), kCTFontReferenceURLAttribute)));
    RetainPtr urlString = retainPtr(CFURLGetString(referenceURL.get()));
    RetainPtr postScriptName = adoptCF(CTFontCopyPostScriptName(font.get())).get();
    return FontPlatformSerializedData { options, WTFMove(urlString), WTFMove(postScriptName), FontPlatformSerializedAttributes::fromCF(attributes.get()) };
}

#define EXTRACT_TYPED_VALUE(key, cfType, target) { \
    RetainPtr extractedValue = checked_cf_cast<cfType##Ref>(CFDictionaryGetValue(dictionary, key));\
    if (extractedValue && CFGetTypeID(extractedValue.get()) == cfType##GetTypeID())\
        target = extractedValue.get();\
    }

std::optional<FontPlatformSerializedAttributes> FontPlatformSerializedAttributes::fromCF(CFDictionaryRef dictionary)
{
    if (!dictionary)
        return std::nullopt;

    FontPlatformSerializedAttributes result;

    EXTRACT_TYPED_VALUE(kCTFontNameAttribute, CFString, result.fontName);
    EXTRACT_TYPED_VALUE(kCTFontDescriptorLanguageAttribute, CFString, result.descriptorLanguage);
    EXTRACT_TYPED_VALUE(kCTFontDescriptorTextStyleAttribute, CFString, result.descriptorTextStyle);

    EXTRACT_TYPED_VALUE(kCTFontMatrixAttribute, CFData, result.matrix);
    EXTRACT_TYPED_VALUE(kCTFontIgnoreLegibilityWeightAttribute, CFBoolean, result.ignoreLegibilityWeight);

    EXTRACT_TYPED_VALUE(kCTFontBaselineAdjustAttribute, CFNumber, result.baselineAdjust);
    EXTRACT_TYPED_VALUE(kCTFontFallbackOptionAttribute, CFNumber, result.fallbackOption);
    EXTRACT_TYPED_VALUE(kCTFontFixedAdvanceAttribute, CFNumber, result.fixedAdvance);
    EXTRACT_TYPED_VALUE(kCTFontOrientationAttribute, CFNumber, result.orientation);
    EXTRACT_TYPED_VALUE(kCTFontPaletteAttribute, CFNumber, result.palette);
    EXTRACT_TYPED_VALUE(kCTFontSizeAttribute, CFNumber, result.size);
    EXTRACT_TYPED_VALUE(kCTFontSizeCategoryAttribute, CFNumber, result.sizeCategory);
    EXTRACT_TYPED_VALUE(kCTFontTrackAttribute, CFNumber, result.track);
    EXTRACT_TYPED_VALUE(kCTFontUnscaledTrackingAttribute, CFNumber, result.unscaledTracking);

#if HAVE(ADDITIONAL_FONT_PLATFORM_SERIALIZED_ATTRIBUTES)
    EXTRACT_TYPED_VALUE(additionalFontPlatformSerializedAttributesNumberDictionaryKey(), CFNumber, result.additionalNumber);
#endif

    auto opticalSize = CFDictionaryGetValue(dictionary, kCTFontOpticalSizeAttribute);
    if (opticalSize)
        result.opticalSize = FontPlatformOpticalSize::fromCF(opticalSize);

    auto pairExtractor = []<typename ValueType> (CFDictionaryRef dictionary, CFTypeID valueCFType) -> std::optional<Vector<std::pair<RetainPtr<CFNumberRef>, RetainPtr<ValueType>>>> {
        if (!dictionary || CFGetTypeID(dictionary) != CFDictionaryGetTypeID())
            return std::nullopt;

        Vector<std::pair<RetainPtr<CFNumberRef>, RetainPtr<ValueType>>> vector;

        CFIndex count = CFDictionaryGetCount(dictionary);
        Vector<void*> keys(count);
        Vector<void*> values(count);
        CFDictionaryGetKeysAndValues(dictionary, const_cast<const void**>(keys.span().data()), const_cast<const void**>(values.span().data()));

        for (CFIndex i = 0; i < count; ++i) {
            RetainPtr key = checked_cf_cast<CFNumberRef>(keys[i]);
            if (CFGetTypeID(key.get()) != CFNumberGetTypeID())
                continue;
            auto value = static_cast<ValueType>(values[i]);
            if (CFGetTypeID(value) != valueCFType)
                continue;

            vector.append({ key, value });
        }

        return WTFMove(vector);
    };

    RetainPtr paletteColors = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(dictionary, kCTFontPaletteColorsAttribute));
    result.paletteColors = pairExtractor.template operator()<CGColorRef>(paletteColors.get(), CGColorGetTypeID());

    RetainPtr variations = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(dictionary, kCTFontVariationAttribute));
    result.variations = pairExtractor.template operator()<CFNumberRef>(variations.get(), CFNumberGetTypeID());

    RetainPtr traits = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(dictionary, kCTFontTraitsAttribute));
    if (traits && CFGetTypeID(traits.get()) == CFDictionaryGetTypeID())
        result.traits = FontPlatformSerializedTraits::fromCF(traits.get());

    RetainPtr settings = checked_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary, kCTFontFeatureSettingsAttribute));
    if (settings && CFGetTypeID(settings.get()) == CFArrayGetTypeID()) {
        Vector<FontPlatformFeatureSetting> featureSettings;
        for (CFIndex i = 0; i < CFArrayGetCount(settings.get()); ++i) {
            RetainPtr object = CFArrayGetValueAtIndex(settings.get(), i);
            if (CFGetTypeID(object.get()) == CFDictionaryGetTypeID()) {
                featureSettings.append(FontPlatformFeatureSetting {
                    checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(checked_cf_cast<CFDictionaryRef>(object.get()), kCTFontFeatureTypeIdentifierKey)),
                    checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(checked_cf_cast<CFDictionaryRef>(object.get()), kCTFontFeatureSelectorIdentifierKey)),
                    checked_cf_cast<CFStringRef>(CFDictionaryGetValue(checked_cf_cast<CFDictionaryRef>(object.get()), kCTFontOpenTypeFeatureTag)),
                    checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(checked_cf_cast<CFDictionaryRef>(object.get()), kCTFontOpenTypeFeatureValue))
                });
            } else {
                RetainPtr<CFTypeRef> typeOrTag = nullptr;
                RetainPtr<CFNumberRef> selectorOrValue = nullptr;
                if (CFGetTypeID(object.get()) == CFArrayGetTypeID()) {
                    RetainPtr array = checked_cf_cast<CFArrayRef>(object.get());
                    CFIndex count = CFArrayGetCount(array.get());
                    if (!count)
                        continue;

                    typeOrTag = CFArrayGetValueAtIndex(array.get(), 0);
                    if (count > 1)
                        selectorOrValue = checked_cf_cast<CFNumberRef>(CFArrayGetValueAtIndex(array.get(), 1));
                }

                if (!typeOrTag)
                    continue;

                if (CFGetTypeID(typeOrTag.get()) == CFNumberGetTypeID()) {
                    featureSettings.append(FontPlatformFeatureSetting {
                        checked_cf_cast<CFNumberRef>(typeOrTag.get()),
                        selectorOrValue,
                        nullptr,
                        nullptr
                    });
                } else {
                    featureSettings.append(FontPlatformFeatureSetting {
                        nullptr,
                        nullptr,
                        checked_cf_cast<CFStringRef>(typeOrTag.get()),
                        selectorOrValue
                    });
                }
            }
        }
        result.featureSettings = featureSettings;
    }
    return WTFMove(result);
}

#define INJECT_STRING_VALUE(key, value) { \
    if (!value.isNull())\
        CFDictionaryAddValue(result.get(), key, value.createCFString().get());\
    }

#define INJECT_CF_VALUE(key, value) { \
    if (value)\
        CFDictionaryAddValue(result.get(), key, value->get());\
    }

#define PAIR_VECTOR_TO_DICTIONARY(key, vector) \
    if (vector) { \
        RetainPtr<CFMutableDictionaryRef> newResult = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)); \
        for (auto& item : *vector) \
            CFDictionaryAddValue(newResult.get(), item.first.get(), item.second.get()); \
        CFDictionaryAddValue(result.get(), key, newResult.get()); \
    }

RetainPtr<CFDictionaryRef> FontPlatformSerializedAttributes::toCFDictionary() const
{
    RetainPtr<CFMutableDictionaryRef> result = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    INJECT_STRING_VALUE(kCTFontNameAttribute, fontName);
    INJECT_STRING_VALUE(kCTFontDescriptorLanguageAttribute, descriptorLanguage);
    INJECT_STRING_VALUE(kCTFontDescriptorTextStyleAttribute, descriptorTextStyle);

    INJECT_CF_VALUE(kCTFontMatrixAttribute, matrix);
    INJECT_CF_VALUE(kCTFontIgnoreLegibilityWeightAttribute, ignoreLegibilityWeight);

    INJECT_CF_VALUE(kCTFontBaselineAdjustAttribute, baselineAdjust);
    INJECT_CF_VALUE(kCTFontFallbackOptionAttribute, fallbackOption);
    INJECT_CF_VALUE(kCTFontFixedAdvanceAttribute, fixedAdvance);
    INJECT_CF_VALUE(kCTFontOrientationAttribute, orientation);
    INJECT_CF_VALUE(kCTFontPaletteAttribute, palette);
    INJECT_CF_VALUE(kCTFontSizeAttribute, size);
    INJECT_CF_VALUE(kCTFontSizeCategoryAttribute, sizeCategory);
    INJECT_CF_VALUE(kCTFontTrackAttribute, track);
    INJECT_CF_VALUE(kCTFontUnscaledTrackingAttribute, unscaledTracking);

#if HAVE(ADDITIONAL_FONT_PLATFORM_SERIALIZED_ATTRIBUTES)
    INJECT_CF_VALUE(additionalFontPlatformSerializedAttributesNumberDictionaryKey(), additionalNumber);
#endif

    if (featureSettings) {
        RetainPtr settingsArray = adoptCF(CFArrayCreateMutable(nullptr, Checked<CFIndex>(featureSettings->size()), &kCFTypeArrayCallBacks));
        for (const FontPlatformFeatureSetting& setting : *featureSettings) {
            RetainPtr destinationSetting = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            if (setting.type)
                CFDictionaryAddValue(destinationSetting.get(), kCTFontFeatureTypeIdentifierKey, setting.type->get());
            if (setting.selector)
                CFDictionaryAddValue(destinationSetting.get(), kCTFontFeatureSelectorIdentifierKey, setting.selector->get());
            if (setting.tag)
                CFDictionaryAddValue(destinationSetting.get(), kCTFontOpenTypeFeatureTag, setting.tag->get());
            if (setting.value)
                CFDictionaryAddValue(destinationSetting.get(), kCTFontOpenTypeFeatureValue, setting.value->get());
            CFArrayAppendValue(settingsArray.get(), destinationSetting.get());
        }
        CFDictionaryAddValue(result.get(), kCTFontFeatureSettingsAttribute, settingsArray.get());
    }

    if (opticalSize) {
        if (auto opticalSizeCF = opticalSize->toCF())
            CFDictionaryAddValue(result.get(), kCTFontOpticalSizeAttribute, opticalSizeCF.get());
    }

    PAIR_VECTOR_TO_DICTIONARY(kCTFontPaletteColorsAttribute, paletteColors);
    PAIR_VECTOR_TO_DICTIONARY(kCTFontVariationAttribute, variations);

    if (traits)
        CFDictionaryAddValue(result.get(), kCTFontTraitsAttribute, traits->toCFDictionary().get());

    return WTFMove(result);
}

std::optional<FontPlatformSerializedTraits> FontPlatformSerializedTraits::fromCF(CFDictionaryRef dictionary)
{
    if (!dictionary)
        return std::nullopt;

    FontPlatformSerializedTraits result;

    EXTRACT_TYPED_VALUE(kCTFontUIFontDesignTrait, CFString, result.uiFontDesign);
    EXTRACT_TYPED_VALUE(kCTFontWeightTrait, CFNumber, result.weight);
    EXTRACT_TYPED_VALUE(kCTFontWidthTrait, CFNumber, result.width);
    EXTRACT_TYPED_VALUE(kCTFontSymbolicTrait, CFNumber, result.symbolic);
    EXTRACT_TYPED_VALUE(kCTFontGradeTrait, CFNumber, result.grade);

    return WTFMove(result);
}

RetainPtr<CFDictionaryRef> FontPlatformSerializedTraits::toCFDictionary() const
{
    RetainPtr<CFMutableDictionaryRef> result = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    INJECT_STRING_VALUE(kCTFontUIFontDesignTrait, uiFontDesign);
    INJECT_CF_VALUE(kCTFontWeightTrait, weight);
    INJECT_CF_VALUE(kCTFontWidthTrait, width);
    INJECT_CF_VALUE(kCTFontSymbolicTrait, symbolic);
    INJECT_CF_VALUE(kCTFontGradeTrait, grade);

    return WTFMove(result);
}

std::optional<FontPlatformOpticalSize> FontPlatformOpticalSize::fromCF(CFTypeRef type)
{
    if (!type)
        return std::nullopt;

    if (CFGetTypeID(type) == CFNumberGetTypeID())
        return FontPlatformOpticalSize { RetainPtr { checked_cf_cast<CFNumberRef>(type) } };
    if (CFGetTypeID(type) == CFStringGetTypeID())
        return FontPlatformOpticalSize { String { checked_cf_cast<CFStringRef>(type) } };

    return std::nullopt;
}

RetainPtr<CFTypeRef> FontPlatformOpticalSize::toCF() const
{
    return WTF::switchOn(opticalSize, [] (const RetainPtr<CFNumberRef>& number) -> RetainPtr<CFTypeRef> {
        return number;
    }, [] (const String& string) -> RetainPtr<CFTypeRef> {
        return string.createCFString();
    });
}

RetainPtr<CTFontRef> InstalledFont::PostScriptFont::toCTFont(double pointSize) const
{
    RetainPtr font = [&] -> RetainPtr<CTFontRef> {
        RetainPtr<CTFontDescriptorRef> fontDescriptor;
        if (fontSerializedAttributes)
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithAttributesAndOptions(fontSerializedAttributes->toCFDictionary().get(), fontDescriptorOptions));
        else
            fontDescriptor = adoptCF(CTFontDescriptorCreateWithNameAndSize(postScriptName.createCFString().get(), pointSize));

        RetainPtr matchedFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), pointSize, nullptr));
        if (String(adoptCF(CTFontCopyPostScriptName(matchedFont.get())).get()) == postScriptName)
            return matchedFont;

        RetainPtr matched = adoptCF(CTFontDescriptorCreateMatchingFontDescriptorsWithOptions(fontDescriptor.get(), nullptr, kCTFontDescriptorMatchingOptionIncludeHiddenFonts));
        if (!matched || !CFArrayGetCount(matched.get()))
            return nullptr;

        RetainPtr matchedDescriptor = dynamic_cf_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(matched.get(), 0));
        matchedFont = adoptCF(CTFontCreateWithFontDescriptor(matchedDescriptor.get(), pointSize, nullptr));
        if (String(adoptCF(CTFontCopyPostScriptName(matchedFont.get())).get()) == postScriptName)
            return matchedFont;

        return nullptr;
    }();

    if (!font)
        font = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, pointSize, nil));

    return font;
}

RetainPtr<CTFontRef> InstalledFont::toCTFont() const
{
    return WTF::switchOn(font,
        [this] (const PostScriptFont& postScriptFont) -> RetainPtr<CTFontRef> {
            return postScriptFont.toCTFont(metadata.pointSize);
        }
    );
}

Ref<Font> InstalledFont::toFont() const
{
    return WTF::switchOn(font,
        [this] (const PostScriptFont& postScriptFont) -> Ref<Font> {
            return Font::create(FontPlatformData(postScriptFont.toCTFont(metadata.pointSize).get(), metadata.pointSize, metadata.syntheticBold, metadata.syntheticOblique, metadata.orientation, metadata.widthVariant, metadata.textRenderingMode));
        }
    );
}

#undef INJECT_STRING_VALUE
#undef INJECT_CF_VALUE
#undef PAIR_VECTOR_TO_DICTIONARY
#undef EXTRACT_TYPED_VALUE

#if HAVE(ADDITIONAL_FONT_PLATFORM_SERIALIZED_ATTRIBUTES)
#include <WebKitAdditions/FontPlatformSerializedAttributesAdditions.cpp>
#endif

} // namespace WebCore
