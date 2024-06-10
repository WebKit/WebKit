/* mapped-ring-buffer.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
