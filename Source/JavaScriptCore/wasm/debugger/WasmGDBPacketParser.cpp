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

#include "config.h"
#include "WasmGDBPacketParser.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <wtf/ASCIICType.h>
#include <wtf/DataLog.h>
#include <wtf/PrintStream.h>

namespace JSC {
namespace Wasm {

void GDBPacketParser::reset()
{
    m_bufferIndex = 0;
    m_checksum = 0;
    m_phase = ReceivePhase::Idle;
    m_checksumBytesRead = 0;
    m_errorReason = ErrorReason::None;
}

void GDBPacketParser::dumpBuffer(std::span<const uint8_t> buffer)
{
    for (auto byte : buffer) {
        if (isASCIIPrintable(byte))
            dataLog("'", static_cast<char>(byte), "' ");
        else
            dataLog("<", byte, "> ");
    }
}

void GDBPacketParser::dump(PrintStream& out) const
{
    out.print(m_bufferIndex, " bytes buffered, phase=", m_phase);
    if (m_bufferIndex > 0) {
        out.print(", buffer: ");
        dumpBuffer({ m_buffer.data(), m_bufferIndex });
    }
}

StringView GDBPacketParser::getCompletedPacket() const
{
    // Return view of the null-terminated packet payload
    auto buffer = byteCast<char>(m_buffer.data());
    return StringView(buffer, strlen(buffer), true);
}

GDBPacketParser::ParseResult GDBPacketParser::processByte(uint8_t byte)
{
    // If parser is in error state, reject further processing until reset
    if (m_errorReason != ErrorReason::None)
        return ParseResult::Error;

    switch (m_phase) {
    case ReceivePhase::Idle:
        m_bufferIndex = 0;
        m_checksum = 0;
        m_checksumBytesRead = 0;

        // Check for interrupt character (Ctrl+C) - treat as complete single-byte packet
        if (byte == 0x03) {
            m_buffer[0] = 0x03;
            m_buffer[1] = '\0';
            m_bufferIndex = 1;
            return ParseResult::CompletePacket;
        }

        // Check for packet start
        if (byte == '$')
            m_phase = ReceivePhase::Payload;

        return ParseResult::Incomplete;

    case ReceivePhase::Payload:
        if (byte == '#') {
            // End of payload, start reading checksum
            m_phase = ReceivePhase::Checksum;

            // Add '#' to buffer but don't include in checksum
            if (!pushByte(byte))
                return ParseResult::Error;
        } else {
            // Regular payload byte - add to buffer and checksum
            if (!pushByte<IncludeInChecksum::Yes>(byte))
                return ParseResult::Error;
        }

        return ParseResult::Incomplete;

    case ReceivePhase::Checksum:
        // Reading checksum bytes (2 hex digits)
        if (!pushByte(byte))
            return ParseResult::Error;

        m_checksumBytesRead++;

        if (m_checksumBytesRead == 2) {
            ASSERT(m_bufferIndex >= 3); // At least '#' + 2 checksum bytes

            uint8_t checksumHigh = m_buffer[m_bufferIndex - 2];
            uint8_t checksumLow = m_buffer[m_bufferIndex - 1];
            if (!isASCIIHexDigit(checksumHigh) || !isASCIIHexDigit(checksumLow)) {
                m_errorReason = ErrorReason::InvalidHexInChecksum;
                return ParseResult::Error;
            }

            uint8_t receivedChecksum = toASCIIHexValue(checksumHigh, checksumLow);
            if (receivedChecksum != m_checksum) {
                m_errorReason = ErrorReason::ChecksumMismatch;
                return ParseResult::Error;
            }

            // Null-terminate the payload (replace '#' with '\0')
            m_buffer[m_bufferIndex - 3] = '\0';
            m_phase = ReceivePhase::Idle;
            return ParseResult::CompletePacket;
        }

        return ParseResult::Incomplete;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return ParseResult::Incomplete;
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
