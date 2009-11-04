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

#include "CString.h"
#include "PlatformString.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/GOwnPtr.h>
#include "Logging.h"

using std::min;

namespace WebCore {

// TextCodec's appendOmittingBOM() is gone (http://trac.webkit.org/changeset/33380). 
// That's why we need to avoid generating extra BOM's for the conversion result.
// This can be achieved by specifying the UTF-16 codecs' endianness explicitly when initializing GLib.

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    const gchar* WebCore::TextCodecGtk::m_internalEncodingName = "UTF-16BE";
#else 
    const gchar* WebCore::TextCodecGtk::m_internalEncodingName = "UTF-16LE";
#endif


// We're specifying the list of text codecs and their aliases here. 
// For each codec the first entry is the canonical name, remaining ones are used as aliases.
// Each alias list must be terminated by a 0.

// Unicode
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_UTF_8            = { "UTF-8", 0 }; 

// Western
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_8859_1       = { "ISO-8859-1", "CP819", "IBM819", "ISO-IR-100", "ISO8859-1", "ISO_8859-1", "ISO_8859-1:1987",  "L1", "LATIN1", "CSISOLATIN1", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_MACROMAN         = { "MACROMAN", "MAC", "MACINTOSH", "CSMACINTOSH", 0 };

// Japanese
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_SHIFT_JIS        = { "Shift_JIS", "MS_KANJI", "SHIFT-JIS", "SJIS", "CSSHIFTJIS", 0 };
    TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_EUC_JP       = { "EUC-JP", "EUC_JP", "EUCJP", "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE", "CSEUCPKDFMTJAPANESE", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_2022_JP      = { "ISO-2022-JP", 0 };

// Traditional Chinese
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_BIG5             = { "BIG5", "BIG-5", "BIG-FIVE", "BIG5", "BIGFIVE", "CN-BIG5", "CSBIG5", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_BIG5_HKSCS       = { "BIG5-HKSCS", "BIG5-HKSCS:2004", "BIG5HKSCS", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP950            = { "CP950", 0 };

// Korean
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_2022_KR      = { "ISO-2022-KR", "CSISO2022KR", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP949            = { "CP949", "UHC", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_EUC_KR           = { "EUC-KR", "CSEUCKR", 0 };

// Arabic
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_8859_6       = { "ISO-8859-6", "ARABIC", "ASMO-708", "ECMA-114", "ISO-IR-127", "ISO8859-6", "ISO_8859-6", "ISO_8859-6:1987", "CSISOLATINARABIC", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP1256           = { "windows-1256", "CP1256", "MS-ARAB", 0 }; // rearranged, windows-1256 now declared the canonical name and put to lowercase to fix /fast/encoding/ahram-org-eg.html test case

// Hebrew
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_8859_8       = { "ISO-8859-8", "HEBREW", "ISO-8859-8", "ISO-IR-138", "ISO8859-8", "ISO_8859-8", "ISO_8859-8:1988", "CSISOLATINHEBREW", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP1255           = { "windows-1255", "CP1255", "MS-HEBR", 0 }; // rearranged, moved windows-1255 as canonical and lowercased, fixing /fast/encoding/meta-charset.html

// Greek
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_8859_7       = { "ISO-8859-7", "ECMA-118", "ELOT_928", "GREEK", "GREEK8", "ISO-IR-126", "ISO8859-7", "ISO_8859-7", "ISO_8859-7:1987", "ISO_8859-7:2003", "CSI", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP869            = { "CP869", "869", "CP-GR", "IBM869", "CSIBM869", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_WINDOWS_1253     = { "WINDOWS-1253", 0 };

// Cyrillic
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_8859_5       = { "ISO-8859-5", "CYRILLIC", "ISO-IR-144", "ISO8859-5", "ISO_8859-5", "ISO_8859-5:1988", "CSISOLATINCYRILLIC", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_KOI8_R           = { "KOI8-R", "CSKOI8R", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP866            = { "CP866", "866", "IBM866", "CSIBM866", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_KOI8_U           = { "KOI8-U", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_WINDOWS_1251     = { "windows-1251", "CP1251", 0 }; // CP1251 added to pass /fast/encoding/charset-cp1251.html
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_MACCYRILLIC      = { "mac-cyrillic", "MACCYRILLIC", "x-mac-cyrillic", 0 }; 

// Thai
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP874            = { "CP874", "WINDOWS-874", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_TIS_620          = { "TIS-620", 0 };

// Simplified Chinese
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_GBK              = { "GBK", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_HZ               = { "HZ", "HZ-GB-2312", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_GB18030          = { "GB18030", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_EUC_CN           = { "EUC-CN", "EUCCN", "GB2312", "CN-GB", "CSGB2312", "EUC_CN", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_2312_80          = { "GB_2312-80", "CHINESE", "csISO58GB231280", "GB2312.1980-0", "ISO-IR-58" };

// Central European
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_8859_2       = { "ISO-8859-2", "ISO-IR-101", "ISO8859-2", "ISO_8859-2", "ISO_8859-2:1987", "L2", "LATIN2", "CSISOLATIN2", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP1250           = { "CP1250", "MS-EE", "WINDOWS-1250", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_MACCENTRALEUROPE = { "MAC-CENTRALEUROPE", 0 };

// Vietnamese
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP1258           = { "CP1258", "WINDOWS-1258", 0 };

// Turkish
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP1254           = { "CP1254", "MS-TURK", "WINDOWS-1254", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_8859_9       = { "ISO-8859-9", "ISO-IR-148", "ISO8859-9", "ISO_8859-9", "ISO_8859-9:1989", "L5", "LATIN5", "CSISOLATIN5", 0 };

// Baltic
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_CP1257           = { "CP1257", "WINBALTRIM", "WINDOWS-1257", 0 };
TextCodecGtk::codecAliasList TextCodecGtk::m_codecAliases_ISO_8859_4       = { "ISO-8859-4", "ISO-IR-110", "ISO8859-4", "ISO_8859-4", "ISO_8859-4:1988", "L4", "LATIN4", "CSISOLATIN4", 0 };

gconstpointer const TextCodecGtk::m_iconvBaseCodecList[] = { 
    // Unicode
    &m_codecAliases_UTF_8,

    // Western
    &m_codecAliases_ISO_8859_1
};

gconstpointer const TextCodecGtk::m_iconvExtendedCodecList[] = 
{
    // Western
    &m_codecAliases_MACROMAN,

    // Japanese
    &m_codecAliases_SHIFT_JIS,
    &m_codecAliases_EUC_JP,
    &m_codecAliases_ISO_2022_JP,

    // Simplified Chinese
    &m_codecAliases_BIG5,
    &m_codecAliases_BIG5_HKSCS,
    &m_codecAliases_CP950,

    // Korean
    &m_codecAliases_ISO_2022_KR,
    &m_codecAliases_CP949,
    &m_codecAliases_EUC_KR,

    // Arabic
    &m_codecAliases_ISO_8859_6,
    &m_codecAliases_CP1256,
    
    // Hebrew
    &m_codecAliases_ISO_8859_8,
    &m_codecAliases_CP1255,
    
    // Greek
    &m_codecAliases_ISO_8859_7,
    &m_codecAliases_CP869,
    &m_codecAliases_WINDOWS_1253,
    
    // Cyrillic
    &m_codecAliases_ISO_8859_5,
    &m_codecAliases_KOI8_R,
    &m_codecAliases_CP866,
    &m_codecAliases_KOI8_U,
    &m_codecAliases_WINDOWS_1251,
    &m_codecAliases_MACCYRILLIC,
    
    // Thai
    &m_codecAliases_CP874,
    &m_codecAliases_TIS_620,
    
    // Traditional Chinese
    &m_codecAliases_GBK,
    &m_codecAliases_HZ,
    &m_codecAliases_GB18030,
    &m_codecAliases_EUC_CN,
    &m_codecAliases_2312_80,
    
    // Central European
    &m_codecAliases_ISO_8859_2,
    &m_codecAliases_CP1250,
    &m_codecAliases_MACCENTRALEUROPE,
    
    // Vietnamese
    &m_codecAliases_CP1258,
    
    // Turkish
    &m_codecAliases_CP1254,
    &m_codecAliases_ISO_8859_9,
    
    // Baltic
    &m_codecAliases_CP1257,
    &m_codecAliases_ISO_8859_4
};


const size_t ConversionBufferSize = 16384;
    

static PassOwnPtr<TextCodec> newTextCodecGtk(const TextEncoding& encoding, const void*)
{
    return new TextCodecGtk(encoding);
}

gboolean TextCodecGtk::isEncodingAvailable(const gchar* encName)
{
    GIConv tester;
    // test decoding
    tester = g_iconv_open(m_internalEncodingName, encName);
    if (tester == reinterpret_cast<GIConv>(-1)) {
        return false;
    } else {
        g_iconv_close(tester);
        // test encoding
        tester = g_iconv_open(encName, m_internalEncodingName);
        if (tester == reinterpret_cast<GIConv>(-1)) {
            return false;
        } else {
            g_iconv_close(tester);
            return true;
        }
    }
}

void TextCodecGtk::registerEncodingNames(EncodingNameRegistrar registrar, bool extended)
{
    const void* const* encodingList;
    unsigned int listLength = 0;
    if (extended) {
        encodingList = m_iconvExtendedCodecList;
        listLength = sizeof(m_iconvExtendedCodecList)/sizeof(gpointer);
    } else {
        encodingList = m_iconvBaseCodecList;
        listLength = sizeof(m_iconvBaseCodecList)/sizeof(gpointer);
    }

    for (unsigned int i = 0; i < listLength; ++i) {
        codecAliasList *codecAliases = static_cast<codecAliasList*>(encodingList[i]);
        
        // Our convention is, the first entry in codecAliases is the canonical name, 
        // see above in the list of declarations. 
        // Probe GLib for this one first. If it's not available, we skip the whole group of aliases.

        int codecCount = 0;
        const char *canonicalName;
        canonicalName = (*codecAliases)[codecCount];

        if(!isEncodingAvailable(canonicalName)) {
            LOG(TextConversion, "Canonical encoding %s not available, skipping.", canonicalName);
            continue;
        }
        registrar(canonicalName, canonicalName);

        const char *currentAlias;
        while ((currentAlias = (*codecAliases)[++codecCount])) {
            if (isEncodingAvailable(currentAlias)) {
                LOG(TextConversion, "Registering encoding name alias %s to canonical %s", currentAlias, canonicalName);
                registrar(currentAlias, canonicalName);
            }
        }

    }
}

void TextCodecGtk::registerCodecs(TextCodecRegistrar registrar, bool extended)
{
    const void* const* encodingList;
    unsigned int listLength = 0;
    if (extended) {
        encodingList = m_iconvExtendedCodecList;
        listLength = sizeof(m_iconvExtendedCodecList)/sizeof(gpointer);
    } else {
        encodingList = m_iconvBaseCodecList;
        listLength = sizeof(m_iconvBaseCodecList)/sizeof(gpointer);
    }

    for (unsigned int i = 0; i < listLength; ++i) {
        codecAliasList *codecAliases = static_cast<codecAliasList*>(encodingList[i]);
        // by convention, the first "alias" should be the canonical name, see the definition of the alias lists 
        const gchar *codecName = (*codecAliases)[0];
        if (isEncodingAvailable(codecName))
            registrar(codecName, newTextCodecGtk, 0);
    }
}

void TextCodecGtk::registerBaseEncodingNames(EncodingNameRegistrar registrar)
{
    registerEncodingNames(registrar, false);
}

void TextCodecGtk::registerBaseCodecs(TextCodecRegistrar registrar)
{
    registerCodecs(registrar, false);
}

void TextCodecGtk::registerExtendedEncodingNames(EncodingNameRegistrar registrar)
{
    registerEncodingNames(registrar, true);
}

void TextCodecGtk::registerExtendedCodecs(TextCodecRegistrar registrar)
{
    registerCodecs(registrar, true);
}

TextCodecGtk::TextCodecGtk(const TextEncoding& encoding)
    : m_encoding(encoding)
    , m_numBufferedBytes(0)
    , m_iconvDecoder(reinterpret_cast<GIConv>(-1))
    , m_iconvEncoder(reinterpret_cast<GIConv>(-1))
{
}

TextCodecGtk::~TextCodecGtk()
{
    if (m_iconvDecoder != reinterpret_cast<GIConv>(-1)) {
        g_iconv_close(m_iconvDecoder);
        m_iconvDecoder = reinterpret_cast<GIConv>(-1);
    }
    if (m_iconvEncoder != reinterpret_cast<GIConv>(-1)) {
        g_iconv_close(m_iconvEncoder);
        m_iconvEncoder = reinterpret_cast<GIConv>(-1);
    }
}

void TextCodecGtk::createIConvDecoder() const
{
    ASSERT(m_iconvDecoder == reinterpret_cast<GIConv>(-1));

    m_iconvDecoder = g_iconv_open(m_internalEncodingName, m_encoding.name());
}

void TextCodecGtk::createIConvEncoder() const
{
    ASSERT(m_iconvDecoder == reinterpret_cast<GIConv>(-1));

    m_iconvEncoder = g_iconv_open(m_encoding.name(), m_internalEncodingName);
}

String TextCodecGtk::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    // Get a converter for the passed-in encoding.
    if (m_iconvDecoder == reinterpret_cast<GIConv>(-1)) {
        createIConvDecoder();
        ASSERT(m_iconvDecoder != reinterpret_cast<GIConv>(-1));
        if (m_iconvDecoder == reinterpret_cast<GIConv>(-1)) {
            LOG_ERROR("Error creating IConv encoder even though encoding was in table.");
            return String();
        }
    }

    size_t countWritten, countRead, conversionLength;
    const char* conversionBytes;
    char* prefixedBytes = 0;

    if (m_numBufferedBytes) {
        conversionLength = length + m_numBufferedBytes;
        prefixedBytes = static_cast<char*>(fastMalloc(conversionLength));
        memcpy(prefixedBytes, m_bufferedBytes, m_numBufferedBytes);
        memcpy(prefixedBytes + m_numBufferedBytes, bytes, length);
        
        conversionBytes = prefixedBytes;
        
        // all buffered bytes are consumed now
        m_numBufferedBytes = 0;
    } else {
        // no previously buffered partial data, 
        // just convert the data that was passed in
        conversionBytes = bytes;
        conversionLength = length;
    }

    GOwnPtr<GError> err;
    GOwnPtr<UChar> buffer;

    buffer.outPtr() = reinterpret_cast<UChar*>(g_convert_with_iconv(conversionBytes, conversionLength, m_iconvDecoder, &countRead, &countWritten, &err.outPtr())); 


    if (err) {
        LOG_ERROR("GIConv conversion error, Code %d: \"%s\"", err->code, err->message);
        m_numBufferedBytes = 0; // reset state for subsequent calls to decode
        fastFree(prefixedBytes);
        sawError = true;
        return String();
    }
    
    // Partial input at the end of the string may not result in an error being raised. 
    // From the gnome library documentation on g_convert_with_iconv:
    // "Even if the conversion was successful, this may be less than len if there were partial characters at the end of the input."
    // That's why we need to compare conversionLength against countRead 

    m_numBufferedBytes = conversionLength - countRead;
    if (m_numBufferedBytes > 0) {
        if (flush) {
            LOG_ERROR("Partial bytes at end of input while flush requested.");
            m_numBufferedBytes = 0; // reset state for subsequent calls to decode
            fastFree(prefixedBytes);
            sawError = true;
            return String();
        }
        memcpy(m_bufferedBytes, conversionBytes + countRead, m_numBufferedBytes);
    }

    fastFree(prefixedBytes);
    
    Vector<UChar> result;

    result.append(buffer.get(), countWritten / sizeof(UChar));

    return String::adopt(result);
}

CString TextCodecGtk::encode(const UChar* characters, size_t length, UnencodableHandling handling)
{
    if (!length)
        return "";

    if (m_iconvEncoder == reinterpret_cast<GIConv>(-1))
        createIConvEncoder();
    if (m_iconvEncoder == reinterpret_cast<GIConv>(-1))
        return CString();

    size_t count;

    GOwnPtr<GError> err;
    GOwnPtr<char> buffer;

    buffer.outPtr() = g_convert_with_iconv(reinterpret_cast<const char*>(characters), length * sizeof(UChar), m_iconvEncoder, 0, &count, &err.outPtr());
    if (err) {
        LOG_ERROR("GIConv conversion error, Code %d: \"%s\"", err->code, err->message);
        return CString();
    }

    return CString(buffer.get(), count);
}

} // namespace WebCore
