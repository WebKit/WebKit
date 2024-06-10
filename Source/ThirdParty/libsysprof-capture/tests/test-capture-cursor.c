/* test-capture-cursor.c
 *
 * Copyright 2016o-2019 Christian Hergert <chergert@redhat.com>
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

#include <glib.h>
#include <glib/gstdio.h>
#include <sysprof-capture.h>

static bool
increment (const SysprofCaptureFrame *frame,
           void                      *user_data)
{
  gint *count= user_data;
  (*count)++;
  return true;
}

static void
test_cursor_basic (void)
{
  SysprofCaptureReader *reader;
  SysprofCaptureWriter *writer;
  SysprofCaptureCursor *cursor;
  gint64 t = SYSPROF_CAPTURE_CURRENT_TIME;
  guint i;
  gint r;
  gint count = 0;

  writer = sysprof_capture_writer_new ("capture-cursor-file", 0);
  g_assert_nonnull (writer);

  sysprof_capture_writer_flush (writer);

  reader = sysprof_capture_reader_new ("capture-cursor-file");
  g_assert_nonnull (reader);

  for (i = 0; i < 100; i++)
    {
      r = sysprof_capture_writer_add_timestamp (writer, t, i, -1);
      g_assert_cmpint (r, ==, TRUE);
    }

  sysprof_capture_writer_flush (writer);

  cursor = sysprof_capture_cursor_new (reader);
  sysprof_capture_cursor_foreach (cursor, increment, &count);
  g_assert_cmpint (count, ==, 100);
  g_clear_pointer (&cursor, sysprof_capture_cursor_unref);

  sysprof_capture_reader_unref (reader);
  sysprof_capture_writer_unref (writer);

  g_unlink ("capture-cursor-file");
}

static void
test_cursor_null (void)
{
  SysprofCaptureCursor *cursor = sysprof_capture_cursor_new (NULL);
  gint count = 0;
  sysprof_capture_cursor_foreach (cursor, increment, &count);
  g_assert_cmpint (count, ==, 0);
  g_clear_pointer (&cursor, sysprof_capture_cursor_unref);
}

int
main (int argc,
      char *argv[])
{
  sysprof_clock_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/SysprofCaptureCursor/basic", test_cursor_basic);
  g_test_add_func ("/SysprofCaptureCursor/null", test_cursor_null);
  return g_test_run ();
}
