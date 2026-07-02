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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_pgm_trace.h"

#include <string.h>

/* PlayStation does not currently support the backtrace API. Android API versions < 33 don't, either. Windows does not either. Linux only with GLibc and not uCLibc/Musl. */
#if (PAS_OS(ANDROID) && __ANDROID_API__ >= 33) || PAS_OS(DARWIN) || (PAS_OS(LINUX) && defined(__GLIBC__) && !defined(__UCLIBC__))
#include <execinfo.h>
#else
static size_t backtrace(void** buffer, size_t size)
{
    PAS_UNUSED_PARAM(buffer);
    PAS_UNUSED_PARAM(size);
    return 0;
}
#endif

// step 1. collect all the backtraces
// output_buffer: our empty uint8_t buffer for compressed backtrace data
// buffer_size: size of our buffer
size_t collect_backtrace(uint8_t* output_buffer, size_t buffer_size)
{
    if (!output_buffer || !buffer_size)
        return 0;

    vm_address_t collected_frames[PAS_PGM_BACKTRACE_MAX_FRAMES];
    int total_frames = backtrace((void **)collected_frames, PAS_PGM_BACKTRACE_MAX_FRAMES);

    if (total_frames <= 1 || total_frames > PAS_PGM_BACKTRACE_MAX_FRAMES)
        return 0; // No frames to compress after skipping the current function, or invalid return

    // we dump the first element of the collected frame buffer to not have this function call pollute our stacktrace.
    return compress_backtrace(output_buffer, buffer_size, &collected_frames[1], (uint32_t)(total_frames - 1));
}

// step 1b. start compression (when collecting backtrace)
size_t compress_backtrace(uint8_t* output_buffer, size_t buffer_size, vm_address_t* collected_frames, uint32_t frame_count)
{
    // Store first frame explicitly, then delta encoding -> byte_align -> zigzag -> var_length for the rest
    size_t bytes_written = 0;

    if (!output_buffer || !collected_frames || !frame_count || !buffer_size)
        return 0;

    // First, store the base frame address (first frame) as-is
    if (buffer_size < sizeof(vm_address_t))
        return 0; // Not enough space for even the base address

    // Store first frame directly (uncompressed)
    memcpy(&output_buffer[bytes_written], &collected_frames[0], sizeof(vm_address_t));
    bytes_written += sizeof(vm_address_t);

    // Now compress deltas between consecutive frames
    for (uint32_t i = 1; i < frame_count; ++i) {
        vm_address_t current_frame = collected_frames[i-1];
        vm_address_t next_frame = collected_frames[i];
        intptr_t delta = delta_encode(current_frame, next_frame);

        intptr_t byte_aligned_delta = byte_align_encode(delta);
        uintptr_t zigzag_delta = zig_zag_encode(byte_aligned_delta);

        size_t encoded_bytes = variable_len_encode(zigzag_delta, &output_buffer[bytes_written], buffer_size - bytes_written);
        // we have ran out of space in our output buffer, can no longer pack anything else in. return.
        if (!encoded_bytes)
            return bytes_written;
        bytes_written += encoded_bytes;
    }
    return bytes_written;
}

// final step. decompress (when doing crash report collection)
void decompress_backtrace(const uint8_t* compressed_buffer, size_t buffer_size, vm_address_t* output_frames, uint32_t frame_count)
{
    // First extract base address, then: var_length -> zigzag -> byte_align -> delta decoding
    size_t bytes_read = 0;

    if (!compressed_buffer || !output_frames || !frame_count || !buffer_size)
        return;

    // Extract the first frame (base address) that was stored uncompressed
    if (buffer_size < sizeof(vm_address_t))
        return; // Not enough data

    memcpy(&output_frames[0], &compressed_buffer[bytes_read], sizeof(vm_address_t));
    bytes_read += sizeof(vm_address_t);

    // Decompress the rest using delta decoding
    for (uint32_t i = 1; i < frame_count; ++i) {
        if (bytes_read >= buffer_size)
            return; // Not enough data for more frames

        uintptr_t zigzag_delta;
        size_t decoded_bytes = variable_len_decode(&compressed_buffer[bytes_read], buffer_size - bytes_read, &zigzag_delta);
        // if we have decoded all traces, we are done. return early.
        if (!decoded_bytes)
            return;

        intptr_t byte_aligned_delta = zig_zag_decode(zigzag_delta);
        intptr_t delta = byte_align_decode(byte_aligned_delta);

        vm_address_t final_address = delta_decode(output_frames[i-1], delta);

        output_frames[i] = final_address;
        bytes_read += decoded_bytes;
    }
}

// step 2. compute offsets of each pointer
intptr_t delta_encode(vm_address_t current_frame, vm_address_t next_frame)
{
    return (intptr_t)(next_frame - current_frame);
}

vm_address_t delta_decode(vm_address_t prev_frame, intptr_t delta)
{
    return prev_frame + (vm_address_t)delta;
}

// step 3. byte align them (ARM64 instructions are 4-byte aligned, x86 instructions are 1-byte aligned)
intptr_t byte_align_encode(intptr_t raw_delta)
{
    intptr_t aligned_delta = raw_delta;
    #if __PAS_ARM64
        aligned_delta /= 4; // ARM64 instructions are 4-byte aligned
    #endif
    return aligned_delta;
}

intptr_t byte_align_decode(intptr_t aligned_delta)
{
    intptr_t raw_delta = aligned_delta;
    #if __PAS_ARM64
        raw_delta *= 4; // ARM64 instructions are 4-byte aligned
    #endif
    return raw_delta;
}

// step 4. apply zigzag encoding
uintptr_t zig_zag_encode(intptr_t signed_input)
{
    // Convert signed to unsigned, then apply zigzag encoding
    uintptr_t unsigned_input = (uintptr_t)signed_input;
    uintptr_t shifted = unsigned_input << 1;
    return (signed_input < 0) ? ~shifted : shifted;
}

intptr_t zig_zag_decode(uintptr_t zigzag_input)
{
    uintptr_t shifted = zigzag_input >> 1;
    uintptr_t decoded = (zigzag_input & 1) ? ~shifted : shifted;
    return (intptr_t)decoded;
}

// step 5. variable length integer encoding.
size_t variable_len_encode(uintptr_t value, uint8_t* output_buffer, size_t buffer_size)
{
    if (!output_buffer || !buffer_size)
        return 0;

    for (size_t i = 0; i < buffer_size; i++) {
        output_buffer[i] = value & 0x7f;
        value >>= 7;
        if (!value)
            return i + 1; // Return number of bytes written
        output_buffer[i] |= 0x80; // Set the continuation bit
    }
    return 0; // we ran out of space
}

size_t variable_len_decode(const uint8_t* input_buffer, size_t buffer_size, uintptr_t* output_value)
{
    if (!input_buffer || !output_value || !buffer_size)
        return 0;

    uintptr_t result = 0;
    size_t shift = 0;
    for (size_t i = 0; i < buffer_size; i++) {
        // Disallow overflowing the range of the output integer.
        if (shift >= sizeof(uintptr_t) * 8)
            return 0;

        result |= (uintptr_t)(input_buffer[i] & 0x7f) << shift;
        if (input_buffer[i] < 0x80) {
            *output_value = result;
            return i + 1;
        }

        shift += 7;
    }

    return 0;
}

#endif /* LIBPAS_ENABLED */
