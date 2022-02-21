/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextCodecReplacement.h"

#include <wtf/Function.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

namespace PAL {

void TextCodecReplacement::registerEncodingNames(EncodingNameRegistrar registrar)
{
    registrar("replacement", "replacement");

    registrar("csiso2022kr", "replacement");
    registrar("hz-gb-2312", "replacement");
    registrar("iso-2022-cn", "replacement");
    registrar("iso-2022-cn-ext", "replacement");
    registrar("iso-2022-kr", "replacement");
}

void TextCodecReplacement::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("replacement", [] {
        return makeUnique<TextCodecReplacement>();
    });
}

String TextCodecReplacement::decode(const char*, size_t, bool, bool, bool& sawError)
{
    sawError = true;
    if (m_sentEOF)
        return emptyString();
    m_sentEOF = true;
    return String { &replacementCharacter, 1 };
}

Vector<uint8_t> TextCodecReplacement::encode(StringView string, UnencodableHandling unencodableHandling) const
{
    return TextCodecUTF8::encodeUTF8(string, unencodableHandling);
}

} // namespace PAL
