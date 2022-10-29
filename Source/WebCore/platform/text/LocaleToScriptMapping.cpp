/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "LocaleToScriptMapping.h"

#include <wtf/SortedArrayMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

UScriptCode scriptNameToCode(StringView scriptName)
{
    // This generally maps an ISO 15924 script code to its UScriptCode, but certain families of script codes are
    // treated as a single script for assigning a per-script font in Settings. For example, "hira" is mapped to
    // USCRIPT_KATAKANA_OR_HIRAGANA instead of USCRIPT_HIRAGANA, since we want all Japanese scripts to be rendered
    // using the same font setting.
    using ScriptName = PackedLettersLiteral<uint32_t>;
    static constexpr std::pair<ScriptName, UScriptCode> scriptNameCodeList[] = {
        { "arab", USCRIPT_ARABIC },
        { "armn", USCRIPT_ARMENIAN },
        { "bali", USCRIPT_BALINESE },
        { "batk", USCRIPT_BATAK },
        { "beng", USCRIPT_BENGALI },
        { "blis", USCRIPT_BLISSYMBOLS },
        { "bopo", USCRIPT_BOPOMOFO },
        { "brah", USCRIPT_BRAHMI },
        { "brai", USCRIPT_BRAILLE },
        { "bugi", USCRIPT_BUGINESE },
        { "buhd", USCRIPT_BUHID },
        { "cans", USCRIPT_CANADIAN_ABORIGINAL },
        { "cham", USCRIPT_CHAM },
        { "cher", USCRIPT_CHEROKEE },
        { "cirt", USCRIPT_CIRTH },
        { "copt", USCRIPT_COPTIC },
        { "cprt", USCRIPT_CYPRIOT },
        { "cyrl", USCRIPT_CYRILLIC },
        { "cyrs", USCRIPT_OLD_CHURCH_SLAVONIC_CYRILLIC },
        { "deva", USCRIPT_DEVANAGARI },
        { "dsrt", USCRIPT_DESERET },
        { "egyd", USCRIPT_DEMOTIC_EGYPTIAN },
        { "egyh", USCRIPT_HIERATIC_EGYPTIAN },
        { "egyp", USCRIPT_EGYPTIAN_HIEROGLYPHS },
        { "ethi", USCRIPT_ETHIOPIC },
        { "geok", USCRIPT_KHUTSURI },
        { "geor", USCRIPT_GEORGIAN },
        { "glag", USCRIPT_GLAGOLITIC },
        { "goth", USCRIPT_GOTHIC },
        { "grek", USCRIPT_GREEK },
        { "gujr", USCRIPT_GUJARATI },
        { "guru", USCRIPT_GURMUKHI },
        { "hang", USCRIPT_HANGUL },
        { "hani", USCRIPT_HAN },
        { "hano", USCRIPT_HANUNOO },
        { "hans", USCRIPT_SIMPLIFIED_HAN },
        { "hant", USCRIPT_TRADITIONAL_HAN },
        { "hebr", USCRIPT_HEBREW },
        { "hira", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "hmng", USCRIPT_PAHAWH_HMONG },
        { "hrkt", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "hung", USCRIPT_OLD_HUNGARIAN },
        { "inds", USCRIPT_HARAPPAN_INDUS },
        { "ital", USCRIPT_OLD_ITALIC },
        { "java", USCRIPT_JAVANESE },
        { "jpan", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "kali", USCRIPT_KAYAH_LI },
        { "kana", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "khar", USCRIPT_KHAROSHTHI },
        { "khmr", USCRIPT_KHMER },
        { "knda", USCRIPT_KANNADA },
        { "kore", USCRIPT_HANGUL },
        { "laoo", USCRIPT_LAO },
        { "latf", USCRIPT_LATIN_FRAKTUR },
        { "latg", USCRIPT_LATIN_GAELIC },
        { "latn", USCRIPT_LATIN },
        { "lepc", USCRIPT_LEPCHA },
        { "limb", USCRIPT_LIMBU },
        { "lina", USCRIPT_LINEAR_A },
        { "linb", USCRIPT_LINEAR_B },
        { "mand", USCRIPT_MANDAEAN },
        { "maya", USCRIPT_MAYAN_HIEROGLYPHS },
        { "mero", USCRIPT_MEROITIC },
        { "mlym", USCRIPT_MALAYALAM },
        { "mong", USCRIPT_MONGOLIAN },
        { "mymr", USCRIPT_MYANMAR },
        { "nkoo", USCRIPT_NKO },
        { "ogam", USCRIPT_OGHAM },
        { "orkh", USCRIPT_ORKHON },
        { "orya", USCRIPT_ORIYA },
        { "osma", USCRIPT_OSMANYA },
        { "perm", USCRIPT_OLD_PERMIC },
        { "phag", USCRIPT_PHAGS_PA },
        { "phnx", USCRIPT_PHOENICIAN },
        { "plrd", USCRIPT_PHONETIC_POLLARD },
        { "qaai", USCRIPT_INHERITED },
        { "roro", USCRIPT_RONGORONGO },
        { "runr", USCRIPT_RUNIC },
        { "sara", USCRIPT_SARATI },
        { "shaw", USCRIPT_SHAVIAN },
        { "sinh", USCRIPT_SINHALA },
        { "sylo", USCRIPT_SYLOTI_NAGRI },
        { "syrc", USCRIPT_SYRIAC },
        { "syre", USCRIPT_ESTRANGELO_SYRIAC },
        { "syrj", USCRIPT_WESTERN_SYRIAC },
        { "syrn", USCRIPT_EASTERN_SYRIAC },
        { "tagb", USCRIPT_TAGBANWA },
        { "tale", USCRIPT_TAI_LE },
        { "talu", USCRIPT_NEW_TAI_LUE },
        { "taml", USCRIPT_TAMIL },
        { "telu", USCRIPT_TELUGU },
        { "teng", USCRIPT_TENGWAR },
        { "tfng", USCRIPT_TIFINAGH },
        { "tglg", USCRIPT_TAGALOG },
        { "thaa", USCRIPT_THAANA },
        { "thai", USCRIPT_THAI },
        { "tibt", USCRIPT_TIBETAN },
        { "ugar", USCRIPT_UGARITIC },
        { "vaii", USCRIPT_VAI },
        { "visp", USCRIPT_VISIBLE_SPEECH },
        { "xpeo", USCRIPT_OLD_PERSIAN },
        { "xsux", USCRIPT_CUNEIFORM },
        { "yiii", USCRIPT_YI },
        { "zxxx", USCRIPT_UNWRITTEN_LANGUAGES },
        { "zyyy", USCRIPT_COMMON },
        { "zzzz", USCRIPT_UNKNOWN },
    };
    static_assert(ScriptName("arab").value() == 0x61726162U);
    static_assert(ScriptName("zzzz").value() == 0x7a7a7a7aU);
    static constexpr SortedArrayMap map { scriptNameCodeList };
    return map.get(scriptName, USCRIPT_INVALID_CODE);
}

UScriptCode localeToScriptCodeForFontSelection(const String& locale)
{
    using LocaleName = PackedASCIILowerCodes<uint64_t>;
    static constexpr std::pair<LocaleName, UScriptCode> localeScriptList[] = {
        { "aa", USCRIPT_LATIN },
        { "ab", USCRIPT_CYRILLIC },
        { "ady", USCRIPT_CYRILLIC },
        { "af", USCRIPT_LATIN },
        { "ak", USCRIPT_LATIN },
        { "am", USCRIPT_ETHIOPIC },
        { "ar", USCRIPT_ARABIC },
        { "as", USCRIPT_BENGALI },
        { "ast", USCRIPT_LATIN },
        { "av", USCRIPT_CYRILLIC },
        { "ay", USCRIPT_LATIN },
        { "az", USCRIPT_LATIN },
        { "ba", USCRIPT_CYRILLIC },
        { "be", USCRIPT_CYRILLIC },
        { "bg", USCRIPT_CYRILLIC },
        { "bi", USCRIPT_LATIN },
        { "bn", USCRIPT_BENGALI },
        { "bo", USCRIPT_TIBETAN },
        { "bs", USCRIPT_LATIN },
        { "ca", USCRIPT_LATIN },
        { "ce", USCRIPT_CYRILLIC },
        { "ceb", USCRIPT_LATIN },
        { "ch", USCRIPT_LATIN },
        { "chk", USCRIPT_LATIN },
        { "cs", USCRIPT_LATIN },
        { "cy", USCRIPT_LATIN },
        { "da", USCRIPT_LATIN },
        { "de", USCRIPT_LATIN },
        { "dv", USCRIPT_THAANA },
        { "dz", USCRIPT_TIBETAN },
        { "ee", USCRIPT_LATIN },
        { "efi", USCRIPT_LATIN },
        { "el", USCRIPT_GREEK },
        { "en", USCRIPT_LATIN },
        { "es", USCRIPT_LATIN },
        { "et", USCRIPT_LATIN },
        { "eu", USCRIPT_LATIN },
        { "fa", USCRIPT_ARABIC },
        { "fi", USCRIPT_LATIN },
        { "fil", USCRIPT_LATIN },
        { "fj", USCRIPT_LATIN },
        { "fo", USCRIPT_LATIN },
        { "fr", USCRIPT_LATIN },
        { "fur", USCRIPT_LATIN },
        { "fy", USCRIPT_LATIN },
        { "ga", USCRIPT_LATIN },
        { "gaa", USCRIPT_LATIN },
        { "gd", USCRIPT_LATIN },
        { "gil", USCRIPT_LATIN },
        { "gl", USCRIPT_LATIN },
        { "gn", USCRIPT_LATIN },
        { "gsw", USCRIPT_LATIN },
        { "gu", USCRIPT_GUJARATI },
        { "ha", USCRIPT_LATIN },
        { "haw", USCRIPT_LATIN },
        { "he", USCRIPT_HEBREW },
        { "hi", USCRIPT_DEVANAGARI },
        { "hil", USCRIPT_LATIN },
        { "ho", USCRIPT_LATIN },
        { "hr", USCRIPT_LATIN },
        { "ht", USCRIPT_LATIN },
        { "hu", USCRIPT_LATIN },
        { "hy", USCRIPT_ARMENIAN },
        { "id", USCRIPT_LATIN },
        { "ig", USCRIPT_LATIN },
        { "ii", USCRIPT_YI },
        { "ilo", USCRIPT_LATIN },
        { "inh", USCRIPT_CYRILLIC },
        { "is", USCRIPT_LATIN },
        { "it", USCRIPT_LATIN },
        { "iu", USCRIPT_CANADIAN_ABORIGINAL },
        { "ja", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "jv", USCRIPT_LATIN },
        { "ka", USCRIPT_GEORGIAN },
        { "kaj", USCRIPT_LATIN },
        { "kam", USCRIPT_LATIN },
        { "kbd", USCRIPT_CYRILLIC },
        { "kha", USCRIPT_LATIN },
        { "kk", USCRIPT_CYRILLIC },
        { "kl", USCRIPT_LATIN },
        { "km", USCRIPT_KHMER },
        { "kn", USCRIPT_KANNADA },
        { "ko", USCRIPT_HANGUL },
        { "kok", USCRIPT_DEVANAGARI },
        { "kos", USCRIPT_LATIN },
        { "kpe", USCRIPT_LATIN },
        { "krc", USCRIPT_CYRILLIC },
        { "ks", USCRIPT_ARABIC },
        { "ku", USCRIPT_ARABIC },
        { "kum", USCRIPT_CYRILLIC },
        { "ky", USCRIPT_CYRILLIC },
        { "la", USCRIPT_LATIN },
        { "lah", USCRIPT_ARABIC },
        { "lb", USCRIPT_LATIN },
        { "lez", USCRIPT_CYRILLIC },
        { "ln", USCRIPT_LATIN },
        { "lo", USCRIPT_LAO },
        { "lt", USCRIPT_LATIN },
        { "lv", USCRIPT_LATIN },
        { "mai", USCRIPT_DEVANAGARI },
        { "mdf", USCRIPT_CYRILLIC },
        { "mg", USCRIPT_LATIN },
        { "mh", USCRIPT_LATIN },
        { "mi", USCRIPT_LATIN },
        { "mk", USCRIPT_CYRILLIC },
        { "ml", USCRIPT_MALAYALAM },
        { "mn", USCRIPT_CYRILLIC },
        { "mr", USCRIPT_DEVANAGARI },
        { "ms", USCRIPT_LATIN },
        { "mt", USCRIPT_LATIN },
        { "my", USCRIPT_MYANMAR },
        { "myv", USCRIPT_CYRILLIC },
        { "na", USCRIPT_LATIN },
        { "nb", USCRIPT_LATIN },
        { "ne", USCRIPT_DEVANAGARI },
        { "niu", USCRIPT_LATIN },
        { "nl", USCRIPT_LATIN },
        { "nn", USCRIPT_LATIN },
        { "nr", USCRIPT_LATIN },
        { "nso", USCRIPT_LATIN },
        { "ny", USCRIPT_LATIN },
        { "oc", USCRIPT_LATIN },
        { "om", USCRIPT_LATIN },
        { "or", USCRIPT_ORIYA },
        { "os", USCRIPT_CYRILLIC },
        { "pa", USCRIPT_GURMUKHI },
        { "pag", USCRIPT_LATIN },
        { "pap", USCRIPT_LATIN },
        { "pau", USCRIPT_LATIN },
        { "pl", USCRIPT_LATIN },
        { "pon", USCRIPT_LATIN },
        { "ps", USCRIPT_ARABIC },
        { "pt", USCRIPT_LATIN },
        { "qu", USCRIPT_LATIN },
        { "rm", USCRIPT_LATIN },
        { "rn", USCRIPT_LATIN },
        { "ro", USCRIPT_LATIN },
        { "ru", USCRIPT_CYRILLIC },
        { "rw", USCRIPT_LATIN },
        { "sa", USCRIPT_DEVANAGARI },
        { "sah", USCRIPT_CYRILLIC },
        { "sat", USCRIPT_LATIN },
        { "sd", USCRIPT_ARABIC },
        { "se", USCRIPT_LATIN },
        { "sg", USCRIPT_LATIN },
        { "si", USCRIPT_SINHALA },
        { "sid", USCRIPT_LATIN },
        { "sk", USCRIPT_LATIN },
        { "sl", USCRIPT_LATIN },
        { "sm", USCRIPT_LATIN },
        { "so", USCRIPT_LATIN },
        { "sq", USCRIPT_LATIN },
        { "sr", USCRIPT_CYRILLIC },
        { "ss", USCRIPT_LATIN },
        { "st", USCRIPT_LATIN },
        { "su", USCRIPT_LATIN },
        { "sv", USCRIPT_LATIN },
        { "sw", USCRIPT_LATIN },
        { "ta", USCRIPT_TAMIL },
        { "te", USCRIPT_TELUGU },
        { "tet", USCRIPT_LATIN },
        { "tg", USCRIPT_CYRILLIC },
        { "th", USCRIPT_THAI },
        { "ti", USCRIPT_ETHIOPIC },
        { "tig", USCRIPT_ETHIOPIC },
        { "tk", USCRIPT_LATIN },
        { "tkl", USCRIPT_LATIN },
        { "tl", USCRIPT_LATIN },
        { "tn", USCRIPT_LATIN },
        { "to", USCRIPT_LATIN },
        { "tpi", USCRIPT_LATIN },
        { "tr", USCRIPT_LATIN },
        { "trv", USCRIPT_LATIN },
        { "ts", USCRIPT_LATIN },
        { "tt", USCRIPT_CYRILLIC },
        { "tvl", USCRIPT_LATIN },
        { "tw", USCRIPT_LATIN },
        { "ty", USCRIPT_LATIN },
        { "tyv", USCRIPT_CYRILLIC },
        { "udm", USCRIPT_CYRILLIC },
        { "ug", USCRIPT_ARABIC },
        { "uk", USCRIPT_CYRILLIC },
        { "und", USCRIPT_LATIN },
        { "ur", USCRIPT_ARABIC },
        { "uz", USCRIPT_CYRILLIC },
        { "ve", USCRIPT_LATIN },
        { "vi", USCRIPT_LATIN },
        { "wal", USCRIPT_ETHIOPIC },
        { "war", USCRIPT_LATIN },
        { "wo", USCRIPT_LATIN },
        { "xh", USCRIPT_LATIN },
        { "yap", USCRIPT_LATIN },
        { "yo", USCRIPT_LATIN },
        { "za", USCRIPT_LATIN },
        { "zh", USCRIPT_HAN },
        { "zh_hk", USCRIPT_TRADITIONAL_HAN },
        { "zh_tw", USCRIPT_TRADITIONAL_HAN },
        { "zu", USCRIPT_LATIN },
    };
    static_assert(LocaleName("aa").value() == 0x6161000000000000ULL);
    static_assert(LocaleName("zh_tw").value() == 0x7a685f7477000000ULL);
    static constexpr SortedArrayMap map { localeScriptList };
    String canonicalLocaleString = makeStringByReplacingAll(locale, '-', '_');
    for (StringView canonicalLocale = canonicalLocaleString; !canonicalLocale.isEmpty(); ) {
        if (auto scriptCode = map.tryGet(canonicalLocale))
            return *scriptCode;
        auto underscorePosition = canonicalLocale.reverseFind('_');
        if (underscorePosition == notFound)
            break;
        UScriptCode code = scriptNameToCode(canonicalLocale.substring(underscorePosition + 1));
        if (code != USCRIPT_INVALID_CODE && code != USCRIPT_UNKNOWN)
            return code;
        canonicalLocale = canonicalLocale.left(underscorePosition);
    }
    return USCRIPT_COMMON;
}

} // namespace WebCore
