/*
 * Copyright (C) 2007, 2009 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora, Ltd.
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FileSystem.h"

#include "FileMetadata.h"
#include "UUID.h"
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/gobject/GlibUtilities.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// On linux file names are just raw bytes, so also strings that cannot be encoded in any way
// are valid file names. This mean that we cannot just store a file name as-is in a String
// but we have to escape it.
// On Windows the GLib file name encoding is always UTF-8 so we can optimize this case.
String filenameToString(const char* filename)
{
    if (!filename)
        return String();

    GOwnPtr<gchar> escapedString(g_uri_escape_string(filename, "/:", false));
    return escapedString.get();
}

CString fileSystemRepresentation(const String& path)
{
    GOwnPtr<gchar> filename(g_uri_unescape_string(path.utf8().data(), 0));
    return filename.get();
}

// Converts a string to something suitable to be displayed to the user.
String filenameForDisplay(const String& string)
{
    CString filename = fileSystemRepresentation(string);
    GOwnPtr<gchar> display(g_filename_to_utf8(filename.data(), 0, 0, 0, 0));
    if (!display)
        return string;

    return String::fromUTF8(display.get());
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
        result = !g_remove(filename.data());

    return result;
}

bool deleteEmptyDirectory(const String& path)
{
    bool result = false;
    CString filename = fileSystemRepresentation(path);

    if (!filename.isNull())
        result = !g_rmdir(filename.data());

    return result;
}

bool getFileSize(const String& path, long long& resultSize)
{
    CString filename = fileSystemRepresentation(path);
    if (filename.isNull())
        return false;

    GStatBuf statResult;
    gint result = g_stat(filename.data(), &statResult);
    if (result)
        return false;

    resultSize = statResult.st_size;
    return true;
}

bool getFileModificationTime(const String& path, time_t& modifiedTime)
{
    CString filename = fileSystemRepresentation(path);
    if (filename.isNull())
        return false;

    GStatBuf statResult;
    gint result = g_stat(filename.data(), &statResult);
    if (result)
        return false;

    modifiedTime = statResult.st_mtime;
    return true;
}

bool getFileMetadata(const String& path, FileMetadata& metadata)
{
    CString filename = fileSystemRepresentation(path);
    if (filename.isNull())
        return false;

    struct stat statResult;
    gint result = g_stat(filename.data(), &statResult);
    if (result)
        return false;

    metadata.modificationTime = statResult.st_mtime;
    metadata.length = statResult.st_size;
    metadata.type = S_ISDIR(statResult.st_mode) ? FileMetadata::TypeDirectory : FileMetadata::TypeFile;
    return true;
}

String pathByAppendingComponent(const String& path, const String& component)
{
    if (path.endsWith(G_DIR_SEPARATOR_S))
        return path + component;
    return path + G_DIR_SEPARATOR_S + component;
}

bool makeAllDirectories(const String& path)
{
    CString filename = fileSystemRepresentation(path);
    if (filename.isNull())
        return false;

    gint result = g_mkdir_with_parents(filename.data(), S_IRWXU);

    return !result;
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
    GOwnPtr<gchar> baseName(g_path_get_basename(tmpFilename.data()));
    return String::fromUTF8(baseName.get());
}

CString applicationDirectoryPath()
{
    CString path = getCurrentExecutablePath();
    if (!path.isNull())
        return path;

    // If the above fails, check the PATH env variable.
    GOwnPtr<char> currentExePath(g_find_program_in_path(g_get_prgname()));
    if (!currentExePath.get())
        return CString();

    GOwnPtr<char> dirname(g_path_get_dirname(currentExePath.get()));
    return dirname.get();
}

CString sharedResourcesPath()
{
    DEFINE_STATIC_LOCAL(CString, cachedPath, ());
    if (!cachedPath.isNull())
        return cachedPath;

    GOwnPtr<gchar> dataPath(g_build_filename(DATA_DIR, "webkitnix", NULL));

    cachedPath = dataPath.get();
    return cachedPath;
}

uint64_t getVolumeFreeSizeForPath(const char* path)
{
    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(path));
    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_filesystem_info(file.get(), G_FILE_ATTRIBUTE_FILESYSTEM_FREE, 0, 0));
    if (!fileInfo)
        return 0;

    return g_file_info_get_attribute_uint64(fileInfo.get(), G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
}

String directoryName(const String& path)
{
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

    GPatternSpec* pspec = g_pattern_spec_new((filter.utf8()).data());
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

String openTemporaryFile(const String& prefix, PlatformFileHandle& handle)
{
    GOwnPtr<gchar> filename(g_strdup_printf("%s%s", prefix.utf8().data(), createCanonicalUUIDString().utf8().data()));
    GOwnPtr<gchar> tempPath(g_build_filename(g_get_tmp_dir(), filename.get(), NULL));
    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(tempPath.get()));

    handle = g_file_create_readwrite(file.get(), G_FILE_CREATE_NONE, 0, 0);
    if (!isHandleValid(handle))
        return String();
    return String::fromUTF8(tempPath.get());
}

PlatformFileHandle openFile(const String& path, FileOpenMode mode)
{
    CString fsRep = fileSystemRepresentation(path);
    if (fsRep.isNull())
        return invalidPlatformFileHandle;

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(fsRep.data()));
    GFileIOStream* ioStream = 0;
    if (mode == OpenForRead)
        ioStream = g_file_open_readwrite(file.get(), 0, 0);
    else if (mode == OpenForWrite) {
        if (g_file_test(fsRep.data(), static_cast<GFileTest>(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)))
            ioStream = g_file_open_readwrite(file.get(), 0, 0);
        else
            ioStream = g_file_create_readwrite(file.get(), G_FILE_CREATE_NONE, 0, 0);
    }

    return ioStream;
}

void closeFile(PlatformFileHandle& handle)
{
    if (!isHandleValid(handle))
        return;

    g_io_stream_close(G_IO_STREAM(handle), 0, 0);
    g_object_unref(handle);
    handle = invalidPlatformFileHandle;
}

long long seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    GSeekType seekType = G_SEEK_SET;
    switch (origin) {
    case SeekFromBeginning:
        seekType = G_SEEK_SET;
        break;
    case SeekFromCurrent:
        seekType = G_SEEK_CUR;
        break;
    case SeekFromEnd:
        seekType = G_SEEK_END;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (!g_seekable_seek(G_SEEKABLE(g_io_stream_get_input_stream(G_IO_STREAM(handle))), offset, seekType, 0, 0))
        return -1;
    return g_seekable_tell(G_SEEKABLE(g_io_stream_get_input_stream(G_IO_STREAM(handle))));
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    gsize bytesWritten;
    g_output_stream_write_all(g_io_stream_get_output_stream(G_IO_STREAM(handle)), data, length, &bytesWritten, 0, 0);
    return bytesWritten;
}

int readFromFile(PlatformFileHandle handle, char* data, int length)
{
    GOwnPtr<GError> error;
    do {
        gssize bytesRead = g_input_stream_read(g_io_stream_get_input_stream(G_IO_STREAM(handle)), data, length, 0, &error.outPtr());
        if (bytesRead >= 0)
            return bytesRead;
    } while (error && error->code == G_FILE_ERROR_INTR);
    return -1;
}

bool unloadModule(PlatformModule module)
{
    return g_module_close(module);
}

} // namespace WebCore

