/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

WTF_EXTERN_C_BEGIN

typedef const struct __CTFont* CTFontRef;
typedef const struct __CTLine* CTLineRef;
typedef UInt32 FourCharCode;
typedef FourCharCode CTFontTableTag;

CT_EXPORT const CFStringRef kCTFontAttributeName;
CT_EXPORT const CFStringRef kCTFontCascadeListAttribute;
CT_EXPORT const CFStringRef kCTFontNameAttribute;
CT_EXPORT const CFStringRef kCTForegroundColorFromContextAttributeName;
CT_EXPORT const CFStringRef kCTStrokeWidthAttributeName;
CT_EXPORT const CFStringRef kCTStrokeColorAttributeName;
CT_EXPORT const CFStringRef kCTFontOpenTypeFeatureTag;
CT_EXPORT const CFStringRef kCTFontFeatureTypeIdentifierKey;
CT_EXPORT const CFStringRef kCTFontOpenTypeFeatureValue;
CT_EXPORT const CFStringRef kCTFontFeatureSelectorIdentifierKey;
CT_EXPORT const CFStringRef kCTFontFeatureTypeSelectorsKey;
CT_EXPORT const CFStringRef kCTFontFeatureSelectorDefaultKey;
CT_EXPORT const CFStringRef kCTFontFeatureSettingsAttribute;
CT_EXPORT const CFStringRef kCTFontFixedAdvanceAttribute;
CT_EXPORT const CFStringRef kCTFontVariationAttribute;
CT_EXPORT const CFStringRef kCTFontURLAttribute;
CT_EXPORT const CFStringRef kCTFontReferenceURLAttribute;

// These enums are defined in CTFont.h. To avoid redefinition, only define them here if CTFont.h has not been included. 
#ifndef __CTFONT__
typedef CF_OPTIONS(uint32_t, CTFontSymbolicTraits) {
    kCTFontTraitItalic = (1 << 0),
    kCTFontTraitBold = (1 << 1),
    kCTFontTraitMonoSpace = (1 << 10),
    kCTFontTraitColorGlyphs = (1 << 13),

    kCTFontItalicTrait = kCTFontTraitItalic,
    kCTFontBoldTrait = kCTFontTraitBold,
    kCTFontMonoSpaceTrait = kCTFontTraitMonoSpace,
    kCTFontColorGlyphsTrait = kCTFontTraitColorGlyphs,
};

typedef CF_OPTIONS(uint32_t, CTFontTableOptions) {
    kCTFontTableOptionNoOptions = 0,
    kCTFontTableOptionExcludeSynthetic = (1 << 0)
};

enum {
    kCTFontTableMATH = 'MATH',
    kCTFontTableOS2  = 'OS/2',
    kCTFontTableVORG = 'VORG',
    kCTFontTableHead = 'head',
    kCTFontTableVhea = 'vhea',
};

typedef CF_OPTIONS(uint32_t, CTFontTransformOptions) {
    kCTFontTransformApplyShaping = (1 << 0),
    kCTFontTransformApplyPositioning = (1 << 1)
};

enum {
    kLowerCaseType = 37,
    kUpperCaseType = 38,
};

enum {
    kDefaultLowerCaseSelector = 0,
    kLowerCaseSmallCapsSelector = 1,
    kLowerCasePetiteCapsSelector = 2
};

enum {
    kDefaultUpperCaseSelector = 0,
    kUpperCaseSmallCapsSelector = 1,
    kUpperCasePetiteCapsSelector = 2
};

enum {
    kProportionalTextSelector = 0,
    kHalfWidthTextSelector = 2,
    kThirdWidthTextSelector = 3,
    kQuarterWidthTextSelector = 4,
};

typedef CF_ENUM(uint32_t, CTFontOrientation)
{
    kCTFontOrientationDefault = 0,
    kCTFontOrientationHorizontal = 1,
    kCTFontOrientationVertical = 2,
};
#endif

unsigned CTFontGetUnitsPerEm(CTFontRef);
CGFloat CTFontGetCapHeight(CTFontRef);
CGFloat CTFontGetLeading(CTFontRef);
CGFloat CTFontGetAscent(CTFontRef);
CGFloat CTFontGetDescent(CTFontRef);
CGFloat CTFontGetXHeight(CTFontRef);
CGFloat CTFontGetUnderlinePosition(CTFontRef);
CGFloat CTFontGetUnderlineThickness(CTFontRef);
CGFloat CTFontGetSize(CTFontRef);
CTFontSymbolicTraits CTFontGetSymbolicTraits(CTFontRef);
CFCharacterSetRef CTFontCopyCharacterSet(CTFontRef);
CFStringRef CTFontCopyFamilyName(CTFontRef);
CFStringRef CTFontCopyFullName(CTFontRef);
CTFontRef CTFontCreateCopyWithAttributes(CTFontRef, CGFloat size, const CGAffineTransform* matrix, CTFontDescriptorRef attributes);
CTFontRef CTFontCreateWithName(CFStringRef, CGFloat size, const CGAffineTransform*);
CTFontRef CTFontCreateWithGraphicsFont(CGFontRef, CGFloat size, const CGAffineTransform*, CTFontDescriptorRef attributes);
CTFontRef CTFontCreateWithFontDescriptor(CTFontDescriptorRef, CGFloat size, const CGAffineTransform*);
CTLineRef CTLineCreateWithAttributedString(CFAttributedStringRef);
void CTLineDraw(CTLineRef, CGContextRef);
CFDataRef CTFontCopyTable(CTFontRef, CTFontTableTag, CTFontTableOptions);
CFArrayRef CTFontCopyAvailableTables(CTFontRef, CTFontTableOptions);
CGPathRef CTFontCreatePathForGlyph(CTFontRef, CGGlyph, const CGAffineTransform*);
void CTFontGetVerticalTranslationsForGlyphs(CTFontRef, const CGGlyph[], CGSize translations[], CFIndex count);
void CTFontDrawGlyphs(CTFontRef, const CGGlyph[], const CGPoint[], size_t count, CGContextRef);
CGAffineTransform CTFontGetMatrix(CTFontRef);
CFArrayRef CTFontCopyFeatures(CTFontRef);
CFTypeRef CTFontCopyAttribute(CTFontRef, CFStringRef);
CTFontDescriptorRef CTFontCopyFontDescriptor(CTFontRef);
bool CTFontTransformGlyphs(CTFontRef, CGGlyph[], CGSize advances[], CFIndex count, CTFontTransformOptions);
CGRect CTFontGetBoundingRectsForGlyphs(CTFontRef, CTFontOrientation, const CGGlyph[], CGRect boundingRects[], CFIndex count);
double CTFontGetAdvancesForGlyphs(CTFontRef, CTFontOrientation, const CGGlyph[], CGSize advances[], CFIndex count);
bool CTFontGetGlyphsForCharacters(CTFontRef, const UniChar[], CGGlyph[], CFIndex count);
bool CTFontGetGlyphsForCharacterRange(CTFontRef, CGGlyph[], CFRange);
CFDataRef CTFontCopyTable(CTFontRef, CTFontTableTag, CTFontTableOptions);
bool CTFontGetVerticalGlyphsForCharacters(CTFontRef, const UniChar[], CGGlyph[], CFIndex count);

CTFontDescriptorRef CTFontDescriptorCreateWithAttributes(CFDictionaryRef);
bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef);
CTFontDescriptorRef CTFontDescriptorCreateWithNameAndSize(CFStringRef, CGFloat);

WTF_EXTERN_C_END

