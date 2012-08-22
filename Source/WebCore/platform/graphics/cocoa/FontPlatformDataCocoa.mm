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

#import "PlatformString.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/NSFont.h>

#if PLATFORM(CHROMIUM) && OS(DARWIN)
#import "HarfBuzzNGFace.h"
#endif

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

FontPlatformData::FontPlatformData(NSFont *nsFont, float size, bool isPrinterFont, bool syntheticBold, bool syntheticOblique, FontOrientation orientation,
                                   TextOrientation textOrientation, FontWidthVariant widthVariant)
    : m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
    , m_orientation(orientation)
    , m_textOrientation(textOrientation)
    , m_size(size)
    , m_widthVariant(widthVariant)
    , m_font(nsFont)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
    , m_isPrinterFont(isPrinterFont)
{
    ASSERT_ARG(nsFont, nsFont);

    CGFontRef cgFont = 0;
    loadFont(nsFont, size, m_font, cgFont);
    
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
    // FIXME: Chromium: The following code isn't correct for the Chromium port since the sandbox might
    // have blocked font loading, in which case we'll only have the real loaded font file after the call to loadFont().
    {
        CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(toCTFontRef(m_font));
        m_isColorBitmapFont = traits & kCTFontColorGlyphsTrait;
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
        m_isCompositeFontReference = traits & kCTFontCompositeTrait;
#endif
    }
#endif

    if (m_font)
        CFRetain(m_font);

    m_cgFont.adoptCF(cgFont);
}

FontPlatformData:: ~FontPlatformData()
{
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
}

void FontPlatformData::platformDataInit(const FontPlatformData& f)
{
    m_font = f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1) ? const_cast<NSFont *>(static_cast<const NSFont *>(CFRetain(f.m_font))) : f.m_font;

    m_cgFont = f.m_cgFont;
    m_CTFont = f.m_CTFont;

#if PLATFORM(CHROMIUM) && OS(DARWIN)
    m_inMemoryFont = f.m_inMemoryFont;
    m_harfbuzzFace = f.m_harfbuzzFace;
#endif
}

const FontPlatformData& FontPlatformData::platformDataAssign(const FontPlatformData& f)
{
    m_cgFont = f.m_cgFont;
    if (m_font == f.m_font)
        return *this;
    if (f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1))
        CFRetain(f.m_font);
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
    m_font = f.m_font;
    m_CTFont = f.m_CTFont;
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    m_inMemoryFont = f.m_inMemoryFont;
    m_harfbuzzFace = f.m_harfbuzzFace;
#endif
    return *this;
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    if (m_font || other.m_font)
        return m_font == other.m_font;
    return m_cgFont == other.m_cgFont;
}

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
    
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    // If loadFont replaced m_font with a fallback font, then release the
    // previous font to counter the retain above. Then retain the new font.
    if (loadedFont != m_font) {
        CFRelease(m_font);
        CFRetain(loadedFont);
        m_font = loadedFont;
    }
#endif
    
    m_cgFont.adoptCF(cgFont);
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
    {
        CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(toCTFontRef(m_font));
        m_isColorBitmapFont = traits & kCTFontColorGlyphsTrait;
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
        m_isCompositeFontReference = traits & kCTFontCompositeTrait;
#endif
    }
#endif
    m_CTFont = 0;
}

bool FontPlatformData::roundsGlyphAdvances() const
{
    return [m_font renderingMode] == NSFontAntialiasedIntegerAdvancementsRenderingMode;
}

bool FontPlatformData::allowsLigatures() const
{
    return ![[m_font coveredCharacterSet] characterIsMember:'a'];
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
    RetainPtr<CFNumberRef> featureTypeIdentifierNumber(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureTypeIdentifier));
    RetainPtr<CFNumberRef> featureSelectorIdentifierNumber(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureSelectorIdentifier));

    const void* settingKeys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    const void* settingValues[] = { featureTypeIdentifierNumber.get(), featureSelectorIdentifierNumber.get() };

    return CFDictionaryCreate(kCFAllocatorDefault, settingKeys, settingValues, WTF_ARRAY_LENGTH(settingKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static CTFontDescriptorRef cascadeToLastResortFontDescriptor()
{
    static CTFontDescriptorRef descriptor;
    if (descriptor)
        return descriptor;

    const void* keys[] = { kCTFontCascadeListAttribute };
    const void* descriptors[] = { CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0) };
    const void* values[] = { CFArrayCreate(kCFAllocatorDefault, descriptors, WTF_ARRAY_LENGTH(descriptors), &kCFTypeArrayCallBacks) };
    RetainPtr<CFDictionaryRef> attributes(AdoptCF, CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    descriptor = CTFontDescriptorCreateWithAttributes(attributes.get());

    return descriptor;
}

static CTFontDescriptorRef cascadeToLastResortAndDisableSwashesFontDescriptor()
{
    static CTFontDescriptorRef descriptor;
    if (descriptor)
        return descriptor;

    RetainPtr<CFDictionaryRef> lineInitialSwashesOffSetting(AdoptCF, createFeatureSettingDictionary(kSmartSwashType, kLineInitialSwashesOffSelector));
    RetainPtr<CFDictionaryRef> lineFinalSwashesOffSetting(AdoptCF, createFeatureSettingDictionary(kSmartSwashType, kLineFinalSwashesOffSelector));

    const void* settingDictionaries[] = { lineInitialSwashesOffSetting.get(), lineFinalSwashesOffSetting.get() };
    RetainPtr<CFArrayRef> featureSettings(AdoptCF, CFArrayCreate(kCFAllocatorDefault, settingDictionaries, WTF_ARRAY_LENGTH(settingDictionaries), &kCFTypeArrayCallBacks));

    const void* keys[] = { kCTFontFeatureSettingsAttribute };
    const void* values[] = { featureSettings.get() };
    RetainPtr<CFDictionaryRef> attributes(AdoptCF, CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    descriptor = CTFontDescriptorCreateCopyWithAttributes(cascadeToLastResortFontDescriptor(), attributes.get());

    return descriptor;
}

// Adding a cascade list breaks the font on Leopard
static bool canSetCascadeListForCustomFont()
{
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
    return true;
#else
    return false;
#endif
}

CTFontRef FontPlatformData::ctFont() const
{
    if (m_CTFont)
        return m_CTFont.get();

#if PLATFORM(CHROMIUM)
    if (m_inMemoryFont) {
        m_CTFont.adoptCF(CTFontCreateWithGraphicsFont(m_inMemoryFont->cgFont(), m_size, 0, canSetCascadeListForCustomFont() ? cascadeToLastResortFontDescriptor() : 0));
        return m_CTFont.get();
    }
#endif

    m_CTFont = toCTFontRef(m_font);
    if (m_CTFont) {
        CTFontDescriptorRef fontDescriptor;
        RetainPtr<CFStringRef> postScriptName(AdoptCF, CTFontCopyPostScriptName(m_CTFont.get()));
        // Hoefler Text Italic has line-initial and -final swashes enabled by default, so disable them.
        if (CFEqual(postScriptName.get(), CFSTR("HoeflerText-Italic")) || CFEqual(postScriptName.get(), CFSTR("HoeflerText-BlackItalic")))
            fontDescriptor = cascadeToLastResortAndDisableSwashesFontDescriptor();
        else
            fontDescriptor = cascadeToLastResortFontDescriptor();
        m_CTFont.adoptCF(CTFontCreateCopyWithAttributes(m_CTFont.get(), m_size, 0, fontDescriptor));
    } else
        m_CTFont.adoptCF(CTFontCreateWithGraphicsFont(m_cgFont.get(), m_size, 0, canSetCascadeListForCustomFont() ? cascadeToLastResortFontDescriptor() : 0));

    if (m_widthVariant != RegularWidth) {
        int featureTypeValue = kTextSpacingType;
        int featureSelectorValue = mapFontWidthVariantToCTFeatureSelector(m_widthVariant);
        RetainPtr<CTFontDescriptorRef> sourceDescriptor(AdoptCF, CTFontCopyFontDescriptor(m_CTFont.get()));
        RetainPtr<CFNumberRef> featureType(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureTypeValue));
        RetainPtr<CFNumberRef> featureSelector(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &featureSelectorValue));
        RetainPtr<CTFontDescriptorRef> newDescriptor(AdoptCF, CTFontDescriptorCreateCopyWithFeature(sourceDescriptor.get(), featureType.get(), featureSelector.get()));
        RetainPtr<CTFontRef> newFont(AdoptCF, CTFontCreateWithFontDescriptor(newDescriptor.get(), m_size, 0));

        if (newFont)
            m_CTFont = newFont;
    }

    return m_CTFont.get();
}

#if PLATFORM(CHROMIUM) && OS(DARWIN)
static bool isAATFont(CTFontRef ctFont)
{
    CFDataRef table = CTFontCopyTable(ctFont, kCTFontTableMort, 0);
    if (table) {
        CFRelease(table);
        return true;
    }
    table = CTFontCopyTable(ctFont, kCTFontTableMorx, 0);
    if (table) {
        CFRelease(table);
        return true;
    }
    return false;
}

HarfBuzzNGFace* FontPlatformData::harfbuzzFace()
{
    CTFontRef font = ctFont();
    // HarfBuzz can't handle AAT font
    if (isAATFont(font))
        return 0;

    if (!m_harfbuzzFace) {
        uint64_t uniqueID = reinterpret_cast<uintptr_t>(font);
        m_harfbuzzFace = HarfBuzzNGFace::create(const_cast<FontPlatformData*>(this), uniqueID);
    }
    return m_harfbuzzFace.get();
}
#endif

#ifndef NDEBUG
String FontPlatformData::description() const
{
    RetainPtr<CFStringRef> cgFontDescription(AdoptCF, CFCopyDescription(cgFont()));
    return String(cgFontDescription.get()) + " " + String::number(m_size)
            + (m_syntheticBold ? " synthetic bold" : "") + (m_syntheticOblique ? " synthetic oblique" : "") + (m_orientation ? " vertical orientation" : "");
}
#endif

} // namespace WebCore
