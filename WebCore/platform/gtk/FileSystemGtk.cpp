/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora, Ltd.
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FileSystem.h"

#include "NotImplemented.h"
#include "PlatformString.h"
#include "CString.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gutils.h>

namespace WebCore {

bool fileExists(const String& path)
{
    bool result = false;
    gchar* filename = g_filename_from_utf8(path.utf8().data(), -1, 0, 0, 0);

    if (filename) {
        result = g_file_test(filename, G_FILE_TEST_EXISTS);
        g_free(filename);
    }

    return result;
}

bool deleteFile(const String& path)
{
    bool result = false;
    gchar* filename = g_filename_from_utf8(path.utf8().data(), -1, 0, 0, 0);

    if (filename) {
        result = g_remove(filename) == 0;
        g_free(filename);
    }

    return result;
}

bool deleteEmptyDirectory(const String& path)
{
    bool result = false;
    gchar* filename = g_filename_from_utf8(path.utf8().data(), -1, 0, 0, 0);

    if (filename) {
        result = g_rmdir(filename) == 0;
        g_free(filename);
    }

    return result;
}

bool getFileSize(const String& path, long long& resultSize)
{
    gchar* filename = g_filename_from_utf8(path.utf8().data(), -1, 0, 0, 0);
    if (!filename)
        return false;

    struct stat statResult;
    gint result = g_stat(filename, &statResult);
    g_free(filename);
    if (result != 0)
        return false;

    resultSize = statResult.st_size;
    return true;
}

bool getFileModificationTime(const String&, time_t&)
{
    notImplemented();
    return false;
}

String pathByAppendingComponent(const String& path, const String& component)
{
    if (path.endsWith(G_DIR_SEPARATOR_S))
        return path + component;
    else
        return path + G_DIR_SEPARATOR_S + component;
}

bool makeAllDirectories(const String& path)
{
    gchar* filename = g_filename_from_utf8(path.utf8().data(), -1, 0, 0, 0);
    if (!filename)
        return false;

    gint result = g_mkdir_with_parents(filename, S_IRWXU);
    g_free(filename);

    return result == 0;
}

CString openTemporaryFile(const char* prefix, PlatformFileHandle& handle)
{
    gchar* filename = g_strdup_printf("%sXXXXXX", prefix);
    gchar* tempPath = g_build_filename(g_get_tmp_dir(), filename, NULL);
    g_free(filename);

    int fileDescriptor = g_mkstemp(tempPath);
    if (!isHandleValid(fileDescriptor)) {
        LOG_ERROR("Can't create a temporary file.");
        g_free(tempPath);
        return 0;
    }
    CString tempFilePath = tempPath;
    g_free(tempPath);

    handle = fileDescriptor;
    return tempFilePath;
}

void closeFile(PlatformFileHandle& handle)
{
    if (isHandleValid(handle)) {
        close(handle);
        handle = invalidPlatformFileHandle;
    }
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    int totalBytesWritten = 0;
    while (totalBytesWritten < length) {
        int bytesWritten = write(handle, data, length - totalBytesWritten);
        if (bytesWritten < 0)
            return -1;
        totalBytesWritten += bytesWritten;
    }

    return totalBytesWritten;
}
}
