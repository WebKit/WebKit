/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TextCodecICU.h"

#include "TextEncoding.h"
#include "TextEncodingRegistry.h"
#include "ThreadGlobalData.h"
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <wtf/Assertions.h>
#include <wtf/StringExtras.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

const size_t ConversionBufferSize = 16384;

#if PLATFORM(IOS)
static const char* textCodecMacAliases[] = {
    "macos-7_3-10.2", // xmaccyrillic, maccyrillic
    "macos-6_2-10.4", // xmacgreek
    "macos-6-10.2",   // macgreek
    "macos-29-10.2",  // xmaccentraleurroman, maccentraleurroman
    "macos-35-10.2",  // xmacturkish, macturkish
    "softbank-sjis",  // softbanksjis
    nullptr
};
#endif

ICUConverterWrapper::~ICUConverterWrapper()
{
    if (converter)
        ucnv_close(converter);
}

static UConverter*& cachedConverterICU()
{
    return threadGlobalData().cachedConverterICU().converter;
}

std::unique_ptr<TextCodec> TextCodecICU::create(const TextEncoding& encoding, const void* additionalData)
{
    // Name strings are persistently kept in TextEncodingRegistry maps, so they are never deleted.
    return std::make_unique<TextCodecICU>(encoding.name(), static_cast<const char*>(additionalData));
}

#define DECLARE_ALIASES(encoding, ...) \
    static const char* const encoding##_aliases[] { __VA_ARGS__ }

// From https://encoding.spec.whatwg.org.
DECLARE_ALIASES(IBM866, "866", "cp866", "csibm866");
DECLARE_ALIASES(ISO_8859_2, "csisolatin2", "iso-ir-101", "iso8859-2", "iso88592", "iso_8859-2", "iso_8859-2:1987", "l2", "latin2");
DECLARE_ALIASES(ISO_8859_3, "csisolatin3", "iso-ir-109", "iso8859-3", "iso88593", "iso_8859-3", "iso_8859-3:1988", "l3", "latin3");
DECLARE_ALIASES(ISO_8859_4, "csisolatin4", "iso-ir-110", "iso8859-4", "iso88594", "iso_8859-4", "iso_8859-4:1988", "l4", "latin4");
DECLARE_ALIASES(ISO_8859_5, "csisolatincyrillic", "cyrillic", "iso-ir-144", "iso8859-5", "iso88595", "iso_8859-5", "iso_8859-5:1988");
DECLARE_ALIASES(ISO_8859_6, "arabic", "asmo-708", "csiso88596e", "csiso88596i", "csisolatinarabic", "ecma-114", "iso-8859-6-e", "iso-8859-6-i", "iso-ir-127", "iso8859-6", "iso88596", "iso_8859-6", "iso_8859-6:1987");
DECLARE_ALIASES(ISO_8859_7, "csisolatingreek", "ecma-118", "elot_928", "greek", "greek8", "iso-ir-126", "iso8859-7", "iso88597", "iso_8859-7", "iso_8859-7:1987", "sun_eu_greek");
DECLARE_ALIASES(ISO_8859_8, "csiso88598e", "csisolatinhebrew", "hebrew", "iso-8859-8-e", "iso-ir-138", "iso8859-8", "iso88598", "iso_8859-8", "iso_8859-8:1988", "visual");
DECLARE_ALIASES(ISO_8859_8_I, "csiso88598i", "logical");
DECLARE_ALIASES(ISO_8859_10, "csisolatin6", "iso-ir-157", "iso8859-10", "iso885910", "l6", "latin6");
DECLARE_ALIASES(ISO_8859_13, "iso8859-13", "iso885913");
DECLARE_ALIASES(ISO_8859_14, "iso8859-14", "iso885914");
DECLARE_ALIASES(ISO_8859_15, "csisolatin9", "iso8859-15", "iso885915", "iso_8859-15", "l9");
DECLARE_ALIASES(KOI8_R, "cskoi8r", "koi", "koi8", "koi8_r");
DECLARE_ALIASES(KOI8_U, "koi8-ru");
DECLARE_ALIASES(macintosh, "csmacintosh", "mac", "x-mac-roman", "macroman", "x-macroman");
DECLARE_ALIASES(windows_874, "dos-874", "iso-8859-11", "iso8859-11", "iso885911", "tis-620");
DECLARE_ALIASES(windows_949, "euc-kr", "cseuckr", "csksc56011987", "iso-ir-149", "korean", "ks_c_5601-1987", "ks_c_5601-1989", "ksc5601", "ksc_5601", "ms949", "x-KSC5601", "x-windows-949", "x-uhc");
DECLARE_ALIASES(windows_1250, "cp1250", "x-cp1250", "winlatin2");
DECLARE_ALIASES(windows_1251, "cp1251", "wincyrillic", "x-cp1251");
DECLARE_ALIASES(windows_1253, "wingreek", "cp1253", "x-cp1253");
DECLARE_ALIASES(windows_1254, "winturkish", "cp1254", "csisolatin5", "iso-8859-9", "iso-ir-148", "iso8859-9", "iso88599", "iso_8859-9", "iso_8859-9:1989", "l5", "latin5", "x-cp1254");
DECLARE_ALIASES(windows_1255, "winhebrew", "cp1255", "x-cp1255");
DECLARE_ALIASES(windows_1256, "winarabic", "cp1256", "x-cp1256");
DECLARE_ALIASES(windows_1257, "winbaltic", "cp1257", "x-cp1257");
DECLARE_ALIASES(windows_1258, "winvietnamese", "cp1258", "x-cp1258");
DECLARE_ALIASES(x_mac_cyrillic, "maccyrillic", "x-mac-ukrainian", "windows-10007", "mac-cyrillic", "maccy", "x-MacCyrillic", "x-MacUkraine");
DECLARE_ALIASES(GBK, "cn-gb", "csgb231280", "x-euc-cn", "chinese", "csgb2312", "csiso58gb231280", "gb2312", "gb_2312", "gb_2312-80", "iso-ir-58", "x-gbk", "euc-cn", "cp936", "ms936", "gb2312-1980", "windows-936", "windows-936-2000");
DECLARE_ALIASES(gb18030, "ibm-1392", "windows-54936");
DECLARE_ALIASES(Big5, "cn-big5", "x-x-big5", "csbig5", "windows-950", "windows-950-2000", "ms950", "x-windows-950", "x-big5");
DECLARE_ALIASES(EUC_JP, "x-euc", "cseucpkdfmtjapanese", "x-euc-jp");
DECLARE_ALIASES(ISO_2022_JP, "jis7", "csiso2022jp");
DECLARE_ALIASES(Shift_JIS, "shift-jis", "csshiftjis", "ms932", "ms_kanji", "sjis", "windows-31j", "x-sjis");
// Encodings below are not in the standard.
DECLARE_ALIASES(UTF_32, "ISO-10646-UCS-4", "ibm-1236", "ibm-1237", "csUCS4", "ucs-4");
DECLARE_ALIASES(UTF_32LE, "UTF32_LittleEndian", "ibm-1234", "ibm-1235");
DECLARE_ALIASES(UTF_32BE, "UTF32_BigEndian", "ibm-1232", "ibm-1233", "ibm-9424");
DECLARE_ALIASES(x_mac_greek, "windows-10006", "macgr", "x-MacGreek");
DECLARE_ALIASES(x_mac_centraleurroman, "windows-10029", "x-mac-ce", "macce", "maccentraleurope", "x-MacCentralEurope");
DECLARE_ALIASES(x_mac_turkish, "windows-10081", "mactr", "x-MacTurkish");
DECLARE_ALIASES(Big5_HKSCS, "big5hk", "HKSCS-BIG5", "ibm-1375", "ibm-1375_P100-2008");

#define DECLARE_ENCODING_NAME(encoding, alias_array) \
    { encoding, WTF_ARRAY_LENGTH(alias_array##_aliases), alias_array##_aliases }

#define DECLARE_ENCODING_NAME_NO_ALIASES(encoding) \
    { encoding, 0, nullptr }

static const struct EncodingName {
    const char* name;
    unsigned aliasCount;
    const char* const * aliases;
} encodingNames[] = {
    DECLARE_ENCODING_NAME("IBM866", IBM866),
    DECLARE_ENCODING_NAME("ISO-8859-2", ISO_8859_2),
    DECLARE_ENCODING_NAME("ISO-8859-3", ISO_8859_3),
    DECLARE_ENCODING_NAME("ISO-8859-4", ISO_8859_4),
    DECLARE_ENCODING_NAME("ISO-8859-5", ISO_8859_5),
    DECLARE_ENCODING_NAME("ISO-8859-6", ISO_8859_6),
    DECLARE_ENCODING_NAME("ISO-8859-7", ISO_8859_7),
    DECLARE_ENCODING_NAME("ISO-8859-8", ISO_8859_8),
    DECLARE_ENCODING_NAME("ISO-8859-8-I", ISO_8859_8_I),
    DECLARE_ENCODING_NAME("ISO-8859-10", ISO_8859_10),
    DECLARE_ENCODING_NAME("ISO-8859-13", ISO_8859_13),
    DECLARE_ENCODING_NAME("ISO-8859-14", ISO_8859_14),
    DECLARE_ENCODING_NAME("ISO-8859-15", ISO_8859_15),
    DECLARE_ENCODING_NAME_NO_ALIASES("ISO-8859-16"),
    DECLARE_ENCODING_NAME("KOI8-R", KOI8_R),
    DECLARE_ENCODING_NAME("KOI8-U", KOI8_U),
    DECLARE_ENCODING_NAME("macintosh", macintosh),
    DECLARE_ENCODING_NAME("windows-874", windows_874),
    DECLARE_ENCODING_NAME("windows-949", windows_949),
    DECLARE_ENCODING_NAME("windows-1250", windows_1250),
    DECLARE_ENCODING_NAME("windows-1251", windows_1251),
    DECLARE_ENCODING_NAME("windows-1253", windows_1253),
    DECLARE_ENCODING_NAME("windows-1254", windows_1254),
    DECLARE_ENCODING_NAME("windows-1255", windows_1255),
    DECLARE_ENCODING_NAME("windows-1256", windows_1256),
    DECLARE_ENCODING_NAME("windows-1257", windows_1257),
    DECLARE_ENCODING_NAME("windows-1258", windows_1258),
    DECLARE_ENCODING_NAME("x-mac-cyrillic", x_mac_cyrillic),
    DECLARE_ENCODING_NAME("GBK", GBK),
    DECLARE_ENCODING_NAME("gb18030", gb18030),
    DECLARE_ENCODING_NAME("Big5", Big5),
    DECLARE_ENCODING_NAME("EUC-JP", EUC_JP),
    DECLARE_ENCODING_NAME("ISO-2022-JP", ISO_2022_JP),
    DECLARE_ENCODING_NAME("Shift_JIS", Shift_JIS),
    // Encodings below are not in the standard.
    DECLARE_ENCODING_NAME("UTF-32", UTF_32),
    DECLARE_ENCODING_NAME("UTF-32LE", UTF_32LE),
    DECLARE_ENCODING_NAME("UTF-32BE", UTF_32BE),
    DECLARE_ENCODING_NAME("x-mac-greek", x_mac_greek),
    DECLARE_ENCODING_NAME("x-mac-centraleurroman", x_mac_centraleurroman),
    DECLARE_ENCODING_NAME("x-mac-turkish", x_mac_turkish),
    DECLARE_ENCODING_NAME("Big5-HKSCS", Big5_HKSCS),
};

void TextCodecICU::registerEncodingNames(EncodingNameRegistrar registrar)
{
    for (auto& encodingName : encodingNames) {
        registrar(encodingName.name, encodingName.name);
        for (size_t i = 0; i < encodingName.aliasCount; ++i)
            registrar(encodingName.aliases[i], encodingName.name);
    }

#if PLATFORM(IOS)
    // A.B. adding a few more Mac encodings missing 'cause we don't have TextCodecMac right now
    // luckily, they are supported in ICU, just need to alias them.
    // this handles encodings that OS X uses TEC (TextCodecMac)
    // <http://publib.boulder.ibm.com/infocenter/wmbhelp/v6r0m0/index.jsp?topic=/com.ibm.etools.mft.eb.doc/ac00408_.htm>
    int32_t i = 0;
    for (const char* macAlias = textCodecMacAliases[i]; macAlias; macAlias = textCodecMacAliases[++i]) {
        registrar(macAlias, macAlias);

        UErrorCode error = U_ZERO_ERROR;
        uint16_t numAliases = ucnv_countAliases(macAlias, &error);
        ASSERT(U_SUCCESS(error));
        if (U_SUCCESS(error))
            for (uint16_t j = 0; j < numAliases; ++j) {
                error = U_ZERO_ERROR;
                const char* alias = ucnv_getAlias(macAlias, j, &error);
                ASSERT(U_SUCCESS(error));
                if (U_SUCCESS(error) && strcmp(alias, macAlias))
                    registrar(alias, macAlias);
            }
    }
#endif
}

void TextCodecICU::registerCodecs(TextCodecRegistrar registrar)
{
    for (auto& encodingName : encodingNames) {
        // These encodings currently don't have standard names, so we need to register encoders manually.
        // http://demo.icu-project.org/icu-bin/convexp
        if (!strcmp(encodingName.name, "windows-874")) {
            registrar(encodingName.name, create, "windows-874-2000");
            continue;
        }
        if (!strcmp(encodingName.name, "windows-949")) {
            registrar(encodingName.name, create, "windows-949-2000");
            continue;
        }
        if (!strcmp(encodingName.name, "x-mac-cyrillic")) {
            registrar(encodingName.name, create, "macos-7_3-10.2");
            continue;
        }
        if (!strcmp(encodingName.name, "x-mac-greek")) {
            registrar(encodingName.name, create, "macos-6_2-10.4");
            continue;
        }
        if (!strcmp(encodingName.name, "x-mac-centraleurroman")) {
            registrar(encodingName.name, create, "macos-29-10.2");
            continue;
        }
        if (!strcmp(encodingName.name, "x-mac-turkish")) {
            registrar(encodingName.name, create, "macos-35-10.2");
            continue;
        }

        UErrorCode error = U_ZERO_ERROR;
        const char* canonicalConverterName = ucnv_getCanonicalName(encodingName.name, "IANA", &error);
        ASSERT(U_SUCCESS(error));
        registrar(encodingName.name, create, canonicalConverterName);
    }

#if PLATFORM(IOS)
    // See comment above in registerEncodingNames().
    int32_t i = 0;
    for (const char* alias = textCodecMacAliases[i]; alias; alias = textCodecMacAliases[++i])
        registrar(alias, create, 0);
#endif
}

TextCodecICU::TextCodecICU(const char* encoding, const char* canonicalConverterName)
    : m_encodingName(encoding)
    , m_canonicalConverterName(canonicalConverterName)
    , m_converterICU(0)
    , m_needsGBKFallbacks(false)
{
}

TextCodecICU::~TextCodecICU()
{
    releaseICUConverter();
}

void TextCodecICU::releaseICUConverter() const
{
    if (m_converterICU) {
        UConverter*& cachedConverter = cachedConverterICU();
        if (cachedConverter)
            ucnv_close(cachedConverter);
        ucnv_reset(m_converterICU);
        cachedConverter = m_converterICU;
        m_converterICU = 0;
    }
}

void TextCodecICU::createICUConverter() const
{
    ASSERT(!m_converterICU);

    UErrorCode err;

    m_needsGBKFallbacks = !strcmp(m_encodingName, "GBK");

    UConverter*& cachedConverter = cachedConverterICU();
    if (cachedConverter) {
        err = U_ZERO_ERROR;
        const char* cachedConverterName = ucnv_getName(cachedConverter, &err);
        if (U_SUCCESS(err) && !strcmp(m_canonicalConverterName, cachedConverterName)) {
            m_converterICU = cachedConverter;
            cachedConverter = 0;
            return;
        }
    }

    err = U_ZERO_ERROR;
    m_converterICU = ucnv_open(m_canonicalConverterName, &err);
    ASSERT(U_SUCCESS(err));
    if (m_converterICU)
        ucnv_setFallback(m_converterICU, TRUE);
}

int TextCodecICU::decodeToBuffer(UChar* target, UChar* targetLimit, const char*& source, const char* sourceLimit, int32_t* offsets, bool flush, UErrorCode& err)
{
    UChar* targetStart = target;
    err = U_ZERO_ERROR;
    ucnv_toUnicode(m_converterICU, &target, targetLimit, &source, sourceLimit, offsets, flush, &err);
    return target - targetStart;
}

class ErrorCallbackSetter {
public:
    ErrorCallbackSetter(UConverter* converter, bool stopOnError)
        : m_converter(converter)
        , m_shouldStopOnEncodingErrors(stopOnError)
    {
        if (m_shouldStopOnEncodingErrors) {
            UErrorCode err = U_ZERO_ERROR;
            ucnv_setToUCallBack(m_converter, UCNV_TO_U_CALLBACK_SUBSTITUTE,
                           UCNV_SUB_STOP_ON_ILLEGAL, &m_savedAction,
                           &m_savedContext, &err);
            ASSERT(err == U_ZERO_ERROR);
        }
    }
    ~ErrorCallbackSetter()
    {
        if (m_shouldStopOnEncodingErrors) {
            UErrorCode err = U_ZERO_ERROR;
            const void* oldContext;
            UConverterToUCallback oldAction;
            ucnv_setToUCallBack(m_converter, m_savedAction,
                   m_savedContext, &oldAction,
                   &oldContext, &err);
            ASSERT(oldAction == UCNV_TO_U_CALLBACK_SUBSTITUTE);
            ASSERT(!strcmp(static_cast<const char*>(oldContext), UCNV_SUB_STOP_ON_ILLEGAL));
            ASSERT(err == U_ZERO_ERROR);
        }
    }

private:
    UConverter* m_converter;
    bool m_shouldStopOnEncodingErrors;
    const void* m_savedContext;
    UConverterToUCallback m_savedAction;
};

String TextCodecICU::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    // Get a converter for the passed-in encoding.
    if (!m_converterICU) {
        createICUConverter();
        ASSERT(m_converterICU);
        if (!m_converterICU) {
            LOG_ERROR("error creating ICU encoder even though encoding was in table");
            return String();
        }
    }
    
    ErrorCallbackSetter callbackSetter(m_converterICU, stopOnError);

    StringBuilder result;

    UChar buffer[ConversionBufferSize];
    UChar* bufferLimit = buffer + ConversionBufferSize;
    const char* source = reinterpret_cast<const char*>(bytes);
    const char* sourceLimit = source + length;
    int32_t* offsets = NULL;
    UErrorCode err = U_ZERO_ERROR;

    do {
        int ucharsDecoded = decodeToBuffer(buffer, bufferLimit, source, sourceLimit, offsets, flush, err);
        result.append(buffer, ucharsDecoded);
    } while (err == U_BUFFER_OVERFLOW_ERROR);

    if (U_FAILURE(err)) {
        // flush the converter so it can be reused, and not be bothered by this error.
        do {
            decodeToBuffer(buffer, bufferLimit, source, sourceLimit, offsets, true, err);
        } while (source < sourceLimit);
        sawError = true;
    }

    String resultString = result.toString();

    // <http://bugs.webkit.org/show_bug.cgi?id=17014>
    // Simplified Chinese pages use the code A3A0 to mean "full-width space", but ICU decodes it as U+E5E5.
    // FIXME: strcasecmp is locale sensitive, we should not be using it.
    if (strcmp(m_encodingName, "GBK") == 0 || strcasecmp(m_encodingName, "gb18030") == 0)
        resultString.replace(0xE5E5, ideographicSpace);

    return resultString;
}

// We need to apply these fallbacks ourselves as they are not currently supported by ICU and
// they were provided by the old TEC encoding path. Needed to fix <rdar://problem/4708689>.
static UChar fallbackForGBK(UChar32 character)
{
    switch (character) {
    case 0x01F9:
        return 0xE7C8;
    case 0x1E3F:
        return 0xE7C7;
    case 0x22EF:
        return 0x2026;
    case 0x301C:
        return 0xFF5E;
    }
    return 0;
}

// Invalid character handler when writing escaped entities for unrepresentable
// characters. See the declaration of TextCodec::encode for more.
static void urlEscapedEntityCallback(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
    UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* err)
{
    if (reason == UCNV_UNASSIGNED) {
        *err = U_ZERO_ERROR;

        UnencodableReplacementArray entity;
        int entityLen = TextCodec::getUnencodableReplacement(codePoint, URLEncodedEntitiesForUnencodables, entity);
        ucnv_cbFromUWriteBytes(fromUArgs, entity, entityLen, 0, err);
    } else
        UCNV_FROM_U_CALLBACK_ESCAPE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}

// Substitutes special GBK characters, escaping all other unassigned entities.
static void gbkCallbackEscape(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
    UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* err) 
{
    UChar outChar;
    if (reason == UCNV_UNASSIGNED && (outChar = fallbackForGBK(codePoint))) {
        const UChar* source = &outChar;
        *err = U_ZERO_ERROR;
        ucnv_cbFromUWriteUChars(fromUArgs, &source, source + 1, 0, err);
        return;
    }
    UCNV_FROM_U_CALLBACK_ESCAPE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}

// Combines both gbkUrlEscapedEntityCallback and GBK character substitution.
static void gbkUrlEscapedEntityCallack(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
    UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* err) 
{
    if (reason == UCNV_UNASSIGNED) {
        if (UChar outChar = fallbackForGBK(codePoint)) {
            const UChar* source = &outChar;
            *err = U_ZERO_ERROR;
            ucnv_cbFromUWriteUChars(fromUArgs, &source, source + 1, 0, err);
            return;
        }
        urlEscapedEntityCallback(context, fromUArgs, codeUnits, length, codePoint, reason, err);
        return;
    }
    UCNV_FROM_U_CALLBACK_ESCAPE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}

static void gbkCallbackSubstitute(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
    UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* err) 
{
    UChar outChar;
    if (reason == UCNV_UNASSIGNED && (outChar = fallbackForGBK(codePoint))) {
        const UChar* source = &outChar;
        *err = U_ZERO_ERROR;
        ucnv_cbFromUWriteUChars(fromUArgs, &source, source + 1, 0, err);
        return;
    }
    UCNV_FROM_U_CALLBACK_SUBSTITUTE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}

CString TextCodecICU::encode(const UChar* characters, size_t length, UnencodableHandling handling)
{
    if (!length)
        return "";

    if (!m_converterICU)
        createICUConverter();
    if (!m_converterICU)
        return CString();

    // FIXME: We should see if there is "force ASCII range" mode in ICU;
    // until then, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    Vector<UChar> copy;
    const UChar* source = characters;
    if (shouldShowBackslashAsCurrencySymbolIn(m_encodingName)) {
        for (size_t i = 0; i < length; ++i) {
            if (characters[i] == '\\') {
                copy.reserveInitialCapacity(length);
                for (size_t j = 0; j < i; ++j)
                    copy.uncheckedAppend(characters[j]);
                for (size_t j = i; j < length; ++j) {
                    UChar character = characters[j];
                    if (character == '\\')
                        character = yenSign;
                    copy.uncheckedAppend(character);
                }
                source = copy.data();
                break;
            }
        }
    }
    const UChar* sourceLimit = source + length;

    UErrorCode err = U_ZERO_ERROR;

    switch (handling) {
        case QuestionMarksForUnencodables:
            ucnv_setSubstChars(m_converterICU, "?", 1, &err);
            ucnv_setFromUCallBack(m_converterICU, m_needsGBKFallbacks ? gbkCallbackSubstitute : UCNV_FROM_U_CALLBACK_SUBSTITUTE, 0, 0, 0, &err);
            break;
        case EntitiesForUnencodables:
            ucnv_setFromUCallBack(m_converterICU, m_needsGBKFallbacks ? gbkCallbackEscape : UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_DEC, 0, 0, &err);
            break;
        case URLEncodedEntitiesForUnencodables:
            ucnv_setFromUCallBack(m_converterICU, m_needsGBKFallbacks ? gbkUrlEscapedEntityCallack : urlEscapedEntityCallback, 0, 0, 0, &err);
            break;
    }

    ASSERT(U_SUCCESS(err));
    if (U_FAILURE(err))
        return CString();

    Vector<char> result;
    size_t size = 0;
    do {
        char buffer[ConversionBufferSize];
        char* target = buffer;
        char* targetLimit = target + ConversionBufferSize;
        err = U_ZERO_ERROR;
        ucnv_fromUnicode(m_converterICU, &target, targetLimit, &source, sourceLimit, 0, true, &err);
        size_t count = target - buffer;
        result.grow(size + count);
        memcpy(result.data() + size, buffer, count);
        size += count;
    } while (err == U_BUFFER_OVERFLOW_ERROR);

    return CString(result.data(), size);
}

} // namespace WebCore
