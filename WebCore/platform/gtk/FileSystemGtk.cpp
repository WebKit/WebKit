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

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <wtf/text/CString.h>

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

CString fileSystemRepresentation(const String& path)
{
#if OS(WINDOWS)
    return path.utf8();
#else
    char* filename = g_uri_unescape_string(path.utf8().data(), 0);
    CString cfilename(filename);
    g_free(filename);
    return cfilename;
#endif
}

// Converts a string to something suitable to be displayed to the user.
String filenameForDisplay(const String& string)
{
#if OS(WINDOWS)
    return string;
#else
    CString filename = fileSystemRepresentation(string);
    gchar* display = g_filename_to_utf8(filename.data(), 0, 0, 0, 0);
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
    CString filename = fileSystemRepresentation(path);

    if (!filename.isNull())
        result = g_file_test(filename.data(), G_FILE_TEST_EXISTS);

    return result;
}

bool deleteFile(const String& path)
{
    bool result = false;
    CString filename = fileSystemRepresentation(path);

    if (!filename.isNull())
        result = g_remove(filename.data()) == 0;

    return result;
}

bool deleteEmptyDirectory(const String& path)
{
    bool result = false;
    CString filename = fileSystemRepresentation(path);

    if (!filename.isNull())
        result = g_rmdir(filename.data()) == 0;

    return result;
}

bool getFileSize(const String& path, long long& resultSize)
{
    CString filename = fileSystemRepresentation(path);
    if (filename.isNull())
        return false;

    struct stat statResult;
    gint result = g_stat(filename.data(), &statResult);
    if (result != 0)
        return false;

    resultSize = statResult.st_size;
    return true;
}

bool getFileModificationTime(const String& path, time_t& modifiedTime)
{
    CString filename = fileSystemRepresentation(path);
    if (filename.isNull())
        return false;

    struct stat statResult;
    gint result = g_stat(filename.data(), &statResult);
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
    CString filename = fileSystemRepresentation(path);
    if (filename.isNull())
        return false;

    gint result = g_mkdir_with_parents(filename.data(), S_IRWXU);

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

    CString tmpFilename = fileSystemRepresentation(pathName);
    char* baseName = g_path_get_basename(tmpFilename.data());
    String fileName = String::fromUTF8(baseName);
    g_free(baseName);

    return fileName;
}

String directoryName(const String& path)
{
    /* No null checking needed */
    GOwnPtr<char> dirname(g_path_get_dirname(fileSystemRepresentation(path).data()));
    return String::fromUTF8(dirname.get());
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;

    CString filename = fileSystemRepresentation(path);
    GDir* dir = g_dir_open(filename.data(), 0, 0);
    if (!dir)
        return entries;

    GPatternSpec *pspec = g_pattern_spec_new((filter.utf8()).data());
    while (const char* name = g_dir_read_name(dir)) {
        if (!g_pattern_match_string(pspec, name))
            continue;

        GOwnPtr<gchar> entry(g_build_filename(filename.data(), name, NULL));
        entries.append(filenameToString(entry.get()));
    }
    g_pattern_spec_free(pspec);
    g_dir_close(dir);

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

PlatformFileHandle openFile(const String& path, FileOpenMode mode)
{
    CString fsRep = fileSystemRepresentation(path);

    if (fsRep.isNull())
        return invalidPlatformFileHandle;

    int platformFlag = 0;
    if (mode == OpenForRead)
        platformFlag |= O_RDONLY;
    else if (mode == OpenForWrite)
        platformFlag |= (O_WRONLY | O_CREAT | O_TRUNC);

    return g_open(fsRep.data(), platformFlag, 0666);
}

void closeFile(PlatformFileHandle& handle)
{
    if (isHandleValid(handle)) {
        close(handle);
        handle = invalidPlatformFileHandle;
    }
}

long long seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    // FIXME - Awaiting implementation, see https://bugs.webkit.org/show_bug.cgi?id=43878
    return -1;
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    int totalBytesWritten = 0;
    while (totalBytesWritten < length) {
        int bytesWritten = write(handle, data, length - totalBytesWritten);
        if (bytesWritten < 0)
            return -1;
        totalBytesWritten += bytesWritten;
        data += bytesWritten;
    }

    return totalBytesWritten;
}

int readFromFile(PlatformFileHandle handle, char* data, int length)
{
    do {
        int bytesRead = read(handle, data, static_cast<size_t>(length));
        if (bytesRead >= 0)
            return bytesRead;
    } while (errno == EINTR);

    return -1;
}

bool unloadModule(PlatformModule module)
{
    return g_module_close(module);
}
}
