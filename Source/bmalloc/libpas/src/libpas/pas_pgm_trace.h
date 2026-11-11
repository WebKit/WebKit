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

#ifndef PAS_PGM_TRACE_H
#define PAS_PGM_TRACE_H

#include "pas_report_crash_pgm_report.h"
#include "pas_utils.h"
#include <stdint.h>

PAS_BEGIN_EXTERN_C;

/* Format: a header byte, then one entry per frame. Header N <= PAS_PGM_TRACE_MAX_SHIFT: each entry
   is a varint of zigzag((frame - previous frame) >> N), with 0 before the first frame, and N is the
   largest shift that keeps this exact. PAS_PGM_TRACE_RAW_HEADER: each entry is 8 little-endian bytes,
   used when that is smaller. The header is all a decoder needs, whatever its architecture. */
#define PAS_PGM_TRACE_MAX_SHIFT 3u
#define PAS_PGM_TRACE_RAW_HEADER 0x80u
#define PAS_PGM_TRACE_MAX_COMPRESSED_SIZE(frame_count) (1u + 8u * (size_t)(frame_count))

/* Return the size written, or 0 if no frame fits. A short buffer gets a decodable prefix. */
PAS_API size_t pas_pgm_trace_collect(uint8_t* output_buffer, size_t buffer_size);
PAS_API size_t pas_pgm_trace_compress(uint8_t* output_buffer, size_t buffer_size, const uintptr_t* frames, uint32_t frame_count);

/* Returns the number of frames decoded. It is less than frame_count if the buffer ends early. */
PAS_API uint32_t pas_pgm_trace_decompress(const uint8_t* compressed_buffer, size_t buffer_size, uintptr_t* output_frames, uint32_t frame_count);

PAS_END_EXTERN_C;

#endif /* PAS_PGM_TRACE_H */
