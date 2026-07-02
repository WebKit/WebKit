/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstddef>
#include <span>
#include <wtf/Compiler.h>

namespace WTF {

// For printing chunks of memory in the traditional hex dump form using dataLog or PrintStream.
// Examples:
//   dataLogLn("Memory dump: ", MemoryDump(startPtr, endPtr));
//   dataLogLn("Memory dump: ", MemoryDump(std::span(data)));
// By default, the output is truncated past the DefaultSizeLimit number of bytes (4K).
// To change the limit, pass the desired limit value to HexDump() as an additional parameter.

class MemoryDump {
public:
    static constexpr size_t DefaultSizeLimit = 4 * 1024;

    template<typename T, std::size_t N = std::dynamic_extent>
    explicit MemoryDump(std::span<T, N> span, size_t sizeLimit = DefaultSizeLimit)
        : m_data(std::as_bytes(span))
        , m_sizeLimit(sizeLimit)
    { }

    MemoryDump() = default;

    std::span<const std::byte> span() const { return m_data; }
    size_t sizeLimit() const { return m_sizeLimit; }

private:
    std::span<const std::byte> m_data { };
    size_t m_sizeLimit { DefaultSizeLimit };
    // TODO: enhance to support larger chunks than 1 bytes
};

} // namespace WTF

using WTF::MemoryDump;
