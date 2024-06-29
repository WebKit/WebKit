/* sysprof-capture-reader.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include "sysprof-capture-types.h"
#include "sysprof-macros.h"
#include "sysprof-version-macros.h"

SYSPROF_BEGIN_DECLS

SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureReader               *sysprof_capture_reader_new                 (const char                *filename);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureReader               *sysprof_capture_reader_new_from_fd         (int                        fd);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureReader               *sysprof_capture_reader_copy                (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureReader               *sysprof_capture_reader_ref                 (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
void                                sysprof_capture_reader_unref               (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
int                                 sysprof_capture_reader_get_byte_order      (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const char                         *sysprof_capture_reader_get_filename        (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const char                         *sysprof_capture_reader_get_time            (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
int64_t                             sysprof_capture_reader_get_start_time      (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
int64_t                             sysprof_capture_reader_get_end_time        (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
bool                                sysprof_capture_reader_skip                (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
bool                                sysprof_capture_reader_peek_type           (SysprofCaptureReader      *self,
                                                                                SysprofCaptureFrameType   *type);
SYSPROF_AVAILABLE_IN_ALL
bool                                sysprof_capture_reader_peek_frame          (SysprofCaptureReader      *self,
                                                                                SysprofCaptureFrame       *frame);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureLog            *sysprof_capture_reader_read_log            (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureMap            *sysprof_capture_reader_read_map            (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureMark           *sysprof_capture_reader_read_mark           (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureMetadata       *sysprof_capture_reader_read_metadata       (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureDBusMessage    *sysprof_capture_reader_read_dbus_message   (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureExit           *sysprof_capture_reader_read_exit           (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureFork           *sysprof_capture_reader_read_fork           (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureTimestamp      *sysprof_capture_reader_read_timestamp      (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureProcess        *sysprof_capture_reader_read_process        (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureSample         *sysprof_capture_reader_read_sample         (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureTrace          *sysprof_capture_reader_read_trace          (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureJitmap         *sysprof_capture_reader_read_jitmap         (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureCounterDefine  *sysprof_capture_reader_read_counter_define (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureCounterSet     *sysprof_capture_reader_read_counter_set    (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureFileChunk      *sysprof_capture_reader_read_file           (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_3_36
const SysprofCaptureAllocation     *sysprof_capture_reader_read_allocation     (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_3_40
const SysprofCaptureOverlay        *sysprof_capture_reader_read_overlay       (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
bool                                sysprof_capture_reader_reset               (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
bool                                sysprof_capture_reader_splice              (SysprofCaptureReader      *self,
                                                                                SysprofCaptureWriter      *dest);
SYSPROF_AVAILABLE_IN_ALL
bool                                sysprof_capture_reader_save_as             (SysprofCaptureReader      *self,
                                                                                const char                *filename);
SYSPROF_AVAILABLE_IN_ALL
bool                                sysprof_capture_reader_get_stat            (SysprofCaptureReader      *self,
                                                                                SysprofCaptureStat        *st_buf);
SYSPROF_AVAILABLE_IN_ALL
void                                sysprof_capture_reader_set_stat            (SysprofCaptureReader      *self,
                                                                                const SysprofCaptureStat  *st_buf);
SYSPROF_AVAILABLE_IN_ALL
const SysprofCaptureFileChunk      *sysprof_capture_reader_find_file           (SysprofCaptureReader      *self,
                                                                                const char                *path);
SYSPROF_AVAILABLE_IN_ALL
const char                        **sysprof_capture_reader_list_files          (SysprofCaptureReader      *self);
SYSPROF_AVAILABLE_IN_ALL
bool                                sysprof_capture_reader_read_file_fd        (SysprofCaptureReader      *self,
                                                                                const char                *path,
                                                                                int                        fd);

typedef struct {
  /*< private >*/
  void *p1;
  void *p2;
  unsigned int u1;
  void *p3;
  void *p4;
} SysprofCaptureJitmapIter;

SYSPROF_AVAILABLE_IN_3_38
void                                sysprof_capture_jitmap_iter_init           (SysprofCaptureJitmapIter    *iter,
                                                                                const SysprofCaptureJitmap  *jitmap);
SYSPROF_AVAILABLE_IN_3_38
bool                                sysprof_capture_jitmap_iter_next           (SysprofCaptureJitmapIter    *iter,
                                                                                SysprofCaptureAddress       *addr,
                                                                                const char                 **path);

SYSPROF_END_DECLS
