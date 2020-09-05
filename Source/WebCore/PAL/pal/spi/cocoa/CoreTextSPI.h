/*
 * Copyright (C) 2014-2016 Apple Inc.  All rights reserved.
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

#pragma once

#include <CoreText/CoreText.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>

#if USE(APPLE_INTERNAL_SDK)

#include <CoreText/CoreTextPriv.h>

#else

enum {
    kCTFontUIFontSystemItalic = 27,
    kCTFontUIFontSystemThin = 102,
    kCTFontUIFontSystemLight = 103,
    kCTFontUIFontSystemUltraLight = 104,
};

typedef CF_OPTIONS(uint32_t, CTFontTransformOptions) {
    kCTFontTransformApplyShaping = (1 << 0),
    kCTFontTransformApplyPositioning = (1 << 1)
};

typedef CF_OPTIONS(CFOptionFlags, CTFontShapeOptions) {
    kCTFontShapeWithKerning = (1 << 0),
    kCTFontShapeWithClusterComposition = (1 << 1),
    kCTFontShapeRightToLeft = (1 << 2),
};

typedef CF_OPTIONS(uint32_t, CTFontDescriptorOptions) {
    kCTFontDescriptorOptionSystemUIFont = 1 << 1,
    kCTFontDescriptorOptionPreferAppleSystemFont = kCTFontOptionsPreferSystemFont
};

enum {
    kCTRunStatusHasOrigins = (1 << 4),
};

typedef CF_OPTIONS(CFOptionFlags, CTFontFallbackOption) {
    kCTFontFallbackOptionNone = 0,
    kCTFontFallbackOptionSystem = 1 << 0,
    kCTFontFallbackOptionUserInstalled = 1 << 1,
    kCTFontFallbackOptionDefault = kCTFontFallbackOptionSystem | kCTFontFallbackOptionUserInstalled,
};

typedef CF_ENUM(uint8_t, CTCompositionLanguage)
{
    kCTCompositionLanguageUnset,
    kCTCompositionLanguageNone,
    kCTCompositionLanguageJapanese,
    kCTCompositionLanguageSimplifiedChinese,
    kCTCompositionLanguageTraditionalChinese,
};

#endif

WTF_EXTERN_C_BEGIN

typedef const UniChar* (*CTUniCharProviderCallback)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void* refCon);
typedef void (*CTUniCharDisposeCallback)(const UniChar* chars, void* refCon);

extern const CFStringRef kCTFontReferenceURLAttribute;
extern const CFStringRef kCTFontOpticalSizeAttribute;
extern const CFStringRef kCTFontPostScriptNameAttribute;
extern const CFStringRef kCTFontUserInstalledAttribute;
extern const CFStringRef kCTFontFallbackOptionAttribute;

extern const CFStringRef kCTFontCSSFamilySerif;
extern const CFStringRef kCTFontCSSFamilySansSerif;
extern const CFStringRef kCTFontCSSFamilyCursive;
extern const CFStringRef kCTFontCSSFamilyFantasy;
extern const CFStringRef kCTFontCSSFamilyMonospace;
extern const CFStringRef kCTFontCSSFamilySystemUI;

bool CTFontTransformGlyphs(CTFontRef, CGGlyph glyphs[], CGSize advances[], CFIndex count, CTFontTransformOptions);
#if USE(CTFONTSHAPEGLYPHS)
// CTFontShapeOptions isn't defined on Mojave.
CGSize CTFontShapeGlyphs(CTFontRef, CGGlyph glyphs[], CGSize advances[], CGPoint origins[], CFIndex indexes[], const UniChar chars[], CFIndex count, CTFontShapeOptions, CFStringRef language, void (^handler)(CFRange, CGGlyph**, CGSize**, CGPoint**, CFIndex**));
#endif

CGSize CTRunGetInitialAdvance(CTRunRef);
CTLineRef CTLineCreateWithUniCharProvider(CTUniCharProviderCallback, CTUniCharDisposeCallback, void* refCon);
void CTRunGetBaseAdvancesAndOrigins(CTRunRef, CFRange, CGSize baseAdvances[], CGPoint origins[]);
CTTypesetterRef CTTypesetterCreateWithUniCharProviderAndOptions(CTUniCharProviderCallback, CTUniCharDisposeCallback, void* refCon, CFDictionaryRef options);
bool CTFontGetVerticalGlyphsForCharacters(CTFontRef, const UniChar characters[], CGGlyph glyphs[], CFIndex count);
void CTFontGetUnsummedAdvancesForGlyphsAndStyle(CTFontRef, CTFontOrientation, CGFontRenderingStyle, const CGGlyph[], CGSize advances[], CFIndex count);
CTFontDescriptorRef CTFontDescriptorCreateForCSSFamily(CFStringRef cssFamily, CFStringRef language);
bool CTFontGetGlyphsForCharacterRange(CTFontRef, CGGlyph glyphs[], CFRange);

CTFontDescriptorRef CTFontDescriptorCreateForUIType(CTFontUIFontType, CGFloat size, CFStringRef language);
CTFontDescriptorRef CTFontDescriptorCreateWithTextStyle(CFStringRef style, CFStringRef size, CFStringRef language);
CTFontDescriptorRef CTFontDescriptorCreateCopyWithSymbolicTraits(CTFontDescriptorRef original, CTFontSymbolicTraits symTraitValue, CTFontSymbolicTraits symTraitMask);
CFBitVectorRef CTFontCopyGlyphCoverageForFeature(CTFontRef, CFDictionaryRef feature);

CTFontDescriptorRef CTFontDescriptorCreateWithAttributesAndOptions(CFDictionaryRef attributes, CTFontDescriptorOptions);
CTFontDescriptorRef CTFontDescriptorCreateLastResort();

CFArrayRef CTFontManagerCreateFontDescriptorsFromData(CFDataRef);

void CTParagraphStyleSetCompositionLanguage(CTParagraphStyleRef, CTCompositionLanguage);

extern const CFStringRef kCTFontCSSWeightAttribute;
extern const CFStringRef kCTFontCSSWidthAttribute;
extern const CFStringRef kCTFontDescriptorTextStyleAttribute;
extern const CFStringRef kCTFontUIFontDesignTrait;

extern const CFStringRef kCTFontUIFontDesignDefault;
extern const CFStringRef kCTFontUIFontDesignSerif;
extern const CFStringRef kCTFontUIFontDesignMonospaced;
extern const CFStringRef kCTFontUIFontDesignRounded;

extern const CFStringRef kCTFrameMaximumNumberOfLinesAttributeName;

bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef);
bool CTFontIsSystemUIFont(CTFontRef);
CTFontRef CTFontCreateForCSS(CFStringRef name, uint16_t weight, CTFontSymbolicTraits, CGFloat size);
CTFontRef CTFontCreateForCharactersWithLanguage(CTFontRef currentFont, const UTF16Char *characters, CFIndex length, CFStringRef language, CFIndex *coveredLength);
CTFontRef CTFontCreateForCharactersWithLanguageAndOption(CTFontRef currentFont, const UTF16Char *characters, CFIndex length, CFStringRef language, CTFontFallbackOption option, CFIndex *coveredLength);
CTFontRef CTFontCopyPhysicalFont(CTFontRef);

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

extern const CFStringRef kCTFontContentSizeCategoryL;
extern const CFStringRef kCTFontContentSizeCategoryXXXL;

extern const CGFloat kCTFontWeightUltraLight;
extern const CGFloat kCTFontWeightThin;
extern const CGFloat kCTFontWeightLight;
extern const CGFloat kCTFontWeightRegular;
extern const CGFloat kCTFontWeightMedium;
extern const CGFloat kCTFontWeightSemibold;
extern const CGFloat kCTFontWeightBold;
extern const CGFloat kCTFontWeightHeavy;
extern const CGFloat kCTFontWeightBlack;

extern const CFStringRef kCTUIFontTextStyleTitle0;
extern const CFStringRef kCTUIFontTextStyleTitle1;
extern const CFStringRef kCTUIFontTextStyleTitle2;
extern const CFStringRef kCTUIFontTextStyleTitle3;
extern const CFStringRef kCTUIFontTextStyleTitle4;
CTFontDescriptorRef CTFontCreatePhysicalFontDescriptorForCharactersWithLanguage(CTFontRef currentFont, const UTF16Char* characters, CFIndex length, CFStringRef language, CFIndex* coveredLength);

bool CTFontIsAppleColorEmoji(CTFontRef);
CTFontRef CTFontCreateForCharacters(CTFontRef currentFont, const UTF16Char *characters, CFIndex length, CFIndex *coveredLength);

WTF_EXTERN_C_END
