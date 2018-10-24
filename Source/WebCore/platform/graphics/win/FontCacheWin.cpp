/*
 * Copyright (C) 2006-2008, 2013-2014 Apple Inc.  All rights reserved.
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

#include "config.h"
#include <winsock2.h>
#include "Font.h"
#include "FontCache.h"
#include "HWndDC.h"
#include <mlang.h>
#include <windows.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>
#include <wtf/text/win/WCharStringExtras.h>
#include <wtf/win/GDIObject.h>

#if USE(CG)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#endif

#if USE(DIRECT2D)
#include <dwrite.h>
#endif

using std::min;

namespace WebCore
{

void FontCache::platformInit()
{
#if USE(CG)
    // Turn on CG's local font cache.
    size_t size = 1536 * 1024 * 4; // This size matches Mac.
    CGFontSetShouldUseMulticache(true);
    CGFontCache* fontCache = CGFontCacheGetLocalCache();
    CGFontCacheSetShouldAutoExpire(fontCache, false);
    CGFontCacheSetMaxSize(fontCache, size);
#endif
}

IMLangFontLinkType* FontCache::getFontLinkInterface()
{
    static IMultiLanguage *multiLanguage;
    if (!multiLanguage) {
        if (CoCreateInstance(CLSID_CMultiLanguage, 0, CLSCTX_ALL, IID_IMultiLanguage, (void**)&multiLanguage) != S_OK)
            return 0;
    }

    static IMLangFontLinkType* langFontLink;
    if (!langFontLink) {
        if (multiLanguage->QueryInterface(&langFontLink) != S_OK)
            return 0;
    }

    return langFontLink;
}

static int CALLBACK metaFileEnumProc(HDC hdc, HANDLETABLE* table, CONST ENHMETARECORD* record, int tableEntries, LPARAM logFont)
{
    if (record->iType == EMR_EXTCREATEFONTINDIRECTW) {
        const EMREXTCREATEFONTINDIRECTW* createFontRecord = reinterpret_cast<const EMREXTCREATEFONTINDIRECTW*>(record);
        *reinterpret_cast<LOGFONT*>(logFont) = createFontRecord->elfw.elfLogFont;
    }
    return true;
}

static int CALLBACK linkedFontEnumProc(CONST LOGFONT* logFont, CONST TEXTMETRIC* metrics, DWORD fontType, LPARAM hfont)
{
    *reinterpret_cast<HFONT*>(hfont) = CreateFontIndirect(logFont);
    return false;
}

WEBCORE_EXPORT void appendLinkedFonts(WCHAR* linkedFonts, unsigned length, Vector<String>* result)
{
    unsigned i = 0;
    while (i < length) {
        while (i < length && linkedFonts[i] != ',')
            i++;
        // Break if we did not find a comma.
        if (i == length)
            break;
        i++;
        unsigned j = i;
        while (j < length && linkedFonts[j])
            j++;
        result->append(wcharToString(linkedFonts + i, j - i));
        i = j + 1;
    }
}

static const Vector<String>* getLinkedFonts(String& family)
{
    static HashMap<String, Vector<String>*> systemLinkMap;
    Vector<String>* result = systemLinkMap.get(family);
    if (result)
        return result;

    result = new Vector<String>;
    systemLinkMap.set(family, result);
    HKEY fontLinkKey = nullptr;
    if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\FontLink\\SystemLink", 0, KEY_READ, &fontLinkKey) != ERROR_SUCCESS)
        return result;

    DWORD linkedFontsBufferSize = 0;
    if (::RegQueryValueEx(fontLinkKey, stringToNullTerminatedWChar(family).data(), 0, nullptr, nullptr, &linkedFontsBufferSize) == ERROR_FILE_NOT_FOUND) {
        WTFLogAlways("The font link key %s does not exist in the registry.", family.utf8().data());
        return result;
    }

    static const constexpr unsigned InitialBufferSize { 256 / sizeof(WCHAR) };
    Vector<WCHAR, InitialBufferSize> linkedFonts(roundUpToMultipleOf<sizeof(WCHAR)>(linkedFontsBufferSize) / sizeof(WCHAR));
    if (::RegQueryValueEx(fontLinkKey, stringToNullTerminatedWChar(family).data(), 0, nullptr, reinterpret_cast<BYTE*>(linkedFonts.data()), &linkedFontsBufferSize) == ERROR_SUCCESS) {
        unsigned length = linkedFontsBufferSize / sizeof(WCHAR);
        appendLinkedFonts(linkedFonts.data(), length, result);
    }
    RegCloseKey(fontLinkKey);
    return result;
}

static const Vector<DWORD, 4>& getCJKCodePageMasks()
{
    // The default order in which we look for a font for a CJK character. If the user's default code page is
    // one of these, we will use it first.
    static const UINT CJKCodePages[] = { 
        932, /* Japanese */
        936, /* Simplified Chinese */
        950, /* Traditional Chinese */
        949  /* Korean */
    };

    static Vector<DWORD, 4> codePageMasks;
    static bool initialized;
    if (!initialized) {
        initialized = true;
        IMLangFontLinkType* langFontLink = FontCache::singleton().getFontLinkInterface();
        if (!langFontLink)
            return codePageMasks;

        UINT defaultCodePage;
        DWORD defaultCodePageMask = 0;
        if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_RETURN_NUMBER | LOCALE_IDEFAULTANSICODEPAGE, reinterpret_cast<LPWSTR>(&defaultCodePage), sizeof(defaultCodePage)))
            langFontLink->CodePageToCodePages(defaultCodePage, &defaultCodePageMask);

        if (defaultCodePage == CJKCodePages[0] || defaultCodePage == CJKCodePages[1] || defaultCodePage == CJKCodePages[2] || defaultCodePage == CJKCodePages[3])
            codePageMasks.append(defaultCodePageMask);
        for (unsigned i = 0; i < 4; ++i) {
            if (defaultCodePage != CJKCodePages[i]) {
                DWORD codePageMask;
                langFontLink->CodePageToCodePages(CJKCodePages[i], &codePageMask);
                codePageMasks.append(codePageMask);
            }
        }
    }
    return codePageMasks;
}

static bool currentFontContainsCharacter(HDC hdc, UChar character)
{
    static Vector<char, 512> glyphsetBuffer;
    glyphsetBuffer.resize(GetFontUnicodeRanges(hdc, 0));
    GLYPHSET* glyphset = reinterpret_cast<GLYPHSET*>(glyphsetBuffer.data());
    GetFontUnicodeRanges(hdc, glyphset);

    // FIXME: Change this to a binary search.
    unsigned i = 0;
    while (i < glyphset->cRanges && glyphset->ranges[i].wcLow <= character)
        i++;

    return i && glyphset->ranges[i - 1].wcLow + glyphset->ranges[i - 1].cGlyphs > character;
}

static HFONT createMLangFont(IMLangFontLinkType* langFontLink, HDC hdc, DWORD codePageMask, UChar character = 0)
{
    HFONT MLangFont;
    HFONT hfont = 0;
    if (SUCCEEDED(langFontLink->MapFont(hdc, codePageMask, character, &MLangFont)) && MLangFont) {
        LOGFONT lf;
        GetObject(MLangFont, sizeof(LOGFONT), &lf);
        langFontLink->ReleaseFont(MLangFont);
        hfont = CreateFontIndirect(&lf);
    }
    return hfont;
}

RefPtr<Font> FontCache::systemFallbackForCharacters(const FontDescription& description, const Font* originalFontData, bool, const UChar* characters, unsigned length)
{
    UChar character = characters[0];
    RefPtr<Font> fontData;
    HWndDC hdc(0);
    HFONT primaryFont = originalFontData->platformData().hfont();
    HGDIOBJ oldFont = SelectObject(hdc, primaryFont);
    HFONT hfont = 0;

    if (IMLangFontLinkType* langFontLink = getFontLinkInterface()) {
        // Try MLang font linking first.
        DWORD codePages = 0;
        if (SUCCEEDED(langFontLink->GetCharCodePages(character, &codePages))) {
            if (codePages && u_getIntPropertyValue(character, UCHAR_UNIFIED_IDEOGRAPH)) {
                // The CJK character may belong to multiple code pages. We want to
                // do font linking against a single one of them, preferring the default
                // code page for the user's locale.
                const Vector<DWORD, 4>& CJKCodePageMasks = getCJKCodePageMasks();
                unsigned numCodePages = CJKCodePageMasks.size();
                for (unsigned i = 0; i < numCodePages && !hfont; ++i) {
                    hfont = createMLangFont(langFontLink, hdc, CJKCodePageMasks[i]);
                    if (hfont && !(codePages & CJKCodePageMasks[i])) {
                        // We asked about a code page that is not one of the code pages
                        // returned by MLang, so the font might not contain the character.
                        SelectObject(hdc, hfont);
                        if (!currentFontContainsCharacter(hdc, character)) {
                            DeleteObject(hfont);
                            hfont = 0;
                        }
                        SelectObject(hdc, primaryFont);
                    }
                }
            } else
                hfont = createMLangFont(langFontLink, hdc, codePages, character);
        }
    }

    // A font returned from MLang is trusted to contain the character.
    bool containsCharacter = hfont;

    if (!hfont) {
        // To find out what font Uniscribe would use, we make it draw into a metafile and intercept
        // calls to CreateFontIndirect().
        HDC metaFileDc = CreateEnhMetaFile(hdc, NULL, NULL, NULL);
        SelectObject(metaFileDc, primaryFont);

        bool scriptStringOutSucceeded = false;
        SCRIPT_STRING_ANALYSIS ssa;

        // FIXME: If length is greater than 1, we actually return the font for the last character.
        // This function should be renamed getFontDataForCharacter and take a single 32-bit character.
        if (SUCCEEDED(ScriptStringAnalyse(metaFileDc, characters, length, 0, -1, SSA_METAFILE | SSA_FALLBACK | SSA_GLYPHS | SSA_LINK,
            0, NULL, NULL, NULL, NULL, NULL, &ssa))) {
            scriptStringOutSucceeded = SUCCEEDED(ScriptStringOut(ssa, 0, 0, 0, NULL, 0, 0, FALSE));
            ScriptStringFree(&ssa);
        }
        HENHMETAFILE metaFile = CloseEnhMetaFile(metaFileDc);
        if (scriptStringOutSucceeded) {
            LOGFONT logFont;
            logFont.lfFaceName[0] = 0;
            EnumEnhMetaFile(0, metaFile, metaFileEnumProc, &logFont, NULL);
            if (logFont.lfFaceName[0])
                hfont = CreateFontIndirect(&logFont);
        }
        DeleteEnhMetaFile(metaFile);
    }

    String familyName;
    const Vector<String>* linkedFonts = 0;
    unsigned linkedFontIndex = 0;
    while (hfont) {
        SelectObject(hdc, hfont);
        WCHAR name[LF_FACESIZE];
        GetTextFace(hdc, LF_FACESIZE, name);
        familyName = nullTerminatedWCharToString(name);

        if (containsCharacter || currentFontContainsCharacter(hdc, character))
            break;

        if (!linkedFonts)
            linkedFonts = getLinkedFonts(familyName);
        SelectObject(hdc, oldFont);
        DeleteObject(hfont);
        hfont = 0;

        if (linkedFonts->size() <= linkedFontIndex)
            break;

        LOGFONT logFont;
        logFont.lfCharSet = DEFAULT_CHARSET;
        StringView(linkedFonts->at(linkedFontIndex)).getCharactersWithUpconvert(logFont.lfFaceName);
        logFont.lfFaceName[linkedFonts->at(linkedFontIndex).length()] = 0;
        EnumFontFamiliesEx(hdc, &logFont, linkedFontEnumProc, reinterpret_cast<LPARAM>(&hfont), 0);
        linkedFontIndex++;
    }

    if (hfont) {
        if (!familyName.isEmpty()) {
            FontPlatformData* result = getCachedFontPlatformData(description, familyName);
            if (result)
                fontData = fontForPlatformData(*result);
        }

        SelectObject(hdc, oldFont);
        DeleteObject(hfont);
    }

    return fontData;
}

Vector<String> FontCache::systemFontFamilies()
{
    // FIXME: <https://webkit.org/b/147017> Web Inspector: [Win] Allow inspector to retrieve a list of system fonts
    Vector<String> fontFamilies;
    return fontFamilies;
}

RefPtr<Font> FontCache::fontFromDescriptionAndLogFont(const FontDescription& fontDescription, const LOGFONT& font, AtomicString& outFontFamilyName)
{
    AtomicString familyName = wcharToString(font.lfFaceName, wcsnlen(font.lfFaceName, LF_FACESIZE));
    RefPtr<Font> fontData = fontForFamily(fontDescription, familyName);
    if (fontData)
        outFontFamilyName = familyName;
    return fontData;
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    static NeverDestroyed<AtomicString> fallbackFontName;

    if (!fallbackFontName.get().isEmpty())
        return *fontForFamily(fontDescription, fallbackFontName);

    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.

    // Search all typical Windows-installed full Unicode fonts.
    // Sorted by most to least glyphs according to http://en.wikipedia.org/wiki/Unicode_typefaces
    // Start with Times New Roman also since it is the default if the user doesn't change prefs.
    static NeverDestroyed<AtomicString> fallbackFonts[] = {
        AtomicString("Times New Roman", AtomicString::ConstructFromLiteral),
        AtomicString("Microsoft Sans Serif", AtomicString::ConstructFromLiteral),
        AtomicString("Tahoma", AtomicString::ConstructFromLiteral),
        AtomicString("Lucida Sans Unicode", AtomicString::ConstructFromLiteral),
        AtomicString("Arial", AtomicString::ConstructFromLiteral)
    };
    RefPtr<Font> simpleFont;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(fallbackFonts); ++i) {
        if (simpleFont = fontForFamily(fontDescription, fallbackFonts[i])) {
            fallbackFontName.get() = fallbackFonts[i];
            return *simpleFont;
        }
    }

    // Fall back to the DEFAULT_GUI_FONT if no known Unicode fonts are available.
    if (HFONT defaultGUIFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT))) {
        LOGFONT defaultGUILogFont;
        GetObject(defaultGUIFont, sizeof(defaultGUILogFont), &defaultGUILogFont);
        if (simpleFont = fontFromDescriptionAndLogFont(fontDescription, defaultGUILogFont, fallbackFontName))
            return *simpleFont;
    }

    // Fall back to Non-client metrics fonts.
    NONCLIENTMETRICS nonClientMetrics = {0};
    nonClientMetrics.cbSize = sizeof(nonClientMetrics);
    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(nonClientMetrics), &nonClientMetrics, 0)) {
        if (simpleFont = fontFromDescriptionAndLogFont(fontDescription, nonClientMetrics.lfMessageFont, fallbackFontName))
            return *simpleFont;
        if (simpleFont = fontFromDescriptionAndLogFont(fontDescription, nonClientMetrics.lfMenuFont, fallbackFontName))
            return *simpleFont;
        if (simpleFont = fontFromDescriptionAndLogFont(fontDescription, nonClientMetrics.lfStatusFont, fallbackFontName))
            return *simpleFont;
        if (simpleFont = fontFromDescriptionAndLogFont(fontDescription, nonClientMetrics.lfCaptionFont, fallbackFontName))
            return *simpleFont;
        if (simpleFont = fontFromDescriptionAndLogFont(fontDescription, nonClientMetrics.lfSmCaptionFont, fallbackFontName))
            return *simpleFont;
    }
    
    ASSERT_NOT_REACHED();
    return *simpleFont;
}

static LONG toGDIFontWeight(FontSelectionValue fontWeight)
{
    if (fontWeight < FontSelectionValue(150))
        return FW_THIN;
    if (fontWeight < FontSelectionValue(250))
        return FW_EXTRALIGHT;
    if (fontWeight < FontSelectionValue(350))
        return FW_LIGHT;
    if (fontWeight < FontSelectionValue(450))
        return FW_NORMAL;
    if (fontWeight < FontSelectionValue(550))
        return FW_MEDIUM;
    if (fontWeight < FontSelectionValue(650))
        return FW_SEMIBOLD;
    if (fontWeight < FontSelectionValue(750))
        return FW_BOLD;
    if (fontWeight < FontSelectionValue(850))
        return FW_EXTRABOLD;
    return FW_HEAVY;
}

static inline bool isGDIFontWeightBold(LONG gdiFontWeight)
{
    return gdiFontWeight >= FW_SEMIBOLD;
}

static LONG adjustedGDIFontWeight(LONG gdiFontWeight, const String& family)
{
    if (equalLettersIgnoringASCIICase(family, "lucida grande")) {
        if (gdiFontWeight == FW_NORMAL)
            return FW_MEDIUM;
        if (gdiFontWeight == FW_BOLD)
            return FW_SEMIBOLD;
    }
    return gdiFontWeight;
}

struct MatchImprovingProcData {
    MatchImprovingProcData(LONG desiredWeight, bool desiredItalic)
        : m_desiredWeight(desiredWeight)
        , m_desiredItalic(desiredItalic)
        , m_hasMatched(false)
    {
    }

    LONG m_desiredWeight;
    bool m_desiredItalic;
    bool m_hasMatched;
    LOGFONT m_chosen;
};

static int CALLBACK matchImprovingEnumProc(CONST LOGFONT* candidate, CONST TEXTMETRIC* metrics, DWORD fontType, LPARAM lParam)
{
    MatchImprovingProcData* matchData = reinterpret_cast<MatchImprovingProcData*>(lParam);

    if (!matchData->m_hasMatched) {
        matchData->m_hasMatched = true;
        matchData->m_chosen = *candidate;
        return 1;
    }

    if (!candidate->lfItalic != !matchData->m_chosen.lfItalic) {
        if (!candidate->lfItalic == !matchData->m_desiredItalic)
            matchData->m_chosen = *candidate;

        return 1;
    }

    unsigned chosenWeightDeltaMagnitude = abs(matchData->m_chosen.lfWeight - matchData->m_desiredWeight);
    unsigned candidateWeightDeltaMagnitude = abs(candidate->lfWeight - matchData->m_desiredWeight);

    // If both are the same distance from the desired weight, prefer the candidate if it is further from regular.
    if (chosenWeightDeltaMagnitude == candidateWeightDeltaMagnitude && abs(candidate->lfWeight - FW_NORMAL) > abs(matchData->m_chosen.lfWeight - FW_NORMAL)) {
        matchData->m_chosen = *candidate;
        return 1;
    }

    // Otherwise, prefer the one closer to the desired weight.
    if (candidateWeightDeltaMagnitude < chosenWeightDeltaMagnitude)
        matchData->m_chosen = *candidate;

    return 1;
}

static GDIObject<HFONT> createGDIFont(const AtomicString& family, LONG desiredWeight, bool desiredItalic, int size, bool synthesizeItalic)
{
    HWndDC hdc(0);

    LOGFONT logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    StringView truncatedFamily = StringView(family).substring(0, static_cast<unsigned>(LF_FACESIZE - 1));
    truncatedFamily.getCharactersWithUpconvert(logFont.lfFaceName);
    logFont.lfFaceName[truncatedFamily.length()] = 0;
    logFont.lfPitchAndFamily = 0;

    MatchImprovingProcData matchData(desiredWeight, desiredItalic);
    EnumFontFamiliesEx(hdc, &logFont, matchImprovingEnumProc, reinterpret_cast<LPARAM>(&matchData), 0);

    if (!matchData.m_hasMatched)
        return nullptr;

    matchData.m_chosen.lfHeight = -size;
    matchData.m_chosen.lfWidth = 0;
    matchData.m_chosen.lfEscapement = 0;
    matchData.m_chosen.lfOrientation = 0;
    matchData.m_chosen.lfUnderline = false;
    matchData.m_chosen.lfStrikeOut = false;
    matchData.m_chosen.lfCharSet = DEFAULT_CHARSET;
#if USE(CG) || USE(CAIRO)
    matchData.m_chosen.lfOutPrecision = OUT_TT_ONLY_PRECIS;
#else
    matchData.m_chosen.lfOutPrecision = OUT_TT_PRECIS;
#endif
    matchData.m_chosen.lfQuality = DEFAULT_QUALITY;
    matchData.m_chosen.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

#if USE(CAIRO)
    if (isGDIFontWeightBold(desiredWeight) && !isGDIFontWeightBold(matchData.m_chosen.lfWeight))
        matchData.m_chosen.lfWeight = desiredWeight;
#endif

    if (desiredItalic && !matchData.m_chosen.lfItalic && synthesizeItalic)
        matchData.m_chosen.lfItalic = 1;

    auto chosenFont = adoptGDIObject(::CreateFontIndirect(&matchData.m_chosen));
    if (!chosenFont)
        return nullptr;

    HWndDC dc(0);
    SaveDC(dc);
    SelectObject(dc, chosenFont.get());
    WCHAR actualName[LF_FACESIZE];
    GetTextFace(dc, LF_FACESIZE, actualName);
    RestoreDC(dc, -1);

    if (wcsicmp(matchData.m_chosen.lfFaceName, actualName))
        return nullptr;

    return chosenFont;
}

struct TraitsInFamilyProcData {
    TraitsInFamilyProcData(const AtomicString& familyName)
        : m_familyName(familyName)
    {
    }

    const AtomicString& m_familyName;
    Vector<FontSelectionCapabilities> m_capabilities;
};

static int CALLBACK traitsInFamilyEnumProc(CONST LOGFONT* logFont, CONST TEXTMETRIC* metrics, DWORD fontType, LPARAM lParam)
{
    TraitsInFamilyProcData* procData = reinterpret_cast<TraitsInFamilyProcData*>(lParam);

    FontSelectionValue italic = logFont->lfItalic ? italicThreshold() : FontSelectionValue();

    FontSelectionValue weight;
    switch (adjustedGDIFontWeight(logFont->lfWeight, procData->m_familyName)) {
    case FW_THIN:
        weight = FontSelectionValue(100);
        break;
    case FW_EXTRALIGHT:
        weight = FontSelectionValue(200);
        break;
    case FW_LIGHT:
        weight = FontSelectionValue(300);
        break;
    case FW_NORMAL:
        weight = FontSelectionValue(400);
        break;
    case FW_MEDIUM:
        weight = FontSelectionValue(500);
        break;
    case FW_SEMIBOLD:
        weight = FontSelectionValue(600);
        break;
    case FW_BOLD:
        weight = FontSelectionValue(700);
        break;
    case FW_EXTRABOLD:
        weight = FontSelectionValue(800);
        break;
    default:
        weight = FontSelectionValue(900);
        break;
    }

    FontSelectionValue stretch = normalStretchValue();

    FontSelectionCapabilities result;
    result.weight = FontSelectionRange(weight, weight);
    result.width = FontSelectionRange(stretch, stretch);
    result.slope = FontSelectionRange(italic, italic);
    procData->m_capabilities.append(WTFMove(result));
    return 1;
}

Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomicString& familyName, AllowUserInstalledFonts)
{
    HWndDC hdc(0);

    LOGFONT logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    StringView truncatedFamily = StringView(familyName).substring(0, static_cast<unsigned>(LF_FACESIZE - 1));
    truncatedFamily.getCharactersWithUpconvert(logFont.lfFaceName);
    logFont.lfFaceName[truncatedFamily.length()] = 0;
    logFont.lfPitchAndFamily = 0;

    TraitsInFamilyProcData procData(familyName);
    EnumFontFamiliesEx(hdc, &logFont, traitsInFamilyEnumProc, reinterpret_cast<LPARAM>(&procData), 0);
    Vector<FontSelectionCapabilities> result;
    result.reserveInitialCapacity(procData.m_capabilities.size());
    for (auto capabilities : procData.m_capabilities)
        result.uncheckedAppend(capabilities);
    return result;
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family, const FontFeatureSettings*, const FontVariantSettings*, FontSelectionSpecifiedCapabilities)
{
    bool isLucidaGrande = equalLettersIgnoringASCIICase(family, "lucida grande");

    bool useGDI = fontDescription.renderingMode() == FontRenderingMode::Alternate && !isLucidaGrande;

    // The logical size constant is 32. We do this for subpixel precision when rendering using Uniscribe.
    // This masks rounding errors related to the HFONT metrics being  different from the CGFont metrics.
    // FIXME: We will eventually want subpixel precision for GDI mode, but the scaled rendering doesn't
    // look as nice. That may be solvable though.
    LONG weight = adjustedGDIFontWeight(toGDIFontWeight(fontDescription.weight()), family);
    auto hfont = createGDIFont(family, weight, isItalic(fontDescription.italic()),
        fontDescription.computedPixelSize() * (useGDI ? 1 : 32), useGDI);

    if (!hfont)
        return nullptr;

    if (isLucidaGrande)
        useGDI = false; // Never use GDI for Lucida Grande.

    LOGFONT logFont;
    GetObject(hfont.get(), sizeof(LOGFONT), &logFont);

    bool synthesizeBold = isGDIFontWeightBold(weight) && !isGDIFontWeightBold(logFont.lfWeight);
    bool synthesizeItalic = isItalic(fontDescription.italic()) && !logFont.lfItalic;

    auto result = std::make_unique<FontPlatformData>(WTFMove(hfont), fontDescription.computedPixelSize(), synthesizeBold, synthesizeItalic, useGDI);

#if USE(CG)
    bool fontCreationFailed = !result->cgFont();
#elif USE(DIRECT2D)
    bool fontCreationFailed = !result->dwFont();
#elif USE(CAIRO)
    bool fontCreationFailed = !result->scaledFont();
#endif

    if (fontCreationFailed) {
        // The creation of the CGFontRef failed for some reason.  We already asserted in debug builds, but to make
        // absolutely sure that we don't use this font, go ahead and return 0 so that we can fall back to the next
        // font.
        return nullptr;
    }

    return result;
}

const AtomicString& FontCache::platformAlternateFamilyName(const AtomicString& familyName)
{
    static NeverDestroyed<AtomicString> timesNewRoman("Times New Roman", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> microsoftSansSerif("Microsoft Sans Serif", AtomicString::ConstructFromLiteral);

    switch (familyName.length()) {
    // On Windows, we don't support bitmap fonts, but legacy content expects support.
    // Thus we allow Times New Roman as an alternative for the bitmap font MS Serif,
    // even if the webpage does not specify fallback.
    // FIXME: Seems unlikely this is still needed. If it was really needed, I think we
    // would need it on other platforms too.
    case 8:
        if (equalLettersIgnoringASCIICase(familyName, "ms serif"))
            return timesNewRoman;
        break;
    // On Windows, we don't support bitmap fonts, but legacy content expects support.
    // Thus we allow Microsoft Sans Serif as an alternative for the bitmap font MS Sans Serif,
    // even if the webpage does not specify fallback.
    // FIXME: Seems unlikely this is still needed. If it was really needed, I think we
    // would need it on other platforms too.
    case 13:
        if (equalLettersIgnoringASCIICase(familyName, "ms sans serif"))
            return microsoftSansSerif;
        break;
    }
    return nullAtom();
}

}
