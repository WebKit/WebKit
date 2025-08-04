/* sysprof-recording-private.h
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

#include <libdex.h>

#include "sysprof-diagnostic-private.h"
#include "sysprof-instrument-private.h"
#include "sysprof-recording.h"
#include "sysprof-spawnable.h"

G_BEGIN_DECLS

typedef enum _SysprofRecordingCommand
{
  SYSPROF_RECORDING_COMMAND_STOP = 1,
} SysprofRecordingCommand;

struct _SysprofRecording
{
  GObject parent_instance;

  /* Used to calculate the duration of the recording */
  gint64 start_time;
  gint64 end_time;

  /* Used to calculate event count */
  SysprofCaptureStat stat;

  /* Diagnostics that may be added by instruments during the recording.
   * Some may be fatal, meaning that they stop the recording when the
   * diagnostic is submitted. That can happen in situations like
   * miss-configuration or failed authorization.
   */
  GListStore *diagnostics;

  /* If we are spawning a process as part of this recording, this
   * is the SysprofSpawnable used to spawn the process.
   */
  SysprofSpawnable *spawnable;

  /* This is where all of the instruments will write to. They are
   * expected to do this from the main-thread only. To work from
   * additional threads they need to proxy that state to the
   * main thread for writing.
   */
  SysprofCaptureWriter *writer;

  /* An array of SysprofInstrument that are part of this recording */
  GPtrArray *instruments;

  /* A DexFiber that will complete when the recording has finished,
   * been stopped, or failed.
   */
  DexFuture *fiber;

  /* The channel is used ot send state change messages to the fiber
   * from outside of the fiber.
   */
  DexChannel *channel;

  /* The process we have spawned, if any */
  GSubprocess *subprocess;
};

SysprofRecording     *_sysprof_recording_new           (SysprofCaptureWriter  *writer,
                                                        SysprofSpawnable      *spawnable,
                                                        SysprofInstrument    **instruments,
                                                        guint                  n_instruments);
void                  _sysprof_recording_start         (SysprofRecording      *self);
SysprofSpawnable     *_sysprof_recording_get_spawnable (SysprofRecording      *self);
DexFuture            *_sysprof_recording_add_file      (SysprofRecording      *self,
                                                        const char            *path,
                                                        gboolean               compress);
void                  _sysprof_recording_add_file_data (SysprofRecording      *self,
                                                        const char            *path,
                                                        const char            *contents,
                                                        gssize                 length,
                                                        gboolean               compress);
void                  _sysprof_recording_follow_process(SysprofRecording      *self,
                                                        int                    pid,
                                                        const char            *comm);
void                  _sysprof_recording_diagnostic    (SysprofRecording      *self,
                                                        const char            *domain,
                                                        const char            *format,
                                                        ...) G_GNUC_PRINTF (3, 4);
void                  _sysprof_recording_error         (SysprofRecording      *self,
                                                        const char            *domain,
                                                        const char            *format,
                                                        ...) G_GNUC_PRINTF (3, 4);

static inline SysprofCaptureWriter *
_sysprof_recording_writer (const SysprofRecording *self)
{
  return self->writer;
}

G_END_DECLS
