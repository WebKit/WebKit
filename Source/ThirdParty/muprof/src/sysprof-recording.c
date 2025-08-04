/* sysprof-recording.c
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

#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

#include <libdex.h>

#include "sysprof-recording-private.h"

enum {
  PROP_0,
  PROP_DURATION,
  PROP_EVENT_COUNT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (SysprofRecording, sysprof_recording, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static DexFuture *
_sysprof_recording_spawn (SysprofSpawnable  *spawnable,
                          GSubprocess      **subprocess)
{
  g_autoptr(GError) error = NULL;
  DexFuture *ret;

  g_assert (SYSPROF_IS_SPAWNABLE (spawnable));
  g_assert (subprocess != NULL);
  g_assert (*subprocess == NULL);

  if (!(*subprocess = sysprof_spawnable_spawn (spawnable, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  ret = dex_subprocess_wait_check (*subprocess);
  dex_async_pair_set_cancel_on_discard (DEX_ASYNC_PAIR (ret), FALSE);
  return ret;
}

static inline void
add_metadata (SysprofRecording *self,
              const char       *id,
              const char       *value)
{
  if (value == NULL)
    return;

  sysprof_capture_writer_add_metadata (self->writer,
                                       SYSPROF_CAPTURE_CURRENT_TIME,
                                       -1, -1, id, value, -1);
}

static inline void
add_metadata_int (SysprofRecording *self,
                  const char       *id,
                  int               value)
{
  char str[16];
  g_snprintf (str, sizeof str, "%d", value);
  sysprof_capture_writer_add_metadata (self->writer,
                                       SYSPROF_CAPTURE_CURRENT_TIME,
                                       -1, -1, id, str, -1);
}

static inline void
add_metadata_int64 (SysprofRecording *self,
                    const char       *id,
                    gint64            value)
{
  char str[32];
  g_snprintf (str, sizeof str, "%"G_GINT64_FORMAT, value);
  sysprof_capture_writer_add_metadata (self->writer,
                                       SYSPROF_CAPTURE_CURRENT_TIME,
                                       -1, -1, id, str, -1);
}

static DexFuture *
sysprof_recording_fiber (gpointer user_data)
{
  SysprofRecording *self = user_data;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(DexFuture) record = NULL;
  g_autoptr(DexFuture) monitor = NULL;
  g_autoptr(DexFuture) message = NULL;
  g_autoptr(GError) error = NULL;
  struct utsname uts;
  struct sysinfo si;
  gint64 begin_time;
  gint64 end_time;
  char hostname[64] = {0};

  g_assert (SYSPROF_IS_RECORDING (self));

  cancellable = g_cancellable_new ();

  /* First ensure that all our required policy have been acquired on
   * the bus so that we don't need to individually acquire them from
   * each of the instruments after the recording starts.
   */
  if (!dex_await (_sysprof_instruments_acquire_policy (self->instruments, self), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Now allow instruments to prepare for the recording */
  if (!dex_await (_sysprof_instruments_prepare (self->instruments, self), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Ask instruments to start recording and stop if cancelled. */
  record = _sysprof_instruments_record (self->instruments, self, cancellable);

  /* Now take our begin time now that all instruments are notified */
  begin_time = SYSPROF_CAPTURE_CURRENT_TIME;

  g_assert (self->subprocess == NULL);

  /* If we need to spawn a subprocess, do it now */
  if (self->spawnable != NULL)
    monitor = _sysprof_recording_spawn (self->spawnable, &self->subprocess);
  else
    monitor = dex_future_new_infinite ();

  /* Track various app metadata */
  add_metadata (self, "org.gnome.sysprof.app-id", "com.feaneron.Muprof");
  add_metadata (self, "org.gnome.sysprof.version", PACKAGE_VERSION);

  /* Give a readable timestamp to the user */
  {
    g_autoptr(GDateTime) now = g_date_time_new_now_local ();
    g_autofree char *now_str = g_date_time_format_iso8601 (now);
    add_metadata (self, "capture-time", now_str);
  }

  /* Include some host/kernel/arch information */
  add_metadata_int (self, "n-cpu", g_get_num_processors ());
  add_metadata_int (self, "page-size", sysprof_getpagesize ());
  add_metadata_int (self, "buffer-size", sysprof_capture_writer_get_buffer_size (self->writer));
  if (uname (&uts) == 0)
    {
      add_metadata (self, "uname.sysname", uts.sysname);
      add_metadata (self, "uname.release", uts.release);
      add_metadata (self, "uname.version", uts.version);
      add_metadata (self, "uname.machine", uts.machine);
    }

  if (gethostname (hostname, sizeof hostname-1) == 0)
    add_metadata (self, "hostname", hostname);

  /* More system information via sysinfo */
  if (sysinfo (&si) == 0)
    {
      add_metadata_int64 (self, "sysinfo.uptime", si.uptime);
      add_metadata_int64 (self, "sysinfo.totalram", si.totalram);
      add_metadata_int64 (self, "sysinfo.freeram", si.freeram);
      add_metadata_int64 (self, "sysinfo.sharedram", si.sharedram);
      add_metadata_int64 (self, "sysinfo.bufferram", si.bufferram);
      add_metadata_int64 (self, "sysinfo.totalswap", si.totalswap);
      add_metadata_int64 (self, "sysinfo.freeswap", si.freeswap);
      add_metadata_int64 (self, "sysinfo.procs", si.procs);
      add_metadata_int64 (self, "sysinfo.totalhigh", si.totalhigh);
      add_metadata_int64 (self, "sysinfo.freehigh", si.freehigh);
      add_metadata_int64 (self, "sysinfo.mem_unit", si.mem_unit);
    }

  /* Some environment variables/info for correlating */
  add_metadata (self, "USER", g_get_user_name ());
  add_metadata (self, "DISPLAY", g_getenv ("DISPLAY"));
  add_metadata (self, "WAYLAND_DISPLAY", g_getenv ("WAYLAND_DISPLAY"));
  add_metadata (self, "DESKTOP_SESSION", g_getenv ("DESKTOP_SESSION"));
  add_metadata (self, "HOSTTYPE", g_getenv ("HOSTTYPE"));
  add_metadata (self, "OSTYPE", g_getenv ("OSTYPE"));

  /* Log information about the spawning process */
  if (self->spawnable != NULL)
    {
      const char * const *argv = sysprof_spawnable_get_argv (self->spawnable);
      const char * const *env = sysprof_spawnable_get_environ (self->spawnable);
      const char *cwd = sysprof_spawnable_get_cwd (self->spawnable);

      if (cwd)
        add_metadata (self, "spawnable.cwd", cwd);

      if (env != NULL)
        {
          g_autoptr(GString) str = g_string_new (NULL);

          for (guint e = 0; env[e]; e++)
            {
              g_autofree char *quoted = g_shell_quote (env[e]);

              g_string_append (str, quoted);
              g_string_append_c (str, ' ');
            }

          add_metadata (self, "spawnable.environ", str->str);
        }

      if (argv != NULL)
        {
          g_autoptr(GString) str = g_string_new (NULL);

          for (guint a = 0; argv[a]; a++)
            {
              g_autofree char *quoted = g_shell_quote (argv[a]);

              g_string_append (str, quoted);
              g_string_append_c (str, ' ');
            }

          add_metadata (self, "spawnable.argv", str->str);
        }
    }

  /* Save a copy of os-release for troubleshooting */
  dex_await (_sysprof_recording_add_file (self, "/etc/os-release", FALSE), NULL);

  self->start_time = g_get_monotonic_time ();

  /* Queue receive of first message */
  message = dex_channel_receive (self->channel);

  /* Wait for messages on our channel or the recording to complete */
  for (;;)
    {
      g_autoptr(DexFuture) duration = dex_timeout_new_seconds (1);

      g_debug ("Recording loop iteration");

      /* Wait for either recording of all instruments to complete or a
       * message from our channel with what to do next.
       */
      if (!dex_await (dex_future_first (dex_ref (record),
                                        dex_ref (message),
                                        dex_ref (monitor),
                                        dex_ref (duration),
                                        NULL),
                      &error) &&
          !g_error_matches (error, DEX_ERROR, DEX_ERROR_TIMED_OUT))
        goto stop_recording;

      /* Clear any ignored error */
      g_clear_error (&error);

      /* If record is not pending, then everything resolved/rejected */
      if (dex_future_get_status (record) != DEX_FUTURE_STATUS_PENDING ||
          dex_future_get_status (monitor) != DEX_FUTURE_STATUS_PENDING)
        goto stop_recording;

      /* If message resolved, then we got a command to process */
      if (dex_future_get_status (message) == DEX_FUTURE_STATUS_RESOLVED)
        {
          SysprofRecordingCommand command = dex_await_uint (dex_ref (message), NULL);

          switch (command)
            {
            case SYSPROF_RECORDING_COMMAND_STOP:
              g_debug ("Recording received stop command");
              goto stop_recording;

            default:
              break;
            }

          /* Queue receive of next message */
          dex_clear (&message);
          message = dex_channel_receive (self->channel);
        }

      /* Update duration each pass through the loop */
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);

      /* Update event count each pass through the loop */
      sysprof_capture_writer_stat (self->writer, &self->stat);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EVENT_COUNT]);
    }

stop_recording:
  g_debug ("Stopping recording");

  end_time = SYSPROF_CAPTURE_CURRENT_TIME;

  self->end_time = g_get_monotonic_time ();
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);

  /* Signal cancellable so that anything lingering has a chance to be
   * cleaned up, cascading into other subsystems.
   */
  g_cancellable_cancel (cancellable);

  /* But we must still wait for instruments to respond to
   * the cancellation and clean up before we can move onto
   * the augmentation phase.
   */
  dex_await (dex_ref (record), NULL);

  /* Let instruments augment the capture. Some instruments may include
   * extra information about the capture such as symbol names and their
   * address ranges per-process.
   */
  dex_await (_sysprof_instruments_augment (self->instruments, self), NULL);

  /* Update start/end times to be the "running time" */
  _sysprof_capture_writer_set_time_range (self->writer, begin_time, end_time);

  /* Clear buffers and ensure the disk layer has access to them */
  sysprof_capture_writer_flush (self->writer);

  /* Aggressively release our instruments so if they have references back
   * to the recording any reference cycles are broken.
   */
  if (self->instruments->len > 0)
    g_ptr_array_remove_range (self->instruments, 0, self->instruments->len);

  /* Ignore error types we use to bail out of loops */
  if (error != NULL &&
      !g_error_matches (error, DEX_ERROR, DEX_ERROR_TIMED_OUT) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_for_boolean (TRUE);
}

static void
sysprof_recording_finalize (GObject *object)
{
  SysprofRecording *self = (SysprofRecording *)object;

  if (self->channel)
    {
      dex_channel_close_send (self->channel);
      dex_clear (&self->channel);
    }

  g_clear_pointer (&self->writer, sysprof_capture_writer_unref);
  g_clear_pointer (&self->instruments, g_ptr_array_unref);
  g_clear_object (&self->spawnable);
  g_clear_object (&self->diagnostics);
  g_clear_object (&self->subprocess);
  dex_clear (&self->fiber);

  G_OBJECT_CLASS (sysprof_recording_parent_class)->finalize (object);
}

static void
sysprof_recording_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  SysprofRecording *self = SYSPROF_RECORDING (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      g_value_set_int64 (value, sysprof_recording_get_duration (self));
      break;

    case PROP_EVENT_COUNT:
      g_value_set_int64 (value, sysprof_recording_get_event_count (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_recording_class_init (SysprofRecordingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = sysprof_recording_finalize;
  object_class->get_property = sysprof_recording_get_property;

  properties [PROP_DURATION] =
    g_param_spec_int64 ("duration", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_EVENT_COUNT] =
    g_param_spec_int64 ("event-count", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
sysprof_recording_init (SysprofRecording *self)
{
  self->channel = dex_channel_new (0);
  self->instruments = g_ptr_array_new_with_free_func (g_object_unref);
  self->diagnostics = g_list_store_new (SYSPROF_TYPE_DIAGNOSTIC);
}

SysprofRecording *
_sysprof_recording_new (SysprofCaptureWriter  *writer,
                        SysprofSpawnable      *spawnable,
                        SysprofInstrument    **instruments,
                        guint                  n_instruments)
{
  SysprofRecording *self;

  g_return_val_if_fail (writer != NULL, NULL);

  self = g_object_new (SYSPROF_TYPE_RECORDING, NULL);
  self->writer = sysprof_capture_writer_ref (writer);

  g_set_object (&self->spawnable, spawnable);

  for (guint i = 0; i < n_instruments; i++)
    g_ptr_array_add (self->instruments, g_object_ref (instruments[i]));

  return self;
}

void
_sysprof_recording_start (SysprofRecording *self)
{

  g_return_if_fail (SYSPROF_IS_RECORDING (self));
  g_return_if_fail (self->fiber == NULL);

  self->fiber = dex_scheduler_spawn (NULL, 0,
                                     sysprof_recording_fiber,
                                     g_object_ref (self),
                                     g_object_unref);
}

void
sysprof_recording_stop_async (SysprofRecording    *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(DexAsyncResult) result = NULL;

  g_return_if_fail (SYSPROF_IS_RECORDING (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  result = dex_async_result_new (self, cancellable, callback, user_data);
  dex_async_result_await (result,
                          dex_channel_send (self->channel,
                                            dex_future_new_for_uint (SYSPROF_RECORDING_COMMAND_STOP)));
}

gboolean
sysprof_recording_stop_finish (SysprofRecording  *self,
                               GAsyncResult      *result,
                               GError           **error)
{
  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

void
sysprof_recording_wait_async (SysprofRecording    *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(DexAsyncResult) result = NULL;

  g_return_if_fail (SYSPROF_IS_RECORDING (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  result = dex_async_result_new (self, cancellable, callback, user_data);
  dex_async_result_await (result, dex_ref (self->fiber));
}

gboolean
sysprof_recording_wait_finish (SysprofRecording  *self,
                               GAsyncResult      *result,
                               GError           **error)
{
  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

SysprofSpawnable *
_sysprof_recording_get_spawnable (SysprofRecording *self)
{
  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), NULL);

  return self->spawnable;
}

typedef struct _AddFile
{
  SysprofCaptureWriter *writer;
  char *path;
  guint compress : 1;
} AddFile;

static void
add_file_free (AddFile *add_file)
{
  g_clear_pointer (&add_file->writer, sysprof_capture_writer_unref);
  g_clear_pointer (&add_file->path, g_free);
  g_free (add_file);
}

static DexFuture *
sysprof_recording_add_file_fiber (gpointer user_data)
{
  AddFile *add_file = user_data;
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(GOutputStream) memory_stream = NULL;
  g_autoptr(GOutputStream) zlib_stream = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GFile) proc = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *dest_path = NULL;
  const guint8 *data = NULL;
  GOutputStream *output;
  gsize len;

  g_assert (add_file != NULL);
  g_assert (add_file->path != NULL);
  g_assert (add_file->writer != NULL);

  /* If we are compressing the file, store it as ".gz" in the capture
   * so that the reader knows to decompress it automatically.
   */
  dest_path = add_file->compress ? g_strdup_printf ("%s.gz", add_file->path)
                                 : g_strdup (add_file->path);

  file = g_file_new_for_path (add_file->path);
  proc = g_file_new_for_path ("/proc");

  /* If the file has a prefix of `/proc/` then we need to request the
   * file from sysprofd as our user is not guaranteed to be able to
   * read the file. Even if we can open the file, we may get data that
   * has been redacted (such as /proc/kallsyms).
   *
   * With `/proc/kallsyms` it's even worse in that if we get an FD back
   * from sysprofd and read it from our process, it *too* will be redacted.
   * That leaves us with the only option of letting sysprofd read the file
   * and transfer the contents to our process over D-Bus.
   *
   * We use g_file_has_prefix() for this as it will canonicalize the paths
   * of #GFile rather than us having to be careful here.
   */
  if (g_file_has_prefix (file, proc))
    {
      g_autoptr(GDBusConnection) connection = NULL;
      g_autoptr(GVariant) reply = NULL;
      g_autoptr(GBytes) input_bytes = NULL;

      if (!(connection = dex_await_object (dex_bus_get (G_BUS_TYPE_SYSTEM), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(reply = dex_await_variant (dex_dbus_connection_call (connection,
                                                                 "org.gnome.Sysprof3",
                                                                 "/org/gnome/Sysprof3",
                                                                 "org.gnome.Sysprof3.Service",
                                                                 "GetProcFile",
                                                                 g_variant_new ("(^ay)", g_file_get_path (file)),
                                                                 G_VARIANT_TYPE ("(ay)"),
                                                                 G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
                                                                 G_MAXINT),
                                       &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      input_bytes = g_variant_get_data_as_bytes (reply);
      input = g_memory_input_stream_new_from_bytes (input_bytes);
    }
  else
    {
      if (!(input = dex_await_object (dex_file_read (file, 0), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  g_assert (input != NULL);
  g_assert (G_IS_INPUT_STREAM (input));

  output = memory_stream = g_memory_output_stream_new_resizable ();

  if (add_file->compress)
    {
      g_autoptr(GZlibCompressor) compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, 6);
      output = zlib_stream = g_converter_output_stream_new (memory_stream, G_CONVERTER (compressor));
    }

  g_assert (output != NULL);
  g_assert (G_IS_OUTPUT_STREAM (output));
  g_assert (G_IS_MEMORY_OUTPUT_STREAM (output) || G_IS_CONVERTER_OUTPUT_STREAM (output));
  g_assert (G_IS_MEMORY_OUTPUT_STREAM (memory_stream));
  g_assert (!zlib_stream || G_IS_CONVERTER_OUTPUT_STREAM (zlib_stream));

  if (!dex_await (dex_output_stream_splice (output,
                                            input,
                                            (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                             G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                            0),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (memory_stream));
  data = g_bytes_get_data (bytes, &len);

  while (len > 0)
    {
      gsize to_write = MIN (len, ((4096*8)-sizeof (SysprofCaptureFileChunk)));

      if (!sysprof_capture_writer_add_file (add_file->writer,
                                            SYSPROF_CAPTURE_CURRENT_TIME,
                                            -1,
                                            -1,
                                            dest_path,
                                            to_write == len,
                                            data,
                                            to_write))
        break;

      len -= to_write;
      data += to_write;
    }

  return dex_future_new_for_boolean (TRUE);
}

DexFuture *
_sysprof_recording_add_file (SysprofRecording *self,
                             const char       *path,
                             gboolean          compress)
{
  AddFile *add_file;

  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  add_file = g_new0 (AddFile, 1);
  add_file->writer = sysprof_capture_writer_ref (self->writer);
  add_file->path = g_strdup (path);
  add_file->compress = !!compress;

  return dex_scheduler_spawn (NULL, 0,
                              sysprof_recording_add_file_fiber,
                              g_steal_pointer (&add_file),
                              (GDestroyNotify)add_file_free);
}

static char *
do_compress (const char *data,
             gsize       length,
             gsize      *out_length)
{
  g_autoptr(GZlibCompressor) compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, 6);
  g_autofree char *compressed = g_malloc (length);
  GConverterResult res;
  gsize n_read;
  gsize n_written;

  *out_length = 0;

  res = g_converter_convert (G_CONVERTER (compressor), data, length, compressed, length,
                             G_CONVERTER_INPUT_AT_END | G_CONVERTER_FLUSH,
                             &n_read, &n_written, NULL);

  if (res == G_CONVERTER_FINISHED)
    {
      *out_length = n_written;
      return g_steal_pointer (&compressed);
    }

  return NULL;
}

void
_sysprof_recording_add_file_data (SysprofRecording *self,
                                  const char       *path,
                                  const char       *contents,
                                  gssize            length,
                                  gboolean          compress)
{
  g_autofree char *compress_filename = NULL;
  g_autofree char *compress_bytes = NULL;
  gsize compress_len = 0;

  g_return_if_fail (SYSPROF_IS_RECORDING (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (contents != NULL);

  if (length < 0)
    length = strlen (contents);

  if (length == 0)
    compress = FALSE;

  if (compress)
    {
      compress_bytes = do_compress (contents, length, &compress_len);

      if (compress_bytes)
        {
          compress_filename = g_strdup_printf ("%s.gz", path);
          path = compress_filename;
          contents = compress_bytes;
          length = compress_len;
        }
    }

  while (length > 0)
    {
      gsize to_write = MIN (length, ((4096*8)-sizeof (SysprofCaptureFileChunk)));

      if (!sysprof_capture_writer_add_file (self->writer,
                                            SYSPROF_CAPTURE_CURRENT_TIME,
                                            -1,
                                            -1,
                                            path,
                                            to_write == length,
                                            (const guint8 *)contents,
                                            to_write))
        break;

      length -= to_write;
      contents += to_write;
    }
}

G_GNUC_PRINTF(3,0)
static void
_sysprof_recording_message_internal (SysprofRecording *self,
                                     const char       *domain,
                                     const char       *format,
                                     va_list          *args,
                                     gboolean          fatal)
{
  g_autoptr(SysprofDiagnostic) diagnostic = NULL;

  g_assert (SYSPROF_IS_RECORDING (self));
  g_assert (domain != NULL);
  g_assert (format != NULL);
  g_assert (args != NULL);

  diagnostic = _sysprof_diagnostic_new (g_strdup (domain),
                                        g_strdup_vprintf (format, *args),
                                        fatal);

  g_list_store_append (self->diagnostics, diagnostic);

  if (fatal)
    sysprof_recording_stop_async (self, NULL, NULL, NULL);
}

void
_sysprof_recording_diagnostic (SysprofRecording *self,
                               const char       *domain,
                               const char       *format,
                               ...)
{
  va_list args;

  va_start (args, format);
  _sysprof_recording_message_internal (self, domain, format, &args, FALSE);
  va_end (args);
}

void
_sysprof_recording_error (SysprofRecording *self,
                          const char       *domain,
                          const char       *format,
                          ...)
{
  va_list args;

  va_start (args, format);
  _sysprof_recording_message_internal (self, domain, format, &args, TRUE);
  va_end (args);
}

/**
 * sysprof_recording_list_diagnostics:
 * @self: a #SysprofRecording
 *
 * Gets the diagnostics for the recording which may be updated as
 * instruments discover issues with the recording or configuration.
 *
 * Returns: (transfer full): a #GListModel of #SysprofDiagnostic
 */
GListModel *
sysprof_recording_list_diagnostics (SysprofRecording *self)
{
  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->diagnostics));
}

/**
 * sysprof_recording_get_duration:
 * @self: a #SysprofRecording
 *
 * Gets the recording duration in microseconds, which is the same
 * precision used by g_get_monotonic_time(). Use %G_USEC_PER_SEC to
 * get the time in seconds.
 *
 * Returns: the duration of the recording, or 0
 */
gint64
sysprof_recording_get_duration (SysprofRecording *self)
{
  gint64 start_time;
  gint64 end_time;

  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), 0);

  if (!(start_time = self->start_time))
    return 0;

  if (!(end_time = self->end_time))
    end_time = g_get_monotonic_time ();

  return end_time - start_time;
}

gint64
sysprof_recording_get_event_count (SysprofRecording *self)
{
  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), 0);

  return self->stat.frame_count[SYSPROF_CAPTURE_FRAME_SAMPLE]
       + self->stat.frame_count[SYSPROF_CAPTURE_FRAME_ALLOCATION]
       + self->stat.frame_count[SYSPROF_CAPTURE_FRAME_FORK]
       + self->stat.frame_count[SYSPROF_CAPTURE_FRAME_EXIT]
       + self->stat.frame_count[SYSPROF_CAPTURE_FRAME_CTRSET]
       + self->stat.frame_count[SYSPROF_CAPTURE_FRAME_MARK]
       + self->stat.frame_count[SYSPROF_CAPTURE_FRAME_LOG];
}

/**
 * sysprof_recording_create_reader:
 * @self: a #SysprofRecording
 *
 * Creates a new reader for the recording's writer.
 *
 * Returns: (transfer full): a #SysprofCaptureReader
 */
SysprofCaptureReader *
sysprof_recording_create_reader (SysprofRecording *self)
{
  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), NULL);

  return sysprof_capture_writer_create_reader (self->writer);
}

/**
 * sysprof_recording_dup_fd:
 * @self: a #SysprofRecording
 *
 * Duplicates the FD that is being used for writing the recording. This can
 * be useful if you want to open the recording using a
 * #SysprofDocumentLoader.
 *
 * Returns: a FD that the caller should `close()` when no longer in use.
 */
int
sysprof_recording_dup_fd (SysprofRecording *self)
{
  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), -1);

  return _sysprof_capture_writer_dup_fd (self->writer);
}

/**
 * sysprof_recording_get_subprocess:
 * @self: a #SysprofRecording
 *
 * Gets the #GSubprocess if one was spawned.
 *
 * Returns: (transfer none) (nullable): a #GSubprocess or %NULL
 */
GSubprocess *
sysprof_recording_get_subprocess (SysprofRecording *self)
{
  g_return_val_if_fail (SYSPROF_IS_RECORDING (self), NULL);

  return self->subprocess;
}

void
_sysprof_recording_follow_process (SysprofRecording *self,
                                   int               pid,
                                   const char       *comm)
{
  g_return_if_fail (SYSPROF_IS_RECORDING (self));
  g_return_if_fail (pid > 0);

  dex_future_disown (_sysprof_instruments_process_started (self->instruments, self, pid, comm));
}
