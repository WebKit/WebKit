/*
 * Copyright (C) 2010 Company 100, Inc. All rights reserved.
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
#include "TextCodecBrew.h"

#include "AEEAppGen.h"
#include "AEEICharsetConv.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include <wtf/Assertions.h>
#include <wtf/text/CString.h>

namespace WebCore {

// FIXME: Not sure if there are Brew MP devices which use big endian.
const char* WebCore::TextCodecBrew::m_internalEncodingName = "UTF-16LE";

static PassOwnPtr<TextCodec> newTextCodecBrew(const TextEncoding& encoding, const void*)
{
    return new TextCodecBrew(encoding);
}

void TextCodecBrew::registerExtendedEncodingNames(EncodingNameRegistrar registrar)
{
    // FIXME: Not sure how to enumerate all available encodings.
    notImplemented();
}

void TextCodecBrew::registerExtendedCodecs(TextCodecRegistrar registrar)
{
    notImplemented();
}

TextCodecBrew::TextCodecBrew(const TextEncoding& encoding)
    : m_charsetConverter(0)
    , m_encoding(encoding)
    , m_numBufferedBytes(0)
{
    String format = String::format("%s>%s", encoding.name(), m_internalEncodingName);

    IShell* shell = reinterpret_cast<AEEApplet*>(GETAPPINSTANCE())->m_pIShell;
    AEECLSID classID = ISHELL_GetHandler(shell, AEEIID_ICharsetConv, format.latin1().data());
    ISHELL_CreateInstance(shell, classID, reinterpret_cast<void**>(&m_charsetConverter));

    ASSERT(m_charsetConverter);
}

TextCodecBrew::~TextCodecBrew()
{
    if (m_charsetConverter)
        ICharsetConv_Release(m_charsetConverter);
}

String TextCodecBrew::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    int code = ICharsetConv_Initialize(m_charsetConverter, m_encoding.name(), m_internalEncodingName, 0);
    ASSERT(code == AEE_SUCCESS);

    Vector<UChar> result;
    Vector<unsigned char> prefixedBytes(length);

    int srcSize;
    unsigned char* srcBegin;

    if (m_numBufferedBytes) {
        srcSize = length + m_numBufferedBytes;
        prefixedBytes.grow(srcSize);
        memcpy(prefixedBytes.data(), m_bufferedBytes, m_numBufferedBytes);
        memcpy(prefixedBytes.data() + m_numBufferedBytes, bytes, length);

        srcBegin = prefixedBytes.data();

        // all buffered bytes are consumed now
        m_numBufferedBytes = 0;
    } else {
        srcSize = length;
        srcBegin = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(bytes));
    }

    unsigned char* src = srcBegin;
    unsigned char* srcEnd = srcBegin + srcSize;

    Vector<UChar> dstBuffer(srcSize);

    while (src < srcEnd) {
        int numCharsConverted;
        unsigned char* dstBegin = reinterpret_cast<unsigned char*>(dstBuffer.data());
        unsigned char* dst = dstBegin;
        int dstSize = dstBuffer.size() * sizeof(UChar);

        code = ICharsetConv_CharsetConvert(m_charsetConverter, &src, &srcSize, &dst, &dstSize, &numCharsConverted);
        ASSERT(code != AEE_ENOSUCH);

        if (code == AEE_EBUFFERTOOSMALL) {
            // Increase the buffer and try it again.
            dstBuffer.grow(dstBuffer.size() * 2);
            continue;
        }

        if (code == AEE_EBADITEM) {
            sawError = true;
            if (stopOnError) {
                result.append(L'?');
                break;
            }

            src++;
        }

        if (code == AEE_EINCOMPLETEITEM) {
            if (flush) {
                LOG_ERROR("Partial bytes at end of input while flush requested.");
                sawError = true;
                return String();
            }

            m_numBufferedBytes = srcEnd - src;
            memcpy(m_bufferedBytes, src, m_numBufferedBytes);
            break;
        }

        int numChars = (dst - dstBegin) / sizeof(UChar);
        if (numChars > 0)
            result.append(dstBuffer.data(), numChars);
    }

    return String::adopt(result);
}

CString TextCodecBrew::encode(const UChar* characters, size_t length, UnencodableHandling handling)
{
    if (!length)
        return "";

    unsigned int replacementCharacter = '?';

    // FIXME: Impossible to handle EntitiesForUnencodables or URLEncodedEntitiesForUnencodables with ICharsetConv.
    int code = ICharsetConv_Initialize(m_charsetConverter, m_internalEncodingName, m_encoding.name(), replacementCharacter);
    ASSERT(code == AEE_SUCCESS);

    Vector<char> result;

    int srcSize = length * sizeof(UChar);
    unsigned char* srcBegin = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(characters));
    unsigned char* src = srcBegin;
    unsigned char* srcEnd = srcBegin + srcSize;

    Vector<unsigned char> dstBuffer(length * sizeof(UChar));

    while (src < srcEnd) {
        int numCharsConverted;
        unsigned char* dstBegin = dstBuffer.data();
        unsigned char* dst = dstBegin;
        int dstSize = dstBuffer.size();

        code = ICharsetConv_CharsetConvert(m_charsetConverter, &src, &srcSize, &dst, &dstSize, &numCharsConverted);
        ASSERT(code != AEE_EINCOMPLETEITEM);

        if (code == AEE_ENOSUCH) {
            LOG_ERROR("Conversion error, Code=%d", code);
            return CString();
        }

        if (code == AEE_EBUFFERTOOSMALL) {
            // Increase the buffer and try it again.
            dstBuffer.grow(dstBuffer.size() * 2);
            continue;
        }

        if (code == AEE_EBADITEM)
            src += sizeof(UChar); // Skip the invalid character

        int numBytes = dst - dstBegin;
        if (numBytes > 0)
            result.append(dstBuffer.data(), numBytes);
    }

    return CString(result.data(), result.size());
}

} // namespace WebCore
