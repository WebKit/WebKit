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

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <array>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

// FIXME: This parser has no WebAssembly-specific dependencies and could potentially be moved to WTF
// if other subsystems need GDB protocol support. Note: This is a simplified implementation that
// doesn't support escape sequences ('}' character) or run-length encoding ('*' character). This is
// sufficient for typical LLDB/WASM debugger communication where all data is hex-encoded, but may
// not handle all possible GDB protocol packets.

// GDB Remote Serial Protocol packet parser
// Implements byte-by-byte state machine parsing for packets of the form: $<data>#<checksum>
// Also handles special interrupt character (0x03 / Ctrl+C)
class JS_EXPORT_PRIVATE GDBPacketParser {
public:
    enum class ParseResult : uint8_t {
        Incomplete, // Continue accumulating bytes
        CompletePacket, // Full packet received and validated (includes interrupt 0x03)
        Error, // Parse error - check getError() for details
    };

    enum class ErrorReason : uint8_t {
        None,
        BufferOverflow,
        InvalidHexInChecksum,
        ChecksumMismatch,
    };

    GDBPacketParser() = default;

    ParseResult processByte(uint8_t byte);
    StringView getCompletedPacket() const;

    void reset();

    bool isIdle() const { return m_phase == ReceivePhase::Idle; }
    ErrorReason getError() const { return m_errorReason; }
    void dump(PrintStream&) const;

    static void dumpBuffer(std::span<const uint8_t> buffer);

private:
    enum class ReceivePhase : uint8_t {
        Idle, // Waiting for '$' or interrupt (0x03)
        Payload, // Reading packet content until '#'
        Checksum, // Reading 2-byte checksum
    };

    enum class IncludeInChecksum : bool { No, Yes };

    static constexpr size_t bufferSize = 4096;

    template<IncludeInChecksum includeInChecksum = IncludeInChecksum::No>
    bool pushByte(uint8_t byte)
    {
        if (m_bufferIndex >= bufferSize) {
            m_errorReason = ErrorReason::BufferOverflow;
            return false;
        }
        m_buffer[m_bufferIndex++] = byte;
        if constexpr (includeInChecksum == IncludeInChecksum::Yes)
            m_checksum += byte;
        return true;
    }

    std::array<uint8_t, bufferSize> m_buffer;
    size_t m_bufferIndex { 0 };
    uint8_t m_checksum { 0 };
    ReceivePhase m_phase { ReceivePhase::Idle };
    uint8_t m_checksumBytesRead { 0 };
    ErrorReason m_errorReason { ErrorReason::None };
};

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
