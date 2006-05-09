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
#include "TextEncoding.h"

#include "CharsetNames.h"
#include <kxmlcore/Assertions.h>
#include <kxmlcore/HashSet.h>
#include "StreamingTextDecoder.h"
#include <unicode/unorm.h>

namespace WebCore {

TextEncoding::TextEncoding(const char* name, bool eightBitOnly)
{
    m_encodingID = textEncodingIDFromCharsetName(name, &m_flags);
    if (eightBitOnly && m_encodingID == UTF16Encoding)
        m_encodingID = UTF8Encoding;
}

const char* TextEncoding::name() const
{
    return charsetNameFromTextEncodingID(m_encodingID);
}

QChar TextEncoding::backslashAsCurrencySymbol() const
{
    if (m_flags & BackslashIsYen)
        return 0x00A5; // yen sign
 
    return '\\';
}

DeprecatedString TextEncoding::toUnicode(const char *chs, int len) const
{
    return StreamingTextDecoder(*this).toUnicode(chs, len, true);
}

DeprecatedString TextEncoding::toUnicode(const DeprecatedByteArray &qba, int len) const
{
    return StreamingTextDecoder(*this).toUnicode(qba, len, true);
}

// We'd like to use ICU for this on OS X as well eventually, but we need to make sure
// it covers all the encodings that we need
#ifndef __APPLE__

static UConverter* cachedConverter;
static TextEncodingID cachedConverterEncoding = InvalidEncoding;

static const int ConversionBufferSize = 16384;

static inline UConverter* getConverter(TextEncodingID encoding, UErrorCode* status)
{
    if (cachedConverter && encoding == cachedConverterEncoding) {
        UConverter* conv = cachedConverter;
        cachedConverter = 0;
        return conv;
    }

    const char* encodingName = charsetNameFromTextEncodingID(encoding);
    UErrorCode err = U_ZERO_ERROR;
    UConverter* conv = ucnv_open(encodingName, &err);
    if (err == U_AMBIGUOUS_ALIAS_WARNING)
        LOG_ERROR("ICU ambiguous alias warning for encoding: %s", encodingName);

    if (!conv) {
        LOG_ERROR("the ICU Converter won't convert to text encoding 0x%X, error %d", encoding, err);
        *status = err;
        return 0;
    }

    return conv;
}

static inline void cacheConverter(TextEncodingID id, UConverter* conv)
{
    if (conv) {
        if (cachedConverter)
            ucnv_close(cachedConverter);
        cachedConverter = conv;
        cachedConverterEncoding = id;
    }
}

static inline TextEncodingID effectiveEncoding(TextEncodingID encoding)
{
    if (encoding == Latin1Encoding || encoding == ASCIIEncoding)
        return WinLatin1Encoding;
    return encoding;
}

DeprecatedCString TextEncoding::fromUnicode(const DeprecatedString &qcs, bool allowEntities) const
{
    TextEncodingID encoding = effectiveEncoding(m_encodingID);

    if (encoding == WinLatin1Encoding && qcs.isAllLatin1())
        return qcs.latin1();

    if ((encoding == WinLatin1Encoding || encoding == UTF8Encoding || encoding == ASCIIEncoding) 
        && qcs.isAllASCII())
        return qcs.ascii();

    // FIXME: We should see if there is "force ASCII range" mode in ICU;
    // until then, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    DeprecatedString copy = qcs;
    copy.replace(QChar('\\'), backslashAsCurrencySymbol());

    UErrorCode err = U_ZERO_ERROR;
    UConverter* conv = getConverter(encoding, &err);
    if (!conv && U_FAILURE(err))
        return DeprecatedCString();

    ASSERT(conv);

    // FIXME: when DeprecatedString buffer is latin1, it would be nice to
    // convert from that w/o having to allocate a unicode buffer

    char buffer[ConversionBufferSize];
    const UChar* source = reinterpret_cast<const UChar*>(copy.unicode());
    const UChar* sourceLimit = source + copy.length();

    DeprecatedString normalizedString;
    if (UNORM_YES != unorm_quickCheck(source, copy.length(), UNORM_NFC, &err)) {
        normalizedString.truncate(copy.length()); // normalization to NFC rarely increases the length, so this first attempt will usually succeed
        
        int32_t normalizedLength = unorm_normalize(source, copy.length(), UNORM_NFC, 0, reinterpret_cast<UChar*>(const_cast<QChar*>(normalizedString.unicode())), copy.length(), &err);
        if (err == U_BUFFER_OVERFLOW_ERROR) {
            err = U_ZERO_ERROR;
            normalizedString.truncate(normalizedLength);
            normalizedLength = unorm_normalize(source, copy.length(), UNORM_NFC, 0, reinterpret_cast<UChar*>(const_cast<QChar*>(normalizedString.unicode())), normalizedLength, &err);
        }
        
        source = reinterpret_cast<const UChar*>(normalizedString.unicode());
        sourceLimit = source + normalizedLength;
    }

    DeprecatedCString result(1); // for trailing zero

    if (allowEntities)
        ucnv_setFromUCallBack(conv, UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_DEC, 0, 0, &err);
    else {
        ucnv_setSubstChars(conv, "?", 1, &err);
        ucnv_setFromUCallBack(conv, UCNV_FROM_U_CALLBACK_SUBSTITUTE, 0, 0, 0, &err);
    }

    ASSERT(U_SUCCESS(err));
    if (U_FAILURE(err))
        return DeprecatedCString();

    do {
        char* target = buffer;
        char* targetLimit = target + ConversionBufferSize;
        err = U_ZERO_ERROR;
        ucnv_fromUnicode(conv, &target, targetLimit, &source, sourceLimit, 0, true,  &err);
        int count = target - buffer;
        buffer[count] = 0;
        result.append(buffer);
    } while (err == U_BUFFER_OVERFLOW_ERROR);

    cacheConverter(encoding, conv);

    return result;
}
#endif


} // namespace WebCore
