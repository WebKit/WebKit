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

/* PlayStation, Windows, Android before API 33, and Linux without glibc have no backtrace(). */
#if (PAS_OS(ANDROID) && __ANDROID_API__ >= 33) || PAS_OS(DARWIN) || (PAS_OS(LINUX) && defined(__GLIBC__) && !defined(__UCLIBC__))
#define PAS_PGM_TRACE_HAS_BACKTRACE 1
#include <execinfo.h>
#else
#define PAS_PGM_TRACE_HAS_BACKTRACE 0
#endif

#define PAS_PGM_TRACE_RAW_FRAME_SIZE 8u

static uintptr_t pas_pgm_trace_zigzag_encode(intptr_t value)
{
    uintptr_t shifted = (uintptr_t)value << 1;
    return value < 0 ? ~shifted : shifted;
}

static intptr_t pas_pgm_trace_zigzag_decode(uintptr_t value)
{
    uintptr_t shifted = value >> 1;
    return (intptr_t)((value & 1) ? ~shifted : shifted);
}

/* Exact, because pas_pgm_trace_select_shift only picks a shift that divides every delta. */
static uintptr_t pas_pgm_trace_encode_delta(uintptr_t previous_frame, uintptr_t frame, unsigned shift)
{
    intptr_t delta = (intptr_t)(frame - previous_frame);
    return pas_pgm_trace_zigzag_encode(delta / ((intptr_t)1 << shift));
}

static uintptr_t pas_pgm_trace_decode_delta(uintptr_t previous_frame, uintptr_t value, unsigned shift)
{
    return previous_frame + ((uintptr_t)pas_pgm_trace_zigzag_decode(value) << shift);
}

static unsigned pas_pgm_trace_select_shift(const uintptr_t* frames, uint32_t frame_count)
{
    uintptr_t delta_bits = frames[0];
    for (uint32_t i = 1; i < frame_count; ++i)
        delta_bits |= frames[i] - frames[i - 1];

    unsigned shift = 0;
    while (shift < PAS_PGM_TRACE_MAX_SHIFT && !(delta_bits & ((uintptr_t)1 << shift)))
        ++shift;
    return shift;
}

static size_t pas_pgm_trace_varint_size(uintptr_t value)
{
    size_t size = 1;
    for (value >>= 7; value; value >>= 7)
        ++size;
    return size;
}

static size_t pas_pgm_trace_varint_encode(uintptr_t value, uint8_t* output_buffer, size_t buffer_size)
{
    for (size_t i = 0; i < buffer_size; ++i) {
        output_buffer[i] = (uint8_t)(value & 0x7f);
        value >>= 7;
        if (!value)
            return i + 1;
        output_buffer[i] |= 0x80;
    }
    return 0;
}

static size_t pas_pgm_trace_varint_decode(const uint8_t* input_buffer, size_t buffer_size, uintptr_t* value)
{
    uintptr_t result = 0;
    unsigned shift = 0;
    for (size_t i = 0; i < buffer_size; ++i) {
        if (shift >= sizeof(uintptr_t) * 8)
            return 0;

        uintptr_t chunk = input_buffer[i] & 0x7f;
        if (shift && (chunk >> (sizeof(uintptr_t) * 8 - shift)))
            return 0;

        result |= chunk << shift;
        if (input_buffer[i] < 0x80) {
            *value = result;
            return i + 1;
        }
        shift += 7;
    }
    return 0;
}

static size_t pas_pgm_trace_raw_encode(uintptr_t frame, uint8_t* output_buffer, size_t buffer_size)
{
    if (buffer_size < PAS_PGM_TRACE_RAW_FRAME_SIZE)
        return 0;

    uint64_t value = frame;
    for (unsigned i = 0; i < PAS_PGM_TRACE_RAW_FRAME_SIZE; ++i)
        output_buffer[i] = (uint8_t)(value >> (8 * i));
    return PAS_PGM_TRACE_RAW_FRAME_SIZE;
}

static size_t pas_pgm_trace_raw_decode(const uint8_t* input_buffer, size_t buffer_size, uintptr_t* frame)
{
    if (buffer_size < PAS_PGM_TRACE_RAW_FRAME_SIZE)
        return 0;

    uint64_t value = 0;
    for (unsigned i = 0; i < PAS_PGM_TRACE_RAW_FRAME_SIZE; ++i)
        value |= (uint64_t)input_buffer[i] << (8 * i);
    *frame = (uintptr_t)value;
    return PAS_PGM_TRACE_RAW_FRAME_SIZE;
}

size_t pas_pgm_trace_compress(uint8_t* output_buffer, size_t buffer_size, const uintptr_t* frames, uint32_t frame_count)
{
    if (!output_buffer || !buffer_size || !frames || !frame_count)
        return 0;

    unsigned shift = pas_pgm_trace_select_shift(frames, frame_count);

    uint64_t varint_size = 0;
    uintptr_t previous_frame = 0;
    for (uint32_t i = 0; i < frame_count; ++i) {
        varint_size += pas_pgm_trace_varint_size(pas_pgm_trace_encode_delta(previous_frame, frames[i], shift));
        previous_frame = frames[i];
    }
    bool is_raw = varint_size > (uint64_t)frame_count * PAS_PGM_TRACE_RAW_FRAME_SIZE;

    output_buffer[0] = (uint8_t)(is_raw ? PAS_PGM_TRACE_RAW_HEADER : shift);
    size_t bytes_written = 1;
    previous_frame = 0;
    for (uint32_t i = 0; i < frame_count; ++i) {
        uint8_t* cursor = output_buffer + bytes_written;
        size_t remaining = buffer_size - bytes_written;
        size_t encoded_bytes = is_raw
            ? pas_pgm_trace_raw_encode(frames[i], cursor, remaining)
            : pas_pgm_trace_varint_encode(pas_pgm_trace_encode_delta(previous_frame, frames[i], shift), cursor, remaining);
        if (!encoded_bytes)
            break;
        bytes_written += encoded_bytes;
        previous_frame = frames[i];
    }
    return bytes_written > 1 ? bytes_written : 0;
}

uint32_t pas_pgm_trace_decompress(const uint8_t* compressed_buffer, size_t buffer_size, uintptr_t* output_frames, uint32_t frame_count)
{
    if (!compressed_buffer || !buffer_size || !output_frames || !frame_count)
        return 0;

    uint8_t header = compressed_buffer[0];
    bool is_raw = header == PAS_PGM_TRACE_RAW_HEADER;
    if (!is_raw && header > PAS_PGM_TRACE_MAX_SHIFT)
        return 0;

    size_t bytes_read = 1;
    uintptr_t previous_frame = 0;
    uint32_t decoded_frames = 0;
    while (decoded_frames < frame_count && bytes_read < buffer_size) {
        const uint8_t* cursor = compressed_buffer + bytes_read;
        size_t remaining = buffer_size - bytes_read;
        uintptr_t value;
        size_t decoded_bytes = is_raw
            ? pas_pgm_trace_raw_decode(cursor, remaining, &value)
            : pas_pgm_trace_varint_decode(cursor, remaining, &value);
        if (!decoded_bytes)
            break;
        previous_frame = is_raw ? value : pas_pgm_trace_decode_delta(previous_frame, value, header);
        output_frames[decoded_frames++] = previous_frame;
        bytes_read += decoded_bytes;
    }
    return decoded_frames;
}

/* Never inlined, so backtrace()'s first frame is always this function and can be dropped. */
PAS_NEVER_INLINE size_t pas_pgm_trace_collect(uint8_t* output_buffer, size_t buffer_size)
{
#if PAS_PGM_TRACE_HAS_BACKTRACE
    if (!output_buffer || !buffer_size)
        return 0;

    void* return_addresses[PAS_PGM_BACKTRACE_MAX_FRAMES + 1];
    int total_frames = backtrace(return_addresses, PAS_PGM_BACKTRACE_MAX_FRAMES + 1);
    if (total_frames <= 1 || total_frames > PAS_PGM_BACKTRACE_MAX_FRAMES + 1)
        return 0;

    uintptr_t frames[PAS_PGM_BACKTRACE_MAX_FRAMES];
    uint32_t frame_count = (uint32_t)(total_frames - 1);
    for (uint32_t i = 0; i < frame_count; ++i)
        frames[i] = (uintptr_t)return_addresses[i + 1];
    return pas_pgm_trace_compress(output_buffer, buffer_size, frames, frame_count);
#else
    PAS_UNUSED_PARAM(output_buffer);
    PAS_UNUSED_PARAM(buffer_size);
    return 0;
#endif
}

#endif /* LIBPAS_ENABLED */
