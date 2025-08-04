/* sysprof-controlfd-instrument.c
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

#include <glib.h>

#ifdef G_OS_UNIX
# include <fcntl.h>
# include <glib-unix.h>
# include <glib/gstdio.h>
# include <gio/gunixinputstream.h>
# include <gio/gunixoutputstream.h>
# include <gio/gunixconnection.h>
# include <sys/socket.h>
# include <sys/types.h>
#endif

#include "sysprof-controlfd-instrument-private.h"
#include "sysprof-recording-private.h"

#ifdef G_OS_UNIX
# include "mapped-ring-buffer.h"
# include "mapped-ring-buffer-source-private.h"
#endif

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SysprofCaptureReader, sysprof_capture_reader_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SysprofCaptureWriter, sysprof_capture_writer_unref)

struct _SysprofControlfdInstrument
{
  SysprofInstrument  parent_instance;

#ifdef G_OS_UNIX
  GUnixConnection   *connection;
  char               read_buf[10];
#endif
};

G_DEFINE_FINAL_TYPE (SysprofControlfdInstrument, sysprof_controlfd_instrument, SYSPROF_TYPE_INSTRUMENT)

#ifdef G_OS_UNIX
typedef struct _RingData
{
  SysprofCaptureWriter *writer;
  GArray *source_ids;
  guint id;
} RingData;

static void
ring_data_free (gpointer data)
{
  RingData *ring_data = data;

  for (guint i = 0; i < ring_data->source_ids->len; i++)
    {
      guint *id = &g_array_index (ring_data->source_ids, guint, i);

      if (*id == ring_data->id)
        {
          *id = 0;
          g_array_remove_index_fast (ring_data->source_ids, i);
          break;
        }
    }

  ring_data->id = 0;

  g_clear_pointer (&ring_data->writer, sysprof_capture_writer_unref);
  g_array_unref (ring_data->source_ids);
  g_free (ring_data);
}

static DexFuture *
sysprof_controlfd_instrument_prepare (SysprofInstrument *instrument,
                                      SysprofRecording  *recording)
{
  SysprofControlfdInstrument *self = (SysprofControlfdInstrument *)instrument;
  SysprofSpawnable *spawnable;
  g_autofree char *child_no_str = NULL;
  g_autoptr(GSocketConnection) stream = NULL;
  g_autoptr(GSocket) sock = NULL;
  int fds[2] = {-1, -1};
  int child_no;

  g_assert (SYSPROF_IS_CONTROLFD_INSTRUMENT (self));
  g_assert (SYSPROF_IS_RECORDING (recording));

  /* If the recording is not spawning a process, then there is
   * nothing for us to do.
   */
  if (!(spawnable = _sysprof_recording_get_spawnable (recording)))
    goto finish;

  /* Create a socket pair to send control messages over */
  if (socketpair (AF_LOCAL, SOCK_STREAM, 0, fds) != 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_CONNECTED,
                                  "Failed to create socketpair");

  /* Set FDs non-blocking so that we can use them from main
   * context iteration without blocking.
   */
  g_unix_set_fd_nonblocking (fds[0], TRUE, NULL);
  g_unix_set_fd_nonblocking (fds[1], TRUE, NULL);

  /* @child_no is assigned the FD the child will receive. We can
   * use that to set the environment variable of the control FD.
   */
  child_no = sysprof_spawnable_take_fd (spawnable, fds[1], -1);
  child_no_str = g_strdup_printf ("%d", child_no);
  sysprof_spawnable_setenv (spawnable, "SYSPROF_CONTROL_FD", child_no_str);

  if (!(sock = g_socket_new_from_fd (fds[0], NULL)))
    {
      close (fds[0]);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_NOT_CONNECTED,
                                    "Failed to create socket from FD");
    }

  self->connection = G_UNIX_CONNECTION (g_socket_connection_factory_create_connection (sock));

finish:
  return dex_future_new_for_boolean (TRUE);
}

typedef struct _SysprofControlfdRecording
{
  GIOStream        *stream;
  SysprofRecording *recording;
  DexFuture        *cancellable;
  GArray           *source_ids;
} SysprofControlfdRecording;

static void
sysprof_controlfd_recording_free (gpointer data)
{
  SysprofControlfdRecording *state = data;

  dex_clear (&state->cancellable);
  g_clear_pointer (&state->source_ids, g_array_unref);
  g_clear_object (&state->recording);
  g_clear_object (&state->stream);
  g_free (state);
}

static bool
sysprof_controlfd_instrument_frame_cb (gconstpointer  data,
                                       gsize         *length,
                                       gpointer       user_data)
{
  const SysprofCaptureFrame *fr = data;
  RingData *ring_data = user_data;

  g_assert (ring_data != NULL);
  g_assert (ring_data->source_ids != NULL);
  g_assert (ring_data->writer != NULL);
  g_assert (ring_data->id > 0);

  if G_UNLIKELY (*length < sizeof *fr ||
                 *length < fr->len ||
                 fr->type >= SYSPROF_CAPTURE_FRAME_LAST)
    return G_SOURCE_REMOVE;

  _sysprof_capture_writer_add_raw (ring_data->writer, fr);

  *length = fr->len;

  return G_SOURCE_CONTINUE;
}

static DexFuture *
sysprof_controlfd_instrument_record_fiber (gpointer user_data)
{
  SysprofControlfdRecording *state = user_data;
  g_autoptr(SysprofCaptureWriter) temp_writer = NULL;
  SysprofCaptureWriter *writer;
  g_autoptr(GError) error = NULL;
  g_autofd int mem_fd = -1;
  GInputStream *input;

  g_assert (state != NULL);
  g_assert (SYSPROF_IS_RECORDING (state->recording));
  g_assert (DEX_IS_CANCELLABLE (state->cancellable));
  g_assert (state->source_ids != NULL);

  input = g_io_stream_get_input_stream (G_IO_STREAM (state->stream));

  if (!(mem_fd = sysprof_memfd_create ("[controlfd-memfd]")))
    return dex_future_new_for_errno (errno);

  temp_writer = sysprof_capture_writer_new_from_fd (g_steal_fd (&mem_fd), 0);
  writer = _sysprof_recording_writer (state->recording);

  for (;;)
    {
      g_autoptr(DexFuture) future = dex_input_stream_read_bytes (input, 10, 0);
      g_autoptr(MappedRingBuffer) ring_buffer = NULL;
      g_autoptr(GBytes) bytes = NULL;
      const guint8 *data;
      gsize len;

      dex_await (dex_future_any (dex_ref (future),
                                 dex_ref (state->cancellable),
                                 NULL),
                 &error);

      if (error != NULL)
        goto handle_error;

      if (!(bytes = dex_await_boxed (dex_ref (future), &error)))
        goto handle_error;

      data = g_bytes_get_data (bytes, &len);
      if (len != 10 || memcmp (data, "CreatRing\0", 10) != 0)
        break;

      if ((ring_buffer = mapped_ring_buffer_new_reader (0)))
        {
          int fd = mapped_ring_buffer_get_fd (ring_buffer);
          RingData *ring_data;

          ring_data = g_new0 (RingData, 1);
          ring_data->writer = sysprof_capture_writer_ref (temp_writer);
          ring_data->source_ids = g_array_ref (state->source_ids);
          ring_data->id = mapped_ring_buffer_create_source_full (G_PRIORITY_HIGH,
                                                                 ring_buffer,
                                                                 sysprof_controlfd_instrument_frame_cb,
                                                                 ring_data,
                                                                 (GDestroyNotify)ring_data_free);

          g_array_append_val (state->source_ids, ring_data->id);

          g_unix_connection_send_fd (G_UNIX_CONNECTION (state->stream), fd, NULL, NULL);
        }
    }

handle_error:
  while (state->source_ids->len > 0)
    {
      guint id = g_array_index (state->source_ids, guint, state->source_ids->len-1);
      state->source_ids->len--;
      g_source_remove (id);
    }

  if (temp_writer != NULL)
    {
      g_autoptr(SysprofCaptureReader) reader = sysprof_capture_writer_create_reader (temp_writer);

      if (reader != NULL)
        sysprof_capture_writer_cat (writer, reader);
    }

  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_for_boolean (TRUE);
}

static void
_g_clear_source (gpointer data)
{
  guint *id = data;

  if (*id != 0)
    g_source_remove (*id);
}

static DexFuture *
sysprof_controlfd_instrument_record (SysprofInstrument *instrument,
                                     SysprofRecording  *recording,
                                     GCancellable      *cancellable)
{
  SysprofControlfdInstrument *self = (SysprofControlfdInstrument *)instrument;
  SysprofControlfdRecording *state;
  SysprofSpawnable *spawnable;

  g_assert (SYSPROF_IS_CONTROLFD_INSTRUMENT (self));
  g_assert (SYSPROF_IS_RECORDING (recording));

  if (!(spawnable = _sysprof_recording_get_spawnable (recording)))
    return dex_future_new_for_boolean (TRUE);

  state = g_new0 (SysprofControlfdRecording, 1);
  state->recording = g_object_ref (recording);
  state->stream = g_object_ref (G_IO_STREAM (self->connection));
  state->cancellable = dex_cancellable_new_from_cancellable (cancellable);
  state->source_ids = g_array_new (FALSE, FALSE, sizeof (guint));
  g_array_set_clear_func (state->source_ids, _g_clear_source);

  return dex_scheduler_spawn (NULL, 0,
                              sysprof_controlfd_instrument_record_fiber,
                              state,
                              sysprof_controlfd_recording_free);
}

static void
sysprof_controlfd_instrument_finalize (GObject *object)
{
  SysprofControlfdInstrument *self = (SysprofControlfdInstrument *)object;

  g_clear_object (&self->connection);

  G_OBJECT_CLASS (sysprof_controlfd_instrument_parent_class)->finalize (object);
}
#endif

static void
sysprof_controlfd_instrument_class_init (SysprofControlfdInstrumentClass *klass)
{
#ifdef G_OS_UNIX
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SysprofInstrumentClass *instrument_class = SYSPROF_INSTRUMENT_CLASS (klass);

  object_class->finalize = sysprof_controlfd_instrument_finalize;

  instrument_class->prepare = sysprof_controlfd_instrument_prepare;
  instrument_class->record = sysprof_controlfd_instrument_record;
#endif
}

static void
sysprof_controlfd_instrument_init (SysprofControlfdInstrument *self)
{
}

SysprofInstrument *
_sysprof_controlfd_instrument_new (void)
{
#ifndef G_OS_UNIX
  g_warning_once ("SysprofControlfdInstrument not supported on this platform");
#endif

  return g_object_new (SYSPROF_TYPE_CONTROLFD_INSTRUMENT, NULL);
}
