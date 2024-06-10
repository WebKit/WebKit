/* rewrite-pid.c
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

#include <gio/gio.h>
#include <sysprof-capture.h>

/* Rewrite a capture file to change PID X to Y */

static void
rewrite_pid (guint8 *data,
             gsize   len,
             int     old_pid,
             int     new_pid)
{
  SysprofCaptureFileHeader header;
  guint8 *endptr = &data[len];
  guint8 *frameptr = (guint8 *)&data[sizeof (SysprofCaptureFileHeader)];
  gboolean swap;

  if (len < sizeof header)
    return;

  memcpy (&header, data, sizeof header);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  swap = !header.little_endian;
#else
  swap = header.little_endian;
#endif

  if (swap)
    {
      old_pid = GUINT16_SWAP_LE_BE (old_pid);
      new_pid = GUINT16_SWAP_LE_BE (new_pid);
    }

  while (frameptr < endptr)
    {
      SysprofCaptureFrame *frame = (SysprofCaptureFrame *)frameptr;

      if (frame->pid == old_pid)
        frame->pid = new_pid;

      if (swap)
        frameptr += GUINT16_SWAP_LE_BE (frame->len);
      else
        frameptr += frame->len;
    }
}

static gboolean
parse_args (int          argc,
            char       **argv,
            const char **file,
            int         *old_pid,
            int         *new_pid)
{
  if (argc != 4)
    return FALSE;

  *file = argv[1];

  if (!g_file_test (*file, G_FILE_TEST_IS_REGULAR))
    return FALSE;

  *old_pid = g_ascii_strtoll (argv[2], NULL, 10);
  *new_pid = g_ascii_strtoll (argv[3], NULL, 10);

  return *old_pid != 0 && *new_pid != 0;
}

int main (int argc,
          char *argv[])
{
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) gfile = NULL;
  const char *file;
  int old;
  int new;

  if (!parse_args (argc, argv, &file, &old, &new))
    {
      g_printerr ("usage: rewrite-pid FILE OLD_PID NEW_PID\n");
      return 1;
    }

  gfile = g_file_new_for_commandline_arg (file);

  if (!(bytes = g_file_load_bytes (gfile, NULL, NULL, &error)))
    {
      g_printerr ("error: %s\n", error->message);
      return 1;
    }

  rewrite_pid ((guint8 *)g_bytes_get_data (bytes, NULL),
               g_bytes_get_size (bytes),
               old, new);

  if (!g_file_set_contents (file,
                            (const char *)g_bytes_get_data (bytes, NULL),
                            g_bytes_get_size (bytes),
                            &error))
    {
      g_printerr ("error: %s\n", error->message);
      return 1;
    }

  return 0;
}
