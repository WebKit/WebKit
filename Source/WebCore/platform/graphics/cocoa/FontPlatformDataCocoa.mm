/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
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

#import "config.h"
#import "FontPlatformData.h"

#import "SharedBuffer.h"
#import <pal/spi/cocoa/CoreTextSPI.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <CoreText/CoreText.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#endif

namespace WebCore {

// These CoreText Text Spacing feature selectors are not defined in CoreText.
enum TextSpacingCTFeatureSelector { TextSpacingProportional, TextSpacingFullWidth, TextSpacingHalfWidth, TextSpacingThirdWidth, TextSpacingQuarterWidth };

FontPlatformData::FontPlatformData(CTFontRef font, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode)
    : FontPlatformData(size, syntheticBold, syntheticOblique, orientation, widthVariant, textRenderingMode)
{
    ASSERT_ARG(font, font);
    m_font = font;
    m_isColorBitmapFont = CTFontGetSymbolicTraits(font) & kCTFontTraitColorGlyphs;
    m_isSystemFont = CTFontDescriptorIsSystemUIFont(adoptCF(CTFontCopyFontDescriptor(m_font.get())).get());
    auto variations = adoptCF(static_cast<CFDictionaryRef>(CTFontCopyAttribute(font, kCTFontVariationAttribute)));
    m_hasVariations = variations && CFDictionaryGetCount(variations.get());

#if PLATFORM(IOS_FAMILY)
    m_isEmoji = CTFontIsAppleColorEmoji(m_font.get());
#endif
}

unsigned FontPlatformData::hash() const
{
    uintptr_t flags = static_cast<uintptr_t>(static_cast<unsigned>(m_widthVariant) << 6
        | m_isHashTableDeletedValue << 5
        | static_cast<unsigned>(m_textRenderingMode) << 3
        | static_cast<unsigned>(m_orientation) << 2
        | m_syntheticBold << 1
        | m_syntheticOblique);

    uintptr_t fontHash = reinterpret_cast<uintptr_t>(CFHash(m_font.get()));
    uintptr_t hashCodes[] = { fontHash, flags };
    return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    if (!m_font || !other.m_font)
        return m_font == other.m_font;
    return CFEqual(m_font.get(), other.m_font.get());
}

CTFontRef FontPlatformData::registeredFont() const
{
    CTFontRef platformFont = font();
    ASSERT(platformFont);
    if (platformFont && adoptCF(CTFontCopyAttribute(platformFont, kCTFontURLAttribute)))
        return platformFont;
    return nullptr;
}

inline int mapFontWidthVariantToCTFeatureSelector(FontWidthVariant variant)
{
    switch(variant) {
    case FontWidthVariant::RegularWidth:
        return TextSpacingProportional;

    case FontWidthVariant::HalfWidth:
        return TextSpacingHalfWidth;

    case FontWidthVariant::ThirdWidth:
        return TextSpacingThirdWidth;

    case FontWidthVariant::QuarterWidth:
        return TextSpacingQuarterWidth;
    }

    ASSERT_NOT_REACHED();
    return TextSpacingProportional;
}

static CFDictionaryRef cascadeToLastResortAttributesDictionary()
{
    static CFDictionaryRef attributes = nullptr;
    if (attributes)
        return attributes;

    RetainPtr<CTFontDescriptorRef> lastResort = adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0));

    CFTypeRef descriptors[] = { lastResort.get() };
    RetainPtr<CFArrayRef> array = adoptCF(CFArrayCreate(kCFAllocatorDefault, descriptors, WTF_ARRAY_LENGTH(descriptors), &kCFTypeArrayCallBacks));

    CFTypeRef keys[] = { kCTFontCascadeListAttribute };
    CFTypeRef values[] = { array.get() };
    attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    return attributes;
}

static RetainPtr<CTFontDescriptorRef> cascadeToLastResortAndVariationsFontDescriptor(CTFontRef originalFont)
{
// FIXME: Remove this when <rdar://problem/28449441> is fixed.
#define WORKAROUND_CORETEXT_VARIATIONS_WITH_FALLBACK_LIST_BUG (ENABLE(VARIATION_FONTS) && ((PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101300) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED < 110000)))

    CFDictionaryRef attributes = cascadeToLastResortAttributesDictionary();
#if WORKAROUND_CORETEXT_VARIATIONS_WITH_FALLBACK_LIST_BUG
    auto variations = adoptCF(static_cast<CFDictionaryRef>(CTFontCopyAttribute(originalFont, kCTFontVariationAttribute)));
    if (!variations || !CFDictionaryGetCount(variations.get()) || CTFontDescriptorIsSystemUIFont(adoptCF(CTFontCopyFontDescriptor(originalFont)).get()))
#endif
    {
        UNUSED_PARAM(originalFont);

        static CTFontDescriptorRef descriptor = nullptr;
        if (descriptor)
            return descriptor;

        descriptor = CTFontDescriptorCreateWithAttributes(attributes);
        return descriptor;
    }
#if WORKAROUND_CORETEXT_VARIATIONS_WITH_FALLBACK_LIST_BUG
    auto mutableAttributes = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 2, attributes));
    CFDictionaryAddValue(mutableAttributes.get(), kCTFontVariationAttribute, variations.get());
    return adoptCF(CTFontDescriptorCreateWithAttributes(mutableAttributes.get()));
#endif
#undef WORKAROUND_CORETEXT_VARIATIONS_WITH_FALLBACK_LIST_BUG
}

CTFontRef FontPlatformData::ctFont() const
{
    if (m_ctFont)
        return m_ctFont.get();

    ASSERT(m_font);
    m_ctFont = adoptCF(CTFontCreateCopyWithAttributes(m_font.get(), m_size, 0, cascadeToLastResortAndVariationsFontDescriptor(m_font.get()).get()));

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
    RetainPtr<CFURLRef> url = adoptCF(static_cast<CFURLRef>(CTFontDescriptorCopyAttribute(fontDescriptor.get(), kCTFontReferenceURLAttribute)));
    ASSERT(!url || CFGetTypeID(url.get()) == CFURLGetTypeID());
    return url;
}

RetainPtr<CFTypeRef> FontPlatformData::objectForEqualityCheck() const
{
    return objectForEqualityCheck(ctFont());
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    if (RetainPtr<CFDataRef> data = adoptCF(CTFontCopyTable(font(), table, kCTFontTableOptionNoOptions)))
        return SharedBuffer::create(data.get());
    
    return nullptr;
}

#if !LOG_DISABLED
String FontPlatformData::description() const
{
    auto fontDescription = adoptCF(CFCopyDescription(font()));
    return String(fontDescription.get()) + " " + String::number(m_size)
        + (m_syntheticBold ? " synthetic bold" : "") + (m_syntheticOblique ? " synthetic oblique" : "") + (m_orientation == FontOrientation::Vertical ? " vertical orientation" : "");
}
#endif

} // namespace WebCore
