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

#pragma once

#include "pas_report_crash_pgm_report.h"
#include "pas_utils.h"
#include <stdint.h>

#if PAS_OS(DARWIN)
#include <mach/vm_types.h>
#else
typedef uintptr_t vm_address_t;
#endif

PAS_BEGIN_EXTERN_C;

size_t collect_backtrace(uint8_t* output_buffer, size_t buffer_size);

size_t compress_backtrace(uint8_t* output_buffer, size_t buffer_size, vm_address_t* collected_frames, uint32_t frame_count);
void decompress_backtrace(const uint8_t* compressed_buffer, size_t buffer_size, vm_address_t* output_frames, uint32_t frame_count);

uintptr_t zig_zag_encode(intptr_t signed_input);
intptr_t zig_zag_decode(uintptr_t zigzag_input);

size_t variable_len_encode(uintptr_t value, uint8_t* output_buffer, size_t buffer_size);
size_t variable_len_decode(const uint8_t* input_buffer, size_t buffer_size, uintptr_t* output_value);

intptr_t delta_encode(vm_address_t current_frame, vm_address_t next_frame);
vm_address_t delta_decode(vm_address_t prev_frame, intptr_t delta);

intptr_t byte_align_encode(intptr_t raw_delta);
intptr_t byte_align_decode(intptr_t aligned_delta);

PAS_END_EXTERN_C;
