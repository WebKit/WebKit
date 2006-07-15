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
#include "StreamingTextDecoder.h"

#include <wtf/OwnPtr.h>

namespace WebCore {

TextEncoding::TextEncoding(const char* name, bool eightBitOnly)
{
    m_encodingID = textEncodingIDFromCharsetName(name, &m_flags);
    if (eightBitOnly && m_encodingID == UTF16Encoding)
        m_encodingID = UTF8Encoding;
}

TextEncoding TextEncoding::effectiveEncoding() const
{
    TextEncodingID id = m_encodingID;
    if (id == Latin1Encoding || id == ASCIIEncoding)
        id = WinLatin1Encoding;
    return TextEncoding(id, m_flags);
}

const char* TextEncoding::name() const
{
    return charsetNameFromTextEncodingID(m_encodingID);
}

UChar TextEncoding::backslashAsCurrencySymbol() const
{
    if (m_flags & BackslashIsYen)
        return 0x00A5; // yen sign
 
    return '\\';
}

DeprecatedString TextEncoding::toUnicode(const char* chs, int len) const
{
    OwnPtr<StreamingTextDecoder> decoder(StreamingTextDecoder::create(*this));
    return decoder->toUnicode(chs, len, true);
}

DeprecatedCString TextEncoding::fromUnicode(const DeprecatedString &qcs, bool allowEntities) const
{
    OwnPtr<StreamingTextDecoder> decoder(StreamingTextDecoder::create(*this));
    return decoder->fromUnicode(qcs, allowEntities);
}

} // namespace WebCore
