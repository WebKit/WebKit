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
#include <pal/spi/cf/CoreTextSPI.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

FontPlatformData::FontPlatformData(RetainPtr<CTFontRef>&& font, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode, const FontCustomPlatformData* customPlatformData)
    : FontPlatformData(size, syntheticBold, syntheticOblique, orientation, widthVariant, textRenderingMode, customPlatformData)
{
    ASSERT_ARG(font, font);
    m_font = font;
    m_isColorBitmapFont = CTFontGetSymbolicTraits(font.get()) & kCTFontColorGlyphsTrait;
    m_isSystemFont = WebCore::isSystemFont(font.get());
    auto variations = adoptCF(static_cast<CFDictionaryRef>(CTFontCopyAttribute(font.get(), kCTFontVariationAttribute)));
    m_hasVariations = variations && CFDictionaryGetCount(variations.get());

#if PLATFORM(IOS_FAMILY)
    m_isEmoji = CTFontIsAppleColorEmoji(m_font.get());
#endif
}

static RetainPtr<CTFontDescriptorRef> findFontDescriptor(CFURLRef url, CFStringRef postScriptName)
{
    if (!url)
        return nullptr;
    auto fontDescriptors = adoptCF(CTFontManagerCreateFontDescriptorsFromURL(url));
    if (!fontDescriptors || !CFArrayGetCount(fontDescriptors.get()))
        return nullptr;
    if (CFArrayGetCount(fontDescriptors.get()) == 1)
        return static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors.get(), 0));

    for (CFIndex i = 0; i < CFArrayGetCount(fontDescriptors.get()); ++i) {
        auto fontDescriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors.get(), i));
        auto currentPostScriptName = adoptCF(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontNameAttribute));
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
        auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));
        auto actualPostScriptName = adoptCF(CTFontCopyPostScriptName(font.get()));
        auto actualReferenceURL = adoptCF(CTFontCopyAttribute(font.get(), kCTFontReferenceURLAttribute));
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
        auto baseFontDescriptor = custom->fontDescriptor.get();
        RELEASE_ASSERT(baseFontDescriptor);
        auto fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(baseFontDescriptor, data.m_attributes.get()));
        ctFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), data.m_size, nullptr));
    } else
        ctFont = createCTFont(data.m_attributes.get(), data.m_size, data.m_options, data.m_url.get(), data.m_psName.get());

    return WebCore::FontPlatformData(ctFont.get(), data.m_size, data.m_syntheticBold, data.m_syntheticOblique, data.m_orientation, data.m_widthVariant, data.m_textRenderingMode, custom);
}

bool isSystemFont(CTFontRef font)
{
    return CTFontIsSystemUIFont(font);
}

CTFontRef FontPlatformData::registeredFont() const
{
    CTFontRef platformFont = ctFont();
    ASSERT(platformFont);
    if (platformFont && adoptCF(CTFontCopyAttribute(platformFont, kCTFontURLAttribute)))
        return platformFont;
    return nullptr;
}

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

static RetainPtr<CFDictionaryRef> cascadeToLastResortAttributesDictionary()
{
    auto lastResort = adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0));

    CFTypeRef descriptors[] = { lastResort.get() };
    RetainPtr<CFArrayRef> array = adoptCF(CFArrayCreate(kCFAllocatorDefault, descriptors, std::size(descriptors), &kCFTypeArrayCallBacks));

    CFTypeRef keys[] = { kCTFontCascadeListAttribute };
    CFTypeRef values[] = { array.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

static CTFontDescriptorRef cascadeToLastResortAndVariationsFontDescriptor()
{
    static CTFontDescriptorRef descriptor;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        descriptor = CTFontDescriptorCreateWithAttributes(cascadeToLastResortAttributesDictionary().get());
    });
    return descriptor;
}

CTFontRef FontPlatformData::ctFont() const
{
    if (m_ctFont)
        return m_ctFont.get();

    ASSERT(m_font);
    m_ctFont = adoptCF(CTFontCreateCopyWithAttributes(m_font.get(), m_size, nullptr, cascadeToLastResortAndVariationsFontDescriptor()));

    if (m_widthVariant != FontWidthVariant::RegularWidth) {
        int featureTypeValue = kTextSpacingType;
        int featureSelectorValue = mapFontWidthVariantToCTFeatureSelector(m_widthVariant);
        RetainPtr<CTFontDescriptorRef> sourceDescriptor = adoptCF(CTFontCopyFontDescriptor(m_ctFont.get()));
        RetainPtr<CFNumberRef> featureType = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureTypeValue));
        RetainPtr<CFNumberRef> featureSelector = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureSelectorValue));
        RetainPtr<CTFontDescriptorRef> newDescriptor = adoptCF(CTFontDescriptorCreateCopyWithFeature(sourceDescriptor.get(), featureType.get(), featureSelector.get()));
        RetainPtr<CTFontRef> newFont = adoptCF(CTFontCreateWithFontDescriptor(newDescriptor.get(), m_size, 0));

        if (newFont)
            m_ctFont = newFont;
    }

    return m_ctFont.get();
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
    return objectForEqualityCheck(ctFont());
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    if (RetainPtr<CFDataRef> data = adoptCF(CTFontCopyTable(ctFont(), table, kCTFontTableOptionNoOptions)))
        return SharedBuffer::create(data.get());

    return platformOpenTypeTable(table);
}

#if !LOG_DISABLED

String FontPlatformData::description() const
{
    String fontDescription { adoptCF(CFCopyDescription(ctFont())).get() };
    return makeString(fontDescription, ' ', m_size,
        (m_syntheticBold ? " synthetic bold" : ""),
        (m_syntheticOblique ? " synthetic oblique" : ""),
        (m_orientation == FontOrientation::Vertical ? " vertical orientation" : ""));
}

#endif

String FontPlatformData::familyName() const
{
    if (auto platformFont = ctFont())
        return adoptCF(CTFontCopyFamilyName(platformFont)).get();
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
    m_ctFont = nullptr;
}

FontPlatformData::Attributes FontPlatformData::attributes() const
{
    Attributes result(m_size, m_orientation, m_widthVariant, m_textRenderingMode, m_syntheticBold, m_syntheticOblique);

    auto fontDescriptor = adoptCF(CTFontCopyFontDescriptor(m_font.get()));
    result.m_attributes = adoptCF(CTFontDescriptorCopyAttributes(fontDescriptor.get()));

    if (!m_customPlatformData) {
        result.m_options = CTFontDescriptorGetOptions(fontDescriptor.get());
        auto referenceURL = adoptCF(static_cast<CFURLRef>(CTFontCopyAttribute(m_font.get(), kCTFontReferenceURLAttribute)));
        result.m_url = CFURLGetString(referenceURL.get());
        result.m_psName = adoptCF(CTFontCopyPostScriptName(m_font.get()));
    }

    return result;
}

std::optional<FontPlatformData> FontPlatformData::tryMakeFontPlatformData(float size, WebCore::FontOrientation&& orientation, WebCore::FontWidthVariant&& widthVariant, WebCore::TextRenderingMode&& textRenderingMode, bool syntheticBold, bool syntheticOblique, FontPlatformData::PlatformDataVariant&& platformSerializationData)
{
    RetainPtr<CTFontRef> font;
    RefPtr<FontCustomPlatformData> customPlatformData;

    bool dataError = WTF::switchOn(platformSerializationData,
        [&] (const FontPlatformSerializedData& d) {
            font = WebCore::createCTFont(d.attributes.get(), size, d.options, d.referenceURL.get(), d.postScriptName.get());
            if (!font)
                return true;
            return false;
        },
        [&] (FontPlatformSerializedCreationData& d) {
            auto fontFaceData = SharedBuffer::create(WTFMove(d.fontFaceData));
            auto fontCustomPlatformData = createFontCustomPlatformData(fontFaceData, d.itemInCollection);
            if (!fontCustomPlatformData)
                return true;
            auto baseFontDescriptor = fontCustomPlatformData->fontDescriptor.get();
            if (!baseFontDescriptor)
                return true;
            auto fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(baseFontDescriptor, d.attributes.get()));

            font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));
            customPlatformData = fontCustomPlatformData;
            return false;
        }
    );
    if (dataError)
        return std::nullopt;

    return FontPlatformData(size, WTFMove(orientation), WTFMove(widthVariant), WTFMove(textRenderingMode), syntheticBold, syntheticOblique, WTFMove(font), WTFMove(customPlatformData));
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
    auto variations = adoptCF(static_cast<CFDictionaryRef>(CTFontCopyAttribute(m_font.get(), kCTFontVariationAttribute)));
    m_hasVariations = variations && CFDictionaryGetCount(variations.get());
#if PLATFORM(IOS_FAMILY)
    m_isEmoji = CTFontIsAppleColorEmoji(m_font.get());
#endif
}

FontPlatformData::PlatformDataVariant FontPlatformData::platformSerializationData() const
{
    auto ctFont = font();
    auto fontDescriptor = adoptCF(CTFontCopyFontDescriptor(ctFont));
    auto attributes = adoptCF(CTFontDescriptorCopyAttributes(fontDescriptor.get()));

    const auto& data = creationData();
    if (data)
        return FontPlatformSerializedCreationData { { data->fontFaceData->dataAsSpanForContiguousData() }, attributes, data->itemInCollection };

    auto options = CTFontDescriptorGetOptions(fontDescriptor.get());
    auto referenceURL = adoptCF(static_cast<CFURLRef>(CTFontCopyAttribute(ctFont, kCTFontReferenceURLAttribute)));
    auto urlString = CFURLGetString(referenceURL.get());
    auto postScriptName = adoptCF(CTFontCopyPostScriptName(ctFont)).get();
    return FontPlatformSerializedData { options, urlString, postScriptName, attributes };
}

} // namespace WebCore
