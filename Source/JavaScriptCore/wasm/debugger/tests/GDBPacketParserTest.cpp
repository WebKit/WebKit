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
#include "GDBPacketParserTest.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "TestUtilities.h"
#include "WasmGDBPacketParser.h"
#include <wtf/DataLog.h>
#include <wtf/text/WTFString.h>

using namespace JSC;
using namespace JSC::Wasm;

static void testGDBPacketParserBasic()
{
    dataLogLn("=== Testing GDB Packet Parser - Basic Packets ===");

    GDBPacketParser parser;

    // Test simple step command: $s#73
    const uint8_t stepPacket[] = { '$', 's', '#', '7', '3' };
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;

    for (size_t i = 0; i < sizeof(stepPacket); i++) {
        result = parser.processByte(stepPacket[i]);
        if (i < sizeof(stepPacket) - 1)
            TEST_ASSERT(result == GDBPacketParser::ParseResult::Incomplete, "Should be incomplete until last byte");
    }

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Should be complete after last byte");
    StringView packet = parser.getCompletedPacket();
    TEST_ASSERT(packet == "s"_s, "Packet payload should be 's'");

    dataLogLn("GDB Packet Parser basic tests completed");
}

static void testGDBPacketParserMultiPacket()
{
    dataLogLn("=== Testing GDB Packet Parser - Multiple Packets in One recv() ===");

    GDBPacketParser parser;

    // Simulate recv() returning "$s#73\x03" (step packet + interrupt)
    const uint8_t multiPacket[] = {
        '$', 's', '#', '7', '3', // Step command
        0x03 // Interrupt
    };

    int packetsReceived = 0;
    for (size_t i = 0; i < sizeof(multiPacket); i++) {
        auto result = parser.processByte(multiPacket[i]);
        if (result == GDBPacketParser::ParseResult::CompletePacket) {
            packetsReceived++;
            StringView packet = parser.getCompletedPacket();
            if (packetsReceived == 1)
                TEST_ASSERT(packet == "s"_s, "First packet should be 's'");
            else if (packetsReceived == 2)
                TEST_ASSERT(packet.length() == 1 && packet[0] == 0x03, "Second packet should be interrupt");
        }
    }

    TEST_ASSERT(packetsReceived == 2, "Should receive exactly 2 packets");

    dataLogLn("GDB Packet Parser multi-packet tests completed");
}

static void testGDBPacketParserPartialPacket()
{
    dataLogLn("=== Testing GDB Packet Parser - Partial Packets Across recv() Calls ===");

    GDBPacketParser parser;

    // Simulate first recv() gets: $c#
    const uint8_t part1[] = { '$', 'c', '#' };
    for (size_t i = 0; i < sizeof(part1); i++) {
        auto result = parser.processByte(part1[i]);
        TEST_ASSERT(result == GDBPacketParser::ParseResult::Incomplete, "Should be incomplete without checksum");
    }

    TEST_ASSERT(!parser.isIdle(), "Parser should not be idle while accumulating");

    // Simulate second recv() gets: 63
    const uint8_t part2[] = { '6', '3' };
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < sizeof(part2); i++)
        result = parser.processByte(part2[i]);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Should complete after checksum");
    StringView packet = parser.getCompletedPacket();
    TEST_ASSERT(packet == "c"_s, "Packet payload should be 'c'");
    TEST_ASSERT(parser.isIdle(), "Parser should be idle after complete packet");

    dataLogLn("GDB Packet Parser partial packet tests completed");
}

static void testGDBPacketParserChecksumValidation()
{
    dataLogLn("=== Testing GDB Packet Parser - Checksum Validation ===");

    GDBPacketParser parser;

    // Test valid checksum: $s#73 (checksum of 's' = 0x73)
    const uint8_t validPacket[] = { '$', 's', '#', '7', '3' };
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < sizeof(validPacket); i++)
        result = parser.processByte(validPacket[i]);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Valid checksum should succeed");
    TEST_ASSERT(parser.getCompletedPacket() == "s"_s, "Should return packet on valid checksum");

    // Test invalid checksum: $s#FF (wrong checksum)
    parser.reset();
    const uint8_t invalidPacket[] = { '$', 's', '#', 'F', 'F' };
    result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < sizeof(invalidPacket); i++)
        result = parser.processByte(invalidPacket[i]);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::Error, "Invalid checksum should be rejected");
    TEST_ASSERT(parser.getError() == GDBPacketParser::ErrorReason::ChecksumMismatch, "Should report checksum mismatch error");
    TEST_ASSERT(!parser.isIdle(), "Parser should not be idle after error (caller must reset)");

    dataLogLn("GDB Packet Parser checksum validation tests completed");
}

static void testGDBPacketParserInterrupt()
{
    dataLogLn("=== Testing GDB Packet Parser - Interrupt Character ===");

    GDBPacketParser parser;

    // Test interrupt character (0x03 / Ctrl+C) as single-byte packet
    auto result = parser.processByte(0x03);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Interrupt should be complete immediately");
    StringView packet = parser.getCompletedPacket();
    TEST_ASSERT(packet.length() == 1, "Interrupt packet should be 1 byte");
    TEST_ASSERT(packet[0] == 0x03, "Interrupt packet should contain 0x03");
    TEST_ASSERT(parser.isIdle(), "Parser should be idle after interrupt");

    dataLogLn("GDB Packet Parser interrupt tests completed");
}

static void testGDBPacketParserReset()
{
    dataLogLn("=== Testing GDB Packet Parser - Reset Functionality ===");

    GDBPacketParser parser;

    // Start parsing a packet
    parser.processByte('$');
    parser.processByte('s');
    TEST_ASSERT(!parser.isIdle(), "Parser should not be idle during parsing");

    // Reset
    parser.reset();
    TEST_ASSERT(parser.isIdle(), "Parser should be idle after reset");

    // Verify parser works after reset
    const uint8_t packet[] = { '$', 'c', '#', '6', '3' };
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < sizeof(packet); i++)
        result = parser.processByte(packet[i]);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Parser should work after reset");
    TEST_ASSERT(parser.getCompletedPacket() == "c"_s, "Should parse correctly after reset");

    dataLogLn("GDB Packet Parser reset tests completed");
}

static void testGDBPacketParserBufferOverflow()
{
    dataLogLn("=== Testing GDB Packet Parser - Buffer Overflow ===");

    GDBPacketParser parser;

    // Test payload exceeding bufferSize (4096 bytes)
    // Build a packet with 4100 byte payload
    parser.processByte('$');

    // Add 4100 'A' characters - should trigger overflow
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < 4100; i++) {
        result = parser.processByte('A');
        if (result == GDBPacketParser::ParseResult::Error)
            break;
    }

    // Parser should have returned error due to overflow
    TEST_ASSERT(result == GDBPacketParser::ParseResult::Error, "Should return error on buffer overflow");
    TEST_ASSERT(parser.getError() == GDBPacketParser::ErrorReason::BufferOverflow, "Should report buffer overflow error");
    TEST_ASSERT(!parser.isIdle(), "Parser should not be idle after error (caller must reset)");

    // Manually reset parser before recovery test
    parser.reset();

    // Verify parser still works after reset
    const uint8_t recoveryPacket[] = { '$', 's', '#', '7', '3' };
    result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < sizeof(recoveryPacket); i++)
        result = parser.processByte(recoveryPacket[i]);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Parser should recover after overflow");
    TEST_ASSERT(parser.getCompletedPacket() == "s"_s, "Parser should work correctly after recovery");

    dataLogLn("GDB Packet Parser buffer overflow tests completed");
}

static void testGDBPacketParserMalformedPackets()
{
    dataLogLn("=== Testing GDB Packet Parser - Malformed Packets ===");

    GDBPacketParser parser;

    // Test 1: $ without closing #
    parser.processByte('$');
    parser.processByte('s');
    parser.processByte('o');
    parser.processByte('m');
    parser.processByte('e');
    TEST_ASSERT(!parser.isIdle(), "Parser should be accumulating without #");

    // Send a valid packet to verify parser can recover
    parser.reset();
    const uint8_t validPacket[] = { '$', 's', '#', '7', '3' };
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < sizeof(validPacket); i++)
        result = parser.processByte(validPacket[i]);
    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Parser should work after reset");

    // Test 2: # before $
    parser.reset();
    parser.processByte('#');
    TEST_ASSERT(parser.isIdle(), "Parser should ignore # when idle");

    // Test 3: Random bytes before $
    parser.reset();
    parser.processByte('x');
    parser.processByte('y');
    parser.processByte('z');
    TEST_ASSERT(parser.isIdle(), "Parser should ignore random bytes when idle");

    dataLogLn("GDB Packet Parser malformed packets tests completed");
}

static void testGDBPacketParserInvalidHexChecksum()
{
    dataLogLn("=== Testing GDB Packet Parser - Invalid Hex in Checksum ===");

    GDBPacketParser parser;

    // Test with non-hex characters in checksum: $s#ZZ
    const uint8_t invalidHexPacket[] = { '$', 's', '#', 'Z', 'Z' };
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;

    for (size_t i = 0; i < sizeof(invalidHexPacket); i++)
        result = parser.processByte(invalidHexPacket[i]);

    // Should detect invalid hex in checksum
    TEST_ASSERT(result == GDBPacketParser::ParseResult::Error, "Invalid hex should fail validation");
    TEST_ASSERT(parser.getError() == GDBPacketParser::ErrorReason::InvalidHexInChecksum, "Should report invalid hex error");
    TEST_ASSERT(!parser.isIdle(), "Parser should not be idle after error (caller must reset)");

    dataLogLn("GDB Packet Parser invalid hex checksum tests completed");
}

static void testGDBPacketParserErrorStateGuard()
{
    dataLogLn("=== Testing GDB Packet Parser - Error State Guard ===");

    GDBPacketParser parser;

    // Trigger a checksum mismatch error
    const uint8_t invalidPacket[] = { '$', 's', '#', 'F', 'F' };
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < sizeof(invalidPacket); i++)
        result = parser.processByte(invalidPacket[i]);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::Error, "Should be in error state");
    TEST_ASSERT(parser.getError() == GDBPacketParser::ErrorReason::ChecksumMismatch, "Should have checksum error");

    // Try to process more bytes without reset - should reject
    result = parser.processByte('$');
    TEST_ASSERT(result == GDBPacketParser::ParseResult::Error, "Should reject bytes while in error state");

    result = parser.processByte('s');
    TEST_ASSERT(result == GDBPacketParser::ParseResult::Error, "Should still reject bytes in error state");

    // Reset should clear error state
    parser.reset();
    TEST_ASSERT(parser.getError() == GDBPacketParser::ErrorReason::None, "Reset should clear error");

    // Now should work normally
    const uint8_t validPacket[] = { '$', 's', '#', '7', '3' };
    result = GDBPacketParser::ParseResult::Incomplete;
    for (size_t i = 0; i < sizeof(validPacket); i++)
        result = parser.processByte(validPacket[i]);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Should work after reset");
    TEST_ASSERT(parser.getCompletedPacket() == "s"_s, "Should parse correctly after error recovery");

    dataLogLn("GDB Packet Parser error state guard tests completed");
}

static void testGDBPacketParserEmptyPayload()
{
    dataLogLn("=== Testing GDB Packet Parser - Empty Payload ===");

    GDBPacketParser parser;

    // Test empty payload packet: $#00 (checksum of empty string is 0x00)
    const uint8_t emptyPacket[] = { '$', '#', '0', '0' };
    GDBPacketParser::ParseResult result = GDBPacketParser::ParseResult::Incomplete;

    for (size_t i = 0; i < sizeof(emptyPacket); i++)
        result = parser.processByte(emptyPacket[i]);

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Empty payload should be valid");
    StringView packet = parser.getCompletedPacket();
    TEST_ASSERT(packet.isEmpty(), "Empty payload packet should have zero length");

    dataLogLn("GDB Packet Parser empty payload tests completed");
}

static void testGDBPacketParserConsecutivePackets()
{
    dataLogLn("=== Testing GDB Packet Parser - Consecutive Packets ===");

    GDBPacketParser parser;

    // Test two packets back-to-back: $s#73$c#63
    const uint8_t consecutivePackets[] = {
        '$', 's', '#', '7', '3', // First packet: step
        '$', 'c', '#', '6', '3' // Second packet: continue
    };

    int packetsReceived = 0;
    for (size_t i = 0; i < sizeof(consecutivePackets); i++) {
        auto result = parser.processByte(consecutivePackets[i]);
        if (result == GDBPacketParser::ParseResult::CompletePacket) {
            packetsReceived++;
            StringView packet = parser.getCompletedPacket();
            if (packetsReceived == 1)
                TEST_ASSERT(packet == "s"_s, "First packet should be 's'");
            else if (packetsReceived == 2)
                TEST_ASSERT(packet == "c"_s, "Second packet should be 'c'");
        }
    }

    TEST_ASSERT(packetsReceived == 2, "Should receive exactly 2 consecutive packets");

    dataLogLn("GDB Packet Parser consecutive packets tests completed");
}

static void testGDBPacketParserMultipleInterrupts()
{
    dataLogLn("=== Testing GDB Packet Parser - Multiple Interrupts ===");

    GDBPacketParser parser;

    // Test multiple consecutive interrupts: 0x03 0x03 0x03
    const uint8_t multipleInterrupts[] = { 0x03, 0x03, 0x03 };

    int interruptsReceived = 0;
    for (size_t i = 0; i < sizeof(multipleInterrupts); i++) {
        auto result = parser.processByte(multipleInterrupts[i]);
        if (result == GDBPacketParser::ParseResult::CompletePacket) {
            interruptsReceived++;
            StringView packet = parser.getCompletedPacket();
            TEST_ASSERT(packet.length() == 1 && packet[0] == 0x03, "Each should be interrupt packet");
        }
    }

    TEST_ASSERT(interruptsReceived == 3, "Should receive exactly 3 interrupts");
    TEST_ASSERT(parser.isIdle(), "Parser should be idle after interrupts");

    dataLogLn("GDB Packet Parser multiple interrupts tests completed");
}

static void testGDBPacketParserEdgeSizePayloads()
{
    dataLogLn("=== Testing GDB Packet Parser - Edge Size Payloads ===");

    GDBPacketParser parser;

    // Test payload exactly at buffer limit
    // bufferSize = 4096, need room for payload + '#' + 2 checksum bytes
    // Maximum payload = 4096 - 3 = 4093 bytes
    const size_t maxPayloadSize = 4093;

    parser.processByte('$');

    uint8_t checksum = 0;
    for (size_t i = 0; i < maxPayloadSize; i++) {
        uint8_t byte = 'A';
        parser.processByte(byte);
        checksum += byte;
    }

    parser.processByte('#');

    char checksumHigh = (checksum >> 4) < 10 ? '0' + (checksum >> 4) : 'a' + (checksum >> 4) - 10;
    char checksumLow = (checksum & 0xF) < 10 ? '0' + (checksum & 0xF) : 'a' + (checksum & 0xF) - 10;

    parser.processByte(static_cast<uint8_t>(checksumHigh));
    auto result = parser.processByte(static_cast<uint8_t>(checksumLow));

    TEST_ASSERT(result == GDBPacketParser::ParseResult::CompletePacket, "Max size payload should succeed");
    StringView packet = parser.getCompletedPacket();
    TEST_ASSERT(packet.length() == maxPayloadSize, "Max payload should have correct length");

    dataLogLn("GDB Packet Parser edge size payload tests completed");
}

void testGDBPacketParser()
{
    testGDBPacketParserBasic();
    testGDBPacketParserMultiPacket();
    testGDBPacketParserPartialPacket();
    testGDBPacketParserChecksumValidation();
    testGDBPacketParserInterrupt();
    testGDBPacketParserReset();
    testGDBPacketParserBufferOverflow();
    testGDBPacketParserMalformedPackets();
    testGDBPacketParserInvalidHexChecksum();
    testGDBPacketParserErrorStateGuard();
    testGDBPacketParserEmptyPayload();
    testGDBPacketParserConsecutivePackets();
    testGDBPacketParserMultipleInterrupts();
    testGDBPacketParserEdgeSizePayloads();
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
