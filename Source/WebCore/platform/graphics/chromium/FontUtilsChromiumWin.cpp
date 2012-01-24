/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010, 2012 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontUtilsChromiumWin.h"

#include <limits>

#include "HWndDC.h"
#include "PlatformString.h"
#include "UniscribeHelper.h"
#include <unicode/locid.h>
#include <unicode/uchar.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

namespace {

bool isFontPresent(const UChar* fontName)
{
    HFONT hfont = CreateFont(12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                             fontName);
    if (!hfont)
        return false;
    HWndDC dc(0);
    HGDIOBJ oldFont = static_cast<HFONT>(SelectObject(dc, hfont));
    WCHAR actualFontName[LF_FACESIZE];
    GetTextFace(dc, LF_FACESIZE, actualFontName);
    actualFontName[LF_FACESIZE - 1] = 0;
    SelectObject(dc, oldFont);
    DeleteObject(hfont);
    // We don't have to worry about East Asian fonts with locale-dependent
    // names here for now.
    return !wcscmp(fontName, actualFontName);
}

// A simple mapping from UScriptCode to family name.  This is a sparse array,
// which works well since the range of UScriptCode values is small.
typedef const UChar* ScriptToFontMap[USCRIPT_CODE_LIMIT];

void initializeScriptFontMap(ScriptToFontMap& scriptFontMap)
{
    struct FontMap {
        UScriptCode script;
        const UChar* family;
    };

    static const FontMap fontMap[] = {
        {USCRIPT_LATIN, L"times new roman"},
        {USCRIPT_GREEK, L"times new roman"},
        {USCRIPT_CYRILLIC, L"times new roman"},
        // FIXME: Consider trying new Vista fonts before XP fonts for CJK.
        // Some Vista users do want to use Vista cleartype CJK fonts. If we
        // did, the results of tests with CJK characters would have to be
        // regenerated for Vista.
        {USCRIPT_SIMPLIFIED_HAN, L"simsun"},
        {USCRIPT_TRADITIONAL_HAN, L"pmingliu"},
        {USCRIPT_HIRAGANA, L"ms pgothic"},
        {USCRIPT_KATAKANA, L"ms pgothic"},
        {USCRIPT_KATAKANA_OR_HIRAGANA, L"ms pgothic"},
        {USCRIPT_HANGUL, L"gulim"},
        {USCRIPT_THAI, L"tahoma"},
        {USCRIPT_HEBREW, L"david"},
        {USCRIPT_ARABIC, L"tahoma"},
        {USCRIPT_DEVANAGARI, L"mangal"},
        {USCRIPT_BENGALI, L"vrinda"},
        {USCRIPT_GURMUKHI, L"raavi"},
        {USCRIPT_GUJARATI, L"shruti"},
        {USCRIPT_TAMIL, L"latha"},
        {USCRIPT_TELUGU, L"gautami"},
        {USCRIPT_KANNADA, L"tunga"},
        {USCRIPT_GEORGIAN, L"sylfaen"},
        {USCRIPT_ARMENIAN, L"sylfaen"},
        {USCRIPT_THAANA, L"mv boli"},
        {USCRIPT_CANADIAN_ABORIGINAL, L"euphemia"},
        {USCRIPT_CHEROKEE, L"plantagenet cherokee"},
        {USCRIPT_MONGOLIAN, L"mongolian balti"},
        // For USCRIPT_COMMON, we map blocks to scripts when
        // that makes sense.
    };

    struct ScriptToFontFamilies {
        UScriptCode script;
        const UChar** families;
    };

    // Kartika on Vista or earlier lacks the support for Chillu 
    // letters added to Unicode 5.1.
    // Try AnjaliOldLipi (a very widely used Malaylalam font with the full
    // Unicode 5.x support) before falling back to Kartika.
    static const UChar* malayalamFonts[] = {L"AnjaliOldLipi", L"Lohit Malayalam", L"Kartika", L"Rachana", 0};
    // Try Khmer OS before Vista fonts because 'Khmer OS' goes along better
    // with Latin and looks better/larger for the same size.
    static const UChar* khmerFonts[] = {L"Khmer OS", L"MoolBoran", L"DaunPenh", L"Code2000", 0};
    // For the following 6 scripts, two or fonts are listed. The fonts in 
    // the 1st slot are not available on Windows XP. To support these
    // scripts on XP, listed in the rest of slots are widely used
    // fonts.
    static const UChar* ethiopicFonts[] = {L"Nyala", L"Abyssinica SIL", L"Ethiopia Jiret", L"Visual Geez Unicode", L"GF Zemen Unicode", 0};
    static const UChar* oriyaFonts[] = {L"Kalinga", L"ori1Uni", L"Lohit Oriya", 0};
    static const UChar* laoFonts[] = {L"DokChampa", L"Saysettha OT", L"Phetsarath OT", L"Code2000", 0};
    static const UChar* tibetanFonts[] = {L"Microsoft Himalaya", L"Jomolhari", L"Tibetan Machine Uni", 0};
    static const UChar* sinhalaFonts[] = {L"Iskoola Pota", L"AksharUnicode", 0};
    static const UChar* yiFonts[] = {L"Microsoft Yi Balti", L"Nuosu SIL", L"Code2000", 0};
    // http://www.bethmardutho.org/support/meltho/download/index.php
    static const UChar* syriacFonts[] = {L"Estrangelo Edessa", L"Estrangelo Nisibin", L"Code2000", 0};
    // No Myanmar/Burmese font is shipped with Windows, yet. Try a few
    // widely available/used ones that supports Unicode 5.1 or later. 
    static const UChar* myanmarFonts[] = {L"Padauk", L"Parabaik", L"Myanmar3", L"Code2000", 0};

    static const ScriptToFontFamilies scriptToFontFamilies[] = {
        {USCRIPT_MALAYALAM, malayalamFonts},
        {USCRIPT_KHMER, khmerFonts},
        {USCRIPT_ETHIOPIC, ethiopicFonts},
        {USCRIPT_ORIYA, oriyaFonts},
        {USCRIPT_LAO, laoFonts},
        {USCRIPT_TIBETAN, tibetanFonts},
        {USCRIPT_SINHALA, sinhalaFonts},
        {USCRIPT_YI, yiFonts},
        {USCRIPT_SYRIAC, syriacFonts},
        {USCRIPT_MYANMAR, myanmarFonts},
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(fontMap); ++i)
        scriptFontMap[fontMap[i].script] = fontMap[i].family;

    // FIXME: Instead of scanning the hard-coded list, we have to 
    // use EnumFont* to 'inspect' fonts to pick up fonts covering scripts
    // when it's possible (e.g. using OS/2 table). If we do that, this 
    // had better be pulled out of here.
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(scriptToFontFamilies); ++i) {
        UScriptCode script = scriptToFontFamilies[i].script;
        scriptFontMap[script] = 0;
        const UChar** familyPtr = scriptToFontFamilies[i].families;
        while (*familyPtr) {
            if (isFontPresent(*familyPtr)) {
                scriptFontMap[script] = *familyPtr;
                break;
            }
            ++familyPtr;
        }
    }

    // Initialize the locale-dependent mapping.
    // Since Chrome synchronizes the ICU default locale with its UI locale,
    // this ICU locale tells the current UI locale of Chrome.
    icu::Locale locale = icu::Locale::getDefault();
    const UChar* localeFamily = 0;
    if (locale == icu::Locale::getJapanese())
        localeFamily = scriptFontMap[USCRIPT_HIRAGANA];
    else if (locale == icu::Locale::getKorean())
        localeFamily = scriptFontMap[USCRIPT_HANGUL];
    else if (locale == icu::Locale::getTraditionalChinese())
        localeFamily = scriptFontMap[USCRIPT_TRADITIONAL_HAN];
    else {
        // For other locales, use the simplified Chinese font for Han.
        localeFamily = scriptFontMap[USCRIPT_SIMPLIFIED_HAN];
    }
    if (localeFamily)
        scriptFontMap[USCRIPT_HAN] = localeFamily;
}

// There are a lot of characters in USCRIPT_COMMON that can be covered
// by fonts for scripts closely related to them. See
// http://unicode.org/cldr/utility/list-unicodeset.jsp?a=[:Script=Common:]
// FIXME: make this more efficient with a wider coverage
UScriptCode getScriptBasedOnUnicodeBlock(int ucs4)
{
    UBlockCode block = ublock_getCode(ucs4);
    switch (block) {
    case UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION:
        return USCRIPT_HAN;
    case UBLOCK_HIRAGANA:
    case UBLOCK_KATAKANA:
        return USCRIPT_HIRAGANA;
    case UBLOCK_ARABIC:
        return USCRIPT_ARABIC;
    case UBLOCK_THAI:
        return USCRIPT_THAI;
    case UBLOCK_GREEK:
        return USCRIPT_GREEK;
    case UBLOCK_DEVANAGARI:
        // For Danda and Double Danda (U+0964, U+0965), use a Devanagari
        // font for now although they're used by other scripts as well.
        // Without a context, we can't do any better.
        return USCRIPT_DEVANAGARI;
    case UBLOCK_ARMENIAN:
        return USCRIPT_ARMENIAN;
    case UBLOCK_GEORGIAN:
        return USCRIPT_GEORGIAN;
    case UBLOCK_KANNADA:
        return USCRIPT_KANNADA;
    default:
        return USCRIPT_COMMON;
    }
}

UScriptCode getScript(int ucs4)
{
    UErrorCode err = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(ucs4, &err);
    // If script is invalid, common or inherited or there's an error,
    // infer a script based on the unicode block of a character.
    if (script <= USCRIPT_INHERITED || U_FAILURE(err))
        script = getScriptBasedOnUnicodeBlock(ucs4);
    return script;
}

const int kUndefinedAscent = std::numeric_limits<int>::min();

// Given an HFONT, return the ascent. If GetTextMetrics fails,
// kUndefinedAscent is returned, instead.
int getAscent(HFONT hfont)
{
    HWndDC dc(0);
    HGDIOBJ oldFont = SelectObject(dc, hfont);
    TEXTMETRIC tm;
    BOOL gotMetrics = GetTextMetrics(dc, &tm);
    SelectObject(dc, oldFont);
    return gotMetrics ? tm.tmAscent : kUndefinedAscent;
}

WORD getSpaceGlyph(HFONT hfont) 
{
    HWndDC dc(0);
    HGDIOBJ oldFont = SelectObject(dc, hfont);
    WCHAR space = L' ';
    WORD spaceGlyph = 0;
    GetGlyphIndices(dc, &space, 1, &spaceGlyph, 0);
    SelectObject(dc, oldFont);
    return spaceGlyph;
}

struct FontData {
    FontData()
        : hfont(0)
        , ascent(kUndefinedAscent)
        , scriptCache(0)
        , spaceGlyph(0)
    {
    }

    HFONT hfont;
    int ascent;
    mutable SCRIPT_CACHE scriptCache;
    WORD spaceGlyph;
};

// Again, using hash_map does not earn us much here.  page_cycler_test intl2
// gave us a 'better' result with map than with hash_map even though they're
// well-within 1-sigma of each other so that the difference is not significant.
// On the other hand, some pages in intl2 seem to take longer to load with map
// in the 1st pass. Need to experiment further.
typedef HashMap<String, FontData> FontDataCache;

} // namespace

// FIXME: this is font fallback code version 0.1
//  - Cover all the scripts
//  - Get the default font for each script/generic family from the
//    preference instead of hardcoding in the source.
//    (at least, read values from the registry for IE font settings).
//  - Support generic families (from FontDescription)
//  - If the default font for a script is not available,
//    try some more fonts known to support it. Finally, we can
//    use EnumFontFamilies or similar APIs to come up with a list of
//    fonts supporting the script and cache the result.
//  - Consider using UnicodeSet (or UnicodeMap) converted from
//    GLYPHSET (BMP) or directly read from truetype cmap tables to
//    keep track of which character is supported by which font
//  - Update script_font_cache in response to WM_FONTCHANGE

const UChar* getFontFamilyForScript(UScriptCode script,
                                    FontDescription::GenericFamilyType generic)
{
    static ScriptToFontMap scriptFontMap;
    static bool initialized = false;
    if (!initialized) {
        initializeScriptFontMap(scriptFontMap);
        initialized = true;
    }
    if (script == USCRIPT_INVALID_CODE)
        return 0;
    ASSERT(script < USCRIPT_CODE_LIMIT);
    return scriptFontMap[script];
}

// FIXME:
//  - Handle 'Inherited', 'Common' and 'Unknown'
//    (see http://www.unicode.org/reports/tr24/#Usage_Model )
//    For 'Inherited' and 'Common', perhaps we need to
//    accept another parameter indicating the previous family
//    and just return it.
//  - All the characters (or characters up to the point a single
//    font can cover) need to be taken into account
const UChar* getFallbackFamily(const UChar* characters,
                               int length,
                               FontDescription::GenericFamilyType generic,
                               UChar32* charChecked,
                               UScriptCode* scriptChecked)
{
    ASSERT(characters && characters[0] && length > 0);
    UScriptCode script = USCRIPT_COMMON;

    // Sometimes characters common to script (e.g. space) is at
    // the beginning of a string so that we need to skip them
    // to get a font required to render the string.
    int i = 0;
    UChar32 ucs4 = 0;
    while (i < length && script == USCRIPT_COMMON) {
        U16_NEXT(characters, i, length, ucs4);
        script = getScript(ucs4);
    }

    // For the full-width ASCII characters (U+FF00 - U+FF5E), use the font for
    // Han (determined in a locale-dependent way above). Full-width ASCII
    // characters are rather widely used in Japanese and Chinese documents and
    // they're fully covered by Chinese, Japanese and Korean fonts.
    if (0xFF00 < ucs4 && ucs4 < 0xFF5F)
        script = USCRIPT_HAN;

    if (script == USCRIPT_COMMON)
        script = getScriptBasedOnUnicodeBlock(ucs4);

    const UChar* family = getFontFamilyForScript(script, generic);
    // Another lame work-around to cover non-BMP characters.
    // If the font family for script is not found or the character is
    // not in BMP (> U+FFFF), we resort to the hard-coded list of
    // fallback fonts for now.
    if (!family || ucs4 > 0xFFFF) {
        int plane = ucs4 >> 16;
        switch (plane) {
        case 1:
            family = L"code2001";
            break;
        case 2:
            // Use a Traditional Chinese ExtB font if in Traditional Chinese locale.
            // Otherwise, use a Simplified Chinese ExtB font. Windows Japanese
            // fonts do support a small subset of ExtB (that are included in JIS X 0213),
            // but its coverage is rather sparse.
            // Eventually, this should be controlled by lang/xml:lang.
            if (icu::Locale::getDefault() == icu::Locale::getTraditionalChinese())
              family = L"pmingliu-extb";
            else
              family = L"simsun-extb";
            break;
        default:
            family = L"lucida sans unicode";
        }
    }

    if (charChecked)
        *charChecked = ucs4;
    if (scriptChecked)
        *scriptChecked = script;
    return family;
}

// Be aware that this is not thread-safe.
bool getDerivedFontData(const UChar* family,
                        int style,
                        LOGFONT* logfont,
                        int* ascent,
                        HFONT* hfont,
                        SCRIPT_CACHE** scriptCache,
                        WORD* spaceGlyph)
{
    ASSERT(logfont);
    ASSERT(family);
    ASSERT(*family);

    // It does not matter that we leak font data when we exit.
    static FontDataCache fontDataCache;

    // FIXME: This comes up pretty high in the profile so that
    // we need to measure whether using SHA256 (after coercing all the
    // fields to char*) is faster than String::format.
    String fontKey = String::format("%1d:%d:%ls", style, logfont->lfHeight, family);
    FontDataCache::iterator iter = fontDataCache.find(fontKey);
    FontData* derived;
    if (iter == fontDataCache.end()) {
        ASSERT(wcslen(family) < LF_FACESIZE);
        wcscpy_s(logfont->lfFaceName, LF_FACESIZE, family);
        // FIXME: CreateFontIndirect always comes up with
        // a font even if there's no font matching the name. Need to
        // check it against what we actually want (as is done in
        // FontCacheWin.cpp)
        pair<FontDataCache::iterator, bool> entry = fontDataCache.add(fontKey, FontData());
        derived = &entry.first->second;
        derived->hfont = CreateFontIndirect(logfont);
        // GetAscent may return kUndefinedAscent, but we still want to
        // cache it so that we won't have to call CreateFontIndirect once
        // more for HFONT next time.
        derived->ascent = getAscent(derived->hfont);
        derived->spaceGlyph = getSpaceGlyph(derived->hfont);
    } else {
        derived = &iter->second;
        // Last time, GetAscent failed so that only HFONT was
        // cached. Try once more assuming that TryPreloadFont
        // was called by a caller between calls.
        if (kUndefinedAscent == derived->ascent)
            derived->ascent = getAscent(derived->hfont);
    }
    *hfont = derived->hfont;
    *ascent = derived->ascent;
    *scriptCache = &(derived->scriptCache);
    *spaceGlyph = derived->spaceGlyph;
    return *ascent != kUndefinedAscent;
}

int getStyleFromLogfont(const LOGFONT* logfont)
{
    // FIXME: consider defining UNDEFINED or INVALID for style and
    //                  returning it when logfont is 0
    if (!logfont) {
        ASSERT_NOT_REACHED();
        return FontStyleNormal;
    }
    return (logfont->lfItalic ? FontStyleItalic : FontStyleNormal) |
           (logfont->lfUnderline ? FontStyleUnderlined : FontStyleNormal) |
           (logfont->lfWeight >= 700 ? FontStyleBold : FontStyleNormal);
}

} // namespace WebCore
