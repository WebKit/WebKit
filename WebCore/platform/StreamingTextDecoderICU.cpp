/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "StreamingTextDecoderICU.h"

#include <wtf/Assertions.h>
#include <unicode/unorm.h>

using std::min;

namespace WebCore {

StreamingTextDecoderICU::StreamingTextDecoderICU(const TextEncoding& encoding)
    : m_encoding(encoding)
    , m_littleEndian(encoding.flags() & LittleEndian)
    , m_atStart(true)
    , m_numBufferedBytes(0)
    , m_converterICU(0)
{
}

static const UChar BOM = 0xFEFF;
static const size_t ConversionBufferSize = 16384;
    
static UConverter* cachedConverterICU;
static TextEncodingID cachedConverterEncoding = InvalidEncoding;

StreamingTextDecoderICU::~StreamingTextDecoderICU()
{
    releaseICUConverter();
}

void StreamingTextDecoderICU::releaseICUConverter()
{
    if (m_converterICU) {
        if (cachedConverterICU != 0)
            ucnv_close(cachedConverterICU);
        cachedConverterICU = m_converterICU;
        cachedConverterEncoding = m_encoding.encodingID();
        m_converterICU = 0;
    }
}

bool StreamingTextDecoderICU::textEncodingSupported()
{
    if (!m_converterICU)
        createICUConverter();
    
    return m_converterICU;
}

DeprecatedString StreamingTextDecoderICU::convertUTF16(const unsigned char* s, int length)
{
    ASSERT(m_numBufferedBytes == 0 || m_numBufferedBytes == 1);

    const unsigned char* p = s;
    size_t len = length;
    
    DeprecatedString result("");
    
    result.reserve(length / 2);

    if (m_numBufferedBytes != 0 && len != 0) {
        ASSERT(m_numBufferedBytes == 1);
        UChar c;
        if (m_littleEndian)
            c = m_bufferedBytes[0] | (p[0] << 8);
        else
            c = (m_bufferedBytes[0] << 8) | p[0];

        if (c)
            result.append(reinterpret_cast<DeprecatedChar*>(&c), 1);

        m_numBufferedBytes = 0;
        p += 1;
        len -= 1;
    }
    
    while (len > 1) {
        UChar buffer[ConversionBufferSize];
        int runLength = min(len / 2, ConversionBufferSize);
        int bufferLength = 0;
        if (m_littleEndian) {
            for (int i = 0; i < runLength; ++i) {
                UChar c = p[0] | (p[1] << 8);
                p += 2;
                if (c != BOM)
                    buffer[bufferLength++] = c;
            }
        } else {
            for (int i = 0; i < runLength; ++i) {
                UChar c = (p[0] << 8) | p[1];
                p += 2;
                if (c != BOM)
                    buffer[bufferLength++] = c;
            }
        }
        result.append(reinterpret_cast<DeprecatedChar*>(buffer), bufferLength);
        len -= runLength * 2;
    }
    
    if (len) {
        ASSERT(m_numBufferedBytes == 0);
        m_numBufferedBytes = 1;
        m_bufferedBytes[0] = p[0];
    }
    
    return result;
}

bool StreamingTextDecoderICU::convertIfASCII(const unsigned char* s, int length, DeprecatedString& str)
{
    ASSERT(m_numBufferedBytes == 0 || m_numBufferedBytes == 1);

    DeprecatedString result("");
    result.reserve(length);

    const unsigned char* p = s;
    size_t len = length;
    unsigned char ored = 0;
    while (len) {
        UChar buffer[ConversionBufferSize];
        int runLength = min(len, ConversionBufferSize);
        int bufferLength = 0;
        for (int i = 0; i < runLength; ++i) {
            unsigned char c = *p++;
            ored |= c;
            buffer[bufferLength++] = c;
        }
        if (ored & 0x80)
            return false;
        result.append(reinterpret_cast<DeprecatedChar*>(buffer), bufferLength);
        len -= runLength;
    }

    str = result;
    return true;
}

void StreamingTextDecoderICU::createICUConverter()
{
    TextEncoding encoding = m_encoding.effectiveEncoding();
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
#if !LOG_DISABLED
        if (err == U_AMBIGUOUS_ALIAS_WARNING)
            LOG_ERROR("ICU ambiguous alias warning for encoding: %s", encodingName);
#endif
    }
}

// We strip BOM characters because they can show up both at the start of content
// and inside content, and we never want them to end up in the decoded text.
void StreamingTextDecoderICU::appendOmittingBOM(DeprecatedString& s, const UChar* characters, int byteCount)
{
    ASSERT(byteCount % sizeof(UChar) == 0);
    int start = 0;
    int characterCount = byteCount / sizeof(UChar);
    for (int i = 0; i != characterCount; ++i) {
        if (BOM == characters[i]) {
            if (start != i)
                s.append(reinterpret_cast<const DeprecatedChar*>(&characters[start]), i - start);
            start = i + 1;
        }
    }
    if (start != characterCount)
        s.append(reinterpret_cast<const DeprecatedChar*>(&characters[start]), characterCount - start);
}

DeprecatedString StreamingTextDecoderICU::convertUsingICU(const unsigned char* chs, int len, bool flush)
{
    // Get a converter for the passed-in encoding.
    if (!m_converterICU) {
        createICUConverter();
        if (!m_converterICU)
            return DeprecatedString();
    }

    DeprecatedString result("");
    result.reserve(len);

    UChar buffer[ConversionBufferSize];
    const char* source = reinterpret_cast<const char*>(chs);
    const char* sourceLimit = source + len;
    int32_t* offsets = NULL;
    UErrorCode err;

    do {
        UChar* target = buffer;
        const UChar* targetLimit = target + ConversionBufferSize;
        err = U_ZERO_ERROR;
        ucnv_toUnicode(m_converterICU, &target, targetLimit, &source, sourceLimit, offsets, flush, &err);
        int count = target - buffer;
        appendOmittingBOM(result, reinterpret_cast<const UChar*>(buffer), count * sizeof(UChar));
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
        return DeprecatedString();
    }

    return result;
}

DeprecatedString StreamingTextDecoderICU::convert(const unsigned char* chs, int len, bool flush)
{
    switch (m_encoding.encodingID()) {
        case UTF16Encoding:
            return convertUTF16(chs, len);

        case ASCIIEncoding:
        case Latin1Encoding:
        case WinLatin1Encoding: {
            DeprecatedString result;
            if (convertIfASCII(chs, len, result))
                return result;
            break;
        }

        case UTF8Encoding:
            // If a previous run used ICU, we might have a partly converted character.
            // If so, don't use the optimized ASCII code path.
            if (!m_converterICU) {
                DeprecatedString result;
                if (convertIfASCII(chs, len, result))
                    return result;
            }
            break;

        default:
            break;
    }

    //#define PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE 1000
#if PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE
    DeprecatedString result;
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

DeprecatedString StreamingTextDecoderICU::toUnicode(const char* chs, int len, bool flush)
{
    ASSERT_ARG(len, len >= 0);
    
    if (!chs)
        return DeprecatedString();

    if (len <= 0 && !flush)
        return "";

    // Handle normal case.
    if (!m_atStart)
        return convert(chs, len, flush);

    // Check to see if we found a BOM.
    int numBufferedBytes = m_numBufferedBytes;
    int buf1Len = numBufferedBytes;
    int buf2Len = len;
    const unsigned char* buf1 = m_bufferedBytes;
    const unsigned char* buf2 = reinterpret_cast<const unsigned char*>(chs);
    unsigned char c1 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c2 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c3 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    int BOMLength = 0;
    if (c1 == 0xFF && c2 == 0xFE) {
        if (m_encoding != TextEncoding(UTF16Encoding, LittleEndian)) {
            releaseICUConverter();
            m_encoding = TextEncoding(UTF16Encoding, LittleEndian);
            m_littleEndian = true;
        }
        BOMLength = 2;
    } else if (c1 == 0xFE && c2 == 0xFF) {
        if (m_encoding != TextEncoding(UTF16Encoding, BigEndian)) {
            releaseICUConverter();
            m_encoding = TextEncoding(UTF16Encoding, BigEndian);
            m_littleEndian = false;
        }
        BOMLength = 2;
    } else if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
        if (m_encoding != TextEncoding(UTF8Encoding)) {
            releaseICUConverter();
            m_encoding = TextEncoding(UTF8Encoding);
        }
        BOMLength = 3;
    }

    // Handle case where we found a BOM.
    if (BOMLength != 0) {
        ASSERT(numBufferedBytes + len >= BOMLength);
        int skip = BOMLength - numBufferedBytes;
        m_numBufferedBytes = 0;
        m_atStart = false;
        return len == skip ? DeprecatedString("") : convert(chs + skip, len - skip, flush);
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
    
DeprecatedCString StreamingTextDecoderICU::fromUnicode(const DeprecatedString &qcs, bool allowEntities)
{
    TextEncodingID encoding = m_encoding.effectiveEncoding().encodingID();

    if (encoding == WinLatin1Encoding && qcs.isAllLatin1())
        return qcs.latin1();

    if ((encoding == WinLatin1Encoding || encoding == UTF8Encoding || encoding == ASCIIEncoding) 
        && qcs.isAllASCII())
        return qcs.ascii();

    // FIXME: We should see if there is "force ASCII range" mode in ICU;
    // until then, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    DeprecatedString copy = qcs;
    copy.replace('\\', m_encoding.backslashAsCurrencySymbol());

    if (!m_converterICU)
        createICUConverter();
    if (!m_converterICU)
        return DeprecatedCString();

    // FIXME: when DeprecatedString buffer is latin1, it would be nice to
    // convert from that w/o having to allocate a unicode buffer

    char buffer[ConversionBufferSize];
    const UChar* source = reinterpret_cast<const UChar*>(copy.unicode());
    const UChar* sourceLimit = source + copy.length();

    UErrorCode err = U_ZERO_ERROR;
    DeprecatedString normalizedString;
    if (UNORM_YES != unorm_quickCheck(source, copy.length(), UNORM_NFC, &err)) {
        normalizedString.truncate(copy.length()); // normalization to NFC rarely increases the length, so this first attempt will usually succeed
        
        int32_t normalizedLength = unorm_normalize(source, copy.length(), UNORM_NFC, 0, reinterpret_cast<UChar*>(const_cast<DeprecatedChar*>(normalizedString.unicode())), copy.length(), &err);
        if (err == U_BUFFER_OVERFLOW_ERROR) {
            err = U_ZERO_ERROR;
            normalizedString.truncate(normalizedLength);
            normalizedLength = unorm_normalize(source, copy.length(), UNORM_NFC, 0, reinterpret_cast<UChar*>(const_cast<DeprecatedChar*>(normalizedString.unicode())), normalizedLength, &err);
        }
        
        source = reinterpret_cast<const UChar*>(normalizedString.unicode());
        sourceLimit = source + normalizedLength;
    }

    DeprecatedCString result(1); // for trailing zero

    if (allowEntities)
        ucnv_setFromUCallBack(m_converterICU, UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_DEC, 0, 0, &err);
    else {
        ucnv_setSubstChars(m_converterICU, "?", 1, &err);
        ucnv_setFromUCallBack(m_converterICU, UCNV_FROM_U_CALLBACK_SUBSTITUTE, 0, 0, 0, &err);
    }

    ASSERT(U_SUCCESS(err));
    if (U_FAILURE(err))
        return DeprecatedCString();

    do {
        char* target = buffer;
        char* targetLimit = target + ConversionBufferSize;
        err = U_ZERO_ERROR;
        ucnv_fromUnicode(m_converterICU, &target, targetLimit, &source, sourceLimit, 0, true,  &err);
        int count = target - buffer;
        buffer[count] = 0;
        result.append(buffer);
    } while (err == U_BUFFER_OVERFLOW_ERROR);

    return result;
}


} // namespace WebCore
