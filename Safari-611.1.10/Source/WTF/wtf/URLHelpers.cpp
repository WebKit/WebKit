/*
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Igalia S.L.
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
#include "URLHelpers.h"

#include "URLParser.h"
#include <mutex>
#include <unicode/uidna.h>
#include <unicode/uscript.h>
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WTF {
namespace URLHelpers {

// Needs to be big enough to hold an IDN-encoded name.
// For host names bigger than this, we won't do IDN encoding, which is almost certainly OK.
constexpr unsigned hostNameBufferLength = 2048;
constexpr unsigned urlBytesBufferLength = 2048;

// This needs to be higher than the UScriptCode for any of the scripts on the IDN allowed list.
// At one point we used USCRIPT_CODE_LIMIT from ICU, but there are two reasons not to use it.
// 1) ICU considers it deprecated, so by setting U_HIDE_DEPRECATED we can't see it.
// 2) No good reason to limit ourselves to scripts that existed in the ICU headers when
//    WebKit was compiled.
// This is only really important for platforms that load an external IDN allowed script list.
// Not important for the compiled-in one.
constexpr auto scriptCodeLimit = static_cast<UScriptCode>(256);

static uint32_t allowedIDNScriptBits[(scriptCodeLimit + 31) / 32];

#if !PLATFORM(COCOA)

// Cocoa has an implementation that reads this from a file in /Library or ~/Library.
void loadIDNAllowedScriptList()
{
    static std::once_flag flag;
    std::call_once(flag, initializeDefaultIDNAllowedScriptList);
}

#endif // !PLATFORM(COCOA)

static bool isArmenianLookalikeCharacter(UChar32 codePoint)
{
    return codePoint == 0x0548 || codePoint == 0x054D || codePoint == 0x0578 || codePoint == 0x057D;
}

static bool isArmenianScriptCharacter(UChar32 codePoint)
{
    UErrorCode error = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(codePoint, &error);
    if (error != U_ZERO_ERROR) {
        LOG_ERROR("got ICU error while trying to look at scripts: %d", error);
        return false;
    }

    return script == USCRIPT_ARMENIAN;
}

template<typename CharacterType> inline bool isASCIIDigitOrValidHostCharacter(CharacterType charCode)
{
    if (!isASCIIDigitOrPunctuation(charCode))
        return false;

    // Things the URL Parser rejects:
    switch (charCode) {
    case '#':
    case '%':
    case '/':
    case ':':
    case '?':
    case '@':
    case '[':
    case '\\':
    case ']':
        return false;
    default:
        return true;
    }
}

static bool isLookalikeCharacter(const Optional<UChar32>& previousCodePoint, UChar32 charCode)
{
    // This function treats the following as unsafe, lookalike characters:
    // any non-printable character, any character considered as whitespace,
    // any ignorable character, and emoji characters related to locks.
    
    // We also considered the characters in Mozilla's list of characters <http://kb.mozillazine.org/Network.IDN.blacklist_chars>.

    // Some of the characters here will never appear once ICU has encoded.
    // For example, ICU transforms most spaces into an ASCII space and most
    // slashes into an ASCII solidus. But one of the two callers uses this
    // on characters that have not been processed by ICU, so they are needed here.
    
    if (!u_isprint(charCode) || u_isUWhiteSpace(charCode) || u_hasBinaryProperty(charCode, UCHAR_DEFAULT_IGNORABLE_CODE_POINT))
        return true;
    
    switch (charCode) {
    case 0x00BC: /* VULGAR FRACTION ONE QUARTER */
    case 0x00BD: /* VULGAR FRACTION ONE HALF */
    case 0x00BE: /* VULGAR FRACTION THREE QUARTERS */
    case 0x00ED: /* LATIN SMALL LETTER I WITH ACUTE */
    /* 0x0131 LATIN SMALL LETTER DOTLESS I is intentionally not considered a lookalike character because it is visually distinguishable from i and it has legitimate use in the Turkish language. */
    case 0x01C0: /* LATIN LETTER DENTAL CLICK */
    case 0x01C3: /* LATIN LETTER RETROFLEX CLICK */
    case 0x0251: /* LATIN SMALL LETTER ALPHA */
    case 0x0261: /* LATIN SMALL LETTER SCRIPT G */
    case 0x027E: /* LATIN SMALL LETTER R WITH FISHHOOK */
    case 0x02D0: /* MODIFIER LETTER TRIANGULAR COLON */
    case 0x0335: /* COMBINING SHORT STROKE OVERLAY */
    case 0x0337: /* COMBINING SHORT SOLIDUS OVERLAY */
    case 0x0338: /* COMBINING LONG SOLIDUS OVERLAY */
    case 0x0589: /* ARMENIAN FULL STOP */
    case 0x05B4: /* HEBREW POINT HIRIQ */
    case 0x05BC: /* HEBREW POINT DAGESH OR MAPIQ */
    case 0x05C3: /* HEBREW PUNCTUATION SOF PASUQ */
    case 0x05F4: /* HEBREW PUNCTUATION GERSHAYIM */
    case 0x0609: /* ARABIC-INDIC PER MILLE SIGN */
    case 0x060A: /* ARABIC-INDIC PER TEN THOUSAND SIGN */
    case 0x0650: /* ARABIC KASRA */
    case 0x0660: /* ARABIC INDIC DIGIT ZERO */
    case 0x066A: /* ARABIC PERCENT SIGN */
    case 0x06D4: /* ARABIC FULL STOP */
    case 0x06F0: /* EXTENDED ARABIC INDIC DIGIT ZERO */
    case 0x0701: /* SYRIAC SUPRALINEAR FULL STOP */
    case 0x0702: /* SYRIAC SUBLINEAR FULL STOP */
    case 0x0703: /* SYRIAC SUPRALINEAR COLON */
    case 0x0704: /* SYRIAC SUBLINEAR COLON */
    case 0x1735: /* PHILIPPINE SINGLE PUNCTUATION */
    case 0x1D04: /* LATIN LETTER SMALL CAPITAL C */
    case 0x1D0F: /* LATIN LETTER SMALL CAPITAL O */
    case 0x1D1C: /* LATIN LETTER SMALL CAPITAL U */
    case 0x1D20: /* LATIN LETTER SMALL CAPITAL V */
    case 0x1D21: /* LATIN LETTER SMALL CAPITAL W */
    case 0x1D22: /* LATIN LETTER SMALL CAPITAL Z */
    case 0x1ECD: /* LATIN SMALL LETTER O WITH DOT BELOW */
    case 0x2010: /* HYPHEN */
    case 0x2011: /* NON-BREAKING HYPHEN */
    case 0x2024: /* ONE DOT LEADER */
    case 0x2027: /* HYPHENATION POINT */
    case 0x2039: /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
    case 0x203A: /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
    case 0x2041: /* CARET INSERTION POINT */
    case 0x2044: /* FRACTION SLASH */
    case 0x2052: /* COMMERCIAL MINUS SIGN */
    case 0x2153: /* VULGAR FRACTION ONE THIRD */
    case 0x2154: /* VULGAR FRACTION TWO THIRDS */
    case 0x2155: /* VULGAR FRACTION ONE FIFTH */
    case 0x2156: /* VULGAR FRACTION TWO FIFTHS */
    case 0x2157: /* VULGAR FRACTION THREE FIFTHS */
    case 0x2158: /* VULGAR FRACTION FOUR FIFTHS */
    case 0x2159: /* VULGAR FRACTION ONE SIXTH */
    case 0x215A: /* VULGAR FRACTION FIVE SIXTHS */
    case 0x215B: /* VULGAR FRACTION ONE EIGHT */
    case 0x215C: /* VULGAR FRACTION THREE EIGHTHS */
    case 0x215D: /* VULGAR FRACTION FIVE EIGHTHS */
    case 0x215E: /* VULGAR FRACTION SEVEN EIGHTHS */
    case 0x215F: /* FRACTION NUMERATOR ONE */
    case 0x2212: /* MINUS SIGN */
    case 0x2215: /* DIVISION SLASH */
    case 0x2216: /* SET MINUS */
    case 0x2236: /* RATIO */
    case 0x233F: /* APL FUNCTIONAL SYMBOL SLASH BAR */
    case 0x23AE: /* INTEGRAL EXTENSION */
    case 0x244A: /* OCR DOUBLE BACKSLASH */
    case 0x2571: /* BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT */
    case 0x2572: /* BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT */
    case 0x29F6: /* SOLIDUS WITH OVERBAR */
    case 0x29F8: /* BIG SOLIDUS */
    case 0x2AFB: /* TRIPLE SOLIDUS BINARY RELATION */
    case 0x2AFD: /* DOUBLE SOLIDUS OPERATOR */
    case 0x2FF0: /* IDEOGRAPHIC DESCRIPTION CHARACTER LEFT TO RIGHT */
    case 0x2FF1: /* IDEOGRAPHIC DESCRIPTION CHARACTER ABOVE TO BELOW */
    case 0x2FF2: /* IDEOGRAPHIC DESCRIPTION CHARACTER LEFT TO MIDDLE AND RIGHT */
    case 0x2FF3: /* IDEOGRAPHIC DESCRIPTION CHARACTER ABOVE TO MIDDLE AND BELOW */
    case 0x2FF4: /* IDEOGRAPHIC DESCRIPTION CHARACTER FULL SURROUND */
    case 0x2FF5: /* IDEOGRAPHIC DESCRIPTION CHARACTER SURROUND FROM ABOVE */
    case 0x2FF6: /* IDEOGRAPHIC DESCRIPTION CHARACTER SURROUND FROM BELOW */
    case 0x2FF7: /* IDEOGRAPHIC DESCRIPTION CHARACTER SURROUND FROM LEFT */
    case 0x2FF8: /* IDEOGRAPHIC DESCRIPTION CHARACTER SURROUND FROM UPPER LEFT */
    case 0x2FF9: /* IDEOGRAPHIC DESCRIPTION CHARACTER SURROUND FROM UPPER RIGHT */
    case 0x2FFA: /* IDEOGRAPHIC DESCRIPTION CHARACTER SURROUND FROM LOWER LEFT */
    case 0x2FFB: /* IDEOGRAPHIC DESCRIPTION CHARACTER OVERLAID */
    case 0x3002: /* IDEOGRAPHIC FULL STOP */
    case 0x3008: /* LEFT ANGLE BRACKET */
    case 0x3014: /* LEFT TORTOISE SHELL BRACKET */
    case 0x3015: /* RIGHT TORTOISE SHELL BRACKET */
    case 0x3033: /* VERTICAL KANA REPEAT MARK UPPER HALF */
    case 0x3035: /* VERTICAL KANA REPEAT MARK LOWER HALF */
    case 0x321D: /* PARENTHESIZED KOREAN CHARACTER OJEON */
    case 0x321E: /* PARENTHESIZED KOREAN CHARACTER O HU */
    case 0x33AE: /* SQUARE RAD OVER S */
    case 0x33AF: /* SQUARE RAD OVER S SQUARED */
    case 0x33C6: /* SQUARE C OVER KG */
    case 0x33DF: /* SQUARE A OVER M */
    case 0x05B9: /* HEBREW POINT HOLAM */
    case 0x05BA: /* HEBREW POINT HOLAM HASER FOR VAV */
    case 0x05C1: /* HEBREW POINT SHIN DOT */
    case 0x05C2: /* HEBREW POINT SIN DOT */
    case 0x05C4: /* HEBREW MARK UPPER DOT */
    case 0xA731: /* LATIN LETTER SMALL CAPITAL S */
    case 0xA771: /* LATIN SMALL LETTER DUM */
    case 0xA789: /* MODIFIER LETTER COLON */
    case 0xFE14: /* PRESENTATION FORM FOR VERTICAL SEMICOLON */
    case 0xFE15: /* PRESENTATION FORM FOR VERTICAL EXCLAMATION MARK */
    case 0xFE3F: /* PRESENTATION FORM FOR VERTICAL LEFT ANGLE BRACKET */
    case 0xFE5D: /* SMALL LEFT TORTOISE SHELL BRACKET */
    case 0xFE5E: /* SMALL RIGHT TORTOISE SHELL BRACKET */
    case 0xFF0E: /* FULLWIDTH FULL STOP */
    case 0xFF0F: /* FULL WIDTH SOLIDUS */
    case 0xFF61: /* HALFWIDTH IDEOGRAPHIC FULL STOP */
    case 0xFFFC: /* OBJECT REPLACEMENT CHARACTER */
    case 0xFFFD: /* REPLACEMENT CHARACTER */
    case 0x1F50F: /* LOCK WITH INK PEN */
    case 0x1F510: /* CLOSED LOCK WITH KEY */
    case 0x1F511: /* KEY */
    case 0x1F512: /* LOCK */
    case 0x1F513: /* OPEN LOCK */
        return true;
    case 0x0307: /* COMBINING DOT ABOVE */
        return previousCodePoint == 0x0237 /* LATIN SMALL LETTER DOTLESS J */
            || previousCodePoint == 0x0131 /* LATIN SMALL LETTER DOTLESS I */
            || previousCodePoint == 0x05D5; /* HEBREW LETTER VAV */
    case 0x0548: /* ARMENIAN CAPITAL LETTER VO */
    case 0x054D: /* ARMENIAN CAPITAL LETTER SEH */
    case 0x0578: /* ARMENIAN SMALL LETTER VO */
    case 0x057D: /* ARMENIAN SMALL LETTER SEH */
        return previousCodePoint
            && !isASCIIDigitOrValidHostCharacter(previousCodePoint.value())
            && !isArmenianScriptCharacter(previousCodePoint.value());
    case '.':
        return false;
    default:
        return previousCodePoint
            && isArmenianLookalikeCharacter(previousCodePoint.value())
            && !(isArmenianScriptCharacter(charCode) || isASCIIDigitOrValidHostCharacter(charCode));
    }
}

static void addScriptToIDNAllowedScriptList(int32_t script)
{
    if (script >= 0 && script < scriptCodeLimit) {
        size_t index = script / 32;
        uint32_t mask = 1 << (script % 32);
        allowedIDNScriptBits[index] |= mask;
    }
}

static void addScriptToIDNAllowedScriptList(UScriptCode script)
{
    addScriptToIDNAllowedScriptList(static_cast<int32_t>(script));
}

void addScriptToIDNAllowedScriptList(const char* scriptName)
{
    addScriptToIDNAllowedScriptList(u_getPropertyValueEnum(UCHAR_SCRIPT, scriptName));
}

void initializeDefaultIDNAllowedScriptList()
{
    constexpr UScriptCode scripts[] = {
        USCRIPT_COMMON,
        USCRIPT_INHERITED,
        USCRIPT_ARABIC,
        USCRIPT_ARMENIAN,
        USCRIPT_BOPOMOFO,
        USCRIPT_CANADIAN_ABORIGINAL,
        USCRIPT_DEVANAGARI,
        USCRIPT_DESERET,
        USCRIPT_GUJARATI,
        USCRIPT_GURMUKHI,
        USCRIPT_HANGUL,
        USCRIPT_HAN,
        USCRIPT_HEBREW,
        USCRIPT_HIRAGANA,
        USCRIPT_KATAKANA_OR_HIRAGANA,
        USCRIPT_KATAKANA,
        USCRIPT_LATIN,
        USCRIPT_TAMIL,
        USCRIPT_THAI,
        USCRIPT_YI,
    };
    for (auto script : scripts)
        addScriptToIDNAllowedScriptList(script);
}

static bool allCharactersInAllowedIDNScriptList(const UChar* buffer, int32_t length)
{
    loadIDNAllowedScriptList();
    int32_t i = 0;
    Optional<UChar32> previousCodePoint;
    while (i < length) {
        UChar32 c;
        U16_NEXT(buffer, i, length, c);
        UErrorCode error = U_ZERO_ERROR;
        UScriptCode script = uscript_getScript(c, &error);
        if (error != U_ZERO_ERROR) {
            LOG_ERROR("got ICU error while trying to look at scripts: %d", error);
            return false;
        }
        if (script < 0) {
            LOG_ERROR("got negative number for script code from ICU: %d", script);
            return false;
        }
        if (script >= scriptCodeLimit)
            return false;

        size_t index = script / 32;
        uint32_t mask = 1 << (script % 32);
        if (!(allowedIDNScriptBits[index] & mask))
            return false;
        
        if (isLookalikeCharacter(previousCodePoint, c))
            return false;
        previousCodePoint = c;
    }
    return true;
}

template<typename Func>
static inline bool isSecondLevelDomainNameAllowedByTLDRules(const UChar* buffer, int32_t length, Func characterIsAllowed)
{
    ASSERT(length > 0);

    for (int32_t i = length - 1; i >= 0; --i) {
        UChar ch = buffer[i];
        
        if (characterIsAllowed(ch))
            continue;
        
        // Only check the second level domain. Lower level registrars may have different rules.
        if (ch == '.')
            break;
        
        return false;
    }
    return true;
}

#define CHECK_RULES_IF_SUFFIX_MATCHES(suffix, function) \
    { \
        static const int32_t suffixLength = sizeof(suffix) / sizeof(suffix[0]); \
        if (length > suffixLength && !memcmp(buffer + length - suffixLength, suffix, sizeof(suffix))) \
            return isSecondLevelDomainNameAllowedByTLDRules(buffer, length - suffixLength, function); \
    }

static bool isRussianDomainNameCharacter(UChar ch)
{
    // Only modern Russian letters, digits and dashes are allowed.
    return (ch >= 0x0430 && ch <= 0x044f) || ch == 0x0451 || isASCIIDigit(ch) || ch == '-';
}

static bool allCharactersAllowedByTLDRules(const UChar* buffer, int32_t length)
{
    // Skip trailing dot for root domain.
    if (buffer[length - 1] == '.')
        length--;

    // http://cctld.ru/files/pdf/docs/rules_ru-rf.pdf
    static const UChar cyrillicRF[] = {
        '.',
        0x0440, // CYRILLIC SMALL LETTER ER
        0x0444, // CYRILLIC SMALL LETTER EF
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicRF, isRussianDomainNameCharacter);

    // http://rusnames.ru/rules.pl
    static const UChar cyrillicRUS[] = {
        '.',
        0x0440, // CYRILLIC SMALL LETTER ER
        0x0443, // CYRILLIC SMALL LETTER U
        0x0441, // CYRILLIC SMALL LETTER ES
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicRUS, isRussianDomainNameCharacter);

    // http://ru.faitid.org/projects/moscow/documents/moskva/idn
    static const UChar cyrillicMOSKVA[] = {
        '.',
        0x043C, // CYRILLIC SMALL LETTER EM
        0x043E, // CYRILLIC SMALL LETTER O
        0x0441, // CYRILLIC SMALL LETTER ES
        0x043A, // CYRILLIC SMALL LETTER KA
        0x0432, // CYRILLIC SMALL LETTER VE
        0x0430, // CYRILLIC SMALL LETTER A
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicMOSKVA, isRussianDomainNameCharacter);

    // http://www.dotdeti.ru/foruser/docs/regrules.php
    static const UChar cyrillicDETI[] = {
        '.',
        0x0434, // CYRILLIC SMALL LETTER DE
        0x0435, // CYRILLIC SMALL LETTER IE
        0x0442, // CYRILLIC SMALL LETTER TE
        0x0438, // CYRILLIC SMALL LETTER I
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicDETI, isRussianDomainNameCharacter);

    // http://corenic.org - rules not published. The word is Russian, so only allowing Russian at this time,
    // although we may need to revise the checks if this ends up being used with other languages spoken in Russia.
    static const UChar cyrillicONLAYN[] = {
        '.',
        0x043E, // CYRILLIC SMALL LETTER O
        0x043D, // CYRILLIC SMALL LETTER EN
        0x043B, // CYRILLIC SMALL LETTER EL
        0x0430, // CYRILLIC SMALL LETTER A
        0x0439, // CYRILLIC SMALL LETTER SHORT I
        0x043D, // CYRILLIC SMALL LETTER EN
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicONLAYN, isRussianDomainNameCharacter);

    // http://corenic.org - same as above.
    static const UChar cyrillicSAYT[] = {
        '.',
        0x0441, // CYRILLIC SMALL LETTER ES
        0x0430, // CYRILLIC SMALL LETTER A
        0x0439, // CYRILLIC SMALL LETTER SHORT I
        0x0442, // CYRILLIC SMALL LETTER TE
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicSAYT, isRussianDomainNameCharacter);

    // http://pir.org/products/opr-domain/ - rules not published. According to the registry site,
    // the intended audience is "Russian and other Slavic-speaking markets".
    // Chrome appears to only allow Russian, so sticking with that for now.
    static const UChar cyrillicORG[] = {
        '.',
        0x043E, // CYRILLIC SMALL LETTER O
        0x0440, // CYRILLIC SMALL LETTER ER
        0x0433, // CYRILLIC SMALL LETTER GHE
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicORG, isRussianDomainNameCharacter);

    // http://cctld.by/rules.html
    static const UChar cyrillicBEL[] = {
        '.',
        0x0431, // CYRILLIC SMALL LETTER BE
        0x0435, // CYRILLIC SMALL LETTER IE
        0x043B, // CYRILLIC SMALL LETTER EL
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicBEL, [](UChar ch) {
        // Russian and Byelorussian letters, digits and dashes are allowed.
        return (ch >= 0x0430 && ch <= 0x044f) || ch == 0x0451 || ch == 0x0456 || ch == 0x045E || ch == 0x2019 || isASCIIDigit(ch) || ch == '-';
    });

    // http://www.nic.kz/docs/poryadok_vnedreniya_kaz_ru.pdf
    static const UChar cyrillicKAZ[] = {
        '.',
        0x049B, // CYRILLIC SMALL LETTER KA WITH DESCENDER
        0x0430, // CYRILLIC SMALL LETTER A
        0x0437, // CYRILLIC SMALL LETTER ZE
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicKAZ, [](UChar ch) {
        // Kazakh letters, digits and dashes are allowed.
        return (ch >= 0x0430 && ch <= 0x044f) || ch == 0x0451 || ch == 0x04D9 || ch == 0x0493 || ch == 0x049B || ch == 0x04A3 || ch == 0x04E9 || ch == 0x04B1 || ch == 0x04AF || ch == 0x04BB || ch == 0x0456 || isASCIIDigit(ch) || ch == '-';
    });

    // http://uanic.net/docs/documents-ukr/Rules%20of%20UKR_v4.0.pdf
    static const UChar cyrillicUKR[] = {
        '.',
        0x0443, // CYRILLIC SMALL LETTER U
        0x043A, // CYRILLIC SMALL LETTER KA
        0x0440, // CYRILLIC SMALL LETTER ER
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicUKR, [](UChar ch) {
        // Russian and Ukrainian letters, digits and dashes are allowed.
        return (ch >= 0x0430 && ch <= 0x044f) || ch == 0x0451 || ch == 0x0491 || ch == 0x0404 || ch == 0x0456 || ch == 0x0457 || isASCIIDigit(ch) || ch == '-';
    });

    // http://www.rnids.rs/data/DOKUMENTI/idn-srb-policy-termsofuse-v1.4-eng.pdf
    static const UChar cyrillicSRB[] = {
        '.',
        0x0441, // CYRILLIC SMALL LETTER ES
        0x0440, // CYRILLIC SMALL LETTER ER
        0x0431, // CYRILLIC SMALL LETTER BE
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicSRB, [](UChar ch) {
        // Serbian letters, digits and dashes are allowed.
        return (ch >= 0x0430 && ch <= 0x0438) || (ch >= 0x043A && ch <= 0x0448) || ch == 0x0452 || ch == 0x0458 || ch == 0x0459 || ch == 0x045A || ch == 0x045B || ch == 0x045F || isASCIIDigit(ch) || ch == '-';
    });

    // http://marnet.mk/doc/pravilnik-mk-mkd.pdf
    static const UChar cyrillicMKD[] = {
        '.',
        0x043C, // CYRILLIC SMALL LETTER EM
        0x043A, // CYRILLIC SMALL LETTER KA
        0x0434, // CYRILLIC SMALL LETTER DE
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicMKD, [](UChar ch) {
        // Macedonian letters, digits and dashes are allowed.
        return (ch >= 0x0430 && ch <= 0x0438) || (ch >= 0x043A && ch <= 0x0448) || ch == 0x0453 || ch == 0x0455 || ch == 0x0458 || ch == 0x0459 || ch == 0x045A || ch == 0x045C || ch == 0x045F || isASCIIDigit(ch) || ch == '-';
    });

    // https://www.mon.mn/cs/
    static const UChar cyrillicMON[] = {
        '.',
        0x043C, // CYRILLIC SMALL LETTER EM
        0x043E, // CYRILLIC SMALL LETTER O
        0x043D, // CYRILLIC SMALL LETTER EN
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicMON, [](UChar ch) {
        // Mongolian letters, digits and dashes are allowed.
        return (ch >= 0x0430 && ch <= 0x044f) || ch == 0x0451 || ch == 0x04E9 || ch == 0x04AF || isASCIIDigit(ch) || ch == '-';
    });

    // https://www.icann.org/sites/default/files/packages/lgr/lgr-second-level-bulgarian-30aug16-en.html
    static const UChar cyrillicBG[] = {
        '.',
        0x0431, // CYRILLIC SMALL LETTER BE
        0x0433 // CYRILLIC SMALL LETTER GHE
    };
    CHECK_RULES_IF_SUFFIX_MATCHES(cyrillicBG, [](UChar ch) {
        return (ch >= 0x0430 && ch <= 0x044A) || ch == 0x044C || (ch >= 0x044E && ch <= 0x0450) || ch == 0x045D || isASCIIDigit(ch) || ch == '-';
    });

    // Not a known top level domain with special rules.
    return false;
}

// Return value of null means no mapping is necessary.
Optional<String> mapHostName(const String& hostName, URLDecodeFunction decodeFunction)
{
    if (hostName.length() > hostNameBufferLength)
        return String();
    
    if (!hostName.length())
        return String();

    String string;
    if (decodeFunction && string.contains('%'))
        string = (*decodeFunction)(hostName);
    else
        string = hostName;

    unsigned length = string.length();

    auto sourceBuffer = string.charactersWithNullTermination();
    
    UChar destinationBuffer[hostNameBufferLength];
    UErrorCode uerror = U_ZERO_ERROR;
    UIDNAInfo processingDetails = UIDNA_INFO_INITIALIZER;
    int32_t numCharactersConverted = (decodeFunction ? uidna_nameToASCII : uidna_nameToUnicode)(&URLParser::internationalDomainNameTranscoder(), sourceBuffer.data(), length, destinationBuffer, hostNameBufferLength, &processingDetails, &uerror);
    int allowedErrors = decodeFunction ? 0 : UIDNA_ERROR_EMPTY_LABEL | UIDNA_ERROR_LEADING_HYPHEN | UIDNA_ERROR_TRAILING_HYPHEN | UIDNA_ERROR_HYPHEN_3_4;
    if (length && (U_FAILURE(uerror) || processingDetails.errors & ~allowedErrors))
        return nullopt;
    
    if (numCharactersConverted == static_cast<int32_t>(length) && !memcmp(sourceBuffer.data(), destinationBuffer, length * sizeof(UChar)))
        return String();

    if (!decodeFunction && !allCharactersInAllowedIDNScriptList(destinationBuffer, numCharactersConverted) && !allCharactersAllowedByTLDRules(destinationBuffer, numCharactersConverted))
        return String();

    return String(destinationBuffer, numCharactersConverted);
}

using MappingRangesVector = Optional<Vector<std::tuple<unsigned, unsigned, String>>>;

static void collectRangesThatNeedMapping(const String& string, unsigned location, unsigned length, MappingRangesVector& array, URLDecodeFunction decodeFunction)
{
    // Generally, we want to optimize for the case where there is one host name that does not need mapping.
    // Therefore, we use null to indicate no mapping here and an empty array to indicate error.

    String substring = string.substringSharingImpl(location, length);
    Optional<String> host = mapHostName(substring, decodeFunction);

    if (host && !*host)
        return;
    
    if (!array)
        array = Vector<std::tuple<unsigned, unsigned, String>>();

    if (host)
        array->constructAndAppend(location, length, *host);
}

static void applyHostNameFunctionToMailToURLString(const String& string, URLDecodeFunction decodeFunction, MappingRangesVector& array)
{
    // In a mailto: URL, host names come after a '@' character and end with a '>' or ',' or '?' character.
    // Skip quoted strings so that characters in them don't confuse us.
    // When we find a '?' character, we are past the part of the URL that contains host names.
    
    unsigned stringLength = string.length();
    unsigned current = 0;
    
    while (1) {
        // Find start of host name or of quoted string.
        auto hostNameOrStringStart = string.find([](UChar ch) {
            return ch == '"' || ch == '@' || ch == '?';
        }, current);
        if (hostNameOrStringStart == notFound)
            return;

        UChar c = string[hostNameOrStringStart];
        current = hostNameOrStringStart + 1;
        
        if (c == '?')
            return;
        
        if (c == '@') {
            // Find end of host name.
            unsigned hostNameStart = current;
            auto hostNameEnd = string.find([](UChar ch) {
                return ch == '>' || ch == ',' || ch == '?';
            }, current);

            bool done;
            if (hostNameEnd == notFound) {
                hostNameEnd = stringLength;
                done = true;
            } else {
                current = hostNameEnd;
                done = false;
            }
            
            // Process host name range.
            collectRangesThatNeedMapping(string, hostNameStart, hostNameEnd - hostNameStart, array, decodeFunction);

            if (done)
                return;
        } else {
            // Skip quoted string.
            ASSERT(c == '"');
            while (1) {
                auto escapedCharacterOrStringEnd = string.find([](UChar ch) {
                    return ch == '"' || ch == '\\';
                }, current);
                if (escapedCharacterOrStringEnd == notFound)
                    return;

                c = string[escapedCharacterOrStringEnd];
                current = escapedCharacterOrStringEnd + 1;

                // If we are the end of the string, then break from the string loop back to the host name loop.
                if (c == '"')
                    break;
                
                // Skip escaped character.
                ASSERT(c == '\\');
                if (current == stringLength)
                    return;

                ++current;
            }
        }
    }
}

static void applyHostNameFunctionToURLString(const String& string, URLDecodeFunction decodeFunction, MappingRangesVector& array)
{
    // Find hostnames. Too bad we can't use any real URL-parsing code to do this,
    // but we have to do it before doing all the %-escaping, and this is the only
    // code we have that parses mailto URLs anyway.
    
    // Maybe we should implement this using a character buffer instead?
    
    if (protocolIs(string, "mailto")) {
        applyHostNameFunctionToMailToURLString(string, decodeFunction, array);
        return;
    }

    // Find the host name in a hierarchical URL.
    // It comes after a "://" sequence, with scheme characters preceding.
    // If ends with the end of the string or a ":", "/", or a "?".
    // If there is a "@" character, the host part is just the part after the "@".
    static const char* separator = "://";
    auto separatorIndex = string.find(separator);
    if (separatorIndex == notFound)
        return;

    unsigned authorityStart = separatorIndex + strlen(separator);

    // Check that all characters before the :// are valid scheme characters.
    if (StringView { string }.left(separatorIndex).contains([](UChar character) {
        return !(isASCIIAlphanumeric(character) || character == '+' || character == '-' || character == '.');
    }))
        return;

    // Find terminating character.
    auto hostNameTerminator = string.find([](UChar character) {
        return character == ':' || character == '/' || character == '?' || character == '#';
    }, authorityStart);
    unsigned hostNameEnd = hostNameTerminator == notFound ? string.length() : hostNameTerminator;

    // Find "@" for the start of the host name.
    auto userInfoTerminator = StringView { string }.left(hostNameEnd).find('@', authorityStart);
    unsigned hostNameStart = userInfoTerminator == notFound ? authorityStart : userInfoTerminator + 1;

    collectRangesThatNeedMapping(string, hostNameStart, hostNameEnd - hostNameStart, array, decodeFunction);
}

String mapHostNames(const String& string, URLDecodeFunction decodeFunction)
{
    // Generally, we want to optimize for the case where there is one host name that does not need mapping.
    
    if (decodeFunction && string.isAllASCII())
        return string;
    
    // Make a list of ranges that actually need mapping.
    MappingRangesVector hostNameRanges;
    applyHostNameFunctionToURLString(string, decodeFunction, hostNameRanges);
    if (!hostNameRanges)
        return string;

    if (hostNameRanges->isEmpty())
        return { };

    // Do the mapping.
    String result = string;
    while (!hostNameRanges->isEmpty()) {
        auto [location, length, mappedHostName] = hostNameRanges->takeLast();
        result = result.replace(location, length, mappedHostName);
    }
    return result;
}

static String escapeUnsafeCharacters(const String& sourceBuffer)
{
    unsigned length = sourceBuffer.length();

    Optional<UChar32> previousCodePoint;

    unsigned i;
    for (i = 0; i < length; ) {
        UChar32 c = sourceBuffer.characterStartingAt(i);
        if (isLookalikeCharacter(previousCodePoint, sourceBuffer.characterStartingAt(i)))
            break;
        previousCodePoint = c;
        i += U16_LENGTH(c);
    }

    if (i == length)
        return sourceBuffer;

    Vector<UChar, urlBytesBufferLength> outBuffer;

    outBuffer.grow(i);
    if (sourceBuffer.is8Bit())
        StringImpl::copyCharacters(outBuffer.data(), sourceBuffer.characters8(), i);
    else
        StringImpl::copyCharacters(outBuffer.data(), sourceBuffer.characters16(), i);

    for (; i < length; ) {
        UChar32 c = sourceBuffer.characterStartingAt(i);
        unsigned characterLength = U16_LENGTH(c);
        if (isLookalikeCharacter(previousCodePoint, c)) {
            uint8_t utf8Buffer[4];
            size_t offset = 0;
            UBool failure = false;
            U8_APPEND(utf8Buffer, offset, 4, c, failure);
            ASSERT_UNUSED(failure, !failure);

            for (size_t j = 0; j < offset; ++j) {
                outBuffer.append('%');
                outBuffer.append(upperNibbleToASCIIHexDigit(utf8Buffer[j]));
                outBuffer.append(lowerNibbleToASCIIHexDigit(utf8Buffer[j]));
            }
        } else {
            for (unsigned j = 0; j < characterLength; ++j)
                outBuffer.append(sourceBuffer[i + j]);
        }
        previousCodePoint = c;
        i += characterLength;
    }

    return String::adopt(WTFMove(outBuffer));
}

String userVisibleURL(const CString& url)
{
    auto* before = reinterpret_cast<const unsigned char*>(url.data());
    int length = url.length();

    if (!length)
        return { };

    bool mayNeedHostNameDecoding = false;

    Checked<int, RecordOverflow> bufferLength = length;
    bufferLength = bufferLength * 3 + 1; // The buffer should be large enough to %-escape every character.
    if (bufferLength.hasOverflowed())
        return { };
    Vector<char, urlBytesBufferLength> after(bufferLength.unsafeGet());

    char* q = after.data();
    {
        const unsigned char* p = before;
        for (int i = 0; i < length; i++) {
            unsigned char c = p[i];
            // unescape escape sequences that indicate bytes greater than 0x7f
            if (c == '%' && i + 2 < length && isASCIIHexDigit(p[i + 1]) && isASCIIHexDigit(p[i + 2])) {
                auto u = toASCIIHexValue(p[i + 1], p[i + 2]);
                if (u > 0x7f) {
                    // unescape
                    *q++ = u;
                } else {
                    // do not unescape
                    *q++ = p[i];
                    *q++ = p[i + 1];
                    *q++ = p[i + 2];
                }
                i += 2;
            } else {
                *q++ = c;
                
                // Check for "xn--" in an efficient, non-case-sensitive, way.
                if (c == '-' && i >= 3 && !mayNeedHostNameDecoding && (q[-4] | 0x20) == 'x' && (q[-3] | 0x20) == 'n' && q[-2] == '-')
                    mayNeedHostNameDecoding = true;
            }
        }
        *q = '\0';
    }
    
    // Check string to see if it can be converted to display using UTF-8  
    String result = String::fromUTF8(after.data());
    if (!result) {
        // Could not convert to UTF-8.
        // Convert characters greater than 0x7f to escape sequences.
        // Shift current string to the end of the buffer
        // then we will copy back bytes to the start of the buffer 
        // as we convert.
        int afterlength = q - after.data();
        char* p = after.data() + bufferLength.unsafeGet() - afterlength - 1;
        memmove(p, after.data(), afterlength + 1); // copies trailing '\0'
        char* q = after.data();
        while (*p) {
            unsigned char c = *p;
            if (c > 0x7f) {
                *q++ = '%';
                *q++ = upperNibbleToASCIIHexDigit(c);
                *q++ = lowerNibbleToASCIIHexDigit(c);
            } else
                *q++ = *p;
            p++;
        }
        *q = '\0';
        // Note: after.data() points to a null-terminated, pure ASCII string.
        result = String::fromUTF8(after.data());
        ASSERT(!!result);
    }

    // Note: result is UTF–16 string, created from either a valid UTF-8 string,
    //       or a pure ASCII string (where all bytes with the high bit set are
    //       percent-encoded).

    if (mayNeedHostNameDecoding) {
        // FIXME: Is it good to ignore the failure of mapHostNames and keep result intact?
        auto mappedResult = mapHostNames(result, nullptr);
        if (!!mappedResult)
            result = mappedResult;
    }

    return escapeUnsafeCharacters(normalizedNFC(result));
}

} // namespace URLHelpers
} // namespace WTF
