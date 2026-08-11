/* sysprof-capture-writer.h
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
#include <stdint.h>
#include <sys/types.h>

#include "sysprof-capture-types.h"
#include "sysprof-version-macros.h"

SYSPROF_BEGIN_DECLS

SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureWriter *sysprof_capture_writer_new_from_env                    (size_t                             buffer_size);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureWriter *sysprof_capture_writer_new                             (const char                        *filename,
                                                                              size_t                             buffer_size);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureWriter *sysprof_capture_writer_new_from_fd                     (int                                fd,
                                                                              size_t                             buffer_size);
SYSPROF_AVAILABLE_IN_ALL
size_t                sysprof_capture_writer_get_buffer_size                 (SysprofCaptureWriter              *self);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureWriter *sysprof_capture_writer_ref                             (SysprofCaptureWriter              *self);
SYSPROF_AVAILABLE_IN_ALL
void                  sysprof_capture_writer_unref                           (SysprofCaptureWriter              *self);
SYSPROF_AVAILABLE_IN_ALL
void                  sysprof_capture_writer_stat                            (SysprofCaptureWriter              *self,
                                                                              SysprofCaptureStat                *stat);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_file                        (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              const char                        *path,
                                                                              bool                               is_last,
                                                                              const uint8_t                     *data,
                                                                              size_t                             data_len);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_file_fd                     (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              const char                        *path,
                                                                              int                                fd);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_map                         (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              uint64_t                           start,
                                                                              uint64_t                           end,
                                                                              uint64_t                           offset,
                                                                              uint64_t                           inode,
                                                                              const char                        *filename);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_map_with_build_id           (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              uint64_t                           start,
                                                                              uint64_t                           end,
                                                                              uint64_t                           offset,
                                                                              uint64_t                           inode,
                                                                              const char                        *filename,
                                                                              const char                        *build_id);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_mark                        (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              uint64_t                           duration,
                                                                              const char                        *group,
                                                                              const char                        *name,
                                                                              const char                        *message);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_metadata                    (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              const char                        *id,
                                                                              const char                        *metadata,
                                                                              ssize_t                            metadata_len);
SYSPROF_AVAILABLE_IN_ALL
uint64_t              sysprof_capture_writer_add_jitmap                      (SysprofCaptureWriter              *self,
                                                                              const char                        *name);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_process                     (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              const char                        *cmdline);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_sample                      (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              int32_t                            tid,
                                                                              const SysprofCaptureAddress       *addrs,
                                                                              unsigned int                       n_addrs);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_trace                       (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              int32_t                            tid,
                                                                              const SysprofCaptureAddress       *addrs,
                                                                              unsigned int                       n_addrs,
                                                                              bool                               entering);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_fork                        (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              int32_t                            child_pid);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_exit                        (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_timestamp                   (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_define_counters                 (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              const SysprofCaptureCounter       *counters,
                                                                              unsigned int                       n_counters);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_set_counters                    (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              const unsigned int                *counters_ids,
                                                                              const SysprofCaptureCounterValue  *values,
                                                                              unsigned int                       n_counters);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_log                         (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              int                                severity,
                                                                              const char                        *domain,
                                                                              const char                        *message);
SYSPROF_AVAILABLE_IN_3_36
bool                  sysprof_capture_writer_add_allocation                  (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              int32_t                            tid,
                                                                              SysprofCaptureAddress              alloc_addr,
                                                                              int64_t                            alloc_size,
                                                                              SysprofBacktraceFunc               backtrace_func,
                                                                              void                              *backtrace_data);
SYSPROF_AVAILABLE_IN_3_36
bool                  sysprof_capture_writer_add_allocation_copy             (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              int32_t                            tid,
                                                                              SysprofCaptureAddress              alloc_addr,
                                                                              int64_t                            alloc_size,
                                                                              const SysprofCaptureAddress       *addrs,
                                                                              unsigned int                       n_addrs);
SYSPROF_AVAILABLE_IN_3_40
bool                  sysprof_capture_writer_add_overlay                     (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              uint32_t                           layer,
                                                                              const char                        *src,
                                                                              const char                        *dst);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_add_dbus_message                (SysprofCaptureWriter              *self,
                                                                              int64_t                            time,
                                                                              int                                cpu,
                                                                              int32_t                            pid,
                                                                              uint16_t                           bus_type,
                                                                              uint16_t                           flags,
                                                                              const uint8_t                     *message_data,
                                                                              size_t                             message_len);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_flush                           (SysprofCaptureWriter              *self);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_save_as                         (SysprofCaptureWriter              *self,
                                                                              const char                        *filename);
SYSPROF_AVAILABLE_IN_ALL
unsigned int          sysprof_capture_writer_request_counter                 (SysprofCaptureWriter              *self,
                                                                              unsigned int                       n_counters);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureReader *sysprof_capture_writer_create_reader                   (SysprofCaptureWriter              *self);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_splice                          (SysprofCaptureWriter              *self,
                                                                              SysprofCaptureWriter              *dest);
SYSPROF_AVAILABLE_IN_ALL
bool                  sysprof_capture_writer_cat                             (SysprofCaptureWriter              *self,
                                                                              SysprofCaptureReader              *reader);
SYSPROF_INTERNAL
bool                  _sysprof_capture_writer_add_raw                        (SysprofCaptureWriter              *self,
                                                                              const SysprofCaptureFrame         *frame);
SYSPROF_INTERNAL
bool                  _sysprof_capture_writer_splice_from_fd                 (SysprofCaptureWriter              *self,
                                                                              int                                fd) SYSPROF_INTERNAL;
SYSPROF_INTERNAL
bool                  _sysprof_capture_writer_set_time_range                 (SysprofCaptureWriter              *self,
                                                                              int64_t                            start_time,
                                                                              int64_t                            end_time) SYSPROF_INTERNAL;
SYSPROF_INTERNAL
int                   _sysprof_capture_writer_dup_fd                         (SysprofCaptureWriter              *self);

SYSPROF_END_DECLS
