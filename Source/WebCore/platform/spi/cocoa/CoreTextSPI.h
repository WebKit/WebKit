/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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

#ifndef CoreTextSPI_h
#define CoreTextSPI_h

#include "CoreGraphicsSPI.h"
#include <CoreText/CoreText.h>

#if USE(APPLE_INTERNAL_SDK)

#include <CoreText/CoreTextPriv.h>

#else

enum {
    kCTFontUIFontSystemItalic = 27,
    kCTFontUIFontSystemThin = 102,
    kCTFontUIFontSystemLight = 103,
    kCTFontUIFontSystemUltraLight = 104,
};

#endif

extern "C" {

typedef const UniChar* (*CTUniCharProviderCallback)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void* refCon);
typedef void (*CTUniCharDisposeCallback)(const UniChar* chars, void* refCon);

extern const CFStringRef kCTFontReferenceURLAttribute;
extern const CFStringRef kCTFontOpticalSizeAttribute;

#if PLATFORM(COCOA)
#if !USE(APPLE_INTERNAL_SDK)
typedef CF_OPTIONS(uint32_t, CTFontTransformOptions)
{
    kCTFontTransformApplyShaping = (1 << 0),
    kCTFontTransformApplyPositioning = (1 << 1)
};
#endif
bool CTFontTransformGlyphs(CTFontRef, CGGlyph glyphs[], CGSize advances[], CFIndex count, CTFontTransformOptions);
#endif

CGSize CTRunGetInitialAdvance(CTRunRef run);
CTLineRef CTLineCreateWithUniCharProvider(CTUniCharProviderCallback provide, CTUniCharDisposeCallback dispose, void* refCon);
CTTypesetterRef CTTypesetterCreateWithUniCharProviderAndOptions(CTUniCharProviderCallback provide, CTUniCharDisposeCallback dispose, void* refCon, CFDictionaryRef options);
bool CTFontGetVerticalGlyphsForCharacters(CTFontRef, const UniChar characters[], CGGlyph glyphs[], CFIndex count);
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 101000
bool CTFontSetRenderingStyle(CTFontRef, CGContextRef, CGFontRenderingStyle* originalStyle, CGSize* originalDilation);
#endif

CTFontDescriptorRef CTFontDescriptorCreateForUIType(CTFontUIFontType, CGFloat size, CFStringRef language);
CTFontDescriptorRef CTFontDescriptorCreateWithTextStyle(CFStringRef style, CFStringRef size, CFStringRef language);
CTFontDescriptorRef CTFontDescriptorCreateCopyWithSymbolicTraits(CTFontDescriptorRef original, CTFontSymbolicTraits symTraitValue, CTFontSymbolicTraits symTraitMask);
CFBitVectorRef CTFontCopyGlyphCoverageForFeature(CTFontRef, CFDictionaryRef feature);

#if PLATFORM(COCOA)
#if !USE(APPLE_INTERNAL_SDK)
typedef CF_OPTIONS(uint32_t, CTFontDescriptorOptions)
{
    kCTFontDescriptorOptionSystemUIFont = 1 << 1,
    kCTFontDescriptorOptionPreferAppleSystemFont = kCTFontOptionsPreferSystemFont
};

CTFontDescriptorRef CTFontDescriptorCreateWithAttributesAndOptions(CFDictionaryRef attributes, CTFontDescriptorOptions);
#endif
#endif

bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef);
CTFontRef CTFontCreateForCSS(CFStringRef name, uint16_t weight, CTFontSymbolicTraits, CGFloat size);
CTFontRef CTFontCreateForCharactersWithLanguage(CTFontRef currentFont, const UTF16Char *characters, CFIndex length, CFStringRef language, CFIndex *coveredLength);

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
extern const CFStringRef kCTUIFontTextStyleShortHeadline;
extern const CFStringRef kCTUIFontTextStyleShortBody;
extern const CFStringRef kCTUIFontTextStyleShortSubhead;
extern const CFStringRef kCTUIFontTextStyleShortFootnote;
extern const CFStringRef kCTUIFontTextStyleShortCaption1;
extern const CFStringRef kCTUIFontTextStyleTallBody;

extern const CFStringRef kCTUIFontTextStyleHeadline;
extern const CFStringRef kCTUIFontTextStyleBody;
extern const CFStringRef kCTUIFontTextStyleSubhead;
extern const CFStringRef kCTUIFontTextStyleFootnote;
extern const CFStringRef kCTUIFontTextStyleCaption1;
extern const CFStringRef kCTUIFontTextStyleCaption2;

extern const CFStringRef kCTFontDescriptorTextStyleEmphasized;
#endif

#if PLATFORM(IOS)
extern const CFStringRef kCTUIFontTextStyleTitle1;
extern const CFStringRef kCTUIFontTextStyleTitle2;
extern const CFStringRef kCTUIFontTextStyleTitle3;
CTFontDescriptorRef CTFontCreatePhysicalFontDescriptorForCharactersWithLanguage(CTFontRef currentFont, const UTF16Char* characters, CFIndex length, CFStringRef language, CFIndex* coveredLength);
#endif

CTFontRef CTFontCreatePhysicalFontForCharactersWithLanguage(CTFontRef, const UTF16Char* characters, CFIndex length, CFStringRef language, CFIndex* coveredLength);
bool CTFontIsAppleColorEmoji(CTFontRef);
bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef);
CTFontRef CTFontCreateForCharacters(CTFontRef currentFont, const UTF16Char *characters, CFIndex length, CFIndex *coveredLength);

}

#endif
