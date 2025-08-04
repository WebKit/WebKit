/* sysprof-profiler.c
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

#include "config.h"

#include "sysprof-controlfd-instrument-private.h"
#include "sysprof-profiler.h"
#include "sysprof-recording-private.h"

struct _SysprofProfiler
{
  GObject           parent_instance;
  GPtrArray        *instruments;
  SysprofSpawnable *spawnable;
};

enum {
  PROP_0,
  PROP_SPAWNABLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

G_DEFINE_FINAL_TYPE (SysprofProfiler, sysprof_profiler, G_TYPE_OBJECT)

static void
sysprof_profiler_finalize (GObject *object)
{
  SysprofProfiler *self = (SysprofProfiler *)object;

  g_clear_pointer (&self->instruments, g_ptr_array_unref);
  g_clear_object (&self->spawnable);

  G_OBJECT_CLASS (sysprof_profiler_parent_class)->finalize (object);
}

static void
sysprof_profiler_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  SysprofProfiler *self = SYSPROF_PROFILER (object);

  switch (prop_id)
    {
    case PROP_SPAWNABLE:
      g_value_set_object (value, sysprof_profiler_get_spawnable (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_profiler_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  SysprofProfiler *self = SYSPROF_PROFILER (object);

  switch (prop_id)
    {
    case PROP_SPAWNABLE:
      sysprof_profiler_set_spawnable (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_profiler_class_init (SysprofProfilerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = sysprof_profiler_finalize;
  object_class->get_property = sysprof_profiler_get_property;
  object_class->set_property = sysprof_profiler_set_property;

  properties [PROP_SPAWNABLE] =
    g_param_spec_object ("spawnable", NULL, NULL,
                         SYSPROF_TYPE_SPAWNABLE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
sysprof_profiler_init (SysprofProfiler *self)
{
  self->instruments = g_ptr_array_new_with_free_func (g_object_unref);

  sysprof_profiler_add_instrument (self, _sysprof_controlfd_instrument_new ());
}

SysprofProfiler *
sysprof_profiler_new (void)
{
  return g_object_new (SYSPROF_TYPE_PROFILER, NULL);
}

/**
 * sysprof_profiler_add_instrument:
 * @self: a #SysprofProfiler
 * @instrument: (transfer full): a #SysprofInstrument
 *
 * Adds @instrument to @profiler.
 *
 * When the recording session is started, @instrument will be directed to
 * capture data into the destination capture file.
 */
void
sysprof_profiler_add_instrument (SysprofProfiler   *self,
                                 SysprofInstrument *instrument)
{
  g_return_if_fail (SYSPROF_IS_PROFILER (self));
  g_return_if_fail (SYSPROF_IS_INSTRUMENT (instrument));

  g_ptr_array_add (self->instruments, instrument);
}

void
sysprof_profiler_record_async (SysprofProfiler      *self,
                               SysprofCaptureWriter *writer,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
  g_autoptr(SysprofRecording) recording = NULL;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (SYSPROF_IS_PROFILER (self));
  g_return_if_fail (writer != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, sysprof_profiler_record_async);

  recording = _sysprof_recording_new (writer,
                                      self->spawnable,
                                      (SysprofInstrument **)self->instruments->pdata,
                                      self->instruments->len);

  g_task_return_pointer (task, g_object_ref (recording), g_object_unref);

  _sysprof_recording_start (recording);
}

/**
 * sysprof_profiler_record_finish:
 * @self: a #SysprofProfiler
 * @result: a #GAsyncResult
 * @error: a location for a #GError
 *
 * Completes an asynchronous request to start recording.
 *
 * Returns: (transfer full): an active #SysprofRecording if successful;
 *   otherwise %NULL and @error is set.
 */
SysprofRecording *
sysprof_profiler_record_finish (SysprofProfiler  *self,
                                GAsyncResult     *result,
                                GError          **error)
{
  g_return_val_if_fail (SYSPROF_IS_PROFILER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * sysprof_profiler_get_spawnable:
 * @self: a #SysprofProfiler
 *
 * Gets the #SysprofProfiler:spawnable property.
 *
 * Returns: (nullable) (transfer none): a #SysprofSpawnable or %NULL
 */
SysprofSpawnable *
sysprof_profiler_get_spawnable (SysprofProfiler *self)
{
  g_return_val_if_fail (SYSPROF_IS_PROFILER (self), NULL);

  return self->spawnable;
}

void
sysprof_profiler_set_spawnable (SysprofProfiler  *self,
                                SysprofSpawnable *spawnable)
{
  g_return_if_fail (SYSPROF_IS_PROFILER (self));
  g_return_if_fail (!spawnable || SYSPROF_IS_SPAWNABLE (spawnable));

  if (g_set_object (&self->spawnable, spawnable))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SPAWNABLE]);
}
