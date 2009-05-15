/*
 * Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#include <webkit/webkit.h>

#if GTK_CHECK_VERSION(2, 14, 0)

static void
test_webkit_download_create(void)
{
    WebKitNetworkRequest* request;
    WebKitDownload* download;
    const gchar* uri = "http://example.com";
    gchar* tmpDir;

    request = webkit_network_request_new(uri);
    download = webkit_download_new(request);
    g_object_unref(request);
    g_assert_cmpstr(webkit_download_get_uri(download), ==, uri);
    g_assert(webkit_download_get_network_request(download) == request);
    g_assert(g_strrstr(uri, webkit_download_get_suggested_filename(download)));
    g_assert(webkit_download_get_status(download) == WEBKIT_DOWNLOAD_STATUS_CREATED);
    g_assert(!webkit_download_get_total_size(download));
    g_assert(!webkit_download_get_current_size(download));
    g_assert(!webkit_download_get_progress(download));
    g_assert(!webkit_download_get_elapsed_time(download));
    tmpDir = g_filename_to_uri(g_get_tmp_dir(), NULL, NULL);
    webkit_download_set_destination_uri(download, tmpDir);
    g_assert_cmpstr(tmpDir, ==, webkit_download_get_destination_uri(download));;
    g_free(tmpDir);
    g_object_unref(download);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/download/create", test_webkit_download_create);
    return g_test_run ();
}

#else

int main(int argc, char** argv)
{
    g_critical("You will need at least GTK+ 2.14.0 to run the unit tests.");
    return 0;
}

#endif
