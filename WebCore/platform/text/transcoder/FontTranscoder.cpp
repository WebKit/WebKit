/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontTranscoder.h"

#include "CharacterNames.h"
#include "TextEncoding.h"

namespace WebCore {

FontTranscoder::FontTranscoder()
{
    m_converterTypes.add("MS PGothic", BackslashToYenSign);
    UChar unicodeNameMSPGothic[] = {0xFF2D, 0xFF33, 0x0020, 0xFF30, 0x30B4, 0x30B7, 0x30C3, 0x30AF};
    m_converterTypes.add(AtomicString(unicodeNameMSPGothic, sizeof(unicodeNameMSPGothic) / sizeof(UChar)), BackslashToYenSign);

    m_converterTypes.add("MS PMincho", BackslashToYenSign);
    UChar unicodeNameMSPMincho[] = {0xFF2D, 0xFF33, 0x0020, 0xFF30, 0x660E, 0x671D};
    m_converterTypes.add(AtomicString(unicodeNameMSPMincho, sizeof(unicodeNameMSPMincho) / sizeof(UChar)), BackslashToYenSign);

    m_converterTypes.add("MS Gothic", BackslashToYenSign);
    UChar unicodeNameMSGothic[] = {0xFF2D, 0xFF33, 0x0020, 0x30B4, 0x30B7, 0x30C3, 0x30AF};
    m_converterTypes.add(AtomicString(unicodeNameMSGothic, sizeof(unicodeNameMSGothic) / sizeof(UChar)), BackslashToYenSign);

    m_converterTypes.add("MS Mincho", BackslashToYenSign);
    UChar unicodeNameMSMincho[] = {0xFF2D, 0xFF33, 0x0020, 0x660E, 0x671D};
    m_converterTypes.add(AtomicString(unicodeNameMSMincho, sizeof(unicodeNameMSMincho) / sizeof(UChar)), BackslashToYenSign);

    m_converterTypes.add("Meiryo", BackslashToYenSign);
    UChar unicodeNameMeiryo[] = {0x30E1, 0x30A4, 0x30EA, 0x30AA};
    m_converterTypes.add(AtomicString(unicodeNameMeiryo, sizeof(unicodeNameMeiryo) / sizeof(UChar)), BackslashToYenSign);
}

FontTranscoder::ConverterType FontTranscoder::converterType(const AtomicString& fontFamily, const TextEncoding* encoding) const
{
    if (!fontFamily.isNull()) {
        HashMap<AtomicString, ConverterType>::const_iterator found = m_converterTypes.find(fontFamily);
        if (found != m_converterTypes.end())
            return found->second;
    }

    // IE's default fonts for Japanese encodings change backslashes into yen signs.
    // FIXME: We don't need transcoding when the document explicitly
    // specifies a font which doesn't change backslashes into yen signs.
    if (encoding && encoding->backslashAsCurrencySymbol() != '\\')
        return BackslashToYenSign;

    return NoConversion;
}

void FontTranscoder::convert(String& text, const AtomicString& fontFamily, const TextEncoding* encoding) const
{
    switch (converterType(fontFamily, encoding)) {
    case BackslashToYenSign: {
        // FIXME: TextEncoding.h has similar code. We need to factor them out.
        text.replace('\\', yenSign);
        break;
    }
    case NoConversion:
    default:
        ASSERT_NOT_REACHED();
    }
}

bool FontTranscoder::needsTranscoding(const AtomicString& fontFamily, const TextEncoding* encoding) const
{
    ConverterType type = converterType(fontFamily, encoding);
    return type != NoConversion;
}

FontTranscoder& fontTranscoder()
{
    static FontTranscoder* transcoder = new FontTranscoder;
    return *transcoder;
}

} // namespace WebCore
