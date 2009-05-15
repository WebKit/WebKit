/*
 * Copyright (c) 2006, 2007, 2008, Google Inc. All rights reserved.
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

#include "PlatformString.h"
#include "StringHash.h"
#include "UniscribeHelper.h"
#include <unicode/locid.h>
#include <unicode/uchar.h>
#include <wtf/HashMap.h>

namespace WebCore {

namespace {

// A simple mapping from UScriptCode to family name.  This is a sparse array,
// which works well since the range of UScriptCode values is small.
typedef const UChar* ScriptToFontMap[USCRIPT_CODE_LIMIT];

void initializeScriptFontMap(ScriptToFontMap& scriptFontMap)
{
    struct FontMap {
        UScriptCode script;
        const UChar* family;
    };

    const static FontMap fontMap[] = {
        {USCRIPT_LATIN, L"times new roman"},
        {USCRIPT_GREEK, L"times new roman"},
        {USCRIPT_CYRILLIC, L"times new roman"},
        {USCRIPT_SIMPLIFIED_HAN, L"simsun"},
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
        {USCRIPT_ORIYA, L"kalinga"},
        {USCRIPT_TAMIL, L"latha"},
        {USCRIPT_TELUGU, L"gautami"},
        {USCRIPT_KANNADA, L"tunga"},
        {USCRIPT_MALAYALAM, L"kartika"},
        {USCRIPT_LAO, L"dokchampa"},
        {USCRIPT_TIBETAN, L"microsoft himalaya"},
        {USCRIPT_GEORGIAN, L"sylfaen"},
        {USCRIPT_ARMENIAN, L"sylfaen"},
        {USCRIPT_ETHIOPIC, L"nyala"},
        {USCRIPT_CANADIAN_ABORIGINAL, L"euphemia"},
        {USCRIPT_CHEROKEE, L"plantagenet cherokee"},
        {USCRIPT_YI, L"microsoft yi balti"},
        {USCRIPT_SINHALA, L"iskoola pota"},
        {USCRIPT_SYRIAC, L"estrangelo edessa"},
        {USCRIPT_KHMER, L"daunpenh"},
        {USCRIPT_THAANA, L"mv boli"},
        {USCRIPT_MONGOLIAN, L"mongolian balti"},
        {USCRIPT_MYANMAR, L"padauk"},
        // For USCRIPT_COMMON, we map blocks to scripts when
        // that makes sense.
    };
    
    for (int i = 0; i < sizeof(fontMap) / sizeof(fontMap[0]); ++i)
        scriptFontMap[fontMap[i].script] = fontMap[i].family;

    // Initialize the locale-dependent mapping.
    // Since Chrome synchronizes the ICU default locale with its UI locale,
    // this ICU locale tells the current UI locale of Chrome.
    Locale locale = Locale::getDefault();
    const UChar* localeFamily = 0;
    if (locale == Locale::getJapanese())
        localeFamily = scriptFontMap[USCRIPT_HIRAGANA];
    else if (locale == Locale::getKorean())
        localeFamily = scriptFontMap[USCRIPT_HANGUL];
    else {
        // Use Simplified Chinese font for all other locales including
        // Traditional Chinese because Simsun (SC font) has a wider
        // coverage (covering both SC and TC) than PMingLiu (TC font).
        // Note that |fontMap| does not have a separate entry for
        // USCRIPT_TRADITIONAL_HAN for that reason.
        // This also speeds up the TC version of Chrome when rendering SC
        // pages.
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
    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, hfont);
    TEXTMETRIC tm;
    BOOL gotMetrics = GetTextMetrics(dc, &tm);
    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
    return gotMetrics ? tm.tmAscent : kUndefinedAscent;
}

struct FontData {
    FontData()
        : hfont(0)
        , ascent(kUndefinedAscent)
        , scriptCache(0)
    {
    }

    HFONT hfont;
    int ascent;
    mutable SCRIPT_CACHE scriptCache;
};

// Again, using hash_map does not earn us much here.  page_cycler_test intl2
// gave us a 'better' result with map than with hash_map even though they're
// well-within 1-sigma of each other so that the difference is not significant.
// On the other hand, some pages in intl2 seem to take longer to load with map
// in the 1st pass. Need to experiment further.
typedef HashMap<String, FontData> FontDataCache;

}  // namespace

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

    // Another lame work-around to cover non-BMP characters.
    const UChar* family = getFontFamilyForScript(script, generic);
    if (!family) {
        int plane = ucs4 >> 16;
        switch (plane) {
        case 1:
            family = L"code2001";
            break;
        case 2:
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
                        SCRIPT_CACHE** scriptCache)
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

}  // namespace WebCore
