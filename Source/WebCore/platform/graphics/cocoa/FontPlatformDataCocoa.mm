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

#import "CoreTextSPI.h"
#import "SharedBuffer.h"
#import "WebCoreSystemInterface.h"
#import <wtf/text/WTFString.h>

#if !PLATFORM(IOS)
#import <AppKit/NSFont.h>
#else
#import "CoreGraphicsSPI.h"
#import <CoreText/CoreText.h>
#endif

namespace WebCore {

// These CoreText Text Spacing feature selectors are not defined in CoreText.
enum TextSpacingCTFeatureSelector { TextSpacingProportional, TextSpacingFullWidth, TextSpacingHalfWidth, TextSpacingThirdWidth, TextSpacingQuarterWidth };

FontPlatformData::FontPlatformData(CTFontRef font, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode)
    : FontPlatformData(adoptCF(CTFontCopyGraphicsFont(font, NULL)).get(), size, syntheticBold, syntheticOblique, orientation, widthVariant, textRenderingMode)
{
    ASSERT_ARG(font, font);
    m_font = font;
    m_isColorBitmapFont = CTFontGetSymbolicTraits(font) & kCTFontTraitColorGlyphs;
    m_isSystemFont = CTFontDescriptorIsSystemUIFont(adoptCF(CTFontCopyFontDescriptor(m_font.get())).get());

#if PLATFORM(IOS)
    m_isEmoji = CTFontIsAppleColorEmoji(m_font.get());
#endif
}

FontPlatformData::~FontPlatformData()
{
}

void FontPlatformData::platformDataInit(const FontPlatformData& f)
{
    m_font = f.m_font;

    m_cgFont = f.m_cgFont;
    m_ctFont = f.m_ctFont;

#if PLATFORM(IOS)
    m_isEmoji = f.m_isEmoji;
#endif
}

const FontPlatformData& FontPlatformData::platformDataAssign(const FontPlatformData& f)
{
    m_cgFont = f.m_cgFont;
    if (m_font && f.m_font && CFEqual(m_font.get(), f.m_font.get()))
        return *this;
    m_font = f.m_font;
    m_ctFont = f.m_ctFont;

#if PLATFORM(IOS)
    m_isEmoji = f.m_isEmoji;
#endif

    return *this;
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    bool result = false;
    if (m_font || other.m_font) {
#if PLATFORM(IOS)
        result = m_font && other.m_font && CFEqual(m_font.get(), other.m_font.get());
#else
        result = m_font == other.m_font;
#endif
        return result;
    }
    return m_cgFont == other.m_cgFont;
}

CTFontRef FontPlatformData::registeredFont() const
{
    CTFontRef platformFont = font();
    ASSERT(platformFont);
    if (platformFont && adoptCF(CTFontCopyAttribute(platformFont, kCTFontURLAttribute)))
        return platformFont;
    return nullptr;
}

void FontPlatformData::setFont(CTFontRef font)
{
    ASSERT_ARG(font, font);

    if (m_font == font)
        return;

    m_font = font;
    m_size = CTFontGetSize(font);
    m_cgFont = adoptCF(CTFontCopyGraphicsFont(font, nullptr));

    CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(m_font.get());
    m_isColorBitmapFont = traits & kCTFontTraitColorGlyphs;
    m_isSystemFont = CTFontDescriptorIsSystemUIFont(adoptCF(CTFontCopyFontDescriptor(m_font.get())).get());

#if PLATFORM(IOS)
    m_isEmoji = CTFontIsAppleColorEmoji(m_font.get());
#endif
    
    m_ctFont = nullptr;
}

inline int mapFontWidthVariantToCTFeatureSelector(FontWidthVariant variant)
{
    switch(variant) {
    case RegularWidth:
        return TextSpacingProportional;

    case HalfWidth:
        return TextSpacingHalfWidth;

    case ThirdWidth:
        return TextSpacingThirdWidth;

    case QuarterWidth:
        return TextSpacingQuarterWidth;
    }

    ASSERT_NOT_REACHED();
    return TextSpacingProportional;
}

static CTFontDescriptorRef cascadeToLastResortFontDescriptor()
{
    static CTFontDescriptorRef descriptor;
    if (descriptor)
        return descriptor;

    RetainPtr<CTFontDescriptorRef> lastResort = adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0));

    const void* descriptors[] = { lastResort.get() };
    RetainPtr<CFArrayRef> array = adoptCF(CFArrayCreate(kCFAllocatorDefault, descriptors, WTF_ARRAY_LENGTH(descriptors), &kCFTypeArrayCallBacks));

    const void* keys[] = { kCTFontCascadeListAttribute };
    const void* values[] = { array.get() };
    RetainPtr<CFDictionaryRef> attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    descriptor = CTFontDescriptorCreateWithAttributes(attributes.get());

    return descriptor;
}

CTFontRef FontPlatformData::ctFont() const
{
    if (m_ctFont)
        return m_ctFont.get();

    ASSERT(m_font);
    ASSERT(m_cgFont);
    m_ctFont = m_font;
    CTFontDescriptorRef fontDescriptor;
    RetainPtr<CFStringRef> postScriptName = adoptCF(CTFontCopyPostScriptName(m_ctFont.get()));
    fontDescriptor = cascadeToLastResortFontDescriptor();
    m_ctFont = adoptCF(CTFontCreateCopyWithAttributes(m_ctFont.get(), m_size, 0, fontDescriptor));

    if (m_widthVariant != RegularWidth) {
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

PassRefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    if (RetainPtr<CFDataRef> data = adoptCF(CGFontCopyTableForTag(cgFont(), table)))
        return SharedBuffer::wrapCFData(data.get());
    
    return nullptr;
}

#ifndef NDEBUG
String FontPlatformData::description() const
{
    RetainPtr<CFStringRef> cgFontDescription = adoptCF(CFCopyDescription(cgFont()));
    return String(cgFontDescription.get()) + " " + String::number(m_size)
            + (m_syntheticBold ? " synthetic bold" : "") + (m_syntheticOblique ? " synthetic oblique" : "") + (m_orientation ? " vertical orientation" : "");
}
#endif

} // namespace WebCore
