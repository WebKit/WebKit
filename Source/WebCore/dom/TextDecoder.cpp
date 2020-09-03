/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <wtf/Optional.h>

namespace WebCore {

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

TextDecoder::TextDecoder(const char* label, Options options)
    : m_textEncoding(label)
    , m_options(options)
{
}

constexpr uint8_t utf8BOMBytes[3] { 0xEF, 0xBB, 0xBF };
constexpr uint8_t utf16BEBOMBytes[2] { 0xFE, 0xFF };
constexpr uint8_t utf16LEBOMBytes[2] { 0xFF, 0xFE };

size_t TextDecoder::bytesNeededForFullBOMIgnoreCheck() const
{
    if (m_textEncoding == UTF8Encoding())
        return sizeof(utf8BOMBytes);
    if (m_textEncoding == UTF16BigEndianEncoding())
        return sizeof(utf16BEBOMBytes);
    if (m_textEncoding == UTF16LittleEndianEncoding())
        return sizeof(utf16LEBOMBytes);
    return 0;
}

bool TextDecoder::isBeginningOfIncompleteBOM(const uint8_t* bytes, size_t length) const
{
    if (!length)
        return true;

    if (m_textEncoding == UTF8Encoding()) {
        if (length == 1)
            return bytes[0] == utf8BOMBytes[0];
        return length == 2 && bytes[0] == utf8BOMBytes[0] && bytes[1] == utf8BOMBytes[1];
    }
    if (m_textEncoding == UTF16BigEndianEncoding())
        return length == 1 && bytes[0] == utf16BEBOMBytes[0];
    if (m_textEncoding == UTF16LittleEndianEncoding())
        return length == 1 && bytes[0] == utf16LEBOMBytes[0];

    return false;
}

auto TextDecoder::ignoreBOMIfNecessary(const uint8_t*& data, size_t& length, bool stream) -> WaitForMoreBOMBytes
{
    if (m_bomIgnoredIfNecessary || m_options.ignoreBOM)
        return WaitForMoreBOMBytes::No;

    if (stream && length < bytesNeededForFullBOMIgnoreCheck()) {
        if (isBeginningOfIncompleteBOM(data, length))
            return WaitForMoreBOMBytes::Yes;
        m_bomIgnoredIfNecessary = true;
        return WaitForMoreBOMBytes::No;
    }

    if (m_textEncoding == UTF8Encoding()
        && length >= sizeof(utf8BOMBytes)
        && data[0] == utf8BOMBytes[0]
        && data[1] == utf8BOMBytes[1]
        && data[2] == utf8BOMBytes[2]) {
        data += sizeof(utf8BOMBytes);
        length -= sizeof(utf8BOMBytes);
    } else if (m_textEncoding == UTF16BigEndianEncoding()
        && length >= sizeof(utf16BEBOMBytes)
        && data[0] == utf16BEBOMBytes[0]
        && data[1] == utf16BEBOMBytes[1]) {
        data += sizeof(utf16BEBOMBytes);
        length -= sizeof(utf16BEBOMBytes);
    } else if (m_textEncoding == UTF16LittleEndianEncoding()
        && length >= sizeof(utf16LEBOMBytes)
        && data[0] == utf16LEBOMBytes[0]
        && data[1] == utf16LEBOMBytes[1]) {
        data += sizeof(utf16LEBOMBytes);
        length -= sizeof(utf16LEBOMBytes);
    }
    m_bomIgnoredIfNecessary = true;
    return WaitForMoreBOMBytes::No;
}

static size_t codeUnitByteSize(const TextEncoding& encoding)
{
    return encoding.isByteBasedEncoding() ? 1 : 2;
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

    if (!options.stream)
        m_bomIgnoredIfNecessary = false;

    bool alreadyBuffered = false;
    if (m_buffer.size()) {
        m_buffer.append(data, length);
        data = m_buffer.data();
        length = m_buffer.size();
        alreadyBuffered = true;
    }

    if (ignoreBOMIfNecessary(data, length, options.stream) == WaitForMoreBOMBytes::Yes) {
        ASSERT(options.stream);
        if (!alreadyBuffered)
            m_buffer.append(data, length);
        return String();
    }

    const bool stopOnError = true;
    bool sawError = false;
    if (length % codeUnitByteSize(m_textEncoding))
        sawError = true;
    const char* charData = reinterpret_cast<const char*>(data);
    String result;
    if (!sawError)
        result = m_textEncoding.decode(charData, length, stopOnError, sawError);

    if (sawError) {
        if (options.stream) {
            result = String();
            if (!m_buffer.size())
                m_buffer.append(data, length);
        } else {
            if (m_options.fatal)
                return Exception { TypeError };
            result = m_textEncoding.decode(charData, length);
        }
    } else
        m_buffer.clear();

    return result;
}

String TextDecoder::encoding() const
{
    return String(m_textEncoding.name()).convertToASCIILowercase();
}

}
