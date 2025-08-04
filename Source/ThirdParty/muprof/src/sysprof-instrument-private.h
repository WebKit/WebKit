/* sysprof-instrument-private.h
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

#include "sysprof-instrument.h"
#include "sysprof-recording.h"

G_BEGIN_DECLS

#define SYSPROF_INSTRUMENT_GET_CLASS(obj) G_TYPE_INSTANCE_GET_CLASS(obj, SYSPROF_TYPE_INSTRUMENT, SysprofInstrumentClass)

struct _SysprofInstrument
{
  GObject parent;
};

struct _SysprofInstrumentClass
{
  GObjectClass parent_class;

  char      **(*list_required_policy) (SysprofInstrument *self);
  DexFuture  *(*prepare)              (SysprofInstrument *self,
                                       SysprofRecording  *recording);
  DexFuture  *(*record)               (SysprofInstrument *self,
                                       SysprofRecording  *recording,
                                       GCancellable      *cancellable);
  DexFuture  *(*augment)              (SysprofInstrument *self,
                                       SysprofRecording  *recording);
  DexFuture  *(*process_started)      (SysprofInstrument *self,
                                       SysprofRecording  *recording,
                                       int                pid,
                                       const char        *comm);
};

DexFuture *_sysprof_instruments_acquire_policy  (GPtrArray        *instruments,
                                                 SysprofRecording *recording);
DexFuture *_sysprof_instruments_prepare         (GPtrArray        *instruments,
                                                 SysprofRecording *recording);
DexFuture *_sysprof_instruments_record          (GPtrArray        *instruments,
                                                 SysprofRecording *recording,
                                                 GCancellable     *cancellable);
DexFuture *_sysprof_instruments_augment         (GPtrArray        *instruments,
                                                 SysprofRecording *recording);
DexFuture *_sysprof_instruments_process_started (GPtrArray        *instruments,
                                                 SysprofRecording *recording,
                                                 int               pid,
                                                 const char       *comm);

G_END_DECLS
