/*
 * Copyright (C) 2010 Arno Renevier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "test_utils.h"

#include <glib.h>
#include <glib/gstdio.h>

int testutils_relative_chdir(const gchar* target_filename, const gchar* executable_path)
{
    if (g_path_is_absolute(executable_path)) {
        if (g_chdir(g_path_get_dirname(executable_path))) {
            return -1;
        }
    }

    while (!g_file_test(target_filename, G_FILE_TEST_EXISTS)) {
        gchar *path_name;
        if (g_chdir("..")) {
            return -1;
        }
        g_assert(!g_str_equal((path_name = g_get_current_dir()), "/"));
        g_free(path_name);
    }

    gchar* dirname = g_path_get_dirname(target_filename);
    if (g_chdir(dirname)) {
        g_free(dirname);
        return -1;
    }

    g_free(dirname);
    return 0;
}
