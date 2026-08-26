/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CorpseByteParser.h"

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include <wtf/LEBDecoder.h>
#include <wtf/StdLibExtras.h>

namespace JSC {
namespace Corpse {

std::optional<uint8_t> ByteParser::consumeByte()
{
    if (m_position >= m_data.size())
        return std::nullopt;
    return m_data[m_position++];
}

std::optional<uint64_t> ByteParser::consumeULEB128()
{
    size_t start = m_position;
    uint64_t result = 0;
    if (WTF::LEBDecoder::decodeUInt64(m_data, m_position, result))
        return result;
    m_position = start;
    return std::nullopt;
}

std::optional<std::string_view> ByteParser::consumeCString()
{
    size_t start = m_position;
    while (m_position < m_data.size() && m_data[m_position])
        ++m_position;
    if (m_position >= m_data.size()) {
        m_position = start;
        return std::nullopt;
    }
    std::string_view result(spanReinterpretCast<const char>(m_data.subspan(start, m_position - start)));
    ++m_position; // Consume the null terminator.
    return result;
}

} // namespace Corpse
} // namespace JSC

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
