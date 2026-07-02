/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#include "TestHarness.h"

#include "pas_pgm_trace.h"

namespace {

// Test basic compression and decompression functionality
void testPGMTraceBasicCompressionDecompression()
{
    // Test with a simple sequence of addresses
    vm_address_t original_frames[4] = {
        0x100000000, // Base address
        0x100000004, // +4 bytes
        0x100000008, // +4 bytes
        0x10000000C // +4 bytes
    };

    uint8_t compressed_buffer[64];
    vm_address_t decompressed_frames[4];

    // Compress the frames
    size_t compressed_size = compress_backtrace(compressed_buffer, sizeof(compressed_buffer),
                                              original_frames, 4);
    CHECK_GREATER(compressed_size, 0);

    // Decompress and verify
    decompress_backtrace(compressed_buffer, compressed_size, decompressed_frames, 4);

    // Verify all frames match
    for (int i = 0; i < 4; i++)
        CHECK_EQUAL(original_frames[i], decompressed_frames[i]);
}

// Test zigzag encoding with positive and negative deltas
void testPGMTraceZigZagEncoding()
{
    // Test positive values
    CHECK_EQUAL(zig_zag_encode(0), 0U);
    CHECK_EQUAL(zig_zag_encode(1), 2U);
    CHECK_EQUAL(zig_zag_encode(2), 4U);

    // Test negative values
    CHECK_EQUAL(zig_zag_encode(-1), 1U);
    CHECK_EQUAL(zig_zag_encode(-2), 3U);

    // Test round trip
    for (intptr_t test_val : { -1000, -1, 0, 1, 1000, 0x7FFFFFFF }) {
        uintptr_t encoded = zig_zag_encode(test_val);
        intptr_t decoded = zig_zag_decode(encoded);
        CHECK_EQUAL(test_val, decoded);
    }
}

// Test variable length encoding
void testPGMTraceVariableLengthEncoding()
{
    uint8_t buffer[16];
    uintptr_t decoded_value;

    // Test small values (single byte)
    CHECK_EQUAL(variable_len_encode(0x00, buffer, sizeof(buffer)), 1);
    CHECK_EQUAL(buffer[0], 0x00);
    CHECK_EQUAL(variable_len_decode(buffer, 1, &decoded_value), 1);
    CHECK_EQUAL(decoded_value, 0x00U);

    CHECK_EQUAL(variable_len_encode(0x7F, buffer, sizeof(buffer)), 1);
    CHECK_EQUAL(buffer[0], 0x7F);
    CHECK_EQUAL(variable_len_decode(buffer, 1, &decoded_value), 1);
    CHECK_EQUAL(decoded_value, 0x7FU);

    // Test multi-byte values
    CHECK_EQUAL(variable_len_encode(0x80, buffer, sizeof(buffer)), 2);
    CHECK_EQUAL(buffer[0], 0x80); // 0x80 | 0x80 (continuation bit)
    CHECK_EQUAL(buffer[1], 0x01);
    CHECK_EQUAL(variable_len_decode(buffer, 2, &decoded_value), 2);
    CHECK_EQUAL(decoded_value, 0x80U);

    // Test larger value
    CHECK_EQUAL(variable_len_encode(0x4000, buffer, sizeof(buffer)), 3);
    CHECK_EQUAL(variable_len_decode(buffer, 3, &decoded_value), 3);
    CHECK_EQUAL(decoded_value, 0x4000U);
}

// Test compression with decreasing sequence (negative deltas)
void testPGMTraceDecreasingSequence()
{
    vm_address_t original_frames[3] = {
        0x100000010,
        0x100000008, // -8 delta
        0x100000004 // -4 delta
    };

    uint8_t compressed_buffer[32];
    vm_address_t decompressed_frames[3];

    size_t compressed_size = compress_backtrace(compressed_buffer, sizeof(compressed_buffer),
                                              original_frames, 3);
    CHECK_GREATER(compressed_size, 0);

    decompress_backtrace(compressed_buffer, compressed_size, decompressed_frames, 3);

    for (int i = 0; i < 3; i++)
        CHECK_EQUAL(original_frames[i], decompressed_frames[i]);
}

// Test edge cases and error handling
void testPGMTraceErrorHandling()
{
    vm_address_t frames[2] = { 0x1000, 0x1004 };
    uint8_t small_buffer[4]; // Too small for compression
    vm_address_t output[2];

    // Test nullptr pointer inputs
    CHECK_EQUAL(compress_backtrace(nullptr, 10, frames, 2), 0);
    CHECK_EQUAL(compress_backtrace(small_buffer, 10, nullptr, 2), 0);

    // Test zero frame count
    CHECK_EQUAL(compress_backtrace(small_buffer, sizeof(small_buffer), frames, 0), 0);

    // Test buffer too small (need at least sizeof(vm_address_t) for base address)
    CHECK_EQUAL(compress_backtrace(small_buffer, sizeof(vm_address_t) - 1, frames, 2), 0);

    // Test decompression with nullptr inputs
    uint8_t valid_buffer[16] = { 0 };
    decompress_backtrace(nullptr, 10, output, 2); // Should not crash
    decompress_backtrace(valid_buffer, 10, nullptr, 2); // Should not crash
}

// Test single frame compression/decompression
void testPGMTraceSingleFrame()
{
    vm_address_t original_frame[1] = { 0x123456789ABCDEF0 };
    uint8_t compressed_buffer[16];
    vm_address_t decompressed_frame[1];

    size_t compressed_size = compress_backtrace(compressed_buffer, sizeof(compressed_buffer),
                                              original_frame, 1);

    // Should be exactly sizeof(vm_address_t) since no deltas to encode
    CHECK_EQUAL(compressed_size, sizeof(vm_address_t));

    decompress_backtrace(compressed_buffer, compressed_size, decompressed_frame, 1);
    CHECK_EQUAL(original_frame[0], decompressed_frame[0]);
}

// Test large delta compression
void testPGMTraceLargeDeltas()
{
    vm_address_t original_frames[3] = {
        0x100000000,
        0x200000000, // Large positive delta
        0x50000000 // Large negative delta
    };

    uint8_t compressed_buffer[64];
    vm_address_t decompressed_frames[3];

    size_t compressed_size = compress_backtrace(compressed_buffer, sizeof(compressed_buffer),
                                              original_frames, 3);
    CHECK_GREATER(compressed_size, 0);

    decompress_backtrace(compressed_buffer, compressed_size, decompressed_frames, 3);

    for (int i = 0; i < 3; i++)
        CHECK_EQUAL(original_frames[i], decompressed_frames[i]);
}

// Test buffer overflow protection in variable length encoding
void testPGMTraceBufferOverflowProtection()
{
    uint8_t buffer[1]; // Very small buffer
    uintptr_t large_value = 0x8000; // Requires 2+ bytes to encode

    // Should fail gracefully
    size_t result = variable_len_encode(large_value, buffer, sizeof(buffer));
    CHECK_EQUAL(result, 0); // Should indicate failure

    // Test decoding with malformed data (all continuation bits)
    uint8_t bad_buffer[10];
    for (int i = 0; i < 10; i++)
        bad_buffer[i] = 0x80; // All continuation bits

    uintptr_t decoded_value;
    result = variable_len_decode(bad_buffer, sizeof(bad_buffer), &decoded_value);
    CHECK_EQUAL(result, 0); // Should indicate failure
}

// Test realistic backtrace compression (simulating real stack addresses)
void testPGMTraceRealisticBacktrace()
{
    // Simulate realistic stack frame addresses (close together)
    vm_address_t realistic_frames[5] = {
        0x0000000100001000, // main function
        0x0000000100001040, // +64 bytes
        0x0000000100001080, // +64 bytes
        0x0000000100001020, // back -96 bytes (tail call)
        0x0000000100001060 // +64 bytes
    };

    uint8_t compressed_buffer[64];
    vm_address_t decompressed_frames[5];

    size_t compressed_size = compress_backtrace(compressed_buffer, sizeof(compressed_buffer),
                                              realistic_frames, 5);
    CHECK_GREATER(compressed_size, 0);

    // Should compress well due to small deltas
    CHECK_LESS(compressed_size, sizeof(realistic_frames));

    decompress_backtrace(compressed_buffer, compressed_size, decompressed_frames, 5);

    for (int i = 0; i < 5; i++)
        CHECK_EQUAL(realistic_frames[i], decompressed_frames[i]);
}

// Test the full collect_backtrace function
void testPGMTraceCollectBacktrace()
{
    uint8_t compressed_buffer[256];

    // This should collect a real backtrace and compress it
    size_t compressed_size = collect_backtrace(compressed_buffer, sizeof(compressed_buffer));

    // Should have captured at least some frames (unless backtrace() fails)
    // We can't guarantee this will always work on all platforms, but on supported ones it should
    if (compressed_size > 0) {
        CHECK_GREATER(compressed_size, sizeof(vm_address_t)); // At least the base frame
        CHECK_LESS(compressed_size, sizeof(compressed_buffer)); // Shouldn't fill entire buffer
    }

    // Test with nullptr buffer
    CHECK_EQUAL(collect_backtrace(nullptr, 100), 0);

    // Test with zero size
    CHECK_EQUAL(collect_backtrace(compressed_buffer, 0), 0);
}

} // anonymous namespace

void addPGMTraceTests()
{
    ADD_TEST(testPGMTraceBasicCompressionDecompression());
    ADD_TEST(testPGMTraceZigZagEncoding());
    ADD_TEST(testPGMTraceVariableLengthEncoding());
    ADD_TEST(testPGMTraceDecreasingSequence());
    ADD_TEST(testPGMTraceErrorHandling());
    ADD_TEST(testPGMTraceSingleFrame());
    ADD_TEST(testPGMTraceLargeDeltas());
    ADD_TEST(testPGMTraceBufferOverflowProtection());
    ADD_TEST(testPGMTraceRealisticBacktrace());
    ADD_TEST(testPGMTraceCollectBacktrace());
}
