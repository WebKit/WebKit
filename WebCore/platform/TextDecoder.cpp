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
#include "TextDecoder.h"

#include "TextEncodingRegistry.h"

// FIXME: Would be nice to also handle BOM for UTF-7 and UTF-32.

namespace WebCore {

TextDecoder::TextDecoder(const TextEncoding& encoding)
    : m_encoding(encoding)
    , m_checkedForBOM(false)
    , m_numBufferedBytes(0)
{
}

void TextDecoder::reset(const TextEncoding& encoding)
{
    m_encoding = encoding;
    m_codec.clear();
    m_checkedForBOM = false;
    m_numBufferedBytes = 0;
}

String TextDecoder::checkForBOM(const char* data, size_t length, bool flush)
{
    // Check to see if we found a BOM.
    size_t numBufferedBytes = m_numBufferedBytes;
    size_t buf1Len = numBufferedBytes;
    size_t buf2Len = length;
    const unsigned char* buf1 = m_bufferedBytes;
    const unsigned char* buf2 = reinterpret_cast<const unsigned char*>(data);
    unsigned char c1 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c2 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c3 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c4 = buf2Len ? (--buf2Len, *buf2++) : 0;

    const TextEncoding* encodingConsideringBOM = &m_encoding;
    bool foundBOM = true;
    if (c1 == 0xFF && c2 == 0xFE) {
        if (c3 != 0 || c4 != 0) 
            encodingConsideringBOM = &UTF16LittleEndianEncoding();
        else if (numBufferedBytes + length > sizeof(m_bufferedBytes))
            encodingConsideringBOM = &UTF32LittleEndianEncoding();
        else
            foundBOM = false;
    }
    else if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)
        encodingConsideringBOM = &UTF8Encoding();
    else if (c1 == 0xFE && c2 == 0xFF)
        encodingConsideringBOM = &UTF16BigEndianEncoding();
    else if (c1 == 0 && c2 == 0 && c3 == 0xFE && c4 == 0xFF)
        encodingConsideringBOM = &UTF32BigEndianEncoding();
    else
        foundBOM = false;
    if (!foundBOM && numBufferedBytes + length <= sizeof(m_bufferedBytes) && !flush) {
        // Continue to look for the BOM.
        memcpy(&m_bufferedBytes[numBufferedBytes], data, length);
        m_numBufferedBytes += length;
        return "";
    }

    // Done checking for BOM.
    m_codec.set(newTextCodec(*encodingConsideringBOM).release());
    if (!m_codec)
        return String();
    m_checkedForBOM = true;

    // Handle case where we have some buffered bytes to deal with.
    if (numBufferedBytes) {
        char bufferedBytes[sizeof(m_bufferedBytes)];
        memcpy(bufferedBytes, m_bufferedBytes, numBufferedBytes);
        m_numBufferedBytes = 0;
        return m_codec->decode(bufferedBytes, numBufferedBytes, false)
            + m_codec->decode(data, length, flush);
    }

    return m_codec->decode(data, length, flush);
}

} // namespace WebCore
