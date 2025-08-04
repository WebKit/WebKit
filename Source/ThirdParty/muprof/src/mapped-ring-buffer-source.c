/* mapped-ring-buffer-source.c
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

#include "config.h"

#include <glib.h>

#include "mapped-ring-buffer-source-private.h"

typedef struct _MappedRingSource
{
  GSource                   source;
  MappedRingBuffer         *buffer;
  MappedRingBufferCallback  callback;
  gpointer                  callback_data;
  GDestroyNotify            callback_data_destroy;
} MappedRingSource;

static gboolean
mapped_ring_source_dispatch (GSource     *source,
                             GSourceFunc  callback,
                             gpointer     user_data)
{
  MappedRingSource *real_source = (MappedRingSource *)source;

  g_assert (source != NULL);

  return mapped_ring_buffer_drain (real_source->buffer, real_source->callback, real_source->callback_data);
}

static void
mapped_ring_source_finalize (GSource *source)
{
  MappedRingSource *real_source = (MappedRingSource *)source;

  if (real_source != NULL)
    {
      mapped_ring_buffer_drain (real_source->buffer, real_source->callback, real_source->callback_data);

      if (real_source->callback_data_destroy)
        real_source->callback_data_destroy (real_source->callback_data);

      real_source->callback = NULL;
      real_source->callback_data = NULL;
      real_source->callback_data_destroy = NULL;

      g_clear_pointer (&real_source->buffer, mapped_ring_buffer_unref);
    }
}

static gboolean
mapped_ring_source_check (GSource *source)
{
  MappedRingSource *real_source = (MappedRingSource *)source;

  g_assert (real_source != NULL);
  g_assert (real_source->buffer != NULL);

  return !mapped_ring_buffer_is_empty (real_source->buffer);
}

static gboolean
mapped_ring_source_prepare (GSource *source,
                            gint    *timeout_)
{
  MappedRingSource *real_source = (MappedRingSource *)source;

  g_assert (real_source != NULL);
  g_assert (real_source->buffer != NULL);

  if (!mapped_ring_buffer_is_empty (real_source->buffer))
    return TRUE;

  *timeout_ = 5;

  return FALSE;
}

static GSourceFuncs mapped_ring_source_funcs = {
  .prepare  = mapped_ring_source_prepare,
  .check    = mapped_ring_source_check,
  .dispatch = mapped_ring_source_dispatch,
  .finalize = mapped_ring_source_finalize,
};

guint
mapped_ring_buffer_create_source_full (int                       priority,
                                       MappedRingBuffer         *self,
                                       MappedRingBufferCallback  source_func,
                                       gpointer                  user_data,
                                       GDestroyNotify            destroy)
{
  MappedRingSource *source;
  guint ret;

  g_return_val_if_fail (self != NULL, 0);
  g_return_val_if_fail (source_func != NULL, 0);

  source = (MappedRingSource *)g_source_new (&mapped_ring_source_funcs, sizeof (MappedRingSource));
  source->buffer = mapped_ring_buffer_ref (self);
  source->callback = source_func;
  source->callback_data = user_data;
  source->callback_data_destroy = destroy;
  g_source_set_static_name ((GSource *)source, "MappedRingSource");
  g_source_set_priority ((GSource *)source, priority);
  ret = g_source_attach ((GSource *)source, g_main_context_default ());
  g_source_unref ((GSource *)source);

  return ret;
}

guint
mapped_ring_buffer_create_source (MappedRingBuffer         *self,
                                  MappedRingBufferCallback  source_func,
                                  gpointer                  user_data)
{
  return mapped_ring_buffer_create_source_full (G_PRIORITY_DEFAULT, self, source_func, user_data, NULL);
}
