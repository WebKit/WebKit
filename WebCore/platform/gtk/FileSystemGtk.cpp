/*
 * Copyright (C) 2007, 2009 Holger Hans Peter Freyther
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

#include "GOwnPtr.h"
#include "PlatformString.h"
#include "CString.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <unistd.h>

namespace WebCore {

/* On linux file names are just raw bytes, so also strings that cannot be encoded in any way
 * are valid file names. This mean that we cannot just store a file name as-is in a String
 * but we have to escape it.
 * On Windows the GLib file name encoding is always UTF-8 so we can optimize this case. */
String filenameToString(const char* filename)
{
    if (!filename)
        return String();

#if OS(WINDOWS)
    return String::fromUTF8(filename);
#else
    gchar* escapedString = g_uri_escape_string(filename, "/:", false);
    String string(escapedString);
    g_free(escapedString);
    return string;
#endif
}

char* filenameFromString(const String& string)
{
#if OS(WINDOWS)
    return g_strdup(string.utf8().data());
#else
    return g_uri_unescape_string(string.utf8().data(), 0);
#endif
}

// Converts a string to something suitable to be displayed to the user.
String filenameForDisplay(const String& string)
{
#if OS(WINDOWS)
    return string;
#else
    gchar* filename = filenameFromString(string);
    gchar* display = g_filename_to_utf8(filename, 0, 0, 0, 0);
    g_free(filename);
    if (!display)
        return string;

    String displayString = String::fromUTF8(display);
    g_free(display);

    return displayString;
#endif
}

bool fileExists(const String& path)
{
    bool result = false;
    gchar* filename = filenameFromString(path);

    if (filename) {
        result = g_file_test(filename, G_FILE_TEST_EXISTS);
        g_free(filename);
    }

    return result;
}

bool deleteFile(const String& path)
{
    bool result = false;
    gchar* filename = filenameFromString(path);

    if (filename) {
        result = g_remove(filename) == 0;
        g_free(filename);
    }

    return result;
}

bool deleteEmptyDirectory(const String& path)
{
    bool result = false;
    gchar* filename = filenameFromString(path);

    if (filename) {
        result = g_rmdir(filename) == 0;
        g_free(filename);
    }

    return result;
}

bool getFileSize(const String& path, long long& resultSize)
{
    gchar* filename = filenameFromString(path);
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

bool getFileModificationTime(const String& path, time_t& modifiedTime)
{
    gchar* filename = filenameFromString(path);
    if (!filename)
        return false;

    struct stat statResult;
    gint result = g_stat(filename, &statResult);
    g_free(filename);
    if (result != 0)
        return false;

    modifiedTime = statResult.st_mtime;
    return true;

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
    gchar* filename = filenameFromString(path);
    if (!filename)
        return false;

    gint result = g_mkdir_with_parents(filename, S_IRWXU);
    g_free(filename);

    return result == 0;
}

String homeDirectoryPath()
{
    return filenameToString(g_get_home_dir());
}

String pathGetFileName(const String& pathName)
{
    if (pathName.isEmpty())
        return pathName;

    char* tmpFilename = filenameFromString(pathName);
    char* baseName = g_path_get_basename(tmpFilename);
    String fileName = String::fromUTF8(baseName);
    g_free(baseName);
    g_free(tmpFilename);

    return fileName;
}

String directoryName(const String& path)
{
    /* No null checking needed */
    GOwnPtr<char> tmpFilename(filenameFromString(path));
    GOwnPtr<char> dirname(g_path_get_dirname(tmpFilename.get()));
    return String::fromUTF8(dirname.get());
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;

    gchar* filename = filenameFromString(path);
    GDir* dir = g_dir_open(filename, 0, 0);
    if (!dir)
        return entries;

    GPatternSpec *pspec = g_pattern_spec_new((filter.utf8()).data());
    while (const char* name = g_dir_read_name(dir)) {
        if (!g_pattern_match_string(pspec, name))
            continue;

        gchar* entry = g_build_filename(filename, name, NULL);
        entries.append(filenameToString(entry));
        g_free(entry);
    }
    g_dir_close(dir);
    g_free(filename);

    return entries;
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
        return CString();
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

bool unloadModule(PlatformModule module)
{
    return g_module_close(module);
}
}
