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

#import "Font.h"
#import "RenderThemeIOS.h"
#import <CoreGraphics/CGFontUnicodeSupport.h>
#import <CoreText/CTFontDescriptorPriv.h>
#import <CoreText/CTFontPriv.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>

namespace WebCore {

void FontCache::platformInit()
{
    wkSetUpFontCache();
}

static inline bool isFontWeightBold(NSInteger fontWeight)
{
    return fontWeight >= FontWeight600;
}

static inline bool requiresCustomFallbackFont(const UInt32 character)
{
    return character == AppleLogo || character == blackCircle;
}

static CFCharacterSetRef copyFontCharacterSet(CFStringRef fontName)
{
    // The size, 10, is arbitrary.
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateWithNameAndSize(fontName, 10));
    RetainPtr<CTFontRef> font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), 10, nullptr));
    return (CFCharacterSetRef)CTFontDescriptorCopyAttribute(fontDescriptor.get(), kCTFontCharacterSetAttribute);
}

static CFCharacterSetRef appleColorEmojiCharacterSet()
{
    static CFCharacterSetRef characterSet = copyFontCharacterSet(CFSTR("AppleColorEmoji"));
    return characterSet;
}

static CFCharacterSetRef phoneFallbackCharacterSet()
{
    static CFCharacterSetRef characterSet = copyFontCharacterSet(CFSTR(".PhoneFallback"));
    return characterSet;
}

PassRefPtr<SimpleFontData> FontCache::getSystemFontFallbackForCharacters(const FontDescription& description, const SimpleFontData* originalFontData, const UChar* characters, int length)
{
    const FontPlatformData& platformData = originalFontData->platformData();
    CTFontRef ctFont = platformData.font();

    CFIndex coveredLength = 0;
    RetainPtr<CTFontRef> substituteFont = adoptCF(CTFontCreatePhysicalFontForCharactersWithLanguage(ctFont, (const UTF16Char*)characters, (CFIndex)length, 0, &coveredLength));
    if (!substituteFont)
        return 0;

    CTFontSymbolicTraits originalTraits = CTFontGetSymbolicTraits(ctFont);
    CTFontSymbolicTraits actualTraits = 0;
    if (isFontWeightBold(description.weight()) || description.italic())
        actualTraits = CTFontGetSymbolicTraits(substituteFont.get());

    bool syntheticBold = (originalTraits & kCTFontTraitBold) && !(actualTraits & kCTFontTraitBold);
    bool syntheticOblique = (originalTraits & kCTFontTraitItalic) && !(actualTraits & kCTFontTraitItalic);

    FontPlatformData alternateFont(substituteFont.get(), platformData.size(), platformData.isPrinterFont(), syntheticBold, syntheticOblique, platformData.m_orientation);
    alternateFont.m_isEmoji = CTFontIsAppleColorEmoji(substituteFont.get());

    return getCachedFontData(&alternateFont, DoNotRetain);
}

PassRefPtr<SimpleFontData> FontCache::systemFallbackForCharacters(const FontDescription& description, const SimpleFontData* originalFontData, bool, const UChar* characters, int length)
{
    static bool isGB18030ComplianceRequired = wkIsGB18030ComplianceRequired();

    // Unlike OS X, our fallback font on iPhone is Arial Unicode, which doesn't have some apple-specific glyphs like F8FF.
    // Fall back to the Apple Fallback font in this case.
    if (length > 0 && requiresCustomFallbackFont(*characters))
        return getCachedFontData(getCustomFallbackFont(*characters, description), DoNotRetain);

    UChar32 c = *characters;
    if (length > 1 && U16_IS_LEAD(c) && U16_IS_TRAIL(characters[1]))
        c = U16_GET_SUPPLEMENTARY(c, characters[1]);

    // For system fonts we use CoreText fallback mechanism.
    if (length) {
        RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontCopyFontDescriptor(originalFontData->getCTFont()));
        if (CTFontDescriptorIsSystemUIFont(fontDescriptor.get()))
            return getSystemFontFallbackForCharacters(description, originalFontData, characters, length);
    }

    bool useCJFont = false;
    bool useKoreanFont = false;
    bool useCyrillicFont = false;
    bool useArabicFont = false;
    bool useHebrewFont = false;
    bool useIndicFont = false;
    bool useThaiFont = false;
    bool useTibetanFont = false;
    bool useCanadianAboriginalSyllabicsFont = false;
    bool useEmojiFont = false;
    if (length > 0) {
        do {
            // This isn't a loop but a way to efficiently check for ranges of characters.

            // The following ranges are Korean Hangul and should be rendered by AppleSDGothicNeo
            // U+1100 - U+11FF
            // U+3130 - U+318F
            // U+AC00 - U+D7A3

            // These are Cyrillic and should be rendered by Helvetica Neue
            // U+0400 - U+052F

            if (c < 0x400)
                break;
            if (c <= 0x52F) {
                useCyrillicFont = true;
                break;
            }
            if (c < 0x590)
                break;
            if (c < 0x600) {
                useHebrewFont = true;
                break;
            }
            if (c <= 0x6FF) {
                useArabicFont = true;
                break;
            }
            if (c < 0x900)
                break;
            if (c < 0xE00) {
                useIndicFont = true;
                break;
            }
            if (c <= 0xE7F) {
                useThaiFont = true;
                break;
            }
            if (c < 0x0F00)
                break;
            if (c <= 0x0FFF) {
                useTibetanFont = true;
                break;
            }
            if (c < 0x1100)
                break;
            if (c <= 0x11FF) {
                useKoreanFont = true;
                break;
            }
            if (c > 0x1400 && c < 0x1780) {
                useCanadianAboriginalSyllabicsFont = true;
                break;
            }
            if (c < 0x2E80)
                break;
            if (c < 0x3130) {
                useCJFont = true;
                break;
            }
            if (c <= 0x318F) {
                useKoreanFont = true;
                break;
            }
            if (c < 0xAC00) {
                useCJFont = true;
                break;
            }
            if (c <= 0xD7A3) {
                useKoreanFont = true;
                break;
            }
            if ( c <= 0xDFFF) {
                useCJFont = true;
                break;
            }
            if ( c < 0xE000)
                break;
            if ( c < 0xE600) {
                if (isGB18030ComplianceRequired)
                    useCJFont = true;
                else
                    useEmojiFont = true;
                break;
            }
            if ( c <= 0xE864 && isGB18030ComplianceRequired) {
                useCJFont = true;
                break;
            }
            if (c <= 0xF8FF)
                break;
            if (c < 0xFB00) {
                useCJFont = true;
                break;
            }
            if (c < 0xFB50)
                break;
            if (c <= 0xFDFF) {
                useArabicFont = true;
                break;
            }
            if (c < 0xFE20)
                break;
            if (c < 0xFE70) {
                useCJFont = true;
                break;
            }
            if (c < 0xFF00) {
                useArabicFont = true;
                break;
            }
            if (c < 0xFFF0) {
                useCJFont = true;
                break;
            }
            if (c >=0x20000 && c <= 0x2FFFF)
                useCJFont = true;
        } while (0);
    }

    RefPtr<SimpleFontData> simpleFontData;

    if (useCJFont) {
        // By default, Chinese font is preferred, fall back on Japanese.

        enum CJKFontVariant {
            kCJKFontUseHiragino = 0,
            kCJKFontUseSTHeitiSC,
            kCJKFontUseSTHeitiTC,
            kCJKFontUseSTHeitiJ,
            kCJKFontUseSTHeitiK,
            kCJKFontsUseHKGPW3UI
        };

        static NeverDestroyed<AtomicString> plainHiragino("HiraKakuProN-W3", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> plainSTHeitiSC("STHeitiSC-Light", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> plainSTHeitiTC("STHeitiTC-Light", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> plainSTHeitiJ("STHeitiJ-Light", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> plainSTHeitiK("STHeitiK-Light", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> plainHKGPW3UI("HKGPW3UI", AtomicString::ConstructFromLiteral);
        static AtomicString* cjkPlain[] = {     
            &plainHiragino.get(),
            &plainSTHeitiSC.get(),
            &plainSTHeitiTC.get(),
            &plainSTHeitiJ.get(),
            &plainSTHeitiK.get(),
            &plainHKGPW3UI.get(),
        };

        static NeverDestroyed<AtomicString> boldHiragino("HiraKakuProN-W6", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> boldSTHeitiSC("STHeitiSC-Medium", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> boldSTHeitiTC("STHeitiTC-Medium", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> boldSTHeitiJ("STHeitiJ-Medium", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> boldSTHeitiK("STHeitiK-Medium", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> boldHKGPW3UI("HKGPW3UI", AtomicString::ConstructFromLiteral);
        static AtomicString* cjkBold[] = {  
            &boldHiragino.get(),
            &boldSTHeitiSC.get(),
            &boldSTHeitiTC.get(),
            &boldSTHeitiJ.get(),
            &boldSTHeitiK.get(),
            &boldHKGPW3UI.get(),
        };

        // Default below is for Simplified Chinese user: zh-Hans - note that Hiragino is the
        // the secondary font as we want its for Hiragana and Katakana. The other CJK fonts
        // do not, and should not, contain Hiragana or Katakana glyphs.
        static CJKFontVariant preferredCJKFont = kCJKFontUseSTHeitiSC;
        static CJKFontVariant secondaryCJKFont = kCJKFontsUseHKGPW3UI;

        static bool CJKFontInitialized;
        if (!CJKFontInitialized) {
            CJKFontInitialized = true;
            // Testing: languageName = (CFStringRef)@"ja";
            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
            NSArray *languages = [defaults stringArrayForKey:@"AppleLanguages"];

            if (languages) {
                for (NSString *language in languages) {
                    RetainPtr<CFStringRef> languageName = adoptCF(CFLocaleCreateCanonicalLanguageIdentifierFromString(nullptr, (CFStringRef)language));
                    if (CFEqual(languageName.get(), CFSTR("zh-Hans")))
                        break; // Simplified Chinese - default settings
                    else if (CFEqual(languageName.get(), CFSTR("ja"))) {
                        preferredCJKFont = kCJKFontUseHiragino; // Japanese - prefer Hiragino and STHeiti Japanse Variant
                        secondaryCJKFont = kCJKFontUseSTHeitiJ;
                        break;
                    } else if (CFEqual(languageName.get(), CFSTR("ko"))) {
                        preferredCJKFont = kCJKFontUseSTHeitiK; // Korean - prefer STHeiti Korean Variant 
                        break;
                    } else if (CFEqual(languageName.get(), CFSTR("zh-Hant"))) {
                        preferredCJKFont = kCJKFontUseSTHeitiTC; // Traditional Chinese - prefer STHeiti Traditional Variant
                        break;
                    }
                }
            }
        }

        simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? *cjkBold[preferredCJKFont] : *cjkPlain[preferredCJKFont], false, DoNotRetain);
        bool useSecondaryFont = true;
        if (simpleFontData) {
            CGGlyph glyphs[2];
            // CGFontGetGlyphsForUnichars takes UTF-16 buffer. Should only be 1 codepoint but since we may pass in two UTF-16 characters,
            // make room for 2 glyphs just to be safe.
            CGFontGetGlyphsForUnichars(simpleFontData->platformData().cgFont(), characters, glyphs, length);

            useSecondaryFont = (glyphs[0] == 0);
        }

        if (useSecondaryFont)
            simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? *cjkBold[secondaryCJKFont] : *cjkPlain[secondaryCJKFont], false, DoNotRetain);
    } else if (useKoreanFont) {
        static NeverDestroyed<AtomicString> koreanPlain("AppleSDGothicNeo-Medium", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> koreanBold("AppleSDGothicNeo-Bold", AtomicString::ConstructFromLiteral);
        simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? koreanBold : koreanPlain, false, DoNotRetain);
    } else if (useCyrillicFont) {
        static NeverDestroyed<AtomicString> cyrillicPlain("HelveticaNeue", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> cyrillicBold("HelveticaNeue-Bold", AtomicString::ConstructFromLiteral);
        simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? cyrillicBold : cyrillicPlain, false, DoNotRetain);
    } else if (useArabicFont) {
        static NeverDestroyed<AtomicString> arabicPlain("GeezaPro", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> arabicBold("GeezaPro-Bold", AtomicString::ConstructFromLiteral);
        simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? arabicBold : arabicPlain, false, DoNotRetain);
    } else if (useHebrewFont) {
        static NeverDestroyed<AtomicString> hebrewPlain("ArialHebrew", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> hebrewBold("ArialHebrew-Bold", AtomicString::ConstructFromLiteral);
        simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? hebrewBold : hebrewPlain, false, DoNotRetain);
    } else if (useIndicFont) {
        static NeverDestroyed<AtomicString> devanagariFont("KohinoorDevanagari-Book", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> bengaliFont("BanglaSangamMN", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> gurmukhiFont("GurmukhiMN", AtomicString::ConstructFromLiteral); // Might be replaced in a future release with a Sangam version.
        static NeverDestroyed<AtomicString> gujaratiFont("GujaratiSangamMN", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> oriyaFont("OriyaSangamMN", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> tamilFont("TamilSangamMN", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> teluguFont("TeluguSangamMN", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> kannadaFont("KannadaSangamMN", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> malayalamFont("MalayalamSangamMN", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> sinhalaFont("SinhalaSangamMN", AtomicString::ConstructFromLiteral);

        static NeverDestroyed<AtomicString> devanagariFontBold("KohinoorDevanagari-Medium", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> bengaliFontBold("BanglaSangamMN-Bold", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> gurmukhiFontBold("GurmukhiMN-Bold", AtomicString::ConstructFromLiteral); // Might be replaced in a future release with a Sangam version.
        static NeverDestroyed<AtomicString> gujaratiFontBold("GujaratiSangamMN-Bold", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> oriyaFontBold("OriyaSangamMN-Bold", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> tamilFontBold("TamilSangamMN-Bold", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> teluguFontBold("TeluguSangamMN-Bold", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> kannadaFontBold("KannadaSangamMN-Bold", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> malayalamFontBold("MalayalamSangamMN-Bold", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> sinhalaFontBold("SinhalaSangamMN-Bold", AtomicString::ConstructFromLiteral);

        static AtomicString* indicUnicodePageFonts[] = {
            &devanagariFont.get(),
            &bengaliFont.get(),
            &gurmukhiFont.get(),
            &gujaratiFont.get(),
            &oriyaFont.get(),
            &tamilFont.get(),
            &teluguFont.get(),
            &kannadaFont.get(),
            &malayalamFont.get(),
            &sinhalaFont.get()
        };

        static AtomicString* indicUnicodePageFontsBold[] = {
            &devanagariFontBold.get(),
            &bengaliFontBold.get(),
            &gurmukhiFontBold.get(),
            &gujaratiFontBold.get(),
            &oriyaFontBold.get(),
            &tamilFontBold.get(),
            &teluguFontBold.get(),
            &kannadaFontBold.get(),
            &malayalamFontBold.get(),
            &sinhalaFontBold.get()
        };

        uint32_t indicPageOrderIndex = (c - 0x0900) / 0x0080; // Indic scripts start at 0x0900 in Unicode. Each script is allocalted a block of 0x80 characters.
        if (indicPageOrderIndex < (sizeof(indicUnicodePageFonts) / sizeof(AtomicString*))) {
            AtomicString* indicFontString = isFontWeightBold(description.weight()) ? indicUnicodePageFontsBold[indicPageOrderIndex] : indicUnicodePageFonts[indicPageOrderIndex];
            if (indicFontString)
                simpleFontData = getCachedFontData(description, *indicFontString, false, DoNotRetain);
        }
    } else if (useThaiFont) {
        static NeverDestroyed<AtomicString> thaiPlain("Thonburi", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> thaiBold("Thonburi-Bold", AtomicString::ConstructFromLiteral);
        simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? thaiBold : thaiPlain, false, DoNotRetain);
    } else if (useTibetanFont) {
        static NeverDestroyed<AtomicString> tibetanPlain("Kailasa", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> tibetanBold("Kailasa-Bold", AtomicString::ConstructFromLiteral);
        simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? tibetanBold : tibetanPlain, false, DoNotRetain);
    } else if (useCanadianAboriginalSyllabicsFont) {
        static NeverDestroyed<AtomicString> casPlain("EuphemiaUCAS", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> casBold("EuphemiaUCAS-Bold", AtomicString::ConstructFromLiteral);
        simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? casBold : casPlain, false, DoNotRetain);
    } else {
        static NeverDestroyed<AtomicString> appleColorEmoji("AppleColorEmoji", AtomicString::ConstructFromLiteral);
        if (!useEmojiFont) {
            if (!CFCharacterSetIsLongCharacterMember(phoneFallbackCharacterSet(), c))
                useEmojiFont = CFCharacterSetIsLongCharacterMember(appleColorEmojiCharacterSet(), c);
        }
        if (useEmojiFont)
            simpleFontData = getCachedFontData(description, appleColorEmoji, false, DoNotRetain);
    }

    if (simpleFontData)
        return simpleFontData.release();

    return getNonRetainedLastResortFallbackFont(description);
}

PassRefPtr<SimpleFontData> FontCache::similarFontPlatformData(const FontDescription& description)
{
    // Attempt to find an appropriate font using a match based on the presence of keywords in
    // the requested names. For example, we'll match any name that contains "Arabic" to Geeza Pro.
    RefPtr<SimpleFontData> simpleFontData;
    for (unsigned i = 0; i < description.familyCount(); ++i) {
        const AtomicString& family = description.familyAt(i);
        if (family.isEmpty())
            continue;

        // Substitute the default monospace font for well-known monospace fonts.
        static NeverDestroyed<AtomicString> monaco("monaco", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> menlo("menlo", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> courier("courier", AtomicString::ConstructFromLiteral);
        if (equalIgnoringCase(family, monaco) || equalIgnoringCase(family, menlo)) {
            simpleFontData = getCachedFontData(description, courier);
            continue;
        }

        // Substitute Verdana for Lucida Grande.
        static NeverDestroyed<AtomicString> lucidaGrande("lucida grande", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> verdana("verdana", AtomicString::ConstructFromLiteral);
        if (equalIgnoringCase(family, lucidaGrande)) {
            simpleFontData = getCachedFontData(description, verdana);
            continue;
        }

        static NeverDestroyed<String> arabic(ASCIILiteral("Arabic"));
        static NeverDestroyed<String> pashto(ASCIILiteral("Pashto"));
        static NeverDestroyed<String> urdu(ASCIILiteral("Urdu"));
        static String* matchWords[3] = { &arabic.get(), &pashto.get(), &urdu.get() };
        static NeverDestroyed<AtomicString> geezaPlain("GeezaPro", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> geezaBold("GeezaPro-Bold", AtomicString::ConstructFromLiteral);
        for (int j = 0; j < 3 && !simpleFontData; ++j)
            if (family.contains(*matchWords[j], false))
                simpleFontData = getCachedFontData(description, isFontWeightBold(description.weight()) ? geezaBold : geezaPlain);
    }

    return simpleFontData.release();
}

PassRefPtr<SimpleFontData> FontCache::getLastResortFallbackFont(const FontDescription& fontDescription, ShouldRetain shouldRetain)
{
    static NeverDestroyed<AtomicString> fallbackFontFamily(".PhoneFallback", AtomicString::ConstructFromLiteral);
    return getCachedFontData(fontDescription, fallbackFontFamily, false, shouldRetain);
}

FontPlatformData* FontCache::getCustomFallbackFont(const UInt32 c, const FontDescription& description)
{
    ASSERT(requiresCustomFallbackFont(c));
    if (c == AppleLogo) {
        static NeverDestroyed<AtomicString> helveticaFamily("Helvetica Neue", AtomicString::ConstructFromLiteral);
        return getCachedFontPlatformData(description, helveticaFamily);
    }
    if (c == blackCircle) {
        static NeverDestroyed<AtomicString> lockClockFamily("LockClock-Light", AtomicString::ConstructFromLiteral);
        return getCachedFontPlatformData(description, lockClockFamily);
    }
    return nullptr;
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
    static NeverDestroyed<AtomicString> systemUIFontWithApplePrefix("-apple-system-font", AtomicString::ConstructFromLiteral);
    if (equalIgnoringCase(familyName, systemUIFontWithWebKitPrefix) || equalIgnoringCase(familyName, systemUIFontWithApplePrefix)) {
        CTFontUIFontType fontType = kCTFontUIFontSystem;
        if (weight > 300) {
            // The comment below has been copied from CoreText/UIFoundation. However, in WebKit we synthesize the oblique,
            // so we should investigate the result <rdar://problem/14449340>:
            // We don't do bold-italic for system fonts. If you ask for it, we'll assume that you're just kidding and that you really want bold. This is a feature.
            if (traits & kCTFontTraitBold)
                fontType = kCTFontUIFontEmphasizedSystem;
            else if (traits & kCTFontTraitItalic)
                fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemItalic);
        } else if (weight > 250)
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemLight);
        else if (weight > 150)
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemThin);
        else
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemUltraLight);
        RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(fontType, size, nullptr));
        return CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr);
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

PassOwnPtr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    // Special case for "Courier" font. We used to have only an oblique variant on iOS, so prior to
    // iOS 6.0, we disallowed its use here. We'll fall back on "Courier New". <rdar://problem/5116477&10850227>
    static NeverDestroyed<AtomicString> courier("Courier", AtomicString::ConstructFromLiteral);
    static bool shouldDisallowCourier = !iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion_6_0);
    if (shouldDisallowCourier && equalIgnoringCase(family, courier))
        return nullptr;

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

    CTFontSymbolicTraits actualTraits = 0;
    if (isFontWeightBold(fontDescription.weight()) || fontDescription.italic())
        actualTraits = CTFontGetSymbolicTraits(ctFont.get());

    bool isAppleColorEmoji = CTFontIsAppleColorEmoji(ctFont.get());

    bool syntheticBold = (traits & kCTFontTraitBold) && !(actualTraits & kCTFontTraitBold) && !isAppleColorEmoji;
    bool syntheticOblique = (traits & kCTFontTraitItalic) && !(actualTraits & kCTFontTraitItalic) && !isAppleColorEmoji;

    FontPlatformData* result = new FontPlatformData(ctFont.get(), size, fontDescription.usePrinterFont(), syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant());
    if (isAppleColorEmoji)
        result->m_isEmoji = true;
    return adoptPtr(result);
}

} // namespace WebCore
