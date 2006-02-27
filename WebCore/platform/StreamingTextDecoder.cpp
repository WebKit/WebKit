/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "StreamingTextDecoder.h"

#include <kxmlcore/Assertions.h>
#include <algorithm>

using std::min;

namespace WebCore {

StreamingTextDecoder::StreamingTextDecoder(const TextEncoding& encoding)
    : m_encoding(encoding)
    , m_littleEndian(encoding.flags() & LittleEndian)
    , m_atStart(true)
    , m_error(false)
    , m_numBufferedBytes(0)
    , m_converterICU(0)
{
}

QString StreamingTextDecoder::convertLatin1(const unsigned char* s, int length)
{
    ASSERT(m_numBufferedBytes == 0);
    return QString(reinterpret_cast<const char *>(s), length);
}

static const UChar replacementCharacter = 0xFFFD;
static const UChar BOM = 0xFEFF;
static const int ConversionBufferSize = 16384;
    
static UConverter* cachedConverterICU;
static TextEncodingID cachedConverterEncoding = InvalidEncoding;

StreamingTextDecoder::~StreamingTextDecoder()
{
    if (m_converterICU) {
        if (cachedConverterICU != 0)
            ucnv_close(cachedConverterICU);
        cachedConverterICU = m_converterICU;
        cachedConverterEncoding = m_encoding.encodingID();
    }
}

QString StreamingTextDecoder::convertUTF16(const unsigned char *s, int length)
{
    ASSERT(m_numBufferedBytes == 0 || m_numBufferedBytes == 1);

    const unsigned char *p = s;
    unsigned len = length;
    
    QString result("");
    
    result.reserve(length / 2);

    if (m_numBufferedBytes != 0 && len != 0) {
        ASSERT(m_numBufferedBytes == 1);
        UChar c;
        if (m_littleEndian)
            c = m_bufferedBytes[0] | (p[0] << 8);
        else
            c = (m_bufferedBytes[0] << 8) | p[0];

        if (c)
            result.append(reinterpret_cast<QChar *>(&c), 1);

        m_numBufferedBytes = 0;
        p += 1;
        len -= 1;
    }
    
    while (len > 1) {
        UChar buffer[ConversionBufferSize];
        int runLength = min(len / 2, sizeof(buffer) / sizeof(buffer[0]));
        int bufferLength = 0;
        if (m_littleEndian) {
            for (int i = 0; i < runLength; ++i) {
                UChar c = p[0] | (p[1] << 8);
                p += 2;
                if (c && c != BOM)
                    buffer[bufferLength++] = c;
            }
        } else {
            for (int i = 0; i < runLength; ++i) {
                UChar c = (p[0] << 8) | p[1];
                p += 2;
                if (c && c != BOM)
                    buffer[bufferLength++] = c;
            }
        }
        result.append(reinterpret_cast<QChar *>(buffer), bufferLength);
        len -= runLength * 2;
    }
    
    if (len) {
        ASSERT(m_numBufferedBytes == 0);
        m_numBufferedBytes = 1;
        m_bufferedBytes[0] = p[0];
    }
    
    return result;
}

static inline TextEncoding effectiveEncoding(const TextEncoding& encoding)
{
    TextEncodingID id = encoding.encodingID();
    if (id == Latin1Encoding || id == ASCIIEncoding)
        id = WinLatin1Encoding;
    return TextEncoding(id, encoding.flags());
}

UErrorCode StreamingTextDecoder::createICUConverter()
{
    TextEncoding encoding = effectiveEncoding(m_encoding);
    const char* encodingName = encoding.name();

    bool cachedEncodingEqual = cachedConverterEncoding == encoding.encodingID();
    cachedConverterEncoding = InvalidEncoding;

    if (cachedEncodingEqual && cachedConverterICU) {
        m_converterICU = cachedConverterICU;
        cachedConverterICU = 0;
    } else {    
        UErrorCode err = U_ZERO_ERROR;
        ASSERT(!m_converterICU);
        m_converterICU = ucnv_open(encodingName, &err);
        if (err == U_AMBIGUOUS_ALIAS_WARNING)
            LOG_ERROR("ICU ambiguous alias warning for encoding: %s", encodingName);

        if (!m_converterICU) {
            LOG_ERROR("the ICU Converter won't convert from text encoding 0x%X, error %d", encoding.encodingID(), err);
            return err;
        }
    }
    
    return U_ZERO_ERROR;
}

// We strip replacement characters because the ICU converter for UTF-8 converts
// invalid sequences into replacement characters, but other browsers discard them.
// We strip BOM characters because they can show up both at the start of content
// and inside content, and we never want them to end up in the decoded text.
static inline bool unwanted(UChar c)
{
    return c == replacementCharacter || c == BOM;
}

void StreamingTextDecoder::appendOmittingUnwanted(QString &s, const UChar *characters, int byteCount)
{
    ASSERT(byteCount % sizeof(UChar) == 0);
    int start = 0;
    int characterCount = byteCount / sizeof(UChar);
    for (int i = 0; i != characterCount; ++i) {
        if (unwanted(characters[i])) {
            if (start != i)
                s.append(reinterpret_cast<const QChar *>(&characters[start]), i - start);
            start = i + 1;
        }
    }
    if (start != characterCount)
        s.append(reinterpret_cast<const QChar *>(&characters[start]), characterCount - start);
}

QString StreamingTextDecoder::convertUsingICU(const unsigned char *chs, int len, bool flush)
{
    // Get a converter for the passed-in encoding.
    if (!m_converterICU && U_FAILURE(createICUConverter()))
        return QString();

    ASSERT(m_converterICU);

    QString result("");
    result.reserve(len);

    UChar buffer[ConversionBufferSize];
    const char *source = reinterpret_cast<const char *>(chs);
    const char *sourceLimit = source + len;
    int32_t *offsets = NULL;
    UErrorCode err;
    
    do {
        UChar *target = buffer;
        const UChar *targetLimit = target + ConversionBufferSize;
        err = U_ZERO_ERROR;
        ucnv_toUnicode(m_converterICU, &target, targetLimit, &source, sourceLimit, offsets, flush, &err);
        int count = target - buffer;
        appendOmittingUnwanted(result, reinterpret_cast<const UChar *>(buffer), count * sizeof(UChar));
    } while (err == U_BUFFER_OVERFLOW_ERROR);

    if (U_FAILURE(err)) {
        // flush the converter so it can be reused, and not be bothered by this error.
        do {
            UChar *target = buffer;
            const UChar *targetLimit = target + ConversionBufferSize;
            err = U_ZERO_ERROR;
            ucnv_toUnicode(m_converterICU, &target, targetLimit, &source, sourceLimit, offsets, true, &err);
        } while (source < sourceLimit);
        LOG_ERROR("ICU conversion error");
        return QString();
    }
    
    return result;
}

QString StreamingTextDecoder::convert(const unsigned char *chs, int len, bool flush)
{
    //#define PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE 1000

    switch (m_encoding.encodingID()) {
    case Latin1Encoding:
    case WinLatin1Encoding:
        return convertLatin1(chs, len);

    case UTF16Encoding:
        return convertUTF16(chs, len);

    default:
#if PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE
        QString result;
        int chunkSize;
        for (int i = 0; i != len; i += chunkSize) {
            chunkSize = len - i;
            if (chunkSize > PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE) {
                chunkSize = PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE;
            }
            result += convertUsingICU(chs + i, chunkSize, flush && (i + chunkSize == len));
        }
        return result;
#else
        return convertUsingICU(chs, len, flush);
#endif
    }
    ASSERT_NOT_REACHED();
    return QString();
}

QString StreamingTextDecoder::toUnicode(const char *chs, int len, bool flush)
{
    ASSERT_ARG(len, len >= 0);
    
    if (m_error || !chs)
        return QString();

    if (len <= 0 && !flush)
        return "";

    // Handle normal case.
    if (!m_atStart)
        return convert(chs, len, flush);

    // Check to see if we found a BOM.
    int numBufferedBytes = m_numBufferedBytes;
    int buf1Len = numBufferedBytes;
    int buf2Len = len;
    const unsigned char *buf1 = m_bufferedBytes;
    const unsigned char *buf2 = reinterpret_cast<const unsigned char *>(chs);
    unsigned char c1 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c2 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c3 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    int BOMLength = 0;
    if (c1 == 0xFF && c2 == 0xFE) {
        m_encoding = TextEncoding(UTF16Encoding, LittleEndian);
        m_littleEndian = true;
        BOMLength = 2;
    } else if (c1 == 0xFE && c2 == 0xFF) {
        m_encoding = TextEncoding(UTF16Encoding, BigEndian);
        m_littleEndian = false;
        BOMLength = 2;
    } else if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
        m_encoding = TextEncoding(UTF8Encoding);
        BOMLength = 3;
    }

    // Handle case where we found a BOM.
    if (BOMLength != 0) {
        ASSERT(numBufferedBytes + len >= BOMLength);
        int skip = BOMLength - numBufferedBytes;
        m_numBufferedBytes = 0;
        m_atStart = false;
        return len == skip ? QString("") : convert(chs + skip, len - skip, flush);
    }

    // Handle case where we know there is no BOM coming.
    const int bufferSize = sizeof(m_bufferedBytes);
    if (numBufferedBytes + len > bufferSize || flush) {
        m_atStart = false;
        if (numBufferedBytes == 0) {
            return convert(chs, len, flush);
        }
        unsigned char bufferedBytes[sizeof(m_bufferedBytes)];
        memcpy(bufferedBytes, m_bufferedBytes, numBufferedBytes);
        m_numBufferedBytes = 0;
        return convert(bufferedBytes, numBufferedBytes, false) + convert(chs, len, flush);
    }

    // Continue to look for the BOM.
    memcpy(&m_bufferedBytes[numBufferedBytes], chs, len);
    m_numBufferedBytes += len;
    return "";
}
    
} // namespace WebCore
