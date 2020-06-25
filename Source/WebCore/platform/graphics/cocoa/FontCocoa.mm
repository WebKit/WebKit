/*
 * Copyright (C) 2005, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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

#import "config.h"
#import "Font.h"

#import "Color.h"
#import "FloatRect.h"
#import "FontCache.h"
#import "FontCascade.h"
#import "FontDescription.h"
#import "LocaleCocoa.h"
#import "OpenTypeCG.h"
#import "SharedBuffer.h"
#import <CoreText/CoreText.h>
#import <float.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/CoreTextSPI.h>
#import <unicode/uchar.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif


#if USE(APPKIT)
@interface NSFont (WebAppKitSecretAPI)
- (BOOL)_isFakeFixedPitch;
@end
#endif

namespace WebCore {

static inline bool caseInsensitiveCompare(CFStringRef a, CFStringRef b)
{
    return a && CFStringCompare(a, b, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
}

static bool fontHasVerticalGlyphs(CTFontRef ctFont)
{
    // The check doesn't look neat but this is what AppKit does for vertical writing...
    RetainPtr<CFArrayRef> tableTags = adoptCF(CTFontCopyAvailableTables(ctFont, kCTFontTableOptionNoOptions));
    if (!tableTags)
        return false;
    CFIndex numTables = CFArrayGetCount(tableTags.get());
    for (CFIndex index = 0; index < numTables; ++index) {
        CTFontTableTag tag = (CTFontTableTag)(uintptr_t)CFArrayGetValueAtIndex(tableTags.get(), index);
        if (tag == kCTFontTableVhea || tag == kCTFontTableVORG)
            return true;
    }
    return false;
}

#if PLATFORM(IOS_FAMILY)
bool fontFamilyShouldNotBeUsedForArabic(CFStringRef fontFamilyName)
{
    if (!fontFamilyName)
        return false;

    // Times New Roman and Arial are not performant enough to use. <rdar://problem/21333326>
    // FIXME <rdar://problem/12096835> remove this function once the above bug is fixed.
    return (CFStringCompare(CFSTR("Times New Roman"), fontFamilyName, 0) == kCFCompareEqualTo)
        || (CFStringCompare(CFSTR("Arial"), fontFamilyName, 0) == kCFCompareEqualTo);
}

static const float kLineHeightAdjustment = 0.15f;

static bool shouldUseAdjustment(CTFontRef font)
{
    RetainPtr<CFStringRef> familyName = adoptCF(CTFontCopyFamilyName(font));

    if (!familyName || !CFStringGetLength(familyName.get()))
        return false;

    return caseInsensitiveCompare(familyName.get(), CFSTR("Times"))
        || caseInsensitiveCompare(familyName.get(), CFSTR("Helvetica"))
        || caseInsensitiveCompare(familyName.get(), CFSTR(".Helvetica NeueUI"));
}

#else

static bool needsAscentAdjustment(CFStringRef familyName)
{
    return familyName && (caseInsensitiveCompare(familyName, CFSTR("Times"))
        || caseInsensitiveCompare(familyName, CFSTR("Helvetica"))
        || caseInsensitiveCompare(familyName, CFSTR("Courier")));
}

#endif

static bool isAhemFont(CFStringRef familyName)
{
    return familyName && caseInsensitiveCompare(familyName, CFSTR("Ahem"));
}

void Font::platformInit()
{
#if PLATFORM(IOS_FAMILY)
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? ceilf(m_platformData.size() / 24.0f) : 0.f;
#else
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? 1.0f : 0.f;
#endif

    unsigned unitsPerEm = CTFontGetUnitsPerEm(m_platformData.font());
    float pointSize = m_platformData.size();
    CGFloat capHeight = pointSize ? CTFontGetCapHeight(m_platformData.font()) : 0;
    CGFloat lineGap = pointSize ? CTFontGetLeading(m_platformData.font()) : 0;
    CGFloat ascent = pointSize ? CTFontGetAscent(m_platformData.font()) : 0;
    CGFloat descent = pointSize ? CTFontGetDescent(m_platformData.font()) : 0;

    // The Open Font Format describes the OS/2 USE_TYPO_METRICS flag as follows:
    // "If set, it is strongly recommended to use OS/2.sTypoAscender - OS/2.sTypoDescender+ OS/2.sTypoLineGap as a value for default line spacing for this font."
    // On OS X, we only apply this rule in the important case of fonts with a MATH table.
    if (OpenType::fontHasMathTable(m_platformData.ctFont())) {
        short typoAscent, typoDescent, typoLineGap;
        if (OpenType::tryGetTypoMetrics(m_platformData.font(), typoAscent, typoDescent, typoLineGap)) {
            ascent = scaleEmToUnits(typoAscent, unitsPerEm) * pointSize;
            descent = -scaleEmToUnits(typoDescent, unitsPerEm) * pointSize;
            lineGap = scaleEmToUnits(typoLineGap, unitsPerEm) * pointSize;
        }
    }

    auto familyName = adoptCF(CTFontCopyFamilyName(m_platformData.font()));

    // Disable antialiasing when rendering with Ahem because many tests require this.
    if (isAhemFont(familyName.get()))
        m_allowsAntialiasing = false;

#if PLATFORM(MAC)
    // We need to adjust Times, Helvetica, and Courier to closely match the
    // vertical metrics of their Microsoft counterparts that are the de facto
    // web standard. The AppKit adjustment of 20% is too big and is
    // incorrectly added to line spacing, so we use a 15% adjustment instead
    // and add it to the ascent.
    if (origin() == Origin::Local && needsAscentAdjustment(familyName.get()))
        ascent += std::round((ascent + descent) * 0.15f);
#endif

    // Compute line spacing before the line metrics hacks are applied.
    float lineSpacing = lroundf(ascent) + lroundf(descent) + lroundf(lineGap);

#if PLATFORM(MAC)
    // Hack Hiragino line metrics to allow room for marked text underlines.
    // <rdar://problem/5386183>
    if (descent < 3 && lineGap >= 3 && familyName && CFStringHasPrefix(familyName.get(), CFSTR("Hiragino"))) {
        lineGap -= 3 - descent;
        descent = 3;
    }
#endif
    
    if (platformData().orientation() == FontOrientation::Vertical && !isTextOrientationFallback())
        m_hasVerticalGlyphs = fontHasVerticalGlyphs(m_platformData.ctFont());

#if PLATFORM(IOS_FAMILY)
    CGFloat adjustment = shouldUseAdjustment(m_platformData.font()) ? ceil((ascent + descent) * kLineHeightAdjustment) : 0;

    lineGap = ceilf(lineGap);
    lineSpacing = ceil(ascent) + adjustment + ceil(descent) + lineGap;
    ascent = ceilf(ascent + adjustment);
    descent = ceilf(descent);

    m_shouldNotBeUsedForArabic = fontFamilyShouldNotBeUsedForArabic(familyName.get());
#endif

    CGFloat xHeight = 0;
    if (m_platformData.size()) {
        if (platformData().orientation() == FontOrientation::Horizontal) {
            // Measure the actual character "x", since it's possible for it to extend below the baseline, and we need the
            // reported x-height to only include the portion of the glyph that is above the baseline.
            Glyph xGlyph = glyphForCharacter('x');
            if (xGlyph)
                xHeight = -CGRectGetMinY(platformBoundsForGlyph(xGlyph));
            else
                xHeight = CTFontGetXHeight(m_platformData.font());
        } else
            xHeight = verticalRightOrientationFont().fontMetrics().xHeight();
    }

    m_fontMetrics.setUnitsPerEm(unitsPerEm);
    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setCapHeight(capHeight);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setXHeight(xHeight);
    m_fontMetrics.setLineSpacing(lineSpacing);
    m_fontMetrics.setUnderlinePosition(-CTFontGetUnderlinePosition(m_platformData.font()));
    m_fontMetrics.setUnderlineThickness(CTFontGetUnderlineThickness(m_platformData.font()));
}

void Font::platformCharWidthInit()
{
    m_avgCharWidth = 0;
    m_maxCharWidth = 0;

    auto os2Table = adoptCF(CTFontCopyTable(m_platformData.font(), kCTFontTableOS2, kCTFontTableOptionNoOptions));
    if (os2Table && CFDataGetLength(os2Table.get()) >= 4) {
        const UInt8* os2 = CFDataGetBytePtr(os2Table.get());
        SInt16 os2AvgCharWidth = os2[2] * 256 + os2[3];
        m_avgCharWidth = scaleEmToUnits(os2AvgCharWidth, m_fontMetrics.unitsPerEm()) * m_platformData.size();
    }

    auto headTable = adoptCF(CTFontCopyTable(m_platformData.font(), kCTFontTableHead, kCTFontTableOptionNoOptions));
    if (headTable && CFDataGetLength(headTable.get()) >= 42) {
        const UInt8* head = CFDataGetBytePtr(headTable.get());
        ushort uxMin = head[36] * 256 + head[37];
        ushort uxMax = head[40] * 256 + head[41];
        SInt16 xMin = static_cast<SInt16>(uxMin);
        SInt16 xMax = static_cast<SInt16>(uxMax);
        float diff = static_cast<float>(xMax - xMin);
        m_maxCharWidth = scaleEmToUnits(diff, m_fontMetrics.unitsPerEm()) * m_platformData.size();
    }

    // Fallback to a cross-platform estimate, which will populate these values if they are non-positive.
    initCharWidths();
}

void Font::platformDestroy()
{
}

bool Font::variantCapsSupportsCharacterForSynthesis(FontVariantCaps fontVariantCaps, UChar32 character) const
{
#if (PLATFORM(IOS_FAMILY) && TARGET_OS_IOS) || PLATFORM(MAC)
    Glyph glyph = glyphForCharacter(character);
    if (!glyph)
        return false;

    switch (fontVariantCaps) {
    case FontVariantCaps::Small: {
        auto& supported = glyphsSupportedBySmallCaps();
        return supported.size() > glyph && supported.get(glyph);
    }
    case FontVariantCaps::Petite: {
        auto& supported = glyphsSupportedByPetiteCaps();
        return supported.size() > glyph && supported.get(glyph);
    }
    case FontVariantCaps::AllSmall: {
        auto& supported = glyphsSupportedByAllSmallCaps();
        return supported.size() > glyph && supported.get(glyph);
    }
    case FontVariantCaps::AllPetite: {
        auto& supported = glyphsSupportedByAllPetiteCaps();
        return supported.size() > glyph && supported.get(glyph);
    }
    default:
        // Synthesis only supports the variant-caps values listed above.
        return true;
    }
#else
    UNUSED_PARAM(character);

    switch (fontVariantCaps) {
    case FontVariantCaps::Small:
    case FontVariantCaps::Petite:
    case FontVariantCaps::AllSmall:
    case FontVariantCaps::AllPetite:
        return false;
    default:
        // Synthesis only supports the variant-caps values listed above.
        return true;
    }
#endif
}

#if (PLATFORM(IOS_FAMILY) && TARGET_OS_IOS) || PLATFORM(MAC)
static RetainPtr<CFDictionaryRef> smallCapsOpenTypeDictionary(CFStringRef key, int rawValue)
{
    RetainPtr<CFNumberRef> value = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawValue));
    CFTypeRef keys[] = { kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue };
    CFTypeRef values[] = { key, value.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

static RetainPtr<CFDictionaryRef> smallCapsTrueTypeDictionary(int rawKey, int rawValue)
{
    RetainPtr<CFNumberRef> key = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawKey));
    RetainPtr<CFNumberRef> value = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawValue));
    CFTypeRef keys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef values[] = { key.get(), value.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

static void unionBitVectors(BitVector& result, CFBitVectorRef source)
{
    CFIndex length = CFBitVectorGetCount(source);
    result.ensureSize(length);
    CFIndex min = 0;
    while (min < length) {
        CFIndex nextIndex = CFBitVectorGetFirstIndexOfBit(source, CFRangeMake(min, length - min), 1);
        if (nextIndex == kCFNotFound)
            break;
        result.set(nextIndex, true);
        min = nextIndex + 1;
    }
}

static void injectOpenTypeCoverage(CFStringRef feature, CTFontRef font, BitVector& result)
{
    RetainPtr<CFBitVectorRef> source = adoptCF(CTFontCopyGlyphCoverageForFeature(font, smallCapsOpenTypeDictionary(feature, 1).get()));
    unionBitVectors(result, source.get());
}

static void injectTrueTypeCoverage(int type, int selector, CTFontRef font, BitVector& result)
{
    RetainPtr<CFBitVectorRef> source = adoptCF(CTFontCopyGlyphCoverageForFeature(font, smallCapsTrueTypeDictionary(type, selector).get()));
    unionBitVectors(result, source.get());
}

const BitVector& Font::glyphsSupportedBySmallCaps() const
{
    if (!m_glyphsSupportedBySmallCaps) {
        m_glyphsSupportedBySmallCaps = BitVector();
        injectOpenTypeCoverage(CFSTR("smcp"), platformData().font(), m_glyphsSupportedBySmallCaps.value());
        injectTrueTypeCoverage(kLowerCaseType, kLowerCaseSmallCapsSelector, platformData().font(), m_glyphsSupportedBySmallCaps.value());
    }
    return m_glyphsSupportedBySmallCaps.value();
}

const BitVector& Font::glyphsSupportedByAllSmallCaps() const
{
    if (!m_glyphsSupportedByAllSmallCaps) {
        m_glyphsSupportedByAllSmallCaps = BitVector();
        injectOpenTypeCoverage(CFSTR("smcp"), platformData().font(), m_glyphsSupportedByAllSmallCaps.value());
        injectOpenTypeCoverage(CFSTR("c2sc"), platformData().font(), m_glyphsSupportedByAllSmallCaps.value());
        injectTrueTypeCoverage(kLowerCaseType, kLowerCaseSmallCapsSelector, platformData().font(), m_glyphsSupportedByAllSmallCaps.value());
        injectTrueTypeCoverage(kUpperCaseType, kUpperCaseSmallCapsSelector, platformData().font(), m_glyphsSupportedByAllSmallCaps.value());
    }
    return m_glyphsSupportedByAllSmallCaps.value();
}

const BitVector& Font::glyphsSupportedByPetiteCaps() const
{
    if (!m_glyphsSupportedByPetiteCaps) {
        m_glyphsSupportedByPetiteCaps = BitVector();
        injectOpenTypeCoverage(CFSTR("pcap"), platformData().font(), m_glyphsSupportedByPetiteCaps.value());
        injectTrueTypeCoverage(kLowerCaseType, kLowerCasePetiteCapsSelector, platformData().font(), m_glyphsSupportedByPetiteCaps.value());
    }
    return m_glyphsSupportedByPetiteCaps.value();
}

const BitVector& Font::glyphsSupportedByAllPetiteCaps() const
{
    if (!m_glyphsSupportedByAllPetiteCaps) {
        m_glyphsSupportedByAllPetiteCaps = BitVector();
        injectOpenTypeCoverage(CFSTR("pcap"), platformData().font(), m_glyphsSupportedByAllPetiteCaps.value());
        injectOpenTypeCoverage(CFSTR("c2pc"), platformData().font(), m_glyphsSupportedByAllPetiteCaps.value());
        injectTrueTypeCoverage(kLowerCaseType, kLowerCasePetiteCapsSelector, platformData().font(), m_glyphsSupportedByAllPetiteCaps.value());
        injectTrueTypeCoverage(kUpperCaseType, kUpperCasePetiteCapsSelector, platformData().font(), m_glyphsSupportedByAllPetiteCaps.value());
    }
    return m_glyphsSupportedByAllPetiteCaps.value();
}
#endif

static RefPtr<Font> createDerivativeFont(CTFontRef font, float size, FontOrientation orientation, CTFontSymbolicTraits fontTraits, bool syntheticBold, bool syntheticItalic)
{
    if (!font)
        return nullptr;

    if (syntheticBold)
        fontTraits |= kCTFontBoldTrait;
    if (syntheticItalic)
        fontTraits |= kCTFontItalicTrait;

    CTFontSymbolicTraits scaledFontTraits = CTFontGetSymbolicTraits(font);

    bool usedSyntheticBold = (fontTraits & kCTFontBoldTrait) && !(scaledFontTraits & kCTFontTraitBold);
    bool usedSyntheticOblique = (fontTraits & kCTFontItalicTrait) && !(scaledFontTraits & kCTFontTraitItalic);
    FontPlatformData scaledFontData(font, size, usedSyntheticBold, usedSyntheticOblique, orientation);

    return Font::create(scaledFontData);
}

static inline bool isOpenTypeFeature(CFDictionaryRef feature)
{
    return CFDictionaryContainsKey(feature, kCTFontOpenTypeFeatureTag) && CFDictionaryContainsKey(feature, kCTFontOpenTypeFeatureValue);
}

static inline bool isTrueTypeFeature(CFDictionaryRef feature)
{
    return CFDictionaryContainsKey(feature, kCTFontFeatureTypeIdentifierKey) && CFDictionaryContainsKey(feature, kCTFontFeatureSelectorIdentifierKey);
}

static inline Optional<CFStringRef> openTypeFeature(CFDictionaryRef feature)
{
    ASSERT(isOpenTypeFeature(feature));
    CFStringRef tag = static_cast<CFStringRef>(CFDictionaryGetValue(feature, kCTFontOpenTypeFeatureTag));
    int rawValue;
    CFNumberRef value = static_cast<CFNumberRef>(CFDictionaryGetValue(feature, kCTFontOpenTypeFeatureValue));
    auto success = CFNumberGetValue(value, kCFNumberIntType, &rawValue);
    ASSERT_UNUSED(success, success);
    return rawValue ? Optional<CFStringRef>(tag) : WTF::nullopt;
}

static inline std::pair<int, int> trueTypeFeature(CFDictionaryRef feature)
{
    ASSERT(isTrueTypeFeature(feature));
    int rawType;
    CFNumberRef type = static_cast<CFNumberRef>(CFDictionaryGetValue(feature, kCTFontFeatureTypeIdentifierKey));
    auto success = CFNumberGetValue(type, kCFNumberIntType, &rawType);
    ASSERT_UNUSED(success, success);
    int rawSelector;
    CFNumberRef selector = static_cast<CFNumberRef>(CFDictionaryGetValue(feature, kCTFontFeatureSelectorIdentifierKey));
    success = CFNumberGetValue(selector, kCFNumberIntType, &rawSelector);
    ASSERT_UNUSED(success, success);
    return std::make_pair(rawType, rawSelector);
}

static inline CFNumberRef defaultSelectorForTrueTypeFeature(int key, CTFontRef font)
{
    RetainPtr<CFArrayRef> features = adoptCF(CTFontCopyFeatures(font));
    CFIndex featureCount = CFArrayGetCount(features.get());
    for (CFIndex i = 0; i < featureCount; ++i) {
        CFDictionaryRef featureType = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(features.get(), i));
        CFNumberRef featureKey = static_cast<CFNumberRef>(CFDictionaryGetValue(featureType, kCTFontFeatureTypeIdentifierKey));
        if (!featureKey)
            continue;
        int rawFeatureKey;
        CFNumberGetValue(featureKey, kCFNumberIntType, &rawFeatureKey);
        if (rawFeatureKey != key)
            continue;

        CFArrayRef featureSelectors = static_cast<CFArrayRef>(CFDictionaryGetValue(featureType, kCTFontFeatureTypeSelectorsKey));
        if (!featureSelectors)
            continue;
        CFIndex selectorsCount = CFArrayGetCount(featureSelectors);
        for (CFIndex j = 0; j < selectorsCount; ++j) {
            CFDictionaryRef featureSelector = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(featureSelectors, j));
            CFNumberRef isDefault = static_cast<CFNumberRef>(CFDictionaryGetValue(featureSelector, kCTFontFeatureSelectorDefaultKey));
            if (!isDefault)
                continue;
            int rawIsDefault;
            CFNumberGetValue(isDefault, kCFNumberIntType, &rawIsDefault);
            if (!rawIsDefault)
                continue;
            return static_cast<CFNumberRef>(CFDictionaryGetValue(featureSelector, kCTFontFeatureSelectorIdentifierKey));
        }
    }
    return nullptr;
}

static inline RetainPtr<CFDictionaryRef> removedFeature(CFDictionaryRef feature, CTFontRef font)
{
    bool isOpenType = isOpenTypeFeature(feature);
    bool isTrueType = isTrueTypeFeature(feature);
    if (!isOpenType && !isTrueType)
        return feature; // We don't understand this font format.
    RetainPtr<CFMutableDictionaryRef> result = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    if (isOpenType) {
        auto featureTag = openTypeFeature(feature);
        if (featureTag && (CFEqual(featureTag.value(), CFSTR("smcp"))
            || CFEqual(featureTag.value(), CFSTR("c2sc"))
            || CFEqual(featureTag.value(), CFSTR("pcap"))
            || CFEqual(featureTag.value(), CFSTR("c2pc")))) {
            int rawZero = 0;
            RetainPtr<CFNumberRef> zero = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawZero));
            CFDictionaryAddValue(result.get(), kCTFontOpenTypeFeatureTag, featureTag.value());
            CFDictionaryAddValue(result.get(), kCTFontOpenTypeFeatureValue, zero.get());
        } else {
            CFDictionaryAddValue(result.get(), kCTFontOpenTypeFeatureTag, CFDictionaryGetValue(feature, kCTFontOpenTypeFeatureTag));
            CFDictionaryAddValue(result.get(), kCTFontOpenTypeFeatureValue, CFDictionaryGetValue(feature, kCTFontOpenTypeFeatureValue));
        }
    }
    if (isTrueType) {
        auto trueTypeFeaturePair = trueTypeFeature(feature);
        if (trueTypeFeaturePair.first == kLowerCaseType && (trueTypeFeaturePair.second == kLowerCaseSmallCapsSelector || trueTypeFeaturePair.second == kLowerCasePetiteCapsSelector)) {
            CFDictionaryAddValue(result.get(), kCTFontFeatureTypeIdentifierKey, CFDictionaryGetValue(feature, kCTFontFeatureTypeIdentifierKey));
            if (CFNumberRef defaultSelector = defaultSelectorForTrueTypeFeature(kLowerCaseType, font))
                CFDictionaryAddValue(result.get(), kCTFontFeatureSelectorIdentifierKey, defaultSelector);
            else
                CFDictionaryAddValue(result.get(), kCTFontFeatureSelectorIdentifierKey, CFDictionaryGetValue(feature, kCTFontFeatureSelectorIdentifierKey));
        } else if (trueTypeFeaturePair.first == kUpperCaseType && (trueTypeFeaturePair.second == kUpperCaseSmallCapsSelector || trueTypeFeaturePair.second == kUpperCasePetiteCapsSelector)) {
            CFDictionaryAddValue(result.get(), kCTFontFeatureTypeIdentifierKey, CFDictionaryGetValue(feature, kCTFontFeatureTypeIdentifierKey));
            if (CFNumberRef defaultSelector = defaultSelectorForTrueTypeFeature(kUpperCaseType, font))
                CFDictionaryAddValue(result.get(), kCTFontFeatureSelectorIdentifierKey, defaultSelector);
            else
                CFDictionaryAddValue(result.get(), kCTFontFeatureSelectorIdentifierKey, CFDictionaryGetValue(feature, kCTFontFeatureSelectorIdentifierKey));
        } else {
            CFDictionaryAddValue(result.get(), kCTFontFeatureTypeIdentifierKey, CFDictionaryGetValue(feature, kCTFontFeatureTypeIdentifierKey));
            CFDictionaryAddValue(result.get(), kCTFontFeatureSelectorIdentifierKey, CFDictionaryGetValue(feature, kCTFontFeatureSelectorIdentifierKey));
        }
    }
    return result;
}

static RetainPtr<CTFontRef> createCTFontWithoutSynthesizableFeatures(CTFontRef font)
{
    RetainPtr<CFArrayRef> features = adoptCF(static_cast<CFArrayRef>(CTFontCopyAttribute(font, kCTFontFeatureSettingsAttribute)));
    if (!features)
        return font;
    CFIndex featureCount = CFArrayGetCount(features.get());
    RetainPtr<CFMutableArrayRef> newFeatures = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, featureCount, &kCFTypeArrayCallBacks));
    for (CFIndex i = 0; i < featureCount; ++i) {
        CFDictionaryRef feature = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(features.get(), i));
        CFArrayAppendValue(newFeatures.get(), removedFeature(feature, font).get());
    }
    CFTypeRef keys[] = { kCTFontFeatureSettingsAttribute };
    CFTypeRef values[] = { newFeatures.get() };
    RetainPtr<CFDictionaryRef> attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr<CTFontDescriptorRef> newDescriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(font, CTFontGetSize(font), nullptr, newDescriptor.get()));
}

RefPtr<Font> Font::createFontWithoutSynthesizableFeatures() const
{
    float size = m_platformData.size();
    CTFontSymbolicTraits fontTraits = CTFontGetSymbolicTraits(m_platformData.font());
    RetainPtr<CTFontRef> ctFont = createCTFontWithoutSynthesizableFeatures(m_platformData.font());
    return createDerivativeFont(ctFont.get(), size, m_platformData.orientation(), fontTraits, m_platformData.syntheticBold(), m_platformData.syntheticOblique());
}

RefPtr<Font> Font::platformCreateScaledFont(const FontDescription&, float scaleFactor) const
{
    float size = m_platformData.size() * scaleFactor;
    CTFontSymbolicTraits fontTraits = CTFontGetSymbolicTraits(m_platformData.font());
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontCopyFontDescriptor(m_platformData.font()));
    RetainPtr<CTFontRef> scaledFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));

    return createDerivativeFont(scaledFont.get(), size, m_platformData.orientation(), fontTraits, m_platformData.syntheticBold(), m_platformData.syntheticOblique());
}

void Font::applyTransforms(GlyphBuffer& glyphBuffer, unsigned beginningIndex, bool enableKerning, bool requiresShaping, const AtomString& locale) const
{
    // FIXME: Implement GlyphBuffer initial advance.
    CTFontTransformOptions options = (enableKerning ? kCTFontTransformApplyPositioning : 0) | (requiresShaping ? kCTFontTransformApplyShaping : 0);
#if USE(CTFONTTRANSFORMGLYPHSWITHLANGUAGE)
    auto handler = ^(CFRange range, CGGlyph** newGlyphsPointer, CGSize** newAdvancesPointer) {
        range.location = std::min(std::max(range.location, static_cast<CFIndex>(0)), static_cast<CFIndex>(glyphBuffer.size()));
        if (range.length < 0) {
            range.length = std::min(range.location, -range.length);
            range.location = range.location - range.length;
            glyphBuffer.remove(beginningIndex + range.location, range.length);
        } else
            glyphBuffer.makeHole(beginningIndex + range.location, range.length, this);
        *newGlyphsPointer = glyphBuffer.glyphs(beginningIndex);
        *newAdvancesPointer = glyphBuffer.advances(beginningIndex);
    };
    CTFontTransformGlyphsWithLanguage(m_platformData.ctFont(), glyphBuffer.glyphs(beginningIndex), reinterpret_cast<CGSize*>(glyphBuffer.advances(beginningIndex)), glyphBuffer.size() - beginningIndex, options, LocaleCocoa::canonicalLanguageIdentifierFromString(locale).string().createCFString().get(), handler);
#else
    UNUSED_PARAM(locale);
    CTFontTransformGlyphs(m_platformData.ctFont(), glyphBuffer.glyphs(beginningIndex), reinterpret_cast<CGSize*>(glyphBuffer.advances(beginningIndex)), glyphBuffer.size() - beginningIndex, options);
#endif
}

static int extractNumber(CFNumberRef number)
{
    int result = 0;
    if (number)
        CFNumberGetValue(number, kCFNumberIntType, &result);
    return result;
}

void Font::determinePitch()
{
    CTFontRef ctFont = m_platformData.font();
    ASSERT(ctFont);

    // Special case Osaka-Mono.
    // According to <rdar://problem/3999467>, we should treat Osaka-Mono as fixed pitch.
    // Note that the AppKit does not report Osaka-Mono as fixed pitch.

    // Special case MS-PGothic.
    // According to <rdar://problem/4032938>, we should not treat MS-PGothic as fixed pitch.
    // Note that AppKit does report MS-PGothic as fixed pitch.

    // Special case MonotypeCorsiva
    // According to <rdar://problem/5454704>, we should not treat MonotypeCorsiva as fixed pitch.
    // Note that AppKit does report MonotypeCorsiva as fixed pitch.

    auto fullName = adoptCF(CTFontCopyFullName(ctFont));
    auto familyName = adoptCF(CTFontCopyFamilyName(ctFont));

    int fixedPitch = extractNumber(adoptCF(static_cast<CFNumberRef>(CTFontCopyAttribute(m_platformData.font(), kCTFontFixedAdvanceAttribute))).get());
    m_treatAsFixedPitch = (CTFontGetSymbolicTraits(ctFont) & kCTFontMonoSpaceTrait) || fixedPitch || (caseInsensitiveCompare(fullName.get(), CFSTR("Osaka-Mono")) || caseInsensitiveCompare(fullName.get(), CFSTR("MS-PGothic")) || caseInsensitiveCompare(fullName.get(), CFSTR("MonotypeCorsiva")));
#if PLATFORM(IOS_FAMILY)
    if (familyName && caseInsensitiveCompare(familyName.get(), CFSTR("Courier New"))) {
        // Special case Courier New to not be treated as fixed pitch, as this will make use of a hacked space width which is undesireable for iPhone (see rdar://6269783).
        m_treatAsFixedPitch = false;
    }
#endif
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    FloatRect boundingBox;
    boundingBox = CTFontGetBoundingRectsForGlyphs(m_platformData.ctFont(), platformData().orientation() == FontOrientation::Vertical ? kCTFontOrientationVertical : kCTFontOrientationHorizontal, &glyph, 0, 1);
    boundingBox.setY(-boundingBox.maxY());
    if (m_syntheticBoldOffset)
        boundingBox.setWidth(boundingBox.width() + m_syntheticBoldOffset);

    return boundingBox;
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    CGSize advance = CGSizeZero;
    bool horizontal = platformData().orientation() == FontOrientation::Horizontal;
    CGFontRenderingStyle style = kCGFontRenderingStyleAntialiasing | kCGFontRenderingStyleSubpixelPositioning | kCGFontRenderingStyleSubpixelQuantization | kCGFontAntialiasingStyleUnfiltered;

    if (platformData().size()) {
        CTFontOrientation orientation = horizontal || m_isBrokenIdeographFallback ? kCTFontOrientationHorizontal : kCTFontOrientationVertical;
        // FIXME: Remove this special-casing when <rdar://problem/28197291> and <rdar://problem/28662086> are fixed.
        if (CTFontIsAppleColorEmoji(m_platformData.ctFont()) || m_platformData.hasVariations())
            CTFontGetAdvancesForGlyphs(m_platformData.ctFont(), orientation, &glyph, &advance, 1);
        else
            CTFontGetUnsummedAdvancesForGlyphsAndStyle(m_platformData.ctFont(), orientation, style, &glyph, &advance, 1);
    }
    return advance.width + m_syntheticBoldOffset;
}

Path Font::platformPathForGlyph(Glyph glyph) const
{
    auto result = adoptCF(CTFontCreatePathForGlyph(platformData().ctFont(), glyph, nullptr));
    auto syntheticBoldOffset = this->syntheticBoldOffset();
    if (syntheticBoldOffset) {
        auto newPath = adoptCF(CGPathCreateMutable());
        CGPathAddPath(newPath.get(), nullptr, result.get());
        auto translation = CGAffineTransformMakeTranslation(syntheticBoldOffset, 0);
        CGPathAddPath(newPath.get(), &translation, result.get());
        return { WTFMove(newPath) };
    }
    return { adoptCF(CGPathCreateMutableCopy(result.get())) };
}

bool Font::platformSupportsCodePoint(UChar32 character, Optional<UChar32> variation) const
{
    if (variation)
        return false;

    UniChar codeUnits[2];
    CGGlyph glyphs[2];
    CFIndex count = 0;
    U16_APPEND_UNSAFE(codeUnits, count, character);
    return CTFontGetGlyphsForCharacters(platformData().ctFont(), codeUnits, glyphs, count);
}

bool Font::isProbablyOnlyUsedToRenderIcons() const
{
    auto platformFont = platformData().font();
    if (!platformFont)
        return false;

    // Allow most non-icon fonts to bail early here by testing a single character 'a', without iterating over all basic latin characters.
    UniChar lowercaseACharacter = 'a';
    CGGlyph lowercaseAGlyph;
    if (CTFontGetGlyphsForCharacters(platformFont, &lowercaseACharacter, &lowercaseAGlyph, 1)) {
        if (!CGRectIsEmpty(CTFontGetBoundingRectsForGlyphs(platformFont, kCTFontOrientationDefault, &lowercaseAGlyph, nullptr, 1)))
            return false;
    }

    auto supportedCharacters = adoptCF(CTFontCopyCharacterSet(platformFont));
    if (CFCharacterSetHasMemberInPlane(supportedCharacters.get(), 1) || CFCharacterSetHasMemberInPlane(supportedCharacters.get(), 2))
        return false;

    // This encompasses all basic Latin non-control characters.
    constexpr UniChar firstCharacterToTest = ' ';
    constexpr UniChar lastCharacterToTest = '~';
    constexpr auto numberOfCharactersToTest = lastCharacterToTest - firstCharacterToTest + 1;

    Vector<CGGlyph> glyphs;
    glyphs.fill(0, numberOfCharactersToTest);
    CTFontGetGlyphsForCharacterRange(platformFont, glyphs.begin(), CFRangeMake(firstCharacterToTest, numberOfCharactersToTest));
    glyphs.removeAll(0);

    if (glyphs.isEmpty())
        return false;

    Vector<CGRect> boundingRects;
    boundingRects.fill(CGRectZero, glyphs.size());
    CTFontGetBoundingRectsForGlyphs(platformFont, kCTFontOrientationDefault, glyphs.begin(), boundingRects.begin(), glyphs.size());
    return notFound == boundingRects.findMatching([](auto& rect) {
        return !CGRectIsEmpty(rect);
    });
}

} // namespace WebCore
