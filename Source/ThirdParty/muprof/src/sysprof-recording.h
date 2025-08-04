/* sysprof-recording.h
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

G_BEGIN_DECLS

#define SYSPROF_TYPE_RECORDING (sysprof_recording_get_type())

typedef enum _SysprofRecordingPhase
{
  SYSPROF_RECORDING_PHASE_PREPARE = 1,
  SYSPROF_RECORDING_PHASE_RECORD,
  SYSPROF_RECORDING_PHASE_AUGMENT,
} SysprofRecordingPhase;

SYSPROF_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (SysprofRecording, sysprof_recording, SYSPROF, RECORDING, GObject)

SYSPROF_AVAILABLE_IN_ALL
GListModel           *sysprof_recording_list_diagnostics (SysprofRecording     *self);
SYSPROF_AVAILABLE_IN_ALL
gint64                sysprof_recording_get_duration     (SysprofRecording     *self);
SYSPROF_AVAILABLE_IN_ALL
gint64                sysprof_recording_get_event_count  (SysprofRecording     *self);
SYSPROF_AVAILABLE_IN_ALL
GSubprocess          *sysprof_recording_get_subprocess   (SysprofRecording     *self);
SYSPROF_AVAILABLE_IN_ALL
void                  sysprof_recording_wait_async       (SysprofRecording     *self,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
SYSPROF_AVAILABLE_IN_ALL
gboolean              sysprof_recording_wait_finish      (SysprofRecording     *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);
SYSPROF_AVAILABLE_IN_ALL
SysprofCaptureReader *sysprof_recording_create_reader    (SysprofRecording     *self);
SYSPROF_AVAILABLE_IN_ALL
int                   sysprof_recording_dup_fd           (SysprofRecording     *self) G_GNUC_WARN_UNUSED_RESULT;
SYSPROF_AVAILABLE_IN_ALL
void                  sysprof_recording_stop_async       (SysprofRecording     *self,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
SYSPROF_AVAILABLE_IN_ALL
gboolean              sysprof_recording_stop_finish      (SysprofRecording     *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS
