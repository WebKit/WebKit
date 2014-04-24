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

#import "WebCoreSystemInterface.h"
#if !PLATFORM(IOS)
#import <AppKit/NSFont.h>
#else
#import <CoreText/CoreText.h>
#import <CoreGraphics/CGFontInfo.h>
#endif

#import <wtf/text/WTFString.h>

namespace WebCore {

// These CoreText Text Spacing feature selectors are not defined in CoreText.
enum TextSpacingCTFeatureSelector { TextSpacingProportional, TextSpacingFullWidth, TextSpacingHalfWidth, TextSpacingThirdWidth, TextSpacingQuarterWidth };

#if PLATFORM(MAC)
void FontPlatformData::loadFont(NSFont* nsFont, float, NSFont*& outNSFont, CGFontRef& cgFont)
{
    outNSFont = nsFont;
    cgFont = CTFontCopyGraphicsFont(toCTFontRef(nsFont), 0);
}
#endif  // PLATFORM(MAC)

#if !PLATFORM(IOS)
FontPlatformData::FontPlatformData(NSFont *nsFont, float size, bool isPrinterFont, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant)
    : m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
    , m_orientation(orientation)
    , m_size(size)
    , m_widthVariant(widthVariant)
    , m_font(nsFont)
    , m_isColorBitmapFont(false)
#else
FontPlatformData::FontPlatformData(CTFontRef ctFont, float size, bool isPrinterFont, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant)
    : m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
    , m_orientation(orientation)
    , m_isEmoji(false)
    , m_size(size)
    , m_widthVariant(widthVariant)
    , m_font(ctFont)
    , m_cgFont(adoptCF(CTFontCopyGraphicsFont(ctFont, NULL)))
    , m_isColorBitmapFont(CTFontGetSymbolicTraits(ctFont) & kCTFontTraitColorGlyphs)
#endif // !PLATFORM(IOS)
    , m_isCompositeFontReference(false)
    , m_isPrinterFont(isPrinterFont)
{
#if !PLATFORM(IOS)
    ASSERT_ARG(nsFont, nsFont);

    CGFontRef cgFont = 0;
    loadFont(nsFont, size, m_font, cgFont);
    {
        CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(toCTFontRef(m_font));
        m_isColorBitmapFont = traits & kCTFontColorGlyphsTrait;
        m_isCompositeFontReference = traits & kCTFontCompositeTrait;
    }

    if (m_font)
        CFRetain(m_font);

    m_cgFont = adoptCF(cgFont);
#else
    ASSERT_ARG(ctFont, ctFont);

    CFRetain(ctFont);
#endif // !PLATFORM(IOS)
}

FontPlatformData::~FontPlatformData()
{
#if !PLATFORM(IOS)
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
#else
    if (m_font && m_font != reinterpret_cast<CTFontRef>(-1))
        CFRelease(m_font);
#endif
}

void FontPlatformData::platformDataInit(const FontPlatformData& f)
{
#if !PLATFORM(IOS)
    m_font = f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1) ? const_cast<NSFont *>(static_cast<const NSFont *>(CFRetain(f.m_font))) : f.m_font;
#else
    m_font = f.m_font && f.m_font != reinterpret_cast<CTFontRef>(-1) ? static_cast<CTFontRef>(const_cast<void *>(CFRetain(f.m_font))) : f.m_font;
#endif

#if PLATFORM(IOS)
    m_isEmoji = f.m_isEmoji;
#endif
    m_cgFont = f.m_cgFont;
    m_CTFont = f.m_CTFont;
}

const FontPlatformData& FontPlatformData::platformDataAssign(const FontPlatformData& f)
{
    m_cgFont = f.m_cgFont;
#if PLATFORM(IOS)
    m_isEmoji = f.m_isEmoji;
#endif
#if !PLATFORM(IOS)
    if (m_font == f.m_font)
        return *this;
    if (f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1))
        CFRetain(f.m_font);
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
#else
    if (m_font && m_font != reinterpret_cast<CTFontRef>(-1) && f.m_font && f.m_font != reinterpret_cast<CTFontRef>(-1) && CFEqual(m_font, f.m_font))
        return *this;
    if (f.m_font && f.m_font != reinterpret_cast<CTFontRef>(-1))
        CFRetain(f.m_font);
    if (m_font && m_font != reinterpret_cast<CTFontRef>(-1))
        CFRelease(m_font);
#endif
    m_font = f.m_font;
    m_CTFont = f.m_CTFont;
    return *this;
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    bool result = false;
    if (m_font || other.m_font) {
#if PLATFORM(IOS)
        result = m_font && m_font != reinterpret_cast<CTFontRef>(-1) && other.m_font && other.m_font != reinterpret_cast<CTFontRef>(-1) && CFEqual(m_font, other.m_font);
#if !ASSERT_DISABLED
        if (result)
            ASSERT(m_isEmoji == other.m_isEmoji);
#endif
#else
        result = m_font == other.m_font;
#endif // PLATFORM(IOS)
        return result;
    }
#if PLATFORM(IOS)
#if !ASSERT_DISABLED
    if (m_cgFont == other.m_cgFont)
        ASSERT(m_isEmoji == other.m_isEmoji);
#endif
#endif // PLATFORM(IOS)
    return m_cgFont == other.m_cgFont;
}

#if !PLATFORM(IOS)
void FontPlatformData::setFont(NSFont *font)
{
    ASSERT_ARG(font, font);
    ASSERT(m_font != reinterpret_cast<NSFont *>(-1));

    if (m_font == font)
        return;

    CFRetain(font);
    if (m_font)
        CFRelease(m_font);
    m_font = font;
    m_size = [font pointSize];
    
    CGFontRef cgFont = 0;
    NSFont* loadedFont = 0;
    loadFont(m_font, m_size, loadedFont, cgFont);

    m_cgFont = adoptCF(cgFont);
    {
        CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(toCTFontRef(m_font));
        m_isColorBitmapFont = traits & kCTFontColorGlyphsTrait;
        m_isCompositeFontReference = traits & kCTFontCompositeTrait;
    }
    m_CTFont = 0;
}
#else
void FontPlatformData::setFont(CTFontRef font)
{
    ASSERT_ARG(font, font);
    ASSERT(m_font != reinterpret_cast<CTFontRef>(-1));

    if (m_font == font)
        return;

    CFRetain(font);
    if (m_font)
        CFRelease(m_font);
    m_font = font;
    m_size = CTFontGetSize(font);
    m_cgFont = adoptCF(CTFontCopyGraphicsFont(font, nullptr));
    m_isColorBitmapFont = CTFontGetSymbolicTraits(font) & kCTFontTraitColorGlyphs;
}
#endif

#if !PLATFORM(IOS)
bool FontPlatformData::roundsGlyphAdvances() const
{
    return [m_font renderingMode] == NSFontAntialiasedIntegerAdvancementsRenderingMode;
}
#endif

bool FontPlatformData::allowsLigatures() const
{
#if !PLATFORM(IOS)
    return ![[m_font coveredCharacterSet] characterIsMember:'a'];
#else
    if (!m_font)
        return false;

    RetainPtr<CFCharacterSetRef> characterSet = adoptCF(CTFontCopyCharacterSet(ctFont()));
    return !(characterSet.get() && CFCharacterSetIsCharacterMember(characterSet.get(), 'a'));
#endif
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

static CFDictionaryRef createFeatureSettingDictionary(int featureTypeIdentifier, int featureSelectorIdentifier)
{
    RetainPtr<CFNumberRef> featureTypeIdentifierNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureTypeIdentifier));
    RetainPtr<CFNumberRef> featureSelectorIdentifierNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureSelectorIdentifier));

    const void* settingKeys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    const void* settingValues[] = { featureTypeIdentifierNumber.get(), featureSelectorIdentifierNumber.get() };

    return CFDictionaryCreate(kCFAllocatorDefault, settingKeys, settingValues, WTF_ARRAY_LENGTH(settingKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
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

static CTFontDescriptorRef cascadeToLastResortAndDisableSwashesFontDescriptor()
{
    static CTFontDescriptorRef descriptor;
    if (descriptor)
        return descriptor;

    RetainPtr<CFDictionaryRef> lineInitialSwashesOffSetting = adoptCF(createFeatureSettingDictionary(kSmartSwashType, kLineInitialSwashesOffSelector));
    RetainPtr<CFDictionaryRef> lineFinalSwashesOffSetting = adoptCF(createFeatureSettingDictionary(kSmartSwashType, kLineFinalSwashesOffSelector));

    const void* settingDictionaries[] = { lineInitialSwashesOffSetting.get(), lineFinalSwashesOffSetting.get() };
    RetainPtr<CFArrayRef> featureSettings = adoptCF(CFArrayCreate(kCFAllocatorDefault, settingDictionaries, WTF_ARRAY_LENGTH(settingDictionaries), &kCFTypeArrayCallBacks));

    const void* keys[] = { kCTFontFeatureSettingsAttribute };
    const void* values[] = { featureSettings.get() };
    RetainPtr<CFDictionaryRef> attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    descriptor = CTFontDescriptorCreateCopyWithAttributes(cascadeToLastResortFontDescriptor(), attributes.get());

    return descriptor;
}

CTFontRef FontPlatformData::ctFont() const
{
    if (m_CTFont)
        return m_CTFont.get();

    ASSERT(m_cgFont.get());
#if !PLATFORM(IOS)
    m_CTFont = toCTFontRef(m_font);
    if (m_CTFont) {
        CTFontDescriptorRef fontDescriptor;
        RetainPtr<CFStringRef> postScriptName = adoptCF(CTFontCopyPostScriptName(m_CTFont.get()));
        // Hoefler Text Italic has line-initial and -final swashes enabled by default, so disable them.
        if (CFEqual(postScriptName.get(), CFSTR("HoeflerText-Italic")) || CFEqual(postScriptName.get(), CFSTR("HoeflerText-BlackItalic")))
            fontDescriptor = cascadeToLastResortAndDisableSwashesFontDescriptor();
        else
            fontDescriptor = cascadeToLastResortFontDescriptor();
        m_CTFont = adoptCF(CTFontCreateCopyWithAttributes(m_CTFont.get(), m_size, 0, fontDescriptor));
    } else
        m_CTFont = adoptCF(CTFontCreateWithGraphicsFont(m_cgFont.get(), m_size, 0, cascadeToLastResortFontDescriptor()));
#else
    // Apple Color Emoji size is adjusted (and then re-adjusted by Core Text) and capped.
    CGFloat size = !m_isEmoji ? m_size : m_size <= 15 ? 4 * (m_size + 2) / static_cast<CGFloat>(5) : 16;
    CTFontDescriptorRef fontDescriptor;
    const char* postScriptName = CGFontGetPostScriptName(m_cgFont.get());
    if (postScriptName && (!strcmp(postScriptName, "HoeflerText-Italic") || !strcmp(postScriptName, "HoeflerText-BlackItalic")))
        fontDescriptor = cascadeToLastResortAndDisableSwashesFontDescriptor();
    else
        fontDescriptor = cascadeToLastResortFontDescriptor();
    m_CTFont = adoptCF(CTFontCreateWithGraphicsFont(m_cgFont.get(), size, 0, fontDescriptor));
#endif // !PLATFORM(IOS)

    if (m_widthVariant != RegularWidth) {
        int featureTypeValue = kTextSpacingType;
        int featureSelectorValue = mapFontWidthVariantToCTFeatureSelector(m_widthVariant);
        RetainPtr<CTFontDescriptorRef> sourceDescriptor = adoptCF(CTFontCopyFontDescriptor(m_CTFont.get()));
        RetainPtr<CFNumberRef> featureType = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureTypeValue));
        RetainPtr<CFNumberRef> featureSelector = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureSelectorValue));
        RetainPtr<CTFontDescriptorRef> newDescriptor = adoptCF(CTFontDescriptorCreateCopyWithFeature(sourceDescriptor.get(), featureType.get(), featureSelector.get()));
        RetainPtr<CTFontRef> newFont = adoptCF(CTFontCreateWithFontDescriptor(newDescriptor.get(), m_size, 0));

        if (newFont)
            m_CTFont = newFont;
    }

    return m_CTFont.get();
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
