/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include <array>
#include <unicode/ucnv_cb.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace PAL {

const size_t ConversionBufferSize = 16384;

#define DECLARE_ALIASES(encoding, ...) \
    static const char* const encoding##_aliases[] { __VA_ARGS__ }

// From https://encoding.spec.whatwg.org. Plus a few extra aliases that macOS had historically from TEC.
DECLARE_ALIASES(ISO_8859_2, "csisolatin2", "iso-ir-101", "iso8859-2", "iso88592", "iso_8859-2", "iso_8859-2:1987", "l2", "latin2");
DECLARE_ALIASES(ISO_8859_4, "csisolatin4", "iso-ir-110", "iso8859-4", "iso88594", "iso_8859-4", "iso_8859-4:1988", "l4", "latin4");
DECLARE_ALIASES(ISO_8859_5, "csisolatincyrillic", "cyrillic", "iso-ir-144", "iso8859-5", "iso88595", "iso_8859-5", "iso_8859-5:1988");
DECLARE_ALIASES(ISO_8859_10, "csisolatin6", "iso-ir-157", "iso8859-10", "iso885910", "l6", "latin6", "iso8859101992", "isoir157");
DECLARE_ALIASES(ISO_8859_13, "iso8859-13", "iso885913");
DECLARE_ALIASES(ISO_8859_14, "iso8859-14", "iso885914", "isoceltic", "iso8859141998", "isoir199", "latin8", "l8");
DECLARE_ALIASES(ISO_8859_15, "csisolatin9", "iso8859-15", "iso885915", "iso_8859-15", "l9");
DECLARE_ALIASES(ISO_8859_16, "isoir226", "iso8859162001", "l10", "latin10");
DECLARE_ALIASES(KOI8_R, "cskoi8r", "koi", "koi8", "koi8_r");
DECLARE_ALIASES(macintosh, "csmacintosh", "mac", "x-mac-roman", "macroman", "x-macroman");
DECLARE_ALIASES(windows_1250, "cp1250", "x-cp1250", "winlatin2");
DECLARE_ALIASES(windows_1251, "cp1251", "wincyrillic", "x-cp1251");
DECLARE_ALIASES(windows_1254, "winturkish", "cp1254", "csisolatin5", "iso-8859-9", "iso-ir-148", "iso8859-9", "iso88599", "iso_8859-9", "iso_8859-9:1989", "l5", "latin5", "x-cp1254");
DECLARE_ALIASES(windows_1256, "winarabic", "cp1256", "x-cp1256");
DECLARE_ALIASES(windows_1258, "winvietnamese", "cp1258", "x-cp1258");
DECLARE_ALIASES(x_mac_cyrillic, "maccyrillic", "x-mac-ukrainian", "windows-10007", "mac-cyrillic", "maccy", "x-MacCyrillic", "x-MacUkraine");
// Encodings below are not in the standard.
DECLARE_ALIASES(x_mac_greek, "windows-10006", "macgr", "x-MacGreek");
DECLARE_ALIASES(x_mac_centraleurroman, "windows-10029", "x-mac-ce", "macce", "maccentraleurope", "x-MacCentralEurope");
DECLARE_ALIASES(x_mac_turkish, "windows-10081", "mactr", "x-MacTurkish");

#define DECLARE_ENCODING_NAME(encoding, alias_array) \
    { encoding, std::size(alias_array##_aliases), alias_array##_aliases }

#define DECLARE_ENCODING_NAME_NO_ALIASES(encoding) \
    { encoding, 0, nullptr }

static const struct EncodingName {
    const char* name;
    unsigned aliasCount;
    const char* const * aliases;
} encodingNames[] = {
    DECLARE_ENCODING_NAME("ISO-8859-2", ISO_8859_2),
    DECLARE_ENCODING_NAME("ISO-8859-4", ISO_8859_4),
    DECLARE_ENCODING_NAME("ISO-8859-5", ISO_8859_5),
    DECLARE_ENCODING_NAME("ISO-8859-10", ISO_8859_10),
    DECLARE_ENCODING_NAME("ISO-8859-13", ISO_8859_13),
    DECLARE_ENCODING_NAME("ISO-8859-14", ISO_8859_14),
    DECLARE_ENCODING_NAME("ISO-8859-15", ISO_8859_15),
    DECLARE_ENCODING_NAME("ISO-8859-16", ISO_8859_16),
    DECLARE_ENCODING_NAME("KOI8-R", KOI8_R),
    DECLARE_ENCODING_NAME("macintosh", macintosh),
    DECLARE_ENCODING_NAME("windows-1250", windows_1250),
    DECLARE_ENCODING_NAME("windows-1251", windows_1251),
    DECLARE_ENCODING_NAME("windows-1254", windows_1254),
    DECLARE_ENCODING_NAME("windows-1256", windows_1256),
    DECLARE_ENCODING_NAME("windows-1258", windows_1258),
    DECLARE_ENCODING_NAME("x-mac-cyrillic", x_mac_cyrillic),
    // Encodings below are not in the standard.
    DECLARE_ENCODING_NAME("x-mac-greek", x_mac_greek),
    DECLARE_ENCODING_NAME("x-mac-centraleurroman", x_mac_centraleurroman),
    DECLARE_ENCODING_NAME("x-mac-turkish", x_mac_turkish),
    DECLARE_ENCODING_NAME_NO_ALIASES("EUC-TW"),
};

void TextCodecICU::registerEncodingNames(EncodingNameRegistrar registrar)
{
    for (auto& encodingName : encodingNames) {
        registrar(encodingName.name, encodingName.name);
        for (size_t i = 0; i < encodingName.aliasCount; ++i)
            registrar(encodingName.aliases[i], encodingName.name);
    }
}

void TextCodecICU::registerCodecs(TextCodecRegistrar registrar)
{
    for (auto& encodingName : encodingNames) {
        const char* name = encodingName.name;

        UErrorCode error = U_ZERO_ERROR;
        const char* canonicalConverterName = ucnv_getCanonicalName(name, "IANA", &error);
        ASSERT(U_SUCCESS(error));
        if (!canonicalConverterName) {
            auto converter = ICUConverterPtr { ucnv_open(name, &error) };
            ASSERT(U_SUCCESS(error));
            canonicalConverterName = ucnv_getName(converter.get(), &error);
            ASSERT(U_SUCCESS(error));
            if (!canonicalConverterName) {
                ASSERT_NOT_REACHED();
                continue;
            }
        }
        registrar(name, [name, canonicalConverterName] {
            return makeUnique<TextCodecICU>(name, canonicalConverterName);
        });
    }
}

TextCodecICU::TextCodecICU(const char* encoding, const char* canonicalConverterName)
    : m_encodingName(encoding)
    , m_canonicalConverterName(canonicalConverterName)
{
    ASSERT(m_canonicalConverterName);
}

TextCodecICU::~TextCodecICU()
{
    if (m_converter) {
        ucnv_reset(m_converter.get());
        threadGlobalData().cachedConverterICU().converter = WTFMove(m_converter);
    }
}

void TextCodecICU::createICUConverter() const
{
    ASSERT(!m_converter);

    auto& cachedConverter = threadGlobalData().cachedConverterICU().converter;
    if (cachedConverter) {
        UErrorCode error = U_ZERO_ERROR;
        const char* cachedConverterName = ucnv_getName(cachedConverter.get(), &error);
        if (U_SUCCESS(error) && !strcmp(m_canonicalConverterName, cachedConverterName)) {
            m_converter = WTFMove(cachedConverter);
            return;
        }
    }

    UErrorCode error = U_ZERO_ERROR;
    m_converter = ICUConverterPtr { ucnv_open(m_canonicalConverterName, &error) };
    if (m_converter)
        ucnv_setFallback(m_converter.get(), true);
}

int TextCodecICU::decodeToBuffer(UChar* target, UChar* targetLimit, const char*& source, const char* sourceLimit, int32_t* offsets, bool flush, UErrorCode& error)
{
    UChar* targetStart = target;
    error = U_ZERO_ERROR;
    ucnv_toUnicode(m_converter.get(), &target, targetLimit, &source, sourceLimit, offsets, flush, &error);
    return target - targetStart;
}

class ErrorCallbackSetter {
public:
    ErrorCallbackSetter(UConverter& converter, bool stopOnError)
        : m_converter(converter)
        , m_shouldStopOnEncodingErrors(stopOnError)
    {
        if (m_shouldStopOnEncodingErrors) {
            UErrorCode err = U_ZERO_ERROR;
            ucnv_setToUCallBack(&m_converter, UCNV_TO_U_CALLBACK_SUBSTITUTE, UCNV_SUB_STOP_ON_ILLEGAL, &m_savedAction, &m_savedContext, &err);
            ASSERT(U_SUCCESS(err));
        }
    }
    ~ErrorCallbackSetter()
    {
        if (m_shouldStopOnEncodingErrors) {
            UErrorCode err = U_ZERO_ERROR;
            const void* oldContext;
            UConverterToUCallback oldAction;
            ucnv_setToUCallBack(&m_converter, m_savedAction, m_savedContext, &oldAction, &oldContext, &err);
            ASSERT(oldAction == UCNV_TO_U_CALLBACK_SUBSTITUTE);
            ASSERT(!strcmp(static_cast<const char*>(oldContext), UCNV_SUB_STOP_ON_ILLEGAL));
            ASSERT(U_SUCCESS(err));
        }
    }

private:
    UConverter& m_converter;
    bool m_shouldStopOnEncodingErrors;
    const void* m_savedContext;
    UConverterToUCallback m_savedAction;
};

String TextCodecICU::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    // Get a converter for the passed-in encoding.
    if (!m_converter) {
        createICUConverter();
        if (!m_converter) {
            LOG_ERROR("error creating ICU encoder even though encoding was in table");
            sawError = true;
            return { };
        }
    }
    
    ErrorCallbackSetter callbackSetter(*m_converter, stopOnError);

    StringBuilder result;

    UChar buffer[ConversionBufferSize];
    UChar* bufferLimit = buffer + ConversionBufferSize;
    const char* source = reinterpret_cast<const char*>(bytes);
    const char* sourceLimit = source + length;
    int32_t* offsets = nullptr;
    UErrorCode err = U_ZERO_ERROR;

    do {
        int ucharsDecoded = decodeToBuffer(buffer, bufferLimit, source, sourceLimit, offsets, flush, err);
        result.appendCharacters(buffer, ucharsDecoded);
    } while (needsToGrowToProduceBuffer(err));

    if (U_FAILURE(err)) {
        // flush the converter so it can be reused, and not be bothered by this error.
        do {
            decodeToBuffer(buffer, bufferLimit, source, sourceLimit, offsets, true, err);
        } while (source < sourceLimit);
        sawError = true;
    }

    String resultString = result.toString();

    return resultString;
}

// Invalid character handler when writing escaped entities for unrepresentable
// characters. See the declaration of TextCodec::encode for more.
static void urlEscapedEntityCallback(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
    UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* error)
{
    if (reason == UCNV_UNASSIGNED) {
        *error = U_ZERO_ERROR;
        UnencodableReplacementArray entity;
        int entityLen = TextCodec::getUnencodableReplacement(codePoint, UnencodableHandling::URLEncodedEntities, entity);
        ucnv_cbFromUWriteBytes(fromUArgs, entity.data(), entityLen, 0, error);
    } else
        UCNV_FROM_U_CALLBACK_ESCAPE(context, fromUArgs, codeUnits, length, codePoint, reason, error);
}

Vector<uint8_t> TextCodecICU::encode(StringView string, UnencodableHandling handling) const
{
    if (string.isEmpty())
        return { };

    if (!m_converter) {
        createICUConverter();
        if (!m_converter)
            return { };
    }

    // FIXME: We should see if there is "force ASCII range" mode in ICU;
    // until then, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    String copy;
    if (shouldShowBackslashAsCurrencySymbolIn(m_encodingName)) {
        copy = makeStringByReplacingAll(string, '\\', yenSign);
        string = copy;
    }

    UErrorCode error;
    switch (handling) {
    case UnencodableHandling::QuestionMarks:
        error = U_ZERO_ERROR;
        ucnv_setSubstChars(m_converter.get(), "?", 1, &error);
        if (U_FAILURE(error))
            return { };
        error = U_ZERO_ERROR;
        ucnv_setFromUCallBack(m_converter.get(), UCNV_FROM_U_CALLBACK_SUBSTITUTE, 0, 0, 0, &error);
        if (U_FAILURE(error))
            return { };
        break;
    case UnencodableHandling::Entities:
        error = U_ZERO_ERROR;
        ucnv_setFromUCallBack(m_converter.get(), UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_DEC, 0, 0, &error);
        if (U_FAILURE(error))
            return { };
        break;
    case UnencodableHandling::URLEncodedEntities:
        error = U_ZERO_ERROR;
        ucnv_setFromUCallBack(m_converter.get(), urlEscapedEntityCallback, 0, 0, 0, &error);
        if (U_FAILURE(error))
            return { };
        break;
    }

    auto upconvertedCharacters = string.upconvertedCharacters();
    auto* source = upconvertedCharacters.get();
    auto* sourceLimit = source + string.length();

    Vector<uint8_t> result;
    do {
        char buffer[ConversionBufferSize];
        char* target = buffer;
        char* targetLimit = target + ConversionBufferSize;
        error = U_ZERO_ERROR;
        ucnv_fromUnicode(m_converter.get(), &target, targetLimit, &source, sourceLimit, 0, true, &error);
        result.append(reinterpret_cast<uint8_t*>(buffer), target - buffer);
    } while (needsToGrowToProduceBuffer(error));
    return result;
}

} // namespace PAL
