/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "CoreGraphicsSPI.h"
#import "CoreTextSPI.h"
#import "FontCascade.h"
#import "RenderThemeIOS.h"
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>

namespace WebCore {

void FontCache::platformInit()
{
}

static inline bool isFontWeightBold(NSInteger fontWeight)
{
    return fontWeight >= FontWeight600;
}

static inline bool requiresCustomFallbackFont(const UInt32 character)
{
    return character == AppleLogo || character == blackCircle || character == narrowNonBreakingSpace;
}

PassRefPtr<Font> FontCache::getSystemFontFallbackForCharacters(const FontDescription& description, const Font* originalFontData, const UChar* characters, unsigned length)
{
    const FontPlatformData& platformData = originalFontData->platformData();
    CTFontRef ctFont = platformData.font();

    RetainPtr<CFStringRef> localeString;
#if __IPHONE_OS_VERSION_MIN_REQUIRED > 90000
    if (!description.locale().isNull())
        localeString = description.locale().string().createCFString();
#endif

    CFIndex coveredLength = 0;
    RetainPtr<CTFontRef> substituteFont = adoptCF(CTFontCreatePhysicalFontForCharactersWithLanguage(ctFont, (const UTF16Char*)characters, (CFIndex)length, localeString.get(), &coveredLength));
    if (!substituteFont)
        return nullptr;

    substituteFont = preparePlatformFont(substituteFont.get(), description.textRenderingMode(), description.featureSettings());

    CTFontSymbolicTraits originalTraits = CTFontGetSymbolicTraits(ctFont);
    CTFontSymbolicTraits actualTraits = 0;
    if (isFontWeightBold(description.weight()) || description.italic())
        actualTraits = CTFontGetSymbolicTraits(substituteFont.get());

    bool syntheticBold = (originalTraits & kCTFontTraitBold) && !(actualTraits & kCTFontTraitBold);
    bool syntheticOblique = (originalTraits & kCTFontTraitItalic) && !(actualTraits & kCTFontTraitItalic);

    FontPlatformData alternateFont(substituteFont.get(), platformData.size(), syntheticBold, syntheticOblique, platformData.m_orientation);

    return fontForPlatformData(alternateFont);
}

RefPtr<Font> FontCache::systemFallbackForCharacters(const FontDescription& description, const Font* originalFontData, bool, const UChar* characters, unsigned length)
{
    // Unlike OS X, our fallback font on iPhone is Arial Unicode, which doesn't have some apple-specific glyphs like F8FF.
    // Fall back to the Apple Fallback font in this case.
    if (length && requiresCustomFallbackFont(*characters)) {
        auto* fallback = getCustomFallbackFont(*characters, description);
        if (!fallback)
            return nullptr;
        return fontForPlatformData(*fallback);
    }

    UChar32 c = *characters;
    if (length > 1 && U16_IS_LEAD(c) && U16_IS_TRAIL(characters[1]))
        c = U16_GET_SUPPLEMENTARY(c, characters[1]);

    // For system fonts we use CoreText fallback mechanism.
    if (length) {
        RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontCopyFontDescriptor(originalFontData->getCTFont()));
        if (CTFontDescriptorIsSystemUIFont(fontDescriptor.get()))
            return getSystemFontFallbackForCharacters(description, originalFontData, characters, length);
    }

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 90000
    RetainPtr<CTFontDescriptorRef> fallbackFontDescriptor = adoptCF(CTFontCreatePhysicalFontDescriptorForCharactersWithLanguage(originalFontData->getCTFont(), characters, length, nullptr, nullptr));
#else
    RetainPtr<CTFontRef> fallbackFont = adoptCF(CTFontCreateForCharactersWithLanguage(originalFontData->getCTFont(), characters, length, nullptr, nullptr));
    RetainPtr<CTFontDescriptorRef> fallbackFontDescriptor = adoptCF(CTFontCopyFontDescriptor(fallbackFont.get()));
#endif
    if (auto foundFontName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fallbackFontDescriptor.get(), kCTFontNameAttribute)))) {
        if (c >= 0x0600 && c <= 0x06ff) { // Arabic
            auto familyName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fallbackFontDescriptor.get(), kCTFontFamilyNameAttribute)));
            if (fontFamilyShouldNotBeUsedForArabic(familyName.get()))
                foundFontName = isFontWeightBold(description.weight()) ? CFSTR("GeezaPro-Bold") : CFSTR("GeezaPro");
        }
        if (RefPtr<Font> font = fontForFamily(description, foundFontName.get(), false))
            return font;
    }

    return lastResortFallbackFont(description);
}

Vector<String> FontCache::systemFontFamilies()
{
    // FIXME: <rdar://problem/21890188>
    Vector<String> fontFamilies;
    auto emptyFontDescriptor = adoptCF(CTFontDescriptorCreateWithAttributes((CFDictionaryRef) @{ }));
    auto matchedDescriptors = adoptCF(CTFontDescriptorCreateMatchingFontDescriptors(emptyFontDescriptor.get(), nullptr));
    if (!matchedDescriptors)
        return fontFamilies;

    CFIndex numMatches = CFArrayGetCount(matchedDescriptors.get());
    if (!numMatches)
        return fontFamilies;

    HashSet<String> visited;
    for (CFIndex i = 0; i < numMatches; ++i) {
        auto fontDescriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(matchedDescriptors.get(), i));
        if (auto familyName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontFamilyNameAttribute))))
            visited.add(familyName.get());
    }

    copyToVector(visited, fontFamilies);
    return fontFamilies;
}

RefPtr<Font> FontCache::similarFont(const FontDescription& description)
{
    // Attempt to find an appropriate font using a match based on the presence of keywords in
    // the requested names. For example, we'll match any name that contains "Arabic" to Geeza Pro.
    RefPtr<Font> font;
    for (unsigned i = 0; i < description.familyCount(); ++i) {
        const AtomicString& family = description.familyAt(i);
        if (family.isEmpty())
            continue;

        // Substitute the default monospace font for well-known monospace fonts.
        static NeverDestroyed<AtomicString> monaco("monaco", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> menlo("menlo", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> courier("courier", AtomicString::ConstructFromLiteral);
        if (equalIgnoringCase(family, monaco) || equalIgnoringCase(family, menlo)) {
            font = fontForFamily(description, courier);
            continue;
        }

        // Substitute Verdana for Lucida Grande.
        static NeverDestroyed<AtomicString> lucidaGrande("lucida grande", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> verdana("verdana", AtomicString::ConstructFromLiteral);
        if (equalIgnoringCase(family, lucidaGrande)) {
            font = fontForFamily(description, verdana);
            continue;
        }

        static NeverDestroyed<String> arabic(ASCIILiteral("Arabic"));
        static NeverDestroyed<String> pashto(ASCIILiteral("Pashto"));
        static NeverDestroyed<String> urdu(ASCIILiteral("Urdu"));
        static String* matchWords[3] = { &arabic.get(), &pashto.get(), &urdu.get() };
        static NeverDestroyed<AtomicString> geezaPlain("GeezaPro", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> geezaBold("GeezaPro-Bold", AtomicString::ConstructFromLiteral);
        for (unsigned j = 0; j < 3 && !font; ++j) {
            if (family.contains(*matchWords[j], false))
                font = fontForFamily(description, isFontWeightBold(description.weight()) ? geezaBold : geezaPlain);
        }
    }

    return font.release();
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    static NeverDestroyed<AtomicString> fallbackFontFamily(".PhoneFallback", AtomicString::ConstructFromLiteral);
    return *fontForFamily(fontDescription, fallbackFontFamily, false);
}

FontPlatformData* FontCache::getCustomFallbackFont(const UInt32 c, const FontDescription& description)
{
    ASSERT(requiresCustomFallbackFont(c));

    static NeverDestroyed<AtomicString> helveticaFamily("Helvetica Neue", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> lockClockFamily("LockClock-Light", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> timesNewRomanPSMTFamily("TimesNewRomanPSMT", AtomicString::ConstructFromLiteral);

    AtomicString* family = nullptr;
    switch (c) {
    case AppleLogo:
        family = &helveticaFamily.get();
        break;
    case blackCircle:
        family = &lockClockFamily.get();
        break;
    case narrowNonBreakingSpace:
        family = &timesNewRomanPSMTFamily.get();
        break;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT(family);
    if (!family)
        return nullptr;
    return getCachedFontPlatformData(description, *family);
}

static inline FontTraitsMask toTraitsMask(CTFontSymbolicTraits ctFontTraits)
{
    return static_cast<FontTraitsMask>(((ctFontTraits & kCTFontTraitItalic) ? FontStyleItalicMask : FontStyleNormalMask)
        | FontVariantNormalMask
        // FontWeight600 or higher is bold for CTFonts, so choose middle values for
        // bold (600-900) and non-bold (100-500)
        | ((ctFontTraits & kCTFontTraitBold) ? FontWeight700Mask : FontWeight300Mask));
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
    RetainPtr<CFStringRef> familyNameStr = familyName.string().createCFString();
    NSDictionary *attributes = @{ (id)kCTFontFamilyNameAttribute: (NSString*)familyNameStr.get() };
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateWithAttributes((CFDictionaryRef)attributes));
    RetainPtr<NSArray> matchedDescriptors = adoptNS((NSArray *)CTFontDescriptorCreateMatchingFontDescriptors(fontDescriptor.get(), nullptr));

    NSInteger numMatches = [matchedDescriptors.get() count];
    if (!matchedDescriptors || !numMatches)
        return;

    for (NSInteger i = 0; i < numMatches; ++i) {
        RetainPtr<CFDictionaryRef> traits = adoptCF((CFDictionaryRef)CTFontDescriptorCopyAttribute((CTFontDescriptorRef)[matchedDescriptors.get() objectAtIndex:i], kCTFontTraitsAttribute));
        CFNumberRef resultRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontSymbolicTrait);
        if (resultRef) {
            CTFontSymbolicTraits symbolicTraits;
            CFNumberGetValue(resultRef, kCFNumberIntType, &symbolicTraits);
            traitsMasks.append(toTraitsMask(symbolicTraits));
        }
    }
}

float FontCache::weightOfCTFont(CTFontRef font)
{
    float result = 0;
    RetainPtr<CFDictionaryRef> traits = adoptCF(CTFontCopyTraits(font));
    if (!traits)
        return result;

    CFNumberRef resultRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontWeightTrait);
    if (resultRef)
        CFNumberGetValue(resultRef, kCFNumberFloatType, &result);

    return result;
}

static CTFontRef createCTFontWithTextStyle(const String& familyName, CTFontSymbolicTraits traits, CGFloat size)
{
    if (familyName.isNull())
        return nullptr;

    CTFontSymbolicTraits symbolicTraits = 0;
    if (traits & kCTFontBoldTrait)
        symbolicTraits |= kCTFontBoldTrait;
    if (traits & kCTFontTraitItalic)
        symbolicTraits |= kCTFontTraitItalic;
    RetainPtr<CFStringRef> familyNameStr = familyName.createCFString();
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(familyNameStr.get(), RenderThemeIOS::contentSizeCategory(), nullptr));
    if (symbolicTraits)
        fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), symbolicTraits, symbolicTraits));

    return CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr);
}

static CTFontRef createCTFontWithFamilyNameAndWeight(const String& familyName, CTFontSymbolicTraits traits, float size, uint16_t weight)
{
    if (familyName.isNull())
        return nullptr;

    static NeverDestroyed<AtomicString> systemUIFontWithWebKitPrefix("-webkit-system-font", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> systemUIFontWithApplePrefix("-apple-system", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> systemUIFontWithAppleAlternatePrefix("-apple-system-font", AtomicString::ConstructFromLiteral);
    if (equalIgnoringCase(familyName, systemUIFontWithWebKitPrefix) || equalIgnoringCase(familyName, systemUIFontWithApplePrefix) || equalIgnoringCase(familyName, systemUIFontWithAppleAlternatePrefix)) {
        CTFontUIFontType fontType = kCTFontUIFontSystem;
        if (weight > 300) {
            // The comment below has been copied from CoreText/UIFoundation. However, in WebKit we synthesize the oblique,
            // so we should investigate the result <rdar://problem/14449340>:
            if (traits & kCTFontTraitBold)
                fontType = kCTFontUIFontEmphasizedSystem;
        } else if (weight > 250)
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemLight);
        else if (weight > 150)
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemThin);
        else
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemUltraLight);
        RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(fontType, size, nullptr));
        if (traits & kCTFontTraitItalic)
            fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), kCTFontItalicTrait, kCTFontItalicTrait));
        return CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr);
    }

    static NeverDestroyed<AtomicString> systemUIMonospacedNumbersFontWithApplePrefix("-apple-system-monospaced-numbers", AtomicString::ConstructFromLiteral);
    if (equalIgnoringCase(familyName, systemUIMonospacedNumbersFontWithApplePrefix)) {
        RetainPtr<CTFontDescriptorRef> systemFontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, size, nullptr));
        RetainPtr<CTFontDescriptorRef> monospaceFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithFeature(systemFontDescriptor.get(), (CFNumberRef)@(kNumberSpacingType), (CFNumberRef)@(kMonospacedNumbersSelector)));
        return CTFontCreateWithFontDescriptor(monospaceFontDescriptor.get(), size, nullptr);
    }


    RetainPtr<CFStringRef> familyNameStr = familyName.createCFString();
    CTFontSymbolicTraits requestedTraits = (CTFontSymbolicTraits)(traits & (kCTFontBoldTrait | kCTFontItalicTrait));
    return CTFontCreateForCSS(familyNameStr.get(), weight, requestedTraits, size);
}

static uint16_t toCTFontWeight(FontWeight fontWeight)
{
    switch (fontWeight) {
    case FontWeight100:
        return 100;
    case FontWeight200:
        return 200;
    case FontWeight300:
        return 300;
    case FontWeight400:
        return 400;
    case FontWeight500:
        return 500;
    case FontWeight600:
        return 600;
    case FontWeight700:
        return 700;
    case FontWeight800:
        return 800;
    case FontWeight900:
        return 900;
    default:
        return 400;
    }
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    CTFontSymbolicTraits traits = 0;
    if (fontDescription.italic())
        traits |= kCTFontTraitItalic;
    if (isFontWeightBold(fontDescription.weight()))
        traits |= kCTFontTraitBold;
    float size = fontDescription.computedPixelSize();

    RetainPtr<CTFontRef> ctFont;
    if (family.startsWith("UICTFontTextStyle"))
        ctFont = adoptCF(createCTFontWithTextStyle(family, traits, size));
    else
        ctFont = adoptCF(createCTFontWithFamilyNameAndWeight(family, traits, size, toCTFontWeight(fontDescription.weight())));
    if (!ctFont)
        return nullptr;

    ctFont = preparePlatformFont(ctFont.get(), fontDescription.textRenderingMode(), fontDescription.featureSettings());

    CTFontSymbolicTraits actualTraits = 0;
    if (isFontWeightBold(fontDescription.weight()) || fontDescription.italic())
        actualTraits = CTFontGetSymbolicTraits(ctFont.get());

    bool isAppleColorEmoji = CTFontIsAppleColorEmoji(ctFont.get());

    bool syntheticBold = (fontDescription.fontSynthesis() & FontSynthesisWeight) && (traits & kCTFontTraitBold) && !(actualTraits & kCTFontTraitBold) && !isAppleColorEmoji;
    bool syntheticOblique = (fontDescription.fontSynthesis() & FontSynthesisStyle) && (traits & kCTFontTraitItalic) && !(actualTraits & kCTFontTraitItalic) && !isAppleColorEmoji;

    auto result = std::make_unique<FontPlatformData>(ctFont.get(), size, syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());
    return result;
}

} // namespace WebCore
