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

#pragma once

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include <optional>
#include <span>
#include <stdint.h>
#include <string_view>

namespace JSC {
namespace Corpse {

// A forward byte parser over a local buffer. Every read reports whether it got
// what it asked for, and a read that fails consumes nothing.
class ByteParser {
public:
    ByteParser(std::span<const uint8_t> data, size_t position = 0)
        : m_data(data)
        , m_position(position)
    {
    }

    size_t position() const { return m_position; }

    std::optional<uint8_t> consumeByte();

    // Decodes the ULEB128 at the cursor. Returns nullopt if the buffer ends
    // before the encoding does, or if the value will not fit in 64 bits.
    // Untrusted data can hold either, and silently truncating one would yield a
    // plausible wrong value instead of a detected failure.
    std::optional<uint64_t> consumeULEB128();

    // Returns the null-terminated string at the cursor. Returns nullopt if the
    // buffer ends before the terminator does: without that the trailing bytes of
    // a truncated buffer read back as a complete string.
    std::optional<std::string_view> consumeCString();

private:
    std::span<const uint8_t> m_data;
    size_t m_position;
};

} // namespace Corpse
} // namespace JSC

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
