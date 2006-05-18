/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FontCache.h"
#import "FontPlatformData.h"
#import "Font.h"
#import "WebFontCache.h"
#import "WebCoreSystemInterface.h"
#import "KWQListBox.h"
#import "WebCoreStringTruncator.h"

namespace WebCore
{

static bool getAppDefaultValue(CFStringRef key, int *v)
{
    CFPropertyListRef value;

    value = CFPreferencesCopyValue(key, kCFPreferencesCurrentApplication,
                                   kCFPreferencesAnyUser,
                                   kCFPreferencesAnyHost);
    if (value == 0) {
        value = CFPreferencesCopyValue(key, kCFPreferencesCurrentApplication,
                                       kCFPreferencesCurrentUser,
                                       kCFPreferencesAnyHost);
        if (value == 0)
            return false;
    }

    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        if (v != 0)
            CFNumberGetValue((const CFNumberRef)value, kCFNumberIntType, v);
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        if (v != 0)
            *v = CFStringGetIntValue((const CFStringRef)value);
    } else {
        CFRelease(value);
        return false;
    }

    CFRelease(value);
    return true;
}

static bool getUserDefaultValue(CFStringRef key, int *v)
{
    CFPropertyListRef value;

    value = CFPreferencesCopyValue(key, kCFPreferencesAnyApplication,
                                   kCFPreferencesCurrentUser,
                                   kCFPreferencesCurrentHost);
    if (value == 0)
        return false;

    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        if (v != 0)
            CFNumberGetValue((const CFNumberRef)value, kCFNumberIntType, v);
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        if (v != 0)
            *v = CFStringGetIntValue((const CFStringRef)value);
    } else {
        CFRelease(value);
        return false;
    }

    CFRelease(value);
    return true;
}

static int getLCDScaleParameters(void)
{
    int mode;
    CFStringRef key;

    key = CFSTR("AppleFontSmoothing");
    if (!getAppDefaultValue(key, &mode)) {
        if (!getUserDefaultValue(key, &mode))
            return 1;
    }

    if (wkFontSmoothingModeIsLCD(mode))
        return 4;
    return 1;
}

#define MINIMUM_GLYPH_CACHE_SIZE 1536 * 1024

void FontCache::platformInit()
{
    size_t s = MINIMUM_GLYPH_CACHE_SIZE*getLCDScaleParameters();

    wkSetUpFontCache(s);
}

const FontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    NSFont* nsFont = font.getNSFont();
    NSString *string = [[NSString alloc] initWithCharactersNoCopy:(UniChar*)characters length:length freeWhenDone:NO];
    NSFont* substituteFont = wkGetFontInLanguageForRange(nsFont, string, NSMakeRange(0, [string length]));
    if (!substituteFont && [string length] == 1)
        substituteFont = wkGetFontInLanguageForCharacter(nsFont, [string characterAtIndex:0]);
    [string release];
    
    // Now that we have a substitute font, attempt to match it to the best variation.
    // If we have a good match return that, otherwise return the font the AppKit has found.
    if (substituteFont) {
        NSFontTraitMask traits = 0;
        if (font.fontDescription().italic())
            traits |= NSItalicFontMask;
        if (font.fontDescription().weight() >= WebCore::cBoldWeight)
            traits |= NSBoldFontMask;
        float size = font.fontDescription().computedPixelSize();
    
        NSFontManager *manager = [NSFontManager sharedFontManager];
        NSFont *bestVariation = [manager fontWithFamily:[substituteFont familyName]
            traits:traits
            weight:[manager weightOfFont:nsFont]
            size:size];
        if (bestVariation)
            substituteFont = bestVariation;
        
        substituteFont = font.fontDescription().usePrinterFont() ? [substituteFont printerFont] : [substituteFont screenFont];

        NSFontTraitMask actualTraits = [manager traitsOfFont:substituteFont];
        FontPlatformData alternateFont(substituteFont, 
                                       (traits & NSBoldFontMask) && !(actualTraits & NSBoldFontMask),
                                       (traits & NSItalicFontMask) && !(actualTraits & NSItalicFontMask));
        return getCachedFontData(&alternateFont);
    }

    return 0;
}

FontPlatformData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    // Attempt to find an appropriate font using a match based on 
    // the presence of keywords in the the requested names.  For example, we'll
    // match any name that contains "Arabic" to Geeza Pro.
    FontPlatformData* platformData = 0;
    const FontFamily* currFamily = &font.fontDescription().family();
    while (currFamily && !platformData) {
        if (currFamily->family().length()) {
            static String matchWords[3] = { String("Arabic"), String("Pashto"), String("Urdu") };
            static AtomicString geezaStr("Geeza Pro");
            for (int j = 0; j < 3 && !platformData; ++j)
                if (currFamily->family().contains(matchWords[j], false))
                    platformData = getCachedFontPlatformData(font, geezaStr);
        }
        currFamily = currFamily->next();
    }

    return platformData;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const Font& font)
{
    static AtomicString timesStr("Times");
    static AtomicString lucidaGrandeStr("Lucida Grande");

    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
    FontPlatformData* platformFont = getCachedFontPlatformData(font, timesStr);
    if (!platformFont)
        // The Times fallback will almost always work, but in the highly unusual case where
        // the user doesn't have it, we fall back on Lucida Grande because that's
        // guaranteed to be there, according to Nathan Taylor. This is good enough
        // to avoid a crash at least.
        platformFont = getCachedFontPlatformData(font, lucidaGrandeStr);

    return platformFont;
}

FontPlatformData* FontCache::createFontPlatformData(const Font& font, const AtomicString& family)
{
    NSFontTraitMask traits = 0;
    if (font.italic())
        traits |= NSItalicFontMask;
    if (font.bold())
        traits |= NSBoldFontMask;
    float size = font.fontDescription().computedPixelSize();
    
    NSFont* nsFont = [WebFontCache fontWithFamily:family traits:traits size:size];
    if (!nsFont)
        return 0;

    NSFontTraitMask actualTraits = 0;
    if (font.bold() || font.italic())
        actualTraits = [[NSFontManager sharedFontManager] traitsOfFont:nsFont];
    
    FontPlatformData* result = new FontPlatformData;
    
    // Use the correct font for print vs. screen.
    result->font = font.fontDescription().usePrinterFont() ? [nsFont printerFont] : [nsFont screenFont];
    result->syntheticBold = (traits & NSBoldFontMask) && !(actualTraits & NSBoldFontMask);
    result->syntheticOblique = (traits & NSItalicFontMask) && !(actualTraits & NSItalicFontMask);
    return result;
}

}