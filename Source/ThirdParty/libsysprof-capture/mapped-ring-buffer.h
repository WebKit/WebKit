/* mapped-ring-buffer.h
 *
 * Copyright 2020-2025 Christian Hergert <chergert@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Subject to the terms and conditions of this license, each copyright holder
 * and contributor hereby grants to those receiving rights under this license
 * a perpetual, worldwide, non-exclusive, no-charge, royalty-free,
 * irrevocable (except for failure to satisfy the conditions of this license)
 * patent license to make, have made, use, offer to sell, sell, import, and
 * otherwise transfer this software, where such license applies only to those
 * patent claims, already acquired or hereafter acquired, licensable by such
 * copyright holder or contributor that are necessarily infringed by:
 *
 * (a) their Contribution(s) (the licensed copyrights of copyright holders
 *     and non-copyrightable additions of contributors, in source or binary
 *     form) alone; or
 *
 * (b) combination of their Contribution(s) with the work of authorship to
 *     which such Contribution(s) was added by such copyright holder or
 *     contributor, if, at the time the Contribution is added, such addition
 *     causes such combination to be necessarily infringed. The patent license
 *     shall not apply to any other combinations which include the
 *     Contribution.
 *
 * Except as expressly stated above, no rights or licenses from any copyright
 * holder or contributor is granted under this license, whether expressly, by
 * implication, estoppel or otherwise.
 *
 * DISCLAIMER
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "sysprof-macros.h"

SYSPROF_BEGIN_DECLS

typedef struct _MappedRingBuffer MappedRingBuffer;

/**
 * MappedRingBufferCallback:
 * @data: a pointer into the mapped buffer containing the data frame
 * @length: (inout): the number of bytes to advance
 * @user_data: closure data provided to mapped_ring_buffer_drain()
 *
 * Functions matching this prototype will be called from the
 * mapped_ring_buffer_drain() function for each data frame read from the
 * underlying memory mapping.
 *
 * @length is initially set to the max bytes that @data could contain, but
 * should be set by the caller to the amount of bytes known to have been
 * consumed. This allows MappedRingBuffer to avoid storing length data or
 * framing information as that can come from the buffer content.
 *
 * The callback should shorten @length if it knows the frame is less than
 * what was provided.
 *
 * This function can also be used with mapped_ring_buffer_create_source()
 * to automatically drain the ring buffer as part of the main loop progress.
 *
 * Returns: %TRUE to coninue draining, otherwise %FALSE and draining stops
 */
typedef bool (*MappedRingBufferCallback) (const void    *data,
                                          size_t        *length,
                                          void          *user_data);

SYSPROF_INTERNAL
MappedRingBuffer *mapped_ring_buffer_new_reader         (size_t                    buffer_size);
SYSPROF_INTERNAL
MappedRingBuffer *mapped_ring_buffer_new_readwrite      (size_t                    buffer_size);
SYSPROF_INTERNAL
MappedRingBuffer *mapped_ring_buffer_new_writer         (int                       fd);
SYSPROF_INTERNAL
int               mapped_ring_buffer_get_fd             (MappedRingBuffer         *self);
SYSPROF_INTERNAL
MappedRingBuffer *mapped_ring_buffer_ref                (MappedRingBuffer         *self);
SYSPROF_INTERNAL
void              mapped_ring_buffer_unref              (MappedRingBuffer         *self);
SYSPROF_INTERNAL
void              mapped_ring_buffer_clear              (MappedRingBuffer         *self);
SYSPROF_INTERNAL
void             *mapped_ring_buffer_allocate           (MappedRingBuffer         *self,
                                                         size_t                    length);
SYSPROF_INTERNAL
void              mapped_ring_buffer_advance            (MappedRingBuffer         *self,
                                                         size_t                    length);
SYSPROF_INTERNAL
bool              mapped_ring_buffer_drain              (MappedRingBuffer         *self,
                                                         MappedRingBufferCallback  callback,
                                                         void                     *user_data);
SYSPROF_INTERNAL
bool              mapped_ring_buffer_is_empty           (MappedRingBuffer         *self);

SYSPROF_END_DECLS
