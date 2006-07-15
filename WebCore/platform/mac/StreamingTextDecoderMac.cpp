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
#include "StreamingTextDecoderMac.h"

#include <wtf/Assertions.h>

using std::min;

namespace WebCore {

// We need to keep this version because ICU doesn't support some of the encodings that we need:
// <http://bugzilla.opendarwin.org/show_bug.cgi?id=4195>.

StreamingTextDecoderMac::StreamingTextDecoderMac(const TextEncoding& encoding)
    : m_encoding(encoding)
    , m_littleEndian(encoding.flags() & LittleEndian)
    , m_atStart(true)
    , m_error(false)
    , m_numBufferedBytes(0)
    , m_converterTEC(0)
{
}

static const UChar BOM = 0xFEFF;
static const size_t ConversionBufferSize = 16384;

static TECObjectRef cachedConverterTEC;
static TextEncodingID cachedConverterEncoding = InvalidEncoding;

StreamingTextDecoderMac::~StreamingTextDecoderMac()
{
    releaseTECConverter();
}

void StreamingTextDecoderMac::releaseTECConverter()
{
    if (m_converterTEC) {
        if (cachedConverterTEC != 0)
            TECDisposeConverter(cachedConverterTEC);
        cachedConverterTEC = m_converterTEC;
        cachedConverterEncoding = m_encoding.encodingID();
        m_converterTEC = 0;
    }
}

bool StreamingTextDecoderMac::textEncodingSupported()
{
    if (m_encoding.encodingID() == kCFStringEncodingUTF16 || 
        m_encoding.encodingID() == kCFStringEncodingUTF16BE ||
        m_encoding.encodingID() == kCFStringEncodingUTF16LE)
          return true;

    if (!m_converterTEC)
        createTECConverter();
    
    return m_converterTEC;
}

DeprecatedString StreamingTextDecoderMac::convertUTF16(const unsigned char* s, int length)
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

bool StreamingTextDecoderMac::convertIfASCII(const unsigned char* s, int length, DeprecatedString& str)
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

OSStatus StreamingTextDecoderMac::createTECConverter()
{
    const TextEncodingID encoding = m_encoding.effectiveEncoding().encodingID();

    bool cachedEncodingEqual = cachedConverterEncoding == encoding;
    cachedConverterEncoding = InvalidEncoding;

    if (cachedEncodingEqual && cachedConverterTEC) {
        m_converterTEC = cachedConverterTEC;
        cachedConverterTEC = 0;
        TECClearConverterContextInfo(m_converterTEC);
    } else {
        OSStatus status = TECCreateConverter(&m_converterTEC, encoding,
            CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat));
        if (status)
            return status;

        TECSetBasicOptions(m_converterTEC, kUnicodeForceASCIIRangeMask);
    }
    
    return noErr;
}

// We strip BOM characters because they can show up both at the start of content
// and inside content, and we never want them to end up in the decoded text.
void StreamingTextDecoderMac::appendOmittingBOM(DeprecatedString& s, const UChar* characters, int byteCount)
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

OSStatus StreamingTextDecoderMac::convertOneChunkUsingTEC(const unsigned char *inputBuffer, int inputBufferLength, int &inputLength,
    void *outputBuffer, int outputBufferLength, int &outputLength)
{
    OSStatus status;
    unsigned long bytesRead = 0;
    unsigned long bytesWritten = 0;

    if (m_numBufferedBytes != 0) {
        // Finish converting a partial character that's in our buffer.
        
        // First, fill the partial character buffer with as many bytes as are available.
        ASSERT(m_numBufferedBytes < sizeof(m_bufferedBytes));
        const int spaceInBuffer = sizeof(m_bufferedBytes) - m_numBufferedBytes;
        const int bytesToPutInBuffer = MIN(spaceInBuffer, inputBufferLength);
        ASSERT(bytesToPutInBuffer != 0);
        memcpy(m_bufferedBytes + m_numBufferedBytes, inputBuffer, bytesToPutInBuffer);

        // Now, do a conversion on the buffer.
        status = TECConvertText(m_converterTEC, m_bufferedBytes, m_numBufferedBytes + bytesToPutInBuffer, &bytesRead,
            reinterpret_cast<unsigned char *>(outputBuffer), outputBufferLength, &bytesWritten);
        ASSERT(bytesRead <= m_numBufferedBytes + bytesToPutInBuffer);

        if (status == kTECPartialCharErr && bytesRead == 0) {
            // Handle the case where the partial character was not converted.
            if (bytesToPutInBuffer >= spaceInBuffer) {
                LOG_ERROR("TECConvertText gave a kTECPartialCharErr but read none of the %u bytes in the buffer", sizeof(m_bufferedBytes));
                m_numBufferedBytes = 0;
                status = kTECUnmappableElementErr; // should never happen, but use this error code
            } else {
                // Tell the caller we read all the source bytes and keep them in the buffer.
                m_numBufferedBytes += bytesToPutInBuffer;
                bytesRead = bytesToPutInBuffer;
                status = noErr;
            }
        } else {
            // We are done with the partial character buffer.
            // Also, we have read some of the bytes from the main buffer.
            if (bytesRead > m_numBufferedBytes) {
                bytesRead -= m_numBufferedBytes;
            } else {
                LOG_ERROR("TECConvertText accepted some bytes it previously rejected with kTECPartialCharErr");
                bytesRead = 0;
            }
            m_numBufferedBytes = 0;
            if (status == kTECPartialCharErr) {
                // While there may be a partial character problem in the small buffer,
                // we have to try again and not get confused and think there is a partial
                // character problem in the large buffer.
                status = noErr;
            }
        }
    } else {
        status = TECConvertText(m_converterTEC, inputBuffer, inputBufferLength, &bytesRead,
            static_cast<unsigned char *>(outputBuffer), outputBufferLength, &bytesWritten);
        ASSERT(static_cast<int>(bytesRead) <= inputBufferLength);
    }

    // Work around bug 3351093, where sometimes we get kTECBufferBelowMinimumSizeErr instead of kTECOutputBufferFullStatus.
    if (status == kTECBufferBelowMinimumSizeErr && bytesWritten != 0) {
        status = kTECOutputBufferFullStatus;
    }

    inputLength = bytesRead;
    outputLength = bytesWritten;
    return status;
}

DeprecatedString StreamingTextDecoderMac::convertUsingTEC(const unsigned char *chs, int len, bool flush)
{
    // Get a converter for the passed-in encoding.
    if (!m_converterTEC && createTECConverter() != noErr)
        return DeprecatedString();
    
    DeprecatedString result("");

    result.reserve(len);

    const unsigned char *sourcePointer = chs;
    int sourceLength = len;
    bool bufferWasFull = false;
    UniChar buffer[ConversionBufferSize];

    while (sourceLength || bufferWasFull) {
        int bytesRead = 0;
        int bytesWritten = 0;
        OSStatus status = convertOneChunkUsingTEC(sourcePointer, sourceLength, bytesRead, buffer, sizeof(buffer), bytesWritten);
        ASSERT(bytesRead <= sourceLength);
        sourcePointer += bytesRead;
        sourceLength -= bytesRead;
        
        switch (status) {
            case noErr:
            case kTECOutputBufferFullStatus:
                break;
            case kTextMalformedInputErr:
            case kTextUndefinedElementErr:
                // FIXME: Put FFFD character into the output string in this case?
                TECClearConverterContextInfo(m_converterTEC);
                if (sourceLength) {
                    sourcePointer += 1;
                    sourceLength -= 1;
                }
                break;
            case kTECPartialCharErr: {
                // Put the partial character into the buffer.
                ASSERT(m_numBufferedBytes == 0);
                const int bufferSize = sizeof(m_numBufferedBytes);
                if (sourceLength < bufferSize) {
                    memcpy(m_bufferedBytes, sourcePointer, sourceLength);
                    m_numBufferedBytes = sourceLength;
                } else {
                    LOG_ERROR("TECConvertText gave a kTECPartialCharErr, but left %u bytes in the buffer", sourceLength);
                }
                sourceLength = 0;
                break;
            }
            default:
                LOG_ERROR("text decoding failed with error %d", status);
                m_error = true;
                return DeprecatedString();
        }

        appendOmittingBOM(result, buffer, bytesWritten);

        bufferWasFull = status == kTECOutputBufferFullStatus;
    }
    
    if (flush) {
        unsigned long bytesWritten = 0;
        TECFlushText(m_converterTEC, reinterpret_cast<unsigned char *>(buffer), sizeof(buffer), &bytesWritten);
        appendOmittingBOM(result, buffer, bytesWritten);
    }

    // Workaround for a bug in the Text Encoding Converter (see bug 3225472).
    // Simplified Chinese pages use the code U+A3A0 to mean "full-width space".
    // But GB18030 decodes it to U+E5E5, which is correct in theory but not in practice.
    // To work around, just change all occurences of U+E5E5 to U+3000 (ideographic space).
    if (m_encoding.encodingID() == kCFStringEncodingGB_18030_2000)
        result.replace(0xE5E5, 0x3000);
    
    return result;
}

DeprecatedString StreamingTextDecoderMac::convert(const unsigned char *chs, int len, bool flush)
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
            // If a previous run used TEC, we might have a partly converted character.
            // If so, don't use the optimized ASCII code path.
            if (!m_converterTEC) {
                DeprecatedString result;
                if (convertIfASCII(chs, len, result))
                    return result;
            }
            break;

        default:
            break;
    }

    return convertUsingTEC(chs, len, flush);
}

DeprecatedString StreamingTextDecoderMac::toUnicode(const char* chs, int len, bool flush)
{
    ASSERT_ARG(len, len >= 0);
    
    if (m_error || !chs)
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
            releaseTECConverter();
            m_encoding = TextEncoding(UTF16Encoding, LittleEndian);
            m_littleEndian = true;
        }
        BOMLength = 2;
    } else if (c1 == 0xFE && c2 == 0xFF) {
        if (m_encoding != TextEncoding(UTF16Encoding, BigEndian)) {
            releaseTECConverter();
            m_encoding = TextEncoding(UTF16Encoding, BigEndian);
            m_littleEndian = false;
        }
        BOMLength = 2;
    } else if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
        if (m_encoding != TextEncoding(UTF8Encoding)) {
            releaseTECConverter();
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

DeprecatedCString StreamingTextDecoderMac::fromUnicode(const DeprecatedString &qcs, bool allowEntities)
{
    // FIXME: We should really use the same API in both directions.
    // Currently we use TEC to decode and CFString to encode; it would be better to encode with TEC too.
    
    TextEncoding encoding = m_encoding.effectiveEncoding();

    // FIXME: Since there's no "force ASCII range" mode in CFString, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    DeprecatedString copy = qcs;
    copy.replace('\\', encoding.backslashAsCurrencySymbol());
    CFStringRef cfs = copy.getCFString();
    CFMutableStringRef cfms = CFStringCreateMutableCopy(0, 0, cfs); // in rare cases, normalization can make the string longer, thus no limit on its length
    CFStringNormalize(cfms, kCFStringNormalizationFormC);
    
    CFIndex startPos = 0;
    CFIndex charactersLeft = CFStringGetLength(cfms);
    DeprecatedCString result(1); // for trailing zero

    while (charactersLeft > 0) {
        CFRange range = CFRangeMake(startPos, charactersLeft);
        CFIndex bufferLength;
        CFStringGetBytes(cfms, range, encoding.encodingID(), allowEntities ? 0 : '?', false, NULL, 0x7FFFFFFF, &bufferLength);
        
        DeprecatedCString chunk(bufferLength + 1);
        unsigned char *buffer = reinterpret_cast<unsigned char *>(chunk.data());
        CFIndex charactersConverted = CFStringGetBytes(cfms, range, encoding.encodingID(), allowEntities ? 0 : '?', false, buffer, bufferLength, &bufferLength);
        buffer[bufferLength] = 0;
        result.append(chunk);
        
        if (charactersConverted != charactersLeft) {
            unsigned int badChar = CFStringGetCharacterAtIndex(cfms, startPos + charactersConverted);
            ++charactersConverted;

            if ((badChar & 0xfc00) == 0xd800 &&     // is high surrogate
                  charactersConverted != charactersLeft) {
                UniChar low = CFStringGetCharacterAtIndex(cfms, startPos + charactersConverted);
                if ((low & 0xfc00) == 0xdc00) {     // is low surrogate
                    badChar <<= 10;
                    badChar += low;
                    badChar += 0x10000 - (0xd800 << 10) - 0xdc00;
                    ++charactersConverted;
                }
            }
            char buf[16];
            sprintf(buf, "&#%u;", badChar);
            result.append(buf);
        }
        
        startPos += charactersConverted;
        charactersLeft -= charactersConverted;
    }
    CFRelease(cfms);
    return result;
}

} // namespace WebCore
