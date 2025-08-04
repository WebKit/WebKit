/* sysprof-profiler.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include <sysprof-capture.h>

#include "sysprof-instrument.h"
#include "sysprof-recording.h"
#include "sysprof-spawnable.h"

G_BEGIN_DECLS

#define SYSPROF_TYPE_PROFILER (sysprof_profiler_get_type())

SYSPROF_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (SysprofProfiler, sysprof_profiler, SYSPROF, PROFILER, GObject)

SYSPROF_AVAILABLE_IN_ALL
SysprofProfiler  *sysprof_profiler_new             (void);
SYSPROF_AVAILABLE_IN_ALL
SysprofSpawnable *sysprof_profiler_get_spawnable   (SysprofProfiler      *self);
SYSPROF_AVAILABLE_IN_ALL
void              sysprof_profiler_set_spawnable   (SysprofProfiler      *self,
                                                    SysprofSpawnable     *spawnable);
SYSPROF_AVAILABLE_IN_ALL
void              sysprof_profiler_add_instrument  (SysprofProfiler      *self,
                                                    SysprofInstrument    *instrument);
SYSPROF_AVAILABLE_IN_ALL
void              sysprof_profiler_record_async    (SysprofProfiler      *self,
                                                    SysprofCaptureWriter *writer,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
SYSPROF_AVAILABLE_IN_ALL
SysprofRecording *sysprof_profiler_record_finish   (SysprofProfiler      *self,
                                                    GAsyncResult         *result,
                                                    GError              **error);

G_END_DECLS
