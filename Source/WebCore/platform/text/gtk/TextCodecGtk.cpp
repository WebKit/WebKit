/*
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Jürg Billeter <j@bitron.ch>
 * Copyright (C) 2009 Dominik Röttsches <dominik.roettsches@access-company.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TextCodecGtk.h"

#include <gio/gio.h>
#include <wtf/gobject/GOwnPtr.h>
#include "Logging.h"
#include "PlatformString.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>

using std::min;

namespace WebCore {

// TextCodec's appendOmittingBOM() is gone (http://trac.webkit.org/changeset/33380). 
// That's why we need to avoid generating extra BOM's for the conversion result.
// This can be achieved by specifying the UTF-16 codecs' endianness explicitly when initializing GLib.

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
static const gchar* internalEncodingName = "UTF-16BE";
#else
static const gchar* internalEncodingName = "UTF-16LE";
#endif


const size_t ConversionBufferSize = 16384;
    

static PassOwnPtr<TextCodec> newTextCodecGtk(const TextEncoding& encoding, const void*)
{
    return adoptPtr(new TextCodecGtk(encoding));
}

static bool isEncodingAvailable(const gchar* encodingName)
{
    GIConv tester;
    // test decoding
    tester = g_iconv_open(internalEncodingName, encodingName);
    if (tester == reinterpret_cast<GIConv>(-1)) {
        return false;
    } else {
        g_iconv_close(tester);
        // test encoding
        tester = g_iconv_open(encodingName, internalEncodingName);
        if (tester == reinterpret_cast<GIConv>(-1)) {
            return false;
        } else {
            g_iconv_close(tester);
            return true;
        }
    }
}

static bool registerEncodingNameIfAvailable(EncodingNameRegistrar registrar, const char* canonicalName)
{
    if (isEncodingAvailable(canonicalName)) {
        registrar(canonicalName, canonicalName);
        return true;
    }

    return false;
}

static void registerEncodingAliasIfAvailable(EncodingNameRegistrar registrar, const char* canonicalName, const char* aliasName)
{
    if (isEncodingAvailable(aliasName))
        registrar(aliasName, canonicalName);
}

static void registerCodecIfAvailable(TextCodecRegistrar registrar, const char* codecName)
{
    if (isEncodingAvailable(codecName))
        registrar(codecName, newTextCodecGtk, 0);
}

void TextCodecGtk::registerBaseEncodingNames(EncodingNameRegistrar registrar)
{
    // Unicode
    registerEncodingNameIfAvailable(registrar, "UTF-8");
    registerEncodingNameIfAvailable(registrar, "UTF-32");
    registerEncodingNameIfAvailable(registrar, "UTF-32BE");
    registerEncodingNameIfAvailable(registrar, "UTF-32LE");

    // Western
    if (registerEncodingNameIfAvailable(registrar, "ISO-8859-1")) {
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "CP819");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "IBM819");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "ISO-IR-100");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "ISO8859-1");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "ISO_8859-1");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "ISO_8859-1:1987");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "L1");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "LATIN1");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-1", "CSISOLATIN1");
    }
}

void TextCodecGtk::registerBaseCodecs(TextCodecRegistrar registrar)
{
    // Unicode
    registerCodecIfAvailable(registrar, "UTF-8");
    registerCodecIfAvailable(registrar, "UTF-32");
    registerCodecIfAvailable(registrar, "UTF-32BE");
    registerCodecIfAvailable(registrar, "UTF-32LE");

    // Western
    registerCodecIfAvailable(registrar, "ISO-8859-1");
}

void TextCodecGtk::registerExtendedEncodingNames(EncodingNameRegistrar registrar)
{
    // Western
    if (registerEncodingNameIfAvailable(registrar, "MACROMAN")) {
        registerEncodingAliasIfAvailable(registrar, "MACROMAN", "MAC");
        registerEncodingAliasIfAvailable(registrar, "MACROMAN", "MACINTOSH");
        registerEncodingAliasIfAvailable(registrar, "MACROMAN", "CSMACINTOSH");
    }

    // Japanese
    if (registerEncodingNameIfAvailable(registrar, "Shift_JIS")) {
        registerEncodingAliasIfAvailable(registrar, "Shift_JIS", "MS_KANJI");
        registerEncodingAliasIfAvailable(registrar, "Shift_JIS", "SHIFT-JIS");
        registerEncodingAliasIfAvailable(registrar, "Shift_JIS", "SJIS");
        registerEncodingAliasIfAvailable(registrar, "Shift_JIS", "CSSHIFTJIS");
    }
    if (registerEncodingNameIfAvailable(registrar, "EUC-JP")) {
        registerEncodingAliasIfAvailable(registrar, "EUC-JP", "EUC_JP");
        registerEncodingAliasIfAvailable(registrar, "EUC-JP", "EUCJP");
        registerEncodingAliasIfAvailable(registrar, "EUC-JP", "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE");
        registerEncodingAliasIfAvailable(registrar, "EUC-JP", "CSEUCPKDFMTJAPANESE");
    }
    registerEncodingNameIfAvailable(registrar, "ISO-2022-JP");

    // Traditional Chinese
    if (registerEncodingNameIfAvailable(registrar, "BIG5")) {
        registerEncodingAliasIfAvailable(registrar, "BIG5", "BIG-5");
        registerEncodingAliasIfAvailable(registrar, "BIG5", "BIG-FIVE");
        registerEncodingAliasIfAvailable(registrar, "BIG5", "BIGFIVE");
        registerEncodingAliasIfAvailable(registrar, "BIG5", "CN-BIG5");
        registerEncodingAliasIfAvailable(registrar, "BIG5", "CSBIG5");
    }
    if (registerEncodingNameIfAvailable(registrar, "BIG5-HKSCS")) {
        registerEncodingAliasIfAvailable(registrar, "BIG5-HKSCS", "BIG5-HKSCS:2004");
        registerEncodingAliasIfAvailable(registrar, "BIG5-HKSCS", "BIG5HKSCS");
    }
    registerEncodingNameIfAvailable(registrar, "CP950");

    // Korean
    if (registerEncodingNameIfAvailable(registrar, "ISO-2022-KR"))
        registerEncodingAliasIfAvailable(registrar, "ISO-2022-KR", "CSISO2022KR");
    if (registerEncodingNameIfAvailable(registrar, "CP949"))
        registerEncodingAliasIfAvailable(registrar, "CP949", "UHC");
    if (registerEncodingNameIfAvailable(registrar, "EUC-KR"))
        registerEncodingAliasIfAvailable(registrar, "EUC-KR", "CSEUCKR");

    // Arabic
    if (registerEncodingNameIfAvailable(registrar, "ISO-8859-6")) {
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-6", "ARABIC");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-6", "ASMO-708");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-6", "ECMA-114");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-6", "ISO-IR-127");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-6", "ISO8859-6");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-6", "ISO_8859-6");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-6", "ISO_8859-6:1987");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-6", "CSISOLATINARABIC");
    }
    // rearranged, windows-1256 now declared the canonical name and put to lowercase to fix /fast/encoding/ahram-org-eg.html test case
    if (registerEncodingNameIfAvailable(registrar, "windows-1256")) {
        registerEncodingAliasIfAvailable(registrar, "windows-1256", "CP1256");
        registerEncodingAliasIfAvailable(registrar, "windows-1256", "MS-ARAB");
    }

    // Hebrew
    if (registerEncodingNameIfAvailable(registrar, "ISO-8859-8")) {
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-8", "HEBREW");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-8", "ISO-8859-8");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-8", "ISO-IR-138");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-8", "ISO8859-8");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-8", "ISO_8859-8");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-8", "ISO_8859-8:1988");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-8", "CSISOLATINHEBREW");
    }
    // rearranged, moved windows-1255 as canonical and lowercased, fixing /fast/encoding/meta-charset.html
    if (registerEncodingNameIfAvailable(registrar, "windows-1255")) {
        registerEncodingAliasIfAvailable(registrar, "windows-1255", "CP1255");
        registerEncodingAliasIfAvailable(registrar, "windows-1255", "MS-HEBR");
    }

    // Greek
    if (registerEncodingNameIfAvailable(registrar, "ISO-8859-7")) {
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "ECMA-118");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "ELOT_928");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "GREEK");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "GREEK8");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "ISO-IR-126");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "ISO8859-7");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "ISO_8859-7");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "ISO_8859-7:1987");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "ISO_8859-7:2003");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-7", "CSI");
    }
    if (registerEncodingNameIfAvailable(registrar, "CP869")) {
        registerEncodingAliasIfAvailable(registrar, "CP869", "869");
        registerEncodingAliasIfAvailable(registrar, "CP869", "CP-GR");
        registerEncodingAliasIfAvailable(registrar, "CP869", "IBM869");
        registerEncodingAliasIfAvailable(registrar, "CP869", "CSIBM869");
    }
    registerEncodingNameIfAvailable(registrar, "WINDOWS-1253");

    // Cyrillic
    if (registerEncodingNameIfAvailable(registrar, "ISO-8859-5")) {
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-5", "CYRILLIC");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-5", "ISO-IR-144");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-5", "ISO8859-5");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-5", "ISO_8859-5");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-5", "ISO_8859-5:1988");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-5", "CSISOLATINCYRILLIC");
    }
    if (registerEncodingNameIfAvailable(registrar, "KOI8-R"))
        registerEncodingAliasIfAvailable(registrar, "KOI8-R", "CSKOI8R");
    if (registerEncodingNameIfAvailable(registrar, "CP866")) {
        registerEncodingAliasIfAvailable(registrar, "CP866", "866");
        registerEncodingAliasIfAvailable(registrar, "CP866", "IBM866");
        registerEncodingAliasIfAvailable(registrar, "CP866", "CSIBM866");
    }
    registerEncodingNameIfAvailable(registrar, "KOI8-U");
    // CP1251 added to pass /fast/encoding/charset-cp1251.html
    if (registerEncodingNameIfAvailable(registrar, "windows-1251"))
        registerEncodingAliasIfAvailable(registrar, "windows-1251", "CP1251");
    if (registerEncodingNameIfAvailable(registrar, "mac-cyrillic")) {
        registerEncodingAliasIfAvailable(registrar, "mac-cyrillic", "MACCYRILLIC");
        registerEncodingAliasIfAvailable(registrar, "mac-cyrillic", "x-mac-cyrillic");
    }

    // Thai
    if (registerEncodingNameIfAvailable(registrar, "CP874"))
        registerEncodingAliasIfAvailable(registrar, "CP874", "WINDOWS-874");
    registerEncodingNameIfAvailable(registrar, "TIS-620");

    // Simplified Chinese
    registerEncodingNameIfAvailable(registrar, "GBK");
    if (registerEncodingNameIfAvailable(registrar, "HZ"))
        registerEncodingAliasIfAvailable(registrar, "HZ", "HZ-GB-2312");
    registerEncodingNameIfAvailable(registrar, "GB18030");
    if (registerEncodingNameIfAvailable(registrar, "EUC-CN")) {
        registerEncodingAliasIfAvailable(registrar, "EUC-CN", "EUCCN");
        registerEncodingAliasIfAvailable(registrar, "EUC-CN", "GB2312");
        registerEncodingAliasIfAvailable(registrar, "EUC-CN", "CN-GB");
        registerEncodingAliasIfAvailable(registrar, "EUC-CN", "CSGB2312");
        registerEncodingAliasIfAvailable(registrar, "EUC-CN", "EUC_CN");
    }
    if (registerEncodingNameIfAvailable(registrar, "GB_2312-80")) {
        registerEncodingAliasIfAvailable(registrar, "GB_2312-80", "CHINESE");
        registerEncodingAliasIfAvailable(registrar, "GB_2312-80", "csISO58GB231280");
        registerEncodingAliasIfAvailable(registrar, "GB_2312-80", "GB2312.1980-0");
        registerEncodingAliasIfAvailable(registrar, "GB_2312-80", "ISO-IR-58");
    }

    // Central European
    if (registerEncodingNameIfAvailable(registrar, "ISO-8859-2")) {
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-2", "ISO-IR-101");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-2", "ISO8859-2");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-2", "ISO_8859-2");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-2", "ISO_8859-2:1987");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-2", "L2");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-2", "LATIN2");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-2", "CSISOLATIN2");
    }
    if (registerEncodingNameIfAvailable(registrar, "CP1250")) {
        registerEncodingAliasIfAvailable(registrar, "CP1250", "MS-EE");
        registerEncodingAliasIfAvailable(registrar, "CP1250", "WINDOWS-1250");
    }
    registerEncodingNameIfAvailable(registrar, "MAC-CENTRALEUROPE");

    // Vietnamese
    if (registerEncodingNameIfAvailable(registrar, "CP1258"))
        registerEncodingAliasIfAvailable(registrar, "CP1258", "WINDOWS-1258");

    // Turkish
    if (registerEncodingNameIfAvailable(registrar, "CP1254")) {
        registerEncodingAliasIfAvailable(registrar, "CP1254", "MS-TURK");
        registerEncodingAliasIfAvailable(registrar, "CP1254", "WINDOWS-1254");
    }
    if (registerEncodingNameIfAvailable(registrar, "ISO-8859-9")) {
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-9", "ISO-IR-148");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-9", "ISO8859-9");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-9", "ISO_8859-9");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-9", "ISO_8859-9:1989");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-9", "L5");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-9", "LATIN5");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-9", "CSISOLATIN5");
    }

    // Baltic
    if (registerEncodingNameIfAvailable(registrar, "CP1257")) {
        registerEncodingAliasIfAvailable(registrar, "CP1257", "WINBALTRIM");
        registerEncodingAliasIfAvailable(registrar, "CP1257", "WINDOWS-1257");
    }
    if (registerEncodingNameIfAvailable(registrar, "ISO-8859-4")) {
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-4", "ISO-IR-110");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-4", "ISO8859-4");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-4", "ISO_8859-4");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-4", "ISO_8859-4:1988");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-4", "L4");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-4", "LATIN4");
        registerEncodingAliasIfAvailable(registrar, "ISO-8859-4", "CSISOLATIN4");
    }
}

void TextCodecGtk::registerExtendedCodecs(TextCodecRegistrar registrar)
{
    // Western
    registerCodecIfAvailable(registrar, "MACROMAN");

    // Japanese
    registerCodecIfAvailable(registrar, "Shift_JIS");
    registerCodecIfAvailable(registrar, "EUC-JP");
    registerCodecIfAvailable(registrar, "ISO-2022-JP");

    // Traditional Chinese
    registerCodecIfAvailable(registrar, "BIG5");
    registerCodecIfAvailable(registrar, "BIG5-HKSCS");
    registerCodecIfAvailable(registrar, "CP950");

    // Korean
    registerCodecIfAvailable(registrar, "ISO-2022-KR");
    registerCodecIfAvailable(registrar, "CP949");
    registerCodecIfAvailable(registrar, "EUC-KR");

    // Arabic
    registerCodecIfAvailable(registrar, "ISO-8859-6");
    // rearranged, windows-1256 now declared the canonical name and put to lowercase to fix /fast/encoding/ahram-org-eg.html test case
    registerCodecIfAvailable(registrar, "windows-1256");

    // Hebrew
    registerCodecIfAvailable(registrar, "ISO-8859-8");
    // rearranged, moved windows-1255 as canonical and lowercased, fixing /fast/encoding/meta-charset.html
    registerCodecIfAvailable(registrar, "windows-1255");

    // Greek
    registerCodecIfAvailable(registrar, "ISO-8859-7");
    registerCodecIfAvailable(registrar, "CP869");
    registerCodecIfAvailable(registrar, "WINDOWS-1253");

    // Cyrillic
    registerCodecIfAvailable(registrar, "ISO-8859-5");
    registerCodecIfAvailable(registrar, "KOI8-R");
    registerCodecIfAvailable(registrar, "CP866");
    registerCodecIfAvailable(registrar, "KOI8-U");
    // CP1251 added to pass /fast/encoding/charset-cp1251.html
    registerCodecIfAvailable(registrar, "windows-1251");
    registerCodecIfAvailable(registrar, "mac-cyrillic");

    // Thai
    registerCodecIfAvailable(registrar, "CP874");
    registerCodecIfAvailable(registrar, "TIS-620");

    // Simplified Chinese
    registerCodecIfAvailable(registrar, "GBK");
    registerCodecIfAvailable(registrar, "HZ");
    registerCodecIfAvailable(registrar, "GB18030");
    registerCodecIfAvailable(registrar, "EUC-CN");
    registerCodecIfAvailable(registrar, "GB_2312-80");

    // Central European
    registerCodecIfAvailable(registrar, "ISO-8859-2");
    registerCodecIfAvailable(registrar, "CP1250");
    registerCodecIfAvailable(registrar, "MAC-CENTRALEUROPE");

    // Vietnamese
    registerCodecIfAvailable(registrar, "CP1258");

    // Turkish
    registerCodecIfAvailable(registrar, "CP1254");
    registerCodecIfAvailable(registrar, "ISO-8859-9");

    // Baltic
    registerCodecIfAvailable(registrar, "CP1257");
    registerCodecIfAvailable(registrar, "ISO-8859-4");
}

TextCodecGtk::TextCodecGtk(const TextEncoding& encoding)
    : m_encoding(encoding)
    , m_numBufferedBytes(0)
{
}

TextCodecGtk::~TextCodecGtk()
{
}

void TextCodecGtk::createIConvDecoder() const
{
    ASSERT(!m_iconvDecoder);

    m_iconvDecoder = adoptGRef(g_charset_converter_new(internalEncodingName, m_encoding.name(), 0));
}

void TextCodecGtk::createIConvEncoder() const
{
    ASSERT(!m_iconvEncoder);

    m_iconvEncoder = adoptGRef(g_charset_converter_new(m_encoding.name(), internalEncodingName, 0));
}

String TextCodecGtk::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    // Get a converter for the passed-in encoding.
    if (!m_iconvDecoder)
        createIConvDecoder();
    if (!m_iconvDecoder) {
        LOG_ERROR("Error creating IConv encoder even though encoding was in table.");
        return String();
    }

    Vector<UChar> result;

    gsize bytesRead = 0;
    gsize bytesWritten = 0;
    const gchar* input = bytes;
    gsize inputLength = length;
    gchar buffer[ConversionBufferSize];
    int flags = !length ? G_CONVERTER_INPUT_AT_END : G_CONVERTER_NO_FLAGS;
    if (flush)
        flags |= G_CONVERTER_FLUSH;

    bool bufferWasFull = false;
    char* prefixedBytes = 0;

    if (m_numBufferedBytes) {
        inputLength = length + m_numBufferedBytes;
        prefixedBytes = static_cast<char*>(fastMalloc(inputLength));
        memcpy(prefixedBytes, m_bufferedBytes, m_numBufferedBytes);
        memcpy(prefixedBytes + m_numBufferedBytes, bytes, length);

        input = prefixedBytes;

        // all buffered bytes are consumed now
        m_numBufferedBytes = 0;
    }

    do {
        GOwnPtr<GError> error;
        GConverterResult res = g_converter_convert(G_CONVERTER(m_iconvDecoder.get()),
                                                   input, inputLength,
                                                   buffer, sizeof(buffer),
                                                   static_cast<GConverterFlags>(flags),
                                                   &bytesRead, &bytesWritten,
                                                   &error.outPtr());
        input += bytesRead;
        inputLength -= bytesRead;

        if (res == G_CONVERTER_ERROR) {
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT)) {
                // There is not enough input to fully determine what the conversion should produce,
                // save it to a buffer to prepend it to the next input.
                memcpy(m_bufferedBytes, input, inputLength);
                m_numBufferedBytes = inputLength;
                inputLength = 0;
            } else if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_NO_SPACE))
                bufferWasFull = true;
            else if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_INVALID_DATA)) {
                if (stopOnError)
                    sawError = true;
                if (inputLength) {
                    // Ignore invalid character.
                    input += 1;
                    inputLength -= 1;
                }
            } else {
                sawError = true;
                LOG_ERROR("GIConv conversion error, Code %d: \"%s\"", error->code, error->message);
                m_numBufferedBytes = 0; // Reset state for subsequent calls to decode.
                fastFree(prefixedBytes);
                return String();
            }
        }

        result.append(reinterpret_cast<UChar*>(buffer), bytesWritten / sizeof(UChar));
    } while ((inputLength || bufferWasFull) && !sawError);

    fastFree(prefixedBytes);

    return String::adopt(result);
}

CString TextCodecGtk::encode(const UChar* characters, size_t length, UnencodableHandling handling)
{
    if (!length)
        return "";

    if (!m_iconvEncoder)
        createIConvEncoder();
    if (!m_iconvEncoder) {
        LOG_ERROR("Error creating IConv encoder even though encoding was in table.");
        return CString();
    }

    gsize bytesRead = 0;
    gsize bytesWritten = 0;
    const gchar* input = reinterpret_cast<const char*>(characters);
    gsize inputLength = length * sizeof(UChar);
    gchar buffer[ConversionBufferSize];
    Vector<char> result;
    GOwnPtr<GError> error;

    size_t size = 0;
    do {
        g_converter_convert(G_CONVERTER(m_iconvEncoder.get()),
                            input, inputLength,
                            buffer, sizeof(buffer),
                            G_CONVERTER_INPUT_AT_END,
                            &bytesRead, &bytesWritten,
                            &error.outPtr());
        input += bytesRead;
        inputLength -= bytesRead;
        if (bytesWritten > 0) {
            result.grow(size + bytesWritten);
            memcpy(result.data() + size, buffer, bytesWritten);
            size += bytesWritten;
        }

        if (error && g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_INVALID_DATA)) {
            UChar codePoint = reinterpret_cast<const UChar*>(input)[0];
            UnencodableReplacementArray replacement;
            int replacementLength = TextCodec::getUnencodableReplacement(codePoint, handling, replacement);

            // Consume the invalid character.
            input += sizeof(UChar);
            inputLength -= sizeof(UChar);

            // Append replacement string to result buffer.
            result.grow(size + replacementLength);
            memcpy(result.data() + size, replacement, replacementLength);
            size += replacementLength;

            error.clear();
        }
    } while (inputLength && !error.get());

    if (error) {
        LOG_ERROR("GIConv conversion error, Code %d: \"%s\"", error->code, error->message);
        return CString();
    }

    return CString(result.data(), size);
}

} // namespace WebCore
