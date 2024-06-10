/* sysprof-capture-condition.h
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
SysprofCaptureCondition *sysprof_capture_condition_copy                   (const SysprofCaptureCondition *self);
SYSPROF_AVAILABLE_IN_ALL
void                     sysprof_capture_condition_unref                  (SysprofCaptureCondition       *self);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureCondition *sysprof_capture_condition_ref                    (SysprofCaptureCondition       *self);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureCondition *sysprof_capture_condition_new_and                (SysprofCaptureCondition       *left,
                                                                           SysprofCaptureCondition       *right);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureCondition *sysprof_capture_condition_new_or                 (SysprofCaptureCondition       *left,
                                                                           SysprofCaptureCondition       *right);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureCondition *sysprof_capture_condition_new_where_type_in      (unsigned int                   n_types,
                                                                           const SysprofCaptureFrameType *types);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureCondition *sysprof_capture_condition_new_where_time_between (int64_t                        begin_time,
                                                                           int64_t                        end_time);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureCondition *sysprof_capture_condition_new_where_pid_in       (unsigned int                   n_pids,
                                                                           const int32_t                 *pids);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureCondition *sysprof_capture_condition_new_where_counter_in   (unsigned int                   n_counters,
                                                                           const unsigned int            *counters);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureCondition *sysprof_capture_condition_new_where_file         (const char                    *path);
SYSPROF_AVAILABLE_IN_ALL
bool                     sysprof_capture_condition_match                  (const SysprofCaptureCondition *self,
                                                                           const SysprofCaptureFrame     *frame);

SYSPROF_END_DECLS
