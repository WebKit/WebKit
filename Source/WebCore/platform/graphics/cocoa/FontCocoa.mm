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

#import "BlockExceptions.h"
#import "Color.h"
#import "CoreGraphicsSPI.h"
#import "CoreTextSPI.h"
#import "FloatRect.h"
#import "FontCache.h"
#import "FontCascade.h"
#import "FontDescription.h"
#import "SharedBuffer.h"
#import "WebCoreSystemInterface.h"
#import <float.h>
#import <unicode/uchar.h>
#import <wtf/Assertions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

#if USE(APPKIT)
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#else
#import "FontServicesIOS.h"
#import <CoreText/CoreText.h>
#endif

#if USE(APPKIT)
@interface NSFont (WebAppKitSecretAPI)
- (BOOL)_isFakeFixedPitch;
@end
#endif

#if USE(APPKIT)
#import "OpenTypeCG.h"
#endif

namespace WebCore {

static bool fontHasVerticalGlyphs(CTFontRef ctFont)
{
    // The check doesn't look neat but this is what AppKit does for vertical writing...
    RetainPtr<CFArrayRef> tableTags = adoptCF(CTFontCopyAvailableTables(ctFont, kCTFontTableOptionNoOptions));
    CFIndex numTables = CFArrayGetCount(tableTags.get());
    for (CFIndex index = 0; index < numTables; ++index) {
        CTFontTableTag tag = (CTFontTableTag)(uintptr_t)CFArrayGetValueAtIndex(tableTags.get(), index);
        if (tag == kCTFontTableVhea || tag == kCTFontTableVORG)
            return true;
    }
    return false;
}

#if USE(APPKIT)
static NSString *webFallbackFontFamily(void)
{
    static NSString *webFallbackFontFamily = [[[NSFont systemFontOfSize:16.0f] familyName] retain];
    return webFallbackFontFamily;
}
#else
bool fontFamilyShouldNotBeUsedForArabic(CFStringRef fontFamilyName)
{
    if (!fontFamilyName)
        return false;

    // Times New Roman and Arial are not performant enough to use. <rdar://problem/21333326>
    // FIXME <rdar://problem/12096835> remove this function once the above bug is fixed.
    return (CFStringCompare(CFSTR("Times New Roman"), fontFamilyName, 0) == kCFCompareEqualTo)
        || (CFStringCompare(CFSTR("Arial"), fontFamilyName, 0) == kCFCompareEqualTo);
}
#endif

void Font::platformInit()
{
    // FIXME: Unify these two codepaths
#if USE(APPKIT)
    m_syntheticBoldOffset = m_platformData.m_syntheticBold ? 1.0f : 0.f;

    bool failedSetup = false;
    if (!platformData().cgFont()) {
        // Ack! Something very bad happened, like a corrupt font.
        // Try looking for an alternate 'base' font for this renderer.

        // Special case hack to use "Times New Roman" in place of "Times".
        // "Times RO" is a common font whose family name is "Times".
        // It overrides the normal "Times" family font.
        // It also appears to have a corrupt regular variant.
        NSString *fallbackFontFamily;
        if ([[m_platformData.nsFont() familyName] isEqual:@"Times"])
            fallbackFontFamily = @"Times New Roman";
        else
            fallbackFontFamily = webFallbackFontFamily();
        
        // Try setting up the alternate font.
        // This is a last ditch effort to use a substitute font when something has gone wrong.
#if !ERROR_DISABLED
        RetainPtr<NSFont> initialFont = m_platformData.nsFont();
#endif
        if (m_platformData.font())
            m_platformData.setNSFont([[NSFontManager sharedFontManager] convertFont:m_platformData.nsFont() toFamily:fallbackFontFamily]);
        else
            m_platformData.setNSFont([NSFont fontWithName:fallbackFontFamily size:m_platformData.size()]);
        if (!platformData().cgFont()) {
            if ([fallbackFontFamily isEqual:@"Times New Roman"]) {
                // OK, couldn't setup Times New Roman as an alternate to Times, fallback
                // on the system font.  If this fails we have no alternative left.
                m_platformData.setNSFont([[NSFontManager sharedFontManager] convertFont:m_platformData.nsFont() toFamily:webFallbackFontFamily()]);
                if (!platformData().cgFont()) {
                    // We tried, Times, Times New Roman, and the system font. No joy. We have to give up.
                    LOG_ERROR("unable to initialize with font %@", initialFont.get());
                    failedSetup = true;
                }
            } else {
                // We tried the requested font and the system font. No joy. We have to give up.
                LOG_ERROR("unable to initialize with font %@", initialFont.get());
                failedSetup = true;
            }
        }

        // Report the problem.
        LOG_ERROR("Corrupt font detected, using %@ in place of %@.",
            [m_platformData.nsFont() familyName], [initialFont.get() familyName]);
    }

    // If all else fails, try to set up using the system font.
    // This is probably because Times and Times New Roman are both unavailable.
    if (failedSetup) {
        m_platformData.setNSFont([NSFont systemFontOfSize:[m_platformData.nsFont() pointSize]]);
        LOG_ERROR("failed to set up font, using system font %s", m_platformData.font());
    }

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101100
    // Work around <rdar://problem/19433490>
    CGGlyph dummyGlyphs[] = {0, 0};
    CGSize dummySize[] = { CGSizeMake(0, 0), CGSizeMake(0, 0) };
    CTFontTransformGlyphs(m_platformData.ctFont(), dummyGlyphs, dummySize, 2, kCTFontTransformApplyPositioning | kCTFontTransformApplyShaping);
#endif

    unsigned unitsPerEm = CGFontGetUnitsPerEm(m_platformData.cgFont());

    // Some fonts erroneously specify a positive descender value. We follow Core Text in assuming that
    // such fonts meant the same distance, but in the reverse direction.
    float pointSize = m_platformData.m_size;
    float ascent = scaleEmToUnits(CGFontGetAscent(m_platformData.cgFont()), unitsPerEm) * pointSize;
    float descent = -scaleEmToUnits(-abs(CGFontGetDescent(m_platformData.cgFont())), unitsPerEm) * pointSize;
    float capHeight = scaleEmToUnits(CGFontGetCapHeight(m_platformData.cgFont()), unitsPerEm) * pointSize;
    
    float lineGap = scaleEmToUnits(CGFontGetLeading(m_platformData.cgFont()), unitsPerEm) * pointSize;

    // The Open Font Format describes the OS/2 USE_TYPO_METRICS flag as follows:
    // "If set, it is strongly recommended to use OS/2.sTypoAscender - OS/2.sTypoDescender+ OS/2.sTypoLineGap as a value for default line spacing for this font."
    // On OS X, we only apply this rule in the important case of fonts with a MATH table.
    if (OpenType::fontHasMathTable(m_platformData.ctFont())) {
        short typoAscent, typoDescent, typoLineGap;
        if (OpenType::tryGetTypoMetrics(m_platformData.cgFont(), typoAscent, typoDescent, typoLineGap)) {
            ascent = scaleEmToUnits(typoAscent, unitsPerEm) * pointSize;
            descent = -scaleEmToUnits(typoDescent, unitsPerEm) * pointSize;
            lineGap = scaleEmToUnits(typoLineGap, unitsPerEm) * pointSize;
        }
    }

    // We need to adjust Times, Helvetica, and Courier to closely match the
    // vertical metrics of their Microsoft counterparts that are the de facto
    // web standard. The AppKit adjustment of 20% is too big and is
    // incorrectly added to line spacing, so we use a 15% adjustment instead
    // and add it to the ascent.
    RetainPtr<CFStringRef> familyName = adoptCF(CTFontCopyFamilyName(m_platformData.font()));
    if (familyName && (CFStringCompare(familyName.get(), CFSTR("Times"), kCFCompareCaseInsensitive) == kCFCompareEqualTo
        || CFStringCompare(familyName.get(), CFSTR("Helvetica"), kCFCompareCaseInsensitive) == kCFCompareEqualTo
        || CFStringCompare(familyName.get(), CFSTR("Courier"), kCFCompareCaseInsensitive) == kCFCompareEqualTo))
        ascent += floorf(((ascent + descent) * 0.15f) + 0.5f);

    // Compute and store line spacing, before the line metrics hacks are applied.
    m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) + lroundf(lineGap));

    // Hack Hiragino line metrics to allow room for marked text underlines.
    // <rdar://problem/5386183>
    if (descent < 3 && lineGap >= 3 && familyName && CFStringHasPrefix(familyName.get(), CFSTR("Hiragino"))) {
        lineGap -= 3 - descent;
        descent = 3;
    }
    
    if (platformData().orientation() == Vertical && !isTextOrientationFallback())
        m_hasVerticalGlyphs = fontHasVerticalGlyphs(m_platformData.ctFont());

    float xHeight;

    if (platformData().orientation() == Horizontal) {
        // Measure the actual character "x", since it's possible for it to extend below the baseline, and we need the
        // reported x-height to only include the portion of the glyph that is above the baseline.
        NSGlyph xGlyph = glyphForCharacter('x');
        if (xGlyph)
            xHeight = -CGRectGetMinY(platformBoundsForGlyph(xGlyph));
        else
            xHeight = scaleEmToUnits(CGFontGetXHeight(m_platformData.cgFont()), unitsPerEm) * pointSize;
    } else
        xHeight = verticalRightOrientationFont().fontMetrics().xHeight();

    m_fontMetrics.setUnitsPerEm(unitsPerEm);
    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setCapHeight(capHeight);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setXHeight(xHeight);

#else

    m_syntheticBoldOffset = m_platformData.m_syntheticBold ? ceilf(m_platformData.size()  / 24.0f) : 0.f;

    CTFontRef ctFont = m_platformData.font();
    FontServicesIOS fontService(ctFont);
    m_fontMetrics.setUnitsPerEm(fontService.unitsPerEm());
    m_fontMetrics.setAscent(ceilf(fontService.ascent()));
    m_fontMetrics.setDescent(ceilf(fontService.descent()));
    m_fontMetrics.setLineGap(fontService.lineGap());
    m_fontMetrics.setLineSpacing(fontService.lineSpacing());
    m_fontMetrics.setXHeight(fontService.xHeight());
    m_fontMetrics.setCapHeight(fontService.capHeight());
    m_shouldNotBeUsedForArabic = fontFamilyShouldNotBeUsedForArabic(adoptCF(CTFontCopyFamilyName(ctFont)).get());

    if (platformData().orientation() == Vertical && !isTextOrientationFallback())
        m_hasVerticalGlyphs = fontHasVerticalGlyphs(m_platformData.ctFont());
#endif
}

void Font::platformCharWidthInit()
{
    m_avgCharWidth = 0;
    m_maxCharWidth = 0;
    
#if PLATFORM(MAC)
    RetainPtr<CFDataRef> os2Table = adoptCF(CGFontCopyTableForTag(m_platformData.cgFont(), 'OS/2'));
    if (os2Table && CFDataGetLength(os2Table.get()) >= 4) {
        const UInt8* os2 = CFDataGetBytePtr(os2Table.get());
        SInt16 os2AvgCharWidth = os2[2] * 256 + os2[3];
        m_avgCharWidth = scaleEmToUnits(os2AvgCharWidth, m_fontMetrics.unitsPerEm()) * m_platformData.m_size;
    }

    RetainPtr<CFDataRef> headTable = adoptCF(CGFontCopyTableForTag(m_platformData.cgFont(), 'head'));
    if (headTable && CFDataGetLength(headTable.get()) >= 42) {
        const UInt8* head = CFDataGetBytePtr(headTable.get());
        ushort uxMin = head[36] * 256 + head[37];
        ushort uxMax = head[40] * 256 + head[41];
        SInt16 xMin = static_cast<SInt16>(uxMin);
        SInt16 xMax = static_cast<SInt16>(uxMax);
        float diff = static_cast<float>(xMax - xMin);
        m_maxCharWidth = scaleEmToUnits(diff, m_fontMetrics.unitsPerEm()) * m_platformData.m_size;
    }
#endif

    // Fallback to a cross-platform estimate, which will populate these values if they are non-positive.
    initCharWidths();
}

void Font::platformDestroy()
{
}

bool Font::variantCapsSupportsCharacterForSynthesis(FontVariantCaps fontVariantCaps, UChar32 character) const
{
#if (PLATFORM(IOS) && TARGET_OS_IOS && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200)
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

#if (PLATFORM(IOS) && TARGET_OS_IOS && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200)
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

    FontPlatformData scaledFontData(font, size, false, false, orientation);

    if (syntheticBold)
        fontTraits |= kCTFontBoldTrait;
    if (syntheticItalic)
        fontTraits |= kCTFontItalicTrait;

    CTFontSymbolicTraits scaledFontTraits = CTFontGetSymbolicTraits(scaledFontData.font());
    scaledFontData.m_syntheticBold = (fontTraits & kCTFontBoldTrait) && !(scaledFontTraits & kCTFontTraitBold);
    scaledFontData.m_syntheticOblique = (fontTraits & kCTFontItalicTrait) && !(scaledFontTraits & kCTFontTraitItalic);

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
    return rawValue ? Optional<CFStringRef>(tag) : Nullopt;
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
    return createDerivativeFont(ctFont.get(), size, m_platformData.orientation(), fontTraits, m_platformData.m_syntheticBold, m_platformData.m_syntheticOblique);
}

RefPtr<Font> Font::platformCreateScaledFont(const FontDescription&, float scaleFactor) const
{
    float size = m_platformData.size() * scaleFactor;
    CTFontSymbolicTraits fontTraits = CTFontGetSymbolicTraits(m_platformData.font());
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontCopyFontDescriptor(m_platformData.font()));
    RetainPtr<CTFontRef> scaledFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));

    return createDerivativeFont(scaledFont.get(), size, m_platformData.orientation(), fontTraits, m_platformData.m_syntheticBold, m_platformData.m_syntheticOblique);
}

void Font::determinePitch()
{
#if USE(APPKIT)
    NSFont* f = m_platformData.nsFont();
    // Special case Osaka-Mono.
    // According to <rdar://problem/3999467>, we should treat Osaka-Mono as fixed pitch.
    // Note that the AppKit does not report Osaka-Mono as fixed pitch.

    // Special case MS-PGothic.
    // According to <rdar://problem/4032938>, we should not treat MS-PGothic as fixed pitch.
    // Note that AppKit does report MS-PGothic as fixed pitch.

    // Special case MonotypeCorsiva
    // According to <rdar://problem/5454704>, we should not treat MonotypeCorsiva as fixed pitch.
    // Note that AppKit does report MonotypeCorsiva as fixed pitch.

    NSString *name = [f fontName];
    m_treatAsFixedPitch = ([f isFixedPitch]  || [f _isFakeFixedPitch] || [name caseInsensitiveCompare:@"Osaka-Mono"] == NSOrderedSame) && [name caseInsensitiveCompare:@"MS-PGothic"] != NSOrderedSame && [name caseInsensitiveCompare:@"MonotypeCorsiva"] != NSOrderedSame;
#else
    CTFontRef ctFont = m_platformData.font();
    m_treatAsFixedPitch = false;
    if (!ctFont)
        return; // CTFont is null in the case of SVG fonts for example.

    RetainPtr<CFStringRef> fullName = adoptCF(CTFontCopyFullName(ctFont));
    RetainPtr<CFStringRef> familyName = adoptCF(CTFontCopyFamilyName(ctFont));

    m_treatAsFixedPitch = CGFontIsFixedPitch(m_platformData.cgFont()) || (fullName && (CFStringCompare(fullName.get(), CFSTR("Osaka-Mono"), kCFCompareCaseInsensitive) == kCFCompareEqualTo || CFStringCompare(fullName.get(), CFSTR("MS-PGothic"), kCFCompareCaseInsensitive) == kCFCompareEqualTo));
    if (familyName && CFStringCompare(familyName.get(), CFSTR("Courier New"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        // Special case Courier New to not be treated as fixed pitch, as this will make use of a hacked space width which is undesireable for iPhone (see rdar://6269783).
        m_treatAsFixedPitch = false;
    }
#endif
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    FloatRect boundingBox;
    boundingBox = CTFontGetBoundingRectsForGlyphs(m_platformData.ctFont(), platformData().orientation() == Vertical ? kCTFontOrientationVertical : kCTFontOrientationHorizontal, &glyph, 0, 1);
    boundingBox.setY(-boundingBox.maxY());
    if (m_syntheticBoldOffset)
        boundingBox.setWidth(boundingBox.width() + m_syntheticBoldOffset);

    return boundingBox;
}

static inline CGFontRenderingStyle renderingStyle(const FontPlatformData& platformData)
{
#if USE(APPKIT)
    CGFontRenderingStyle style = kCGFontRenderingStyleAntialiasing | kCGFontRenderingStyleSubpixelPositioning | kCGFontRenderingStyleSubpixelQuantization;
    NSFont *font = platformData.nsFont();
    if (font) {
        switch ([font renderingMode]) {
        case NSFontIntegerAdvancementsRenderingMode:
            style = 0;
            break;
        case NSFontAntialiasedIntegerAdvancementsRenderingMode:
            style = kCGFontRenderingStyleAntialiasing;
            break;
        default:
            break;
        }
    }
    return style;

#else
    UNUSED_PARAM(platformData);
    return kCGFontRenderingStyleAntialiasing | kCGFontRenderingStyleSubpixelPositioning | kCGFontRenderingStyleSubpixelQuantization | kCGFontAntialiasingStyleUnfiltered;
#endif
}

static inline Optional<CGSize> advanceForColorBitmapFont(const FontPlatformData& platformData, Glyph glyph)
{
#if PLATFORM(MAC)
    NSFont *font = platformData.nsFont();
    if (!font || !platformData.isColorBitmapFont())
        return Nullopt;
    return NSSizeToCGSize([font advancementForGlyph:glyph]);
#else
    UNUSED_PARAM(platformData);
    UNUSED_PARAM(glyph);
    return Nullopt;
#endif
}

static inline bool canUseFastGlyphAdvanceGetter(const FontPlatformData& platformData, Glyph glyph, CGSize& advance, bool& populatedAdvance)
{
    if (platformData.isEmoji() || platformData.hasCustomTracking() || platformData.textRenderingMode() == OptimizeLegibility)
        return false;
    if (auto size = advanceForColorBitmapFont(platformData, glyph)) {
        populatedAdvance = true;
        advance = size.value();
        return false;
    }
    return true;
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    CGSize advance = CGSizeZero;
    bool horizontal = platformData().orientation() == Horizontal;
    bool populatedAdvance = false;
    if ((horizontal || m_isBrokenIdeographFallback) && canUseFastGlyphAdvanceGetter(this->platformData(), glyph, advance, populatedAdvance)) {
        float pointSize = platformData().m_size;
        CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
        if (!CGFontGetGlyphAdvancesForStyle(platformData().cgFont(), &m, renderingStyle(platformData()), &glyph, 1, &advance)) {
            RetainPtr<CFStringRef> fullName = adoptCF(CGFontCopyFullName(platformData().cgFont()));
            LOG_ERROR("Unable to cache glyph widths for %@ %f", fullName.get(), pointSize);
            advance.width = 0;
        }
    } else if (!populatedAdvance)
        CTFontGetAdvancesForGlyphs(m_platformData.ctFont(), horizontal ? kCTFontOrientationHorizontal : kCTFontOrientationVertical, &glyph, &advance, 1);

    return advance.width + m_syntheticBoldOffset;
}

struct ProviderInfo {
    const UChar* characters;
    size_t length;
    CFDictionaryRef attributes;
};

static const UniChar* provideStringAndAttributes(CFIndex stringIndex, CFIndex* count, CFDictionaryRef* attributes, void* context)
{
    ProviderInfo* info = static_cast<struct ProviderInfo*>(context);
    if (stringIndex < 0 || static_cast<size_t>(stringIndex) >= info->length)
        return 0;

    *count = info->length - stringIndex;
    *attributes = info->attributes;
    return info->characters + stringIndex;
}

bool Font::canRenderCombiningCharacterSequence(const UChar* characters, size_t length) const
{
    ASSERT(isMainThread());

    if (!m_combiningCharacterSequenceSupport)
        m_combiningCharacterSequenceSupport = std::make_unique<HashMap<String, bool>>();

    WTF::HashMap<String, bool>::AddResult addResult = m_combiningCharacterSequenceSupport->add(String(characters, length), false);
    if (!addResult.isNewEntry)
        return addResult.iterator->value;

    RetainPtr<CFTypeRef> fontEqualityObject = platformData().objectForEqualityCheck();

    ProviderInfo info = { characters, length, getCFStringAttributes(false, platformData().orientation()) };
    RetainPtr<CTLineRef> line = adoptCF(CTLineCreateWithUniCharProvider(&provideStringAndAttributes, 0, &info));

    CFArrayRef runArray = CTLineGetGlyphRuns(line.get());
    CFIndex runCount = CFArrayGetCount(runArray);

    for (CFIndex r = 0; r < runCount; r++) {
        CTRunRef ctRun = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runArray, r));
        ASSERT(CFGetTypeID(ctRun) == CTRunGetTypeID());
        CFDictionaryRef runAttributes = CTRunGetAttributes(ctRun);
        CTFontRef runFont = static_cast<CTFontRef>(CFDictionaryGetValue(runAttributes, kCTFontAttributeName));
        if (!CFEqual(fontEqualityObject.get(), FontPlatformData::objectForEqualityCheck(runFont).get()))
            return false;
    }

    addResult.iterator->value = true;
    return true;
}

} // namespace WebCore
