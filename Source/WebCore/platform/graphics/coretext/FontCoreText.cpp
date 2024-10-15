/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Font.h"

#include "Color.h"
#include "FloatRect.h"
#include "FontCache.h"
#include "FontCascade.h"
#include "FontCustomPlatformData.h"
#include "FontDescription.h"
#include "LocaleCocoa.h"
#include "Logging.h"
#include "OpenTypeCG.h"
#include "PathCG.h"
#include "SharedBuffer.h"
#include <CoreText/CoreText.h>
#include <float.h>
#include <pal/spi/cf/CoreTextSPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <unicode/uchar.h>
#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#include "MultiRepresentationHEICMetrics.h"
#endif

#include <pal/cf/CoreTextSoftLink.h>

namespace WebCore {

static inline bool caseInsensitiveCompare(CFStringRef a, CFStringRef b)
{
    return a && CFStringCompare(a, b, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
}

static bool fontHasVerticalGlyphs(CTFontRef font)
{
    return fontHasEitherTable(font, kCTFontTableVhea, kCTFontTableVORG);
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

bool fontHasEitherTable(CTFontRef ctFont, unsigned tableTag1, unsigned tableTag2)
{
    return CTFontHasTable(ctFont, tableTag1) || CTFontHasTable(ctFont, tableTag2);
}

void Font::platformInit()
{
#if PLATFORM(IOS_FAMILY)
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? ceilf(m_platformData.size() / 24.0f) : 0.f;
#else
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? 1.0f : 0.f;
#endif

    unsigned unitsPerEm = CTFontGetUnitsPerEm(getCTFont());
    float pointSize = m_platformData.size();
    CGFloat capHeight = pointSize ? CTFontGetCapHeight(getCTFont()) : 0;
    CGFloat lineGap = pointSize ? CTFontGetLeading(getCTFont()) : 0;
    CGFloat ascent = pointSize ? CTFontGetAscent(getCTFont()) : 0;
    CGFloat descent = pointSize ? CTFontGetDescent(getCTFont()) : 0;

    // The Open Font Format describes the OS/2 USE_TYPO_METRICS flag as follows:
    // "If set, it is strongly recommended to use OS/2.sTypoAscender - OS/2.sTypoDescender+ OS/2.sTypoLineGap as a value for default line spacing for this font."
    // On OS X, we only apply this rule in the important case of fonts with a MATH table.
    if (CTFontHasTable(getCTFont(), kCTFontTableMATH)) {
        short typoAscent, typoDescent, typoLineGap;
        if (OpenType::tryGetTypoMetrics(getCTFont(), typoAscent, typoDescent, typoLineGap)) {
            ascent = scaleEmToUnits(typoAscent, unitsPerEm) * pointSize;
            descent = -scaleEmToUnits(typoDescent, unitsPerEm) * pointSize;
            lineGap = scaleEmToUnits(typoLineGap, unitsPerEm) * pointSize;
        }
    }

    auto familyName = adoptCF(CTFontCopyFamilyName(getCTFont()));

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
#if !PLATFORM(IOS_FAMILY)
    float lineSpacing = std::lround(ascent) + std::lround(descent) + std::lround(lineGap);
#endif

#if PLATFORM(MAC)
    // Hack Hiragino line metrics to allow room for marked text underlines.
    // <rdar://problem/5386183>
    if (descent < 3 && lineGap >= 3 && familyName && CFStringHasPrefix(familyName.get(), CFSTR("Hiragino"))) {
        lineGap -= 3 - descent;
        descent = 3;
    }
#endif
    
    if (platformData().orientation() == FontOrientation::Vertical && !isTextOrientationFallback())
        m_hasVerticalGlyphs = fontHasVerticalGlyphs(getCTFont());

#if PLATFORM(IOS_FAMILY)
    CGFloat adjustment = shouldUseAdjustment(getCTFont()) ? ceil((ascent + descent) * kLineHeightAdjustment) : 0;

    lineGap = ceilf(lineGap);
    float lineSpacing = std::ceil(ascent) + adjustment + std::ceil(descent) + lineGap;
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
                xHeight = CTFontGetXHeight(getCTFont());
        } else
            xHeight = verticalRightOrientationFont().fontMetrics().xHeight().value_or(0);
    }

    if (CTFontGetSymbolicTraits(getCTFont()) & kCTFontTraitColorGlyphs) {
#if HAVE(CTFONTCOPYCOLORGLYPHCOVERAGE)
        // The reason this is guarded with both a preprocessor define and soft linking is that
        // we want to get rid of the soft linking soon,
        // once people have a chance to update to an SDK that includes it.
        // At that point, only the preprocessor define will remain.
        if (PAL::canLoad_CoreText_CTFontCopyColorGlyphCoverage()) {
            if (auto cfBitVector = adoptCF(PAL::softLink_CoreText_CTFontCopyColorGlyphCoverage(getCTFont())))
                m_emojiType = SomeEmojiGlyphs { BitVector(cfBitVector.get()) };
            else
                m_emojiType = NoEmojiGlyphs { };
        } else
#endif
        {
            m_emojiType = AllEmojiGlyphs { };
        }
    } else
        m_emojiType = NoEmojiGlyphs { };

    m_fontMetrics.setUnitsPerEm(unitsPerEm);
    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setCapHeight(capHeight);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setXHeight(xHeight);
    m_fontMetrics.setLineSpacing(lineSpacing);
    m_fontMetrics.setUnderlinePosition(-CTFontGetUnderlinePosition(getCTFont()));
    m_fontMetrics.setUnderlineThickness(CTFontGetUnderlineThickness(getCTFont()));
}

void Font::platformCharWidthInit()
{
    m_avgCharWidth = 0;
    m_maxCharWidth = 0;

    auto os2Table = adoptCF(CTFontCopyTable(getCTFont(), kCTFontTableOS2, kCTFontTableOptionNoOptions));
    if (os2Table && CFDataGetLength(os2Table.get()) >= 4) {
        const UInt8* os2 = CFDataGetBytePtr(os2Table.get());
        SInt16 os2AvgCharWidth = os2[2] * 256 + os2[3];
        m_avgCharWidth = scaleEmToUnits(os2AvgCharWidth, m_fontMetrics.unitsPerEm()) * m_platformData.size();
    }

    auto headTable = adoptCF(CTFontCopyTable(getCTFont(), kCTFontTableHead, kCTFontTableOptionNoOptions));
    if (headTable && CFDataGetLength(headTable.get()) >= 42) {
        const UInt8* head = CFDataGetBytePtr(headTable.get());
        unsigned uxMin = head[36] * 256 + head[37];
        unsigned uxMax = head[40] * 256 + head[41];
        SInt16 xMin = static_cast<SInt16>(uxMin);
        SInt16 xMax = static_cast<SInt16>(uxMax);
        float diff = static_cast<float>(xMax - xMin);
        m_maxCharWidth = scaleEmToUnits(diff, m_fontMetrics.unitsPerEm()) * m_platformData.size();
    }

    // Fallback to a cross-platform estimate, which will populate these values if they are non-positive.
    initCharWidths();
}

bool Font::variantCapsSupportedForSynthesis(FontVariantCaps fontVariantCaps) const
{
    switch (fontVariantCaps) {
    case FontVariantCaps::Small:
        return supportsSmallCaps();
    case FontVariantCaps::Petite:
        return supportsPetiteCaps();
    case FontVariantCaps::AllSmall:
        return supportsAllSmallCaps();
    case FontVariantCaps::AllPetite:
        return supportsAllPetiteCaps();
    default:
        // Synthesis only supports the variant-caps values listed above.
        return true;
    }
}

static RetainPtr<CFDictionaryRef> smallCapsOpenTypeDictionary(CFStringRef key, int rawValue)
{
    RetainPtr<CFNumberRef> value = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawValue));
    CFTypeRef keys[] = { kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue };
    CFTypeRef values[] = { key, value.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

static RetainPtr<CFDictionaryRef> smallCapsTrueTypeDictionary(int rawKey, int rawValue)
{
    RetainPtr<CFNumberRef> key = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawKey));
    RetainPtr<CFNumberRef> value = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rawValue));
    CFTypeRef keys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef values[] = { key.get(), value.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
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

bool Font::supportsOpenTypeAlternateHalfWidths() const
{
    if (m_supportsOpenTypeAlternateHalfWidths == SupportsFeature::Unknown)
        m_supportsOpenTypeAlternateHalfWidths = supportsOpenTypeFeature(getCTFont(), CFSTR("halt")) ? SupportsFeature::Yes : SupportsFeature::No;
    return m_supportsOpenTypeAlternateHalfWidths == SupportsFeature::Yes;
}

bool Font::supportsSmallCaps() const
{
    if (m_supportsSmallCaps == SupportsFeature::Unknown) {
        BitVector glyphsSupportedBySmallCaps;
        injectOpenTypeCoverage(CFSTR("smcp"), getCTFont(), glyphsSupportedBySmallCaps);
        injectTrueTypeCoverage(kLowerCaseType, kLowerCaseSmallCapsSelector, getCTFont(), glyphsSupportedBySmallCaps);
        m_supportsSmallCaps = glyphsSupportedBySmallCaps.isEmpty() ? SupportsFeature::No : SupportsFeature::Yes;
    }
    return m_supportsSmallCaps == SupportsFeature::Yes;
}

bool Font::supportsAllSmallCaps() const
{
    if (m_supportsAllSmallCaps == SupportsFeature::Unknown) {
        BitVector glyphsSupportedByAllSmallCaps;
        injectOpenTypeCoverage(CFSTR("smcp"), getCTFont(), glyphsSupportedByAllSmallCaps);
        injectOpenTypeCoverage(CFSTR("c2sc"), getCTFont(), glyphsSupportedByAllSmallCaps);
        injectTrueTypeCoverage(kLowerCaseType, kLowerCaseSmallCapsSelector, getCTFont(), glyphsSupportedByAllSmallCaps);
        injectTrueTypeCoverage(kUpperCaseType, kUpperCaseSmallCapsSelector, getCTFont(), glyphsSupportedByAllSmallCaps);
        m_supportsAllSmallCaps = glyphsSupportedByAllSmallCaps.isEmpty() ? SupportsFeature::No : SupportsFeature::Yes;
    }
    return m_supportsAllSmallCaps == SupportsFeature::Yes;
}

bool Font::supportsPetiteCaps() const
{
    if (m_supportsPetiteCaps == SupportsFeature::Unknown) {
        BitVector glyphsSupportedByPetiteCaps;
        injectOpenTypeCoverage(CFSTR("pcap"), getCTFont(), glyphsSupportedByPetiteCaps);
        injectTrueTypeCoverage(kLowerCaseType, kLowerCasePetiteCapsSelector, getCTFont(), glyphsSupportedByPetiteCaps);
        m_supportsPetiteCaps = glyphsSupportedByPetiteCaps.isEmpty() ? SupportsFeature::No : SupportsFeature::Yes;
    }
    return m_supportsPetiteCaps == SupportsFeature::Yes;
}

bool Font::supportsAllPetiteCaps() const
{
    if (m_supportsAllPetiteCaps == SupportsFeature::Unknown) {
        BitVector glyphsSupportedByAllPetiteCaps;
        injectOpenTypeCoverage(CFSTR("pcap"), getCTFont(), glyphsSupportedByAllPetiteCaps);
        injectOpenTypeCoverage(CFSTR("c2pc"), getCTFont(), glyphsSupportedByAllPetiteCaps);
        injectTrueTypeCoverage(kLowerCaseType, kLowerCasePetiteCapsSelector, getCTFont(), glyphsSupportedByAllPetiteCaps);
        injectTrueTypeCoverage(kUpperCaseType, kUpperCasePetiteCapsSelector, getCTFont(), glyphsSupportedByAllPetiteCaps);
        m_supportsAllPetiteCaps = glyphsSupportedByAllPetiteCaps.isEmpty() ? SupportsFeature::No : SupportsFeature::Yes;
    }
    return m_supportsAllPetiteCaps == SupportsFeature::Yes;
}

static RefPtr<Font> createDerivativeFont(CTFontRef font, float size, FontOrientation orientation, CTFontSymbolicTraits fontTraits, bool syntheticBold, bool syntheticItalic, FontWidthVariant fontWidthVariant, TextRenderingMode textRenderingMode, const FontCustomPlatformData* customPlatformData)
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
    FontPlatformData scaledFontData(font, size, usedSyntheticBold, usedSyntheticOblique, orientation, fontWidthVariant, textRenderingMode, customPlatformData);

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

static inline std::optional<CFStringRef> openTypeFeature(CFDictionaryRef feature)
{
    ASSERT(isOpenTypeFeature(feature));
    CFStringRef tag = static_cast<CFStringRef>(CFDictionaryGetValue(feature, kCTFontOpenTypeFeatureTag));
    int rawValue;
    CFNumberRef value = static_cast<CFNumberRef>(CFDictionaryGetValue(feature, kCTFontOpenTypeFeatureValue));
    auto success = CFNumberGetValue(value, kCFNumberIntType, &rawValue);
    ASSERT_UNUSED(success, success);
    return rawValue ? std::optional<CFStringRef>(tag) : std::nullopt;
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
    RetainPtr<CFDictionaryRef> attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr<CTFontDescriptorRef> newDescriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    return adoptCF(CTFontCreateCopyWithAttributes(font, CTFontGetSize(font), nullptr, newDescriptor.get()));
}

RefPtr<Font> Font::createFontWithoutSynthesizableFeatures() const
{
    float size = m_platformData.size();
    CTFontSymbolicTraits fontTraits = CTFontGetSymbolicTraits(getCTFont());
    RetainPtr<CTFontRef> ctFont = createCTFontWithoutSynthesizableFeatures(getCTFont());
    return createDerivativeFont(ctFont.get(), size, m_platformData.orientation(), fontTraits, m_platformData.syntheticBold(), m_platformData.syntheticOblique(), m_platformData.widthVariant(), m_platformData.textRenderingMode(), m_platformData.protectedCustomPlatformData().get());
}

RefPtr<Font> Font::platformCreateScaledFont(const FontDescription&, float scaleFactor) const
{
    float size = m_platformData.size() * scaleFactor;
    CTFontSymbolicTraits fontTraits = CTFontGetSymbolicTraits(getCTFont());
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontCopyFontDescriptor(getCTFont()));
    RetainPtr<CTFontRef> scaledFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));

    return createDerivativeFont(scaledFont.get(), size, m_platformData.orientation(), fontTraits, m_platformData.syntheticBold(), m_platformData.syntheticOblique(), m_platformData.widthVariant(), m_platformData.textRenderingMode(), m_platformData.protectedCustomPlatformData().get());
}

bool supportsOpenTypeFeature(CTFontRef font, CFStringRef featureTag)
{
    RetainPtr<CFArrayRef> features = adoptCF(CTFontCopyFeatures(font));
    CFIndex featureCount = CFArrayGetCount(features.get());
    for (CFIndex featureIndex = 0; featureIndex < featureCount; ++featureIndex) {
        auto feature = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(features.get(), featureIndex));
        auto featureTypeIdentifier = static_cast<CFNumberRef>(CFDictionaryGetValue(feature, kCTFontFeatureTypeIdentifierKey));
        if (!featureTypeIdentifier)
            continue;

        int rawFeatureTypeIdentifier;
        CFNumberGetValue(featureTypeIdentifier, kCFNumberIntType, &rawFeatureTypeIdentifier);
        if (rawFeatureTypeIdentifier != kTextSpacingType)
            continue;

        auto featureSelectors = static_cast<CFArrayRef>(CFDictionaryGetValue(feature, kCTFontFeatureTypeSelectorsKey));
        if (!featureSelectors)
            continue;
        auto selectorsCount = CFArrayGetCount(featureSelectors);
        for (CFIndex selectorIndex = 0; selectorIndex < selectorsCount; ++selectorIndex) {
            auto featureSelector = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(featureSelectors, selectorIndex));
            auto openTypeTag = static_cast<CFStringRef>(CFDictionaryGetValue(featureSelector, kCTFontOpenTypeFeatureTag));
            if (!openTypeTag)
                continue;
            if (CFStringCompare(openTypeTag, featureTag, 0) == kCFCompareEqualTo)
                return true;
        }
    }
    return false;
}

RefPtr<Font> Font::platformCreateHalfWidthFont() const
{
    if (!supportsOpenTypeAlternateHalfWidths())
        return nullptr;

    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontCopyFontDescriptor(getCTFont()));
    auto size = m_platformData.size();
    auto fontTraits = CTFontGetSymbolicTraits(getCTFont());
    int enableHaltValue = 1;
    auto featureName = CFSTR("halt");
    auto featureValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &enableHaltValue));

    const void* featureValues[] = { featureName, featureValue.get() };
    auto fontFeatureSettings = adoptCF(CFArrayCreate(kCFAllocatorDefault, featureValues, std::size(featureValues), &kCFTypeArrayCallBacks));

    CFTypeRef fontDescriptorKeys[] = { kCTFontFeatureSettingsAttribute };
    CFTypeRef fontDescriptorValues[] = { fontFeatureSettings.get() };
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, fontDescriptorKeys, fontDescriptorValues, std::size(fontDescriptorValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    auto attributesDescriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    auto halfWidthFont = adoptCF(CTFontCreateCopyWithAttributes(getCTFont(), size, nullptr, attributesDescriptor.get()));

    return createDerivativeFont(halfWidthFont.get(), size, m_platformData.orientation(), fontTraits, m_platformData.syntheticBold(), m_platformData.syntheticOblique(), m_platformData.widthVariant(), m_platformData.textRenderingMode(), m_platformData.protectedCustomPlatformData().get());
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    CGSize advance = CGSizeZero;

    if (platformData().size()) {
        bool horizontal = platformData().orientation() == FontOrientation::Horizontal;
        CTFontOrientation orientation = horizontal || m_isBrokenIdeographFallback ? kCTFontOrientationHorizontal : kCTFontOrientationVertical;
        CTFontGetAdvancesForGlyphs(getCTFont(), orientation, &glyph, &advance, 1);
    }
    return advance.width;
}

GlyphBufferAdvance Font::applyTransforms(GlyphBuffer& glyphBuffer, unsigned beginningGlyphIndex, unsigned beginningStringIndex, bool enableKerning, bool requiresShaping, const AtomString& locale, StringView text, TextDirection textDirection) const
{
    UNUSED_PARAM(requiresShaping);

    auto handler = ^(CFRange range, CGGlyph** newGlyphsPointer, CGSize** newAdvancesPointer, CGPoint** newOffsetsPointer, CFIndex** newIndicesPointer)
    {
        range.location = std::min(std::max(range.location, static_cast<CFIndex>(0)), static_cast<CFIndex>(glyphBuffer.size()));
        if (range.length < 0) {
            range.length = std::min(range.location, -range.length);
            range.location = range.location - range.length;
            glyphBuffer.remove(beginningGlyphIndex + range.location, range.length);
            LOG_WITH_STREAM(TextShaping, stream << "Callback called to remove at location " << range.location << " and length " << range.length);
        } else {
            glyphBuffer.makeHole(beginningGlyphIndex + range.location, range.length, this);
            LOG_WITH_STREAM(TextShaping, stream << "Callback called to insert hole at location " << range.location << " and length " << range.length);
        }

        *newGlyphsPointer = glyphBuffer.glyphs(beginningGlyphIndex);
        *newAdvancesPointer = glyphBuffer.advances(beginningGlyphIndex);
        *newOffsetsPointer = glyphBuffer.origins(beginningGlyphIndex);
        *newIndicesPointer = glyphBuffer.offsetsInString(beginningGlyphIndex);
    };

    auto substring = text.substring(beginningStringIndex);
    constexpr unsigned bufferSize = 256;
    auto upconvertedCharacters = substring.upconvertedCharacters<bufferSize>();
    auto localeString = locale.isNull() ? nullptr : LocaleCocoa::canonicalLanguageIdentifierFromString(locale);
    auto numberOfInputGlyphs = glyphBuffer.size() - beginningGlyphIndex;
    // FIXME: Enable kerning for single glyphs when rdar://82195405 is fixed
    CTFontShapeOptions options = kCTFontShapeWithClusterComposition
        | (enableKerning && numberOfInputGlyphs ? kCTFontShapeWithKerning : 0)
        | (textDirection == TextDirection::RTL ? kCTFontShapeRightToLeft : 0);

    // FIXME: This shouldn't actually be necessary, if we pass in a pointer to the base of the string.
    for (unsigned i = 0; i < glyphBuffer.size() - beginningGlyphIndex; ++i)
        glyphBuffer.offsetsInString(beginningGlyphIndex)[i] -= beginningStringIndex;

    LOG_WITH_STREAM(TextShaping,
        stream << "Simple shaping " << numberOfInputGlyphs << " glyphs in font " << String(adoptCF(CTFontCopyPostScriptName(getCTFont())).get()) << ".\n";
        stream << "Font attributes: " << String(adoptCF(CFCopyDescription(adoptCF(CTFontDescriptorCopyAttributes(adoptCF(CTFontCopyFontDescriptor(getCTFont())).get())).get())).get()) << "\n";
        stream << "Locale: " << String(localeString.get()) << "\n";
        stream << "Options: " << options << "\n";
        const auto* glyphs = glyphBuffer.glyphs(beginningGlyphIndex);
        stream << "Glyphs:";
        for (unsigned i = 0; i < numberOfInputGlyphs; ++i)
            stream << " " << glyphs[i];
        stream << "\n";
        const auto* advances = glyphBuffer.advances(beginningGlyphIndex);
        stream << "Advances:";
        for (unsigned i = 0; i < numberOfInputGlyphs; ++i)
            stream << " " << FloatSize(advances[i]);
        stream << "\n";
        const auto* origins = glyphBuffer.origins(beginningGlyphIndex);
        stream << "Origins:";
        for (unsigned i = 0; i < numberOfInputGlyphs; ++i)
            stream << " " << origins[i];
        stream << "\n";
        const auto* offsets = glyphBuffer.offsetsInString(beginningGlyphIndex);
        stream << "Offsets:";
        for (unsigned i = 0; i < numberOfInputGlyphs; ++i)
            stream << " " << offsets[i];
        stream << "\n";
        const UChar* codeUnits = upconvertedCharacters.get();
        stream << "Code Units:";
        for (unsigned i = 0; i < numberOfInputGlyphs; ++i)
            stream << " U+" << hex(codeUnits[i], 4);
    );

    auto initialAdvance = CTFontShapeGlyphs(
        getCTFont(),
        glyphBuffer.glyphs(beginningGlyphIndex),
        glyphBuffer.advances(beginningGlyphIndex),
        glyphBuffer.origins(beginningGlyphIndex),
        glyphBuffer.offsetsInString(beginningGlyphIndex),
        reinterpret_cast<const UniChar*>(upconvertedCharacters.get()),
        numberOfInputGlyphs,
        options,
        localeString.get(),
        handler);

    LOG_WITH_STREAM(TextShaping,
        stream << "Shaping result: " << glyphBuffer.size() - beginningGlyphIndex << " glyphs.\n";
        const auto* glyphs = glyphBuffer.glyphs(beginningGlyphIndex);
        stream << "Glyphs:";
        for (unsigned i = 0; i < glyphBuffer.size() - beginningGlyphIndex; ++i)
            stream << " " << glyphs[i];
        stream << "\n";
        const auto* advances = glyphBuffer.advances(beginningGlyphIndex);
        stream << "Advances:";
        for (unsigned i = 0; i < glyphBuffer.size() - beginningGlyphIndex; ++i)
            stream << " " << FloatSize(advances[i]);
        stream << "\n";
        const auto* origins = glyphBuffer.origins(beginningGlyphIndex);
        stream << "Origins:";
        for (unsigned i = 0; i < glyphBuffer.size() - beginningGlyphIndex; ++i)
            stream << " " << origins[i];
        stream << "\n";
        const auto* offsets = glyphBuffer.offsetsInString(beginningGlyphIndex);
        stream << "Offsets:";
        for (unsigned i = 0; i < glyphBuffer.size() - beginningGlyphIndex; ++i)
            stream << " " << offsets[i];
        stream << "\n";
        stream << "Initial advance: " << FloatSize(initialAdvance);
    );

    ASSERT(numberOfInputGlyphs || glyphBuffer.size() == beginningGlyphIndex);
    ASSERT(numberOfInputGlyphs || (!initialAdvance.width && !initialAdvance.height));

    for (unsigned i = 0; i < glyphBuffer.size() - beginningGlyphIndex; ++i)
        glyphBuffer.offsetsInString(beginningGlyphIndex)[i] += beginningStringIndex;

    if (textDirection == TextDirection::RTL)
        glyphBuffer.reverse(beginningGlyphIndex, glyphBuffer.size() - beginningGlyphIndex);

    return initialAdvance;
}

static int extractNumber(CFNumberRef number)
{
    int result = 0;
    if (number)
        CFNumberGetValue(number, kCFNumberIntType, &result);
    return result;
}

static bool extractBoolean(CFBooleanRef value)
{
    if (value)
        return CFBooleanGetValue(value);
    return false;
}

void Font::determinePitch()
{
    CTFontRef ctFont = getCTFont();
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

    int fixedPitch = extractNumber(adoptCF(static_cast<CFNumberRef>(CTFontCopyAttribute(ctFont, kCTFontFixedAdvanceAttribute))).get());
    bool userInstalled = extractBoolean(adoptCF(static_cast<CFBooleanRef>(CTFontCopyAttribute(ctFont, kCTFontUserInstalledAttribute))).get());
    m_treatAsFixedPitch = (CTFontGetSymbolicTraits(ctFont) & kCTFontMonoSpaceTrait) || fixedPitch || (caseInsensitiveCompare(fullName.get(), CFSTR("Osaka-Mono")) || caseInsensitiveCompare(fullName.get(), CFSTR("MS-PGothic")) || caseInsensitiveCompare(fullName.get(), CFSTR("MonotypeCorsiva")));
    if (familyName && caseInsensitiveCompare(familyName.get(), CFSTR("Courier New"))) {
#if PLATFORM(IOS_FAMILY)
        // Special case Courier New to not be treated as fixed pitch, as this will make use of a hacked space width which is undesireable for iPhone (see rdar://6269783).
        m_treatAsFixedPitch = false;
#endif
        // "Courier New" has many special case characters which does not have widths (u0181, u0182 etc.). Thus we disable fast content measuring for monospace fonts.
        m_canTakeFixedPitchFastContentMeasuring = false;
    } else
        m_canTakeFixedPitchFastContentMeasuring = m_treatAsFixedPitch && !userInstalled;
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    FloatRect boundingBox;
    CGRect ignoredRect = { };
    boundingBox = CTFontGetBoundingRectsForGlyphs(getCTFont(), platformData().orientation() == FontOrientation::Vertical ? kCTFontOrientationVertical : kCTFontOrientationHorizontal, &glyph, &ignoredRect, 1);
    boundingBox.setY(-boundingBox.maxY());
    boundingBox.setWidth(boundingBox.width() + m_syntheticBoldOffset);

    return boundingBox;
}

Path Font::platformPathForGlyph(Glyph glyph) const
{
    auto result = adoptCF(CTFontCreatePathForGlyph(getCTFont(), glyph, nullptr));
    if (!result)
        return { };

    auto syntheticBoldOffset = this->syntheticBoldOffset();
    if (syntheticBoldOffset) {
        auto newPath = adoptCF(CGPathCreateMutable());
        CGPathAddPath(newPath.get(), nullptr, result.get());
        auto translation = CGAffineTransformMakeTranslation(syntheticBoldOffset, 0);
        CGPathAddPath(newPath.get(), &translation, result.get());
        return { PathCG::create(WTFMove(newPath)) };
    }

    return { PathCG::create(adoptCF(CGPathCreateMutableCopy(result.get()))) };
}

bool Font::platformSupportsCodePoint(char32_t character, std::optional<char32_t> variation) const
{
    if (variation)
        return false;

    UniChar codeUnits[2];
    CGGlyph glyphs[2];
    CFIndex count = 0;
    U16_APPEND_UNSAFE(codeUnits, count, character);
    return CTFontGetGlyphsForCharacters(getCTFont(), codeUnits, glyphs, count);
}

static bool hasGlyphsForCharacterRange(CTFontRef font, UniChar firstCharacter, UniChar lastCharacter, bool expectValidGlyphsForAllCharacters)
{
    const unsigned numberOfCharacters = lastCharacter - firstCharacter + 1;
    Vector<CGGlyph> glyphs(numberOfCharacters, 0);
    CTFontGetGlyphsForCharacterRange(font, glyphs.begin(), CFRangeMake(firstCharacter, numberOfCharacters));
    glyphs.removeAll(0);

    if (glyphs.isEmpty())
        return false;

    Vector<CGRect> boundingRects(glyphs.size(), CGRectZero);
    CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationDefault, glyphs.begin(), boundingRects.begin(), glyphs.size());

    unsigned validGlyphsCount = 0;
    for (auto& rect : boundingRects) {
        if (!CGRectIsEmpty(rect))
            ++validGlyphsCount;
    }

    if (expectValidGlyphsForAllCharacters)
        return validGlyphsCount == glyphs.size();

    return validGlyphsCount;
}

bool Font::isProbablyOnlyUsedToRenderIcons() const
{
    auto platformFont = getCTFont();
    if (!platformFont)
        return false;

    // Allow most non-icon fonts to bail early here by testing a single character 'a', without iterating over all basic latin characters.
    UniChar lowercaseACharacter = 'a';
    CGGlyph lowercaseAGlyph;
    if (CTFontGetGlyphsForCharacters(platformFont, &lowercaseACharacter, &lowercaseAGlyph, 1)) {
        CGRect ignoredRect = { };
        if (!CGRectIsEmpty(CTFontGetBoundingRectsForGlyphs(platformFont, kCTFontOrientationDefault, &lowercaseAGlyph, &ignoredRect, 1)))
            return false;
    }

    auto supportedCharacters = adoptCF(CTFontCopyCharacterSet(platformFont));
    if (CFCharacterSetHasMemberInPlane(supportedCharacters.get(), 1) || CFCharacterSetHasMemberInPlane(supportedCharacters.get(), 2))
        return false;

    return !hasGlyphsForCharacterRange(platformFont, ' ', '~', false) && !hasGlyphsForCharacterRange(platformFont, 0x0600, 0x06FF, true);
}

const PAL::OTSVGTable& Font::otSVGTable() const
{
    if (!m_otSVGTable) {
        if (auto tableData = adoptCF(CTFontCopyTable(getCTFont(), kCTFontTableSVG, kCTFontTableOptionNoOptions)))
            m_otSVGTable = PAL::OTSVGTable(tableData.get(), fontMetrics().unitsPerEm(), platformData().size());
        else
            m_otSVGTable = {{ }};
    }
    return m_otSVGTable.value();
}

Font::ComplexColorFormatGlyphs Font::ComplexColorFormatGlyphs::createWithNoRelevantTables()
{
    return { false, 0 };
}

Font::ComplexColorFormatGlyphs Font::ComplexColorFormatGlyphs::createWithRelevantTablesAndGlyphCount(unsigned glyphCount)
{
    return { true, glyphCount };
}

bool Font::ComplexColorFormatGlyphs::hasValueFor(Glyph glyph) const
{
    return m_bits.contains(bitForInitialized(glyph));
}

bool Font::ComplexColorFormatGlyphs::get(Glyph glyph) const
{
    ASSERT(hasValueFor(glyph));
    return m_bits.contains(bitForValue(glyph));
}

void Font::ComplexColorFormatGlyphs::set(Glyph glyph, bool value)
{
    ASSERT(m_hasRelevantTables);
    ASSERT(!hasValueFor(glyph));
    m_bits.set(bitForInitialized(glyph));
    if (value)
        m_bits.set(bitForValue(glyph));
}

bool Font::hasComplexColorFormatTables() const
{
    if (otSVGTable().table)
        return true;

#if HAVE(CORE_TEXT_SBIX_IMAGE_SIZE_FUNCTIONS)
    if (auto sbixTableData = adoptCF(CTFontCopyTable(getCTFont(), kCTFontTableSbix, kCTFontTableOptionNoOptions)))
        return true;
#endif

    return false;
}

Font::ComplexColorFormatGlyphs& Font::glyphsWithComplexColorFormat() const
{
    if (!m_glyphsWithComplexColorFormat) {
        if (hasComplexColorFormatTables()) {
            CFIndex glyphCount = CTFontGetGlyphCount(getCTFont());
            if (glyphCount >= 0) {
                m_glyphsWithComplexColorFormat = ComplexColorFormatGlyphs::createWithRelevantTablesAndGlyphCount(glyphCount);
                return m_glyphsWithComplexColorFormat.value();
            }
        }
    }
    m_glyphsWithComplexColorFormat = ComplexColorFormatGlyphs::createWithNoRelevantTables();
    return m_glyphsWithComplexColorFormat.value();
}

bool Font::glyphHasComplexColorFormat(Glyph glyphID) const
{
    if (auto svgTable = otSVGTable().table) {
        if (PAL::softLinkOTSVGOTSVGTableGetDocumentIndexForGlyph(svgTable, glyphID) != kCFNotFound)
            return true;
    }

#if HAVE(CORE_TEXT_SBIX_IMAGE_SIZE_FUNCTIONS)
    // There's no function to directly look up the sbix table, so use the fact that this one returns a non-zero value iff there's an sbix entry.
    if (CTFontGetSbixImageSizeForGlyphAndContentsScale(getCTFont(), glyphID, 0))
        return true;
#endif

    return false;
}

std::optional<BitVector> Font::findOTSVGGlyphs(const GlyphBufferGlyph* glyphs, unsigned count) const
{
    auto table = otSVGTable().table;
    if (!table)
        return { };

    std::optional<BitVector> result;
    for (unsigned i = 0; i < count; ++i) {
        if (PAL::softLinkOTSVGOTSVGTableGetDocumentIndexForGlyph(table, glyphs[i]) != kCFNotFound) {
            if (!result)
                result = BitVector(count);
            result.value().quickSet(i);
        }
    }
    return result;
}

bool Font::hasAnyComplexColorFormatGlyphs(const GlyphBufferGlyph* glyphs, unsigned count) const
{
    auto& complexGlyphs = glyphsWithComplexColorFormat();
    if (!complexGlyphs.hasRelevantTables())
        return false;

    for (unsigned i = 0; i < count; ++i) {
        if (!complexGlyphs.hasValueFor(glyphs[i]))
            complexGlyphs.set(glyphs[i], glyphHasComplexColorFormat(glyphs[i]));

        if (complexGlyphs.get(glyphs[i]))
            return true;
    }
    return false;
}

#if ENABLE(MULTI_REPRESENTATION_HEIC)

MultiRepresentationHEICMetrics Font::metricsForMultiRepresentationHEIC() const
{
    CGRect bounds = CTFontGetTypographicBoundsForAdaptiveImageProvider(getCTFont(), nullptr);

    MultiRepresentationHEICMetrics metrics;
    metrics.ascent = CGRectGetMaxY(bounds);
    metrics.descent = -CGRectGetMinY(bounds);
    metrics.width = CGRectGetMaxX(bounds);
    return metrics;
}

#endif

} // namespace WebCore
