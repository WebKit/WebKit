/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "SharedBuffer.h"
#include <CoreText/CoreText.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <pal/spi/cf/CoreTextSPI.h>
#else
#include <pal/spi/win/CoreTextSPIWin.h>
#endif

namespace WebCore {

FontPlatformData::FontPlatformData(RetainPtr<CTFontRef>&& font, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode, const CreationData* creationData)
    : FontPlatformData(size, syntheticBold, syntheticOblique, orientation, widthVariant, textRenderingMode, creationData)
{
    ASSERT_ARG(font, font);
#if PLATFORM(WIN)
    m_ctFont = font;
#else
    m_font = font;
#endif
    m_isColorBitmapFont = CTFontGetSymbolicTraits(font.get()) & kCTFontColorGlyphsTrait;
    m_isSystemFont = WebCore::isSystemFont(font.get());
    auto variations = adoptCF(static_cast<CFDictionaryRef>(CTFontCopyAttribute(font.get(), kCTFontVariationAttribute)));
    m_hasVariations = variations && CFDictionaryGetCount(variations.get());

#if PLATFORM(IOS_FAMILY)
    m_isEmoji = CTFontIsAppleColorEmoji(m_font.get());
#endif
}

bool isSystemFont(CTFontRef font)
{
#if HAVE(CTFONTISSYSTEMUIFONT)
    return CTFontIsSystemUIFont(font);
#else
    return CTFontDescriptorIsSystemUIFont(adoptCF(CTFontCopyFontDescriptor(font)).get());
#endif
}

CTFontRef FontPlatformData::registeredFont() const
{
    CTFontRef platformFont = ctFont();
    ASSERT(platformFont);
    if (platformFont && adoptCF(CTFontCopyAttribute(platformFont, kCTFontURLAttribute)))
        return platformFont;
    return nullptr;
}

#if PLATFORM(COCOA)
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
#endif

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

#if PLATFORM(COCOA)
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
#endif

} // namespace WebCore
