/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextDecoder.h"

#include "HTMLParserIdioms.h"
#include "TextCodec.h"
#include "TextEncodingRegistry.h"
#include <wtf/Optional.h>

namespace WebCore {

TextDecoder::TextDecoder(const char* label, Options options)
    : m_textEncoding(label)
    , m_options(options)
{
}

TextDecoder::~TextDecoder() = default;

ExceptionOr<Ref<TextDecoder>> TextDecoder::create(const String& label, Options options)
{
    String strippedLabel = stripLeadingAndTrailingHTMLSpaces(label);
    const UChar nullCharacter = '\0';
    if (strippedLabel.contains(nullCharacter))
        return Exception { RangeError };
    auto decoder = adoptRef(*new TextDecoder(strippedLabel.utf8().data(), options));
    if (!decoder->m_textEncoding.isValid() || !strcmp(decoder->m_textEncoding.name(), "replacement"))
        return Exception { RangeError };
    return decoder;
}

ExceptionOr<String> TextDecoder::decode(Optional<BufferSource::VariantType> input, DecodeOptions options)
{
    Optional<BufferSource> inputBuffer;
    const uint8_t* data = nullptr;
    size_t length = 0;
    if (input) {
        inputBuffer = BufferSource(WTFMove(input.value()));
        data = inputBuffer->data();
        length = inputBuffer->length();
    }

    if (!m_codec) {
        m_codec = newTextCodec(m_textEncoding);
        if (!m_options.ignoreBOM)
            m_codec->stripByteOrderMark();
    }

    bool sawError = false;
    String result = m_codec->decode(reinterpret_cast<const char*>(data), length, !options.stream, m_options.fatal, sawError);

    if (!options.stream && !m_options.ignoreBOM)
        m_codec->stripByteOrderMark();

    if (sawError && m_options.fatal)
        return Exception { TypeError };
    return result;
}

String TextDecoder::encoding() const
{
    return String(m_textEncoding.name()).convertToASCIILowercase();
}

}
