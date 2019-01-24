/*
 * Copyright (C) 2007, 2009 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora, Ltd.
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include <wtf/FileSystem.h>

#include <gio/gfiledescriptorbased.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <sys/file.h>
#include <wtf/EnumTraits.h>
#include <wtf/FileMetadata.h>
#include <wtf/UUID.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WTF {

namespace FileSystemImpl {

/* On linux file names are just raw bytes, so also strings that cannot be encoded in any way
 * are valid file names. This mean that we cannot just store a file name as-is in a String
 * but we have to escape it.
 * On Windows the GLib file name encoding is always UTF-8 so we can optimize this case. */
String stringFromFileSystemRepresentation(const char* fileSystemRepresentation)
{
    if (!fileSystemRepresentation)
        return String();

#if OS(WINDOWS)
    return String::fromUTF8(fileSystemRepresentation);
#else
    GUniquePtr<gchar> escapedString(g_uri_escape_string(fileSystemRepresentation, "/:", FALSE));
    return escapedString.get();
#endif
}

static GUniquePtr<char> unescapedFilename(const String& path)
{
    if (path.isEmpty())
        return nullptr;
#if OS(WINDOWS)
    return GUniquePtr<char>(g_strdup(path.utf8().data()));
#else
    return GUniquePtr<char>(g_uri_unescape_string(path.utf8().data(), nullptr));
#endif
}

CString fileSystemRepresentation(const String& path)
{
#if OS(WINDOWS)
    return path.utf8();
#else
    GUniquePtr<gchar> filename = unescapedFilename(path);
    return filename.get();
#endif
}

// Converts a string to something suitable to be displayed to the user.
String filenameForDisplay(const String& string)
{
#if OS(WINDOWS)
    return string;
#else
    GUniquePtr<gchar> filename = unescapedFilename(string);
    if (!filename)
        return string;

    GUniquePtr<gchar> display(g_filename_to_utf8(filename.get(), -1, nullptr, nullptr, nullptr));
    if (!display)
        return string;

    return String::fromUTF8(display.get());
#endif
}

bool fileExists(const String& path)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    return filename ? g_file_test(filename.get(), G_FILE_TEST_EXISTS) : false;
}

bool deleteFile(const String& path)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    return filename ? g_remove(filename.get()) != -1 : false;
}

bool deleteEmptyDirectory(const String& path)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    return filename ? g_rmdir(filename.get()) != -1 : false;
}

static bool getFileStat(const String& path, GStatBuf* statBuffer)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    if (!filename)
        return false;

    return g_stat(filename.get(), statBuffer) != -1;
}

static bool getFileLStat(const String& path, GStatBuf* statBuffer)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    if (!filename)
        return false;

    return g_lstat(filename.get(), statBuffer) != -1;
}

bool getFileSize(const String& path, long long& resultSize)
{
    GStatBuf statResult;
    if (!getFileStat(path, &statResult))
        return false;

    resultSize = statResult.st_size;
    return true;
}

bool getFileSize(PlatformFileHandle handle, long long& resultSize)
{
    auto info = g_file_io_stream_query_info(handle, G_FILE_ATTRIBUTE_STANDARD_SIZE, nullptr, nullptr);
    if (!info)
        return false;

    resultSize = g_file_info_get_size(info);
    return true;
}

Optional<WallTime> getFileCreationTime(const String&)
{
    // FIXME: Is there a way to retrieve file creation time with Gtk on platforms that support it?
    return WTF::nullopt;
}

Optional<WallTime> getFileModificationTime(const String& path)
{
    GStatBuf statResult;
    if (!getFileStat(path, &statResult))
        return WTF::nullopt;

    return WallTime::fromRawSeconds(statResult.st_mtime);
}

static FileMetadata::Type toFileMetataType(GStatBuf statResult)
{
    if (S_ISDIR(statResult.st_mode))
        return FileMetadata::Type::Directory;
    if (S_ISLNK(statResult.st_mode))
        return FileMetadata::Type::SymbolicLink;
    return FileMetadata::Type::File;
}

static Optional<FileMetadata> fileMetadataUsingFunction(const String& path, bool (*statFunc)(const String&, GStatBuf*))
{
    GStatBuf statResult;
    if (!statFunc(path, &statResult))
        return WTF::nullopt;

    String filename = pathGetFileName(path);
    bool isHidden = !filename.isEmpty() && filename[0] == '.';

    return FileMetadata {
        WallTime::fromRawSeconds(statResult.st_mtime),
        statResult.st_size,
        isHidden,
        toFileMetataType(statResult)
    };
}

Optional<FileMetadata> fileMetadata(const String& path)
{
    return fileMetadataUsingFunction(path, &getFileLStat);
}

Optional<FileMetadata> fileMetadataFollowingSymlinks(const String& path)
{
    return fileMetadataUsingFunction(path, &getFileStat);
}

String pathByAppendingComponent(const String& path, const String& component)
{
    if (path.endsWith(G_DIR_SEPARATOR_S))
        return path + component;
    return path + G_DIR_SEPARATOR_S + component;
}

String pathByAppendingComponents(StringView path, const Vector<StringView>& components)
{
    StringBuilder builder;
    builder.append(path);
    for (auto& component : components) {
        builder.append(G_DIR_SEPARATOR_S);
        builder.append(component);
    }
    return builder.toString();
}

bool makeAllDirectories(const String& path)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    return filename ? g_mkdir_with_parents(filename.get(), S_IRWXU) != -1 : false;
}

String homeDirectoryPath()
{
    return stringFromFileSystemRepresentation(g_get_home_dir());
}

bool createSymbolicLink(const String& targetPath, const String& symbolicLinkPath)
{
    CString targetPathFSRep = fileSystemRepresentation(targetPath);
    if (!targetPathFSRep.data() || targetPathFSRep.data()[0] == '\0')
        return false;

    CString symbolicLinkPathFSRep = fileSystemRepresentation(symbolicLinkPath);
    if (!symbolicLinkPathFSRep.data() || symbolicLinkPathFSRep.data()[0] == '\0')
        return false;

    return !symlink(targetPathFSRep.data(), symbolicLinkPathFSRep.data());
}

String pathGetFileName(const String& pathName)
{
    GUniquePtr<gchar> tmpFilename = unescapedFilename(pathName);
    if (!tmpFilename)
        return pathName;

    GUniquePtr<gchar> baseName(g_path_get_basename(tmpFilename.get()));
    return String::fromUTF8(baseName.get());
}

bool getVolumeFreeSpace(const String& path, uint64_t& freeSpace)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    if (!filename)
        return false;

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(filename.get()));
    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_filesystem_info(file.get(), G_FILE_ATTRIBUTE_FILESYSTEM_FREE, 0, 0));
    if (!fileInfo)
        return false;

    freeSpace = g_file_info_get_attribute_uint64(fileInfo.get(), G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
    return !!freeSpace;
}

String directoryName(const String& path)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    if (!filename)
        return String();

    GUniquePtr<char> dirname(g_path_get_dirname(filename.get()));
    return String::fromUTF8(dirname.get());
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;

    GUniquePtr<gchar> filename = unescapedFilename(path);
    if (!filename)
        return entries;

    GUniquePtr<GDir> dir(g_dir_open(filename.get(), 0, nullptr));
    if (!dir)
        return entries;

    GUniquePtr<GPatternSpec> pspec(g_pattern_spec_new((filter.utf8()).data()));
    while (const char* name = g_dir_read_name(dir.get())) {
        if (!g_pattern_match_string(pspec.get(), name))
            continue;

        GUniquePtr<gchar> entry(g_build_filename(filename.get(), name, nullptr));
        entries.append(stringFromFileSystemRepresentation(entry.get()));
    }

    return entries;
}

String openTemporaryFile(const String& prefix, PlatformFileHandle& handle)
{
    GUniquePtr<gchar> filename(g_strdup_printf("%s%s", prefix.utf8().data(), createCanonicalUUIDString().utf8().data()));
    GUniquePtr<gchar> tempPath(g_build_filename(g_get_tmp_dir(), filename.get(), NULL));
    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(tempPath.get()));

    handle = g_file_create_readwrite(file.get(), G_FILE_CREATE_NONE, 0, 0);
    if (!isHandleValid(handle))
        return String();
    return String::fromUTF8(tempPath.get());
}

PlatformFileHandle openFile(const String& path, FileOpenMode mode)
{
    GUniquePtr<gchar> filename = unescapedFilename(path);
    if (!filename)
        return invalidPlatformFileHandle;

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(filename.get()));
    GFileIOStream* ioStream = 0;
    if (mode == FileOpenMode::Read)
        ioStream = g_file_open_readwrite(file.get(), 0, 0);
    else if (mode == FileOpenMode::Write) {
        if (g_file_test(filename.get(), static_cast<GFileTest>(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)))
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
    case FileSeekOrigin::Beginning:
        seekType = G_SEEK_SET;
        break;
    case FileSeekOrigin::Current:
        seekType = G_SEEK_CUR;
        break;
    case FileSeekOrigin::End:
        seekType = G_SEEK_END;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (!g_seekable_seek(G_SEEKABLE(g_io_stream_get_input_stream(G_IO_STREAM(handle))),
        offset, seekType, 0, 0))
    {
        return -1;
    }
    return g_seekable_tell(G_SEEKABLE(g_io_stream_get_input_stream(G_IO_STREAM(handle))));
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    gsize bytesWritten;
    g_output_stream_write_all(g_io_stream_get_output_stream(G_IO_STREAM(handle)),
        data, length, &bytesWritten, 0, 0);
    return bytesWritten;
}

int readFromFile(PlatformFileHandle handle, char* data, int length)
{
    GUniqueOutPtr<GError> error;
    do {
        gssize bytesRead = g_input_stream_read(g_io_stream_get_input_stream(G_IO_STREAM(handle)),
            data, length, 0, &error.outPtr());
        if (bytesRead >= 0)
            return bytesRead;
    } while (error && error->code == G_FILE_ERROR_INTR);
    return -1;
}

bool moveFile(const String& oldPath, const String& newPath)
{
    GUniquePtr<gchar> oldFilename = unescapedFilename(oldPath);
    if (!oldFilename)
        return false;

    GUniquePtr<gchar> newFilename = unescapedFilename(newPath);
    if (!newFilename)
        return false;

    GRefPtr<GFile> oldFile = adoptGRef(g_file_new_for_path(oldFilename.get()));
    GRefPtr<GFile> newFile = adoptGRef(g_file_new_for_path(newFilename.get()));

    return g_file_move(oldFile.get(), newFile.get(), G_FILE_COPY_OVERWRITE, nullptr, nullptr, nullptr, nullptr);
}

bool hardLinkOrCopyFile(const String& source, const String& destination)
{
#if OS(WINDOWS)
    return !!::CopyFile(source.charactersWithNullTermination().data(), destination.charactersWithNullTermination().data(), TRUE);
#else
    GUniquePtr<gchar> sourceFilename = unescapedFilename(source);
    if (!sourceFilename)
        return false;

    GUniquePtr<gchar> destinationFilename = unescapedFilename(destination);
    if (!destinationFilename)
        return false;

    if (!link(sourceFilename.get(), destinationFilename.get()))
        return true;

    // Hard link failed. Perform a copy instead.
    GRefPtr<GFile> sourceFile = adoptGRef(g_file_new_for_path(sourceFilename.get()));
    GRefPtr<GFile> destinationFile = adoptGRef(g_file_new_for_path(destinationFilename.get()));
    return g_file_copy(sourceFile.get(), destinationFile.get(), G_FILE_COPY_NONE, nullptr, nullptr, nullptr, nullptr);
#endif
}

Optional<int32_t> getFileDeviceId(const CString& fsFile)
{
    GUniquePtr<gchar> filename = unescapedFilename(fsFile.data());
    if (!filename)
        return WTF::nullopt;

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(filename.get()));
    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_filesystem_info(file.get(), G_FILE_ATTRIBUTE_UNIX_DEVICE, nullptr, nullptr));
    if (!fileInfo)
        return WTF::nullopt;

    return g_file_info_get_attribute_uint32(fileInfo.get(), G_FILE_ATTRIBUTE_UNIX_DEVICE);
}

#if USE(FILE_LOCK)
bool lockFile(PlatformFileHandle handle, OptionSet<FileLockMode> lockMode)
{
    COMPILE_ASSERT(LOCK_SH == WTF::enumToUnderlyingType(FileLockMode::Shared), LockSharedEncodingIsAsExpected);
    COMPILE_ASSERT(LOCK_EX == WTF::enumToUnderlyingType(FileLockMode::Exclusive), LockExclusiveEncodingIsAsExpected);
    COMPILE_ASSERT(LOCK_NB == WTF::enumToUnderlyingType(FileLockMode::Nonblocking), LockNonblockingEncodingIsAsExpected);
    auto* inputStream = g_io_stream_get_input_stream(G_IO_STREAM(handle));
    int result = flock(g_file_descriptor_based_get_fd(G_FILE_DESCRIPTOR_BASED(inputStream)), lockMode.toRaw());
    return result != -1;
}

bool unlockFile(PlatformFileHandle handle)
{
    auto* inputStream = g_io_stream_get_input_stream(G_IO_STREAM(handle));
    int result = flock(g_file_descriptor_based_get_fd(G_FILE_DESCRIPTOR_BASED(inputStream)), LOCK_UN);
    return result != -1;
}
#endif // USE(FILE_LOCK)

} // namespace FileSystemImpl
} // namespace WTF
