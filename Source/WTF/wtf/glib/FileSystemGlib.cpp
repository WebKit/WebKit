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
#include <wtf/UUID.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace WTF {

namespace FileSystemImpl {

String stringFromFileSystemRepresentation(const char* representation)
{
    if (!representation)
        return { };

    // Short-cut to String creation when only ASCII characters are used.
    size_t representationLength = strlen(representation);
    if (charactersAreAllASCII(reinterpret_cast<const LChar*>(representation), representationLength))
        return String(representation, representationLength);

    // If the returned charset is UTF-8 (i.e. g_get_filename_charsets() returns true),
    // go directly to String creation.
    const gchar** filenameCharsets = nullptr;
    if (g_get_filename_charsets(&filenameCharsets))
        return String::fromUTF8(representation, representationLength);

    ASSERT(filenameCharsets);
    // FIXME: If possible, we'd want to convert directly to UTF-16 and construct
    // WTF::String object with resulting data.
    size_t utf8Length = 0;
    GUniquePtr<gchar> utf8(g_convert(representation, representationLength,
        "UTF-8", filenameCharsets[0], nullptr, &utf8Length, nullptr));
    if (!utf8)
        return { };

    return String::fromUTF8(utf8.get(), utf8Length);
}

CString fileSystemRepresentation(const String& path)
{
    if (path.isNull())
        return { };
    if (path.isEmpty())
        return CString("");

    CString utf8 = path.utf8();

    // If the returned charset is UTF-8 (i.e. g_get_filename_charsets() returns true),
    // simply return the CString object.
    const gchar** filenameCharsets = nullptr;
    if (g_get_filename_charsets(&filenameCharsets))
        return utf8;

    ASSERT(filenameCharsets);
    // FIXME: If possible, we'd want to convert directly from WTF::String's UTF-16 data.
    size_t representationLength = 0;
    GUniquePtr<gchar> representation(g_convert(utf8.data(), utf8.length(),
        filenameCharsets[0], "UTF-8", nullptr, &representationLength, nullptr));
    if (!representation)
        return { };

    return CString(representation.get(), representationLength);
}

bool validRepresentation(const CString& representation)
{
    auto* data = representation.data();
    return !!data && data[0] != '\0';
}

// Converts a string to something suitable to be displayed to the user.
String filenameForDisplay(const String& string)
{
#if OS(WINDOWS)
    return string;
#else
    auto filename = fileSystemRepresentation(string);
    if (!validRepresentation(filename))
        return string;

    GUniquePtr<gchar> display(g_filename_display_name(filename.data()));
    if (!display)
        return string;
    return String::fromUTF8(display.get());
#endif
}

// Annoyingly, many important methods are not shared by the different GIO classes. Lacking native polymorphism, we have to dispatch the specific methods by hand:

static bool genericGIOFileClose(PlatformFileHandle handle)
{
    if (G_IS_INPUT_STREAM(handle))
        return g_input_stream_close(G_INPUT_STREAM(handle), nullptr, nullptr);
    if (G_IS_OUTPUT_STREAM(handle))
        return g_output_stream_close(G_OUTPUT_STREAM(handle), nullptr, nullptr);
    if (G_IS_IO_STREAM(handle))
        return g_io_stream_close(G_IO_STREAM(handle), nullptr, nullptr);
    RELEASE_ASSERT_NOT_REACHED();
}

static GRefPtr<GFileInfo> genericGIOFileQueryInfo(PlatformFileHandle handle, const char* attributes)
{
    if (G_IS_FILE_INPUT_STREAM(handle))
        return adoptGRef(g_file_input_stream_query_info(G_FILE_INPUT_STREAM(handle), attributes, nullptr, nullptr));
    if (G_IS_FILE_OUTPUT_STREAM(handle))
        return adoptGRef(g_file_output_stream_query_info(G_FILE_OUTPUT_STREAM(handle), attributes, nullptr, nullptr));
    if (G_IS_FILE_IO_STREAM(handle))
        return adoptGRef(g_file_io_stream_query_info(G_FILE_IO_STREAM(handle), attributes, nullptr, nullptr));
    RELEASE_ASSERT_NOT_REACHED();
}

static GInputStream* genericGIOGetInputStream(PlatformFileHandle handle)
{
    if (G_IS_IO_STREAM(handle))
        return g_io_stream_get_input_stream(G_IO_STREAM(handle));
    if (G_IS_INPUT_STREAM(handle))
        return G_INPUT_STREAM(handle);
    return nullptr; // handle is an incompatible type, i.e. GFileOutputStream
}

static GOutputStream* genericGIOGetOutputStream(PlatformFileHandle handle)
{
    if (G_IS_IO_STREAM(handle))
        return g_io_stream_get_output_stream(G_IO_STREAM(handle));
    if (G_IS_OUTPUT_STREAM(handle))
        return G_OUTPUT_STREAM(handle);
    return nullptr; // handle is an incompatible type, i.e. GFileInputStream
}

static GFileDescriptorBased* genericGIOGetFileDescriptorBased(PlatformFileHandle handle)
{
    // GFileInputStream and GFileOutputStream implement the GFileDescriptorBased interface.
    // GFileIOStream does not, but its inner streams do.
    if (G_IS_FILE_IO_STREAM(handle))
        return G_FILE_DESCRIPTOR_BASED(g_io_stream_get_input_stream(G_IO_STREAM(handle)));
    return G_FILE_DESCRIPTOR_BASED(handle);
}

int posixFileDescriptor(PlatformFileHandle handle)
{
    if (!isHandleValid(handle))
        return -1;

    GFileDescriptorBased* descriptorBased = genericGIOGetFileDescriptorBased(handle);
    return g_file_descriptor_based_get_fd(descriptorBased);
}

std::optional<uint64_t> fileSize(PlatformFileHandle handle)
{

    GRefPtr<GFileInfo> info = genericGIOFileQueryInfo(handle, G_FILE_ATTRIBUTE_STANDARD_SIZE);
    if (!info)
        return std::nullopt;

    return g_file_info_get_size(info.get());
}

std::optional<PlatformFileID> fileID(PlatformFileHandle handle)
{
    if (isHandleValid(handle)) {
        auto info = genericGIOFileQueryInfo(handle, G_FILE_ATTRIBUTE_ID_FILE);
        if (info && g_file_info_has_attribute(info.get(), G_FILE_ATTRIBUTE_ID_FILE))
            return { g_file_info_get_attribute_string(info.get(), G_FILE_ATTRIBUTE_ID_FILE) };
    }
    return std::nullopt;
}

bool fileIDsAreEqual(std::optional<PlatformFileID> a, std::optional<PlatformFileID> b)
{
    return a == b;
}

std::optional<WallTime> fileCreationTime(const String& path)
{
    const auto filename = fileSystemRepresentation(path);
    if (!validRepresentation(filename))
        return std::nullopt;

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(filename.data()));
    GRefPtr<GFileInfo> info = adoptGRef(g_file_query_info(file.get(), G_FILE_ATTRIBUTE_TIME_CREATED, G_FILE_QUERY_INFO_NONE, nullptr, nullptr));
    if (info && g_file_info_has_attribute(info.get(), G_FILE_ATTRIBUTE_TIME_CREATED))
        return WallTime::fromRawSeconds(g_file_info_get_attribute_uint64(info.get(), G_FILE_ATTRIBUTE_TIME_CREATED));

    return std::nullopt;
}

String openTemporaryFile(StringView prefix, PlatformFileHandle& handle, StringView suffix)
{
    // FIXME: Suffix is not supported, but OK for now since the code using it is macOS-port-only.
    ASSERT_UNUSED(suffix, suffix.isEmpty());

    GUniquePtr<gchar> filename(g_strdup_printf("%s%s", prefix.utf8().data(), createVersion4UUIDString().utf8().data()));
    GUniquePtr<gchar> tempPath(g_build_filename(g_get_tmp_dir(), filename.get(), nullptr));
    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(tempPath.get()));

    handle = G_SEEKABLE(g_file_create_readwrite(file.get(), G_FILE_CREATE_NONE, nullptr, nullptr));
    if (!isHandleValid(handle))
        return String();
    return String::fromUTF8(tempPath.get());
}

PlatformFileHandle openFile(const String& path, FileOpenMode mode, FileAccessPermission permission, bool failIfFileExists)
{
    auto filename = fileSystemRepresentation(path);
    if (!validRepresentation(filename))
        return invalidPlatformFileHandle;

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(filename.data()));
    GFileCreateFlags creationFlags = (permission == FileAccessPermission::All) ? G_FILE_CREATE_NONE : G_FILE_CREATE_PRIVATE;

    if (mode == FileOpenMode::Read)
        return G_SEEKABLE(g_file_read(file.get(), nullptr, nullptr));

    // Embarrasingly, GIO doesn't have an atomic create-or-open function.
    //
    // The closest thing to create-or-open in GIO is g_file_replace() and g_file_replace_readwrite(), but they have some problems:
    // (a) In the case of modifying an existing file, they return a new file in /tmp, doing a move swap when closing.
    //     Even calling g_output_stream_flush() won't update the original file.
    // (b) g_file_replace_readwrite() followed by a g_io_stream_close() will truncate the file, but we don't want ReadWrite to truncate.
    //
    // There is no pretty way out of this. Two naive solutions are:
    // (a) We try to create, if it fails we try to open -> If the file is deleted between the two operations we fail.
    // (b) We try to open, if it fails we try to create -> If the file is created between the two operations we fail.
    //
    // The most reliable thing we can do is spinlock retrying the other operation when we get a G_IO_ERROR_NOT_FOUND or G_IO_ERROR_EXISTS.
    while (true) {
        GRefPtr<GSeekable> seekable;
        GError* error = nullptr;

        // Try to create the file
        if (mode == FileOpenMode::Truncate)
            seekable = adoptGRef(G_SEEKABLE(g_file_create(file.get(), creationFlags, nullptr, &error)));
        else
            seekable = adoptGRef(G_SEEKABLE(g_file_create_readwrite(file.get(), creationFlags, nullptr, &error)));

        if (seekable || failIfFileExists || (error && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_EXISTS)))
            return seekable.leakRef();

        // Try to open the file instead.
        // We need to open the file as readwrite because there is no write-only open in GIO.
        // We could use g_io_stream_get_output_stream() to get a GFileOutputStream, but that doesn't seem to be well supported:
        // even if we ref the output stream before closing the IOStream, the latter will still cause the file to be closed and
        // therefore we would be returning a stream with an invalid file handle.
        seekable = adoptGRef(G_SEEKABLE(g_file_open_readwrite(file.get(), nullptr, nullptr)));
        if (seekable && mode == FileOpenMode::Truncate)
            g_seekable_truncate(seekable.get(), 0, nullptr, nullptr);

        if (seekable || (error && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)))
            return seekable.leakRef();
    }
}

void closeFile(PlatformFileHandle& handle)
{
    if (!isHandleValid(handle))
        return;

    genericGIOFileClose(handle);
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

    if (!g_seekable_seek(handle, offset, seekType, nullptr, nullptr))
        return -1;
    return g_seekable_tell(handle);
}

bool truncateFile(PlatformFileHandle handle, long long offset)
{
    return g_seekable_truncate(handle, offset, nullptr, nullptr);
}

bool flushFile(PlatformFileHandle handle)
{
    GOutputStream* outputStream = genericGIOGetOutputStream(handle);
    if (!outputStream)
        return true; // A read-only file needs no flushing.
    return g_output_stream_flush(outputStream, nullptr, nullptr);
}

int writeToFile(PlatformFileHandle handle, const void* data, int length)
{
    if (!length)
        return 0;

    GOutputStream* outputStream = genericGIOGetOutputStream(handle);
    if (!outputStream)
        return -1; // Attempted to write a read-only file.

    // We're not required by the parent class to enforce that the entire `length` gets written, but
    // we'll play safe and do it anyway rather than risking a function not handling partial writes.
    // All GIO backends handle EINTR internally, so we don't need to handle it here.
    gsize bytesWritten;
    if (!g_output_stream_write_all(outputStream, data, length, &bytesWritten, nullptr, nullptr))
        return -1; // Write failed.
    return bytesWritten;
}

int readFromFile(PlatformFileHandle handle, void* data, int length)
{
    GInputStream* inputStream = genericGIOGetInputStream(handle);
    if (!inputStream)
        return -1; // Attempted to read a write-only file.

    // We're not required by the parent class to enforce that the entire `length` gets read, but
    // we'll play safe and do it anyway if possible, rather than risking a function not handling
    // partial reads.
    // All GIO backends handle EINTR internally, so we don't need to handle it here.
    gsize bytesRead;
    if (g_input_stream_read_all(inputStream, data, length, &bytesRead, nullptr, nullptr))
        return bytesRead; // Complete read succeeded.
    if (bytesRead > 0)
        return bytesRead; // Read succeeded partially, then failed: return the bytes successfully read.
    return -1; // Read failed: nothing was read.
}

std::optional<int32_t> getFileDeviceId(const String& path)
{
    auto fsFile = fileSystemRepresentation(path);
    if (fsFile.isNull())
        return std::nullopt;

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(fsFile.data()));
    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_filesystem_info(file.get(), G_FILE_ATTRIBUTE_UNIX_DEVICE, nullptr, nullptr));
    if (!fileInfo)
        return std::nullopt;

    return g_file_info_get_attribute_uint32(fileInfo.get(), G_FILE_ATTRIBUTE_UNIX_DEVICE);
}

std::optional<uint32_t> volumeFileBlockSize(const String&)
{
    return std::nullopt;
}

#if USE(FILE_LOCK)
bool lockFile(PlatformFileHandle handle, OptionSet<FileLockMode> lockMode)
{
    static_assert(LOCK_SH == WTF::enumToUnderlyingType(FileLockMode::Shared), "LockSharedEncoding is as expected");
    static_assert(LOCK_EX == WTF::enumToUnderlyingType(FileLockMode::Exclusive), "LockExclusiveEncoding is as expected");
    static_assert(LOCK_NB == WTF::enumToUnderlyingType(FileLockMode::Nonblocking), "LockNonblockingEncoding is as expected");
    int fd = posixFileDescriptor(handle);
    int result = flock(fd, lockMode.toRaw());
    return result != -1;
}

bool unlockFile(PlatformFileHandle handle)
{
    int fd = posixFileDescriptor(handle);
    int result = flock(fd, LOCK_UN);
    return result != -1;
}
#endif // USE(FILE_LOCK)

#if OS(LINUX)
CString currentExecutablePath()
{
    static char readLinkBuffer[PATH_MAX];
    ssize_t result = readlink("/proc/self/exe", readLinkBuffer, PATH_MAX);
    if (result == -1)
        return { };
    return CString(readLinkBuffer, result);
}
#elif OS(HURD)
CString currentExecutablePath()
{
    return { };
}
#elif OS(UNIX)
CString currentExecutablePath()
{
    static char readLinkBuffer[PATH_MAX];
    ssize_t result = readlink("/proc/curproc/file", readLinkBuffer, PATH_MAX);
    if (result == -1)
        return { };
    return CString(readLinkBuffer, result);
}
#elif OS(WINDOWS)
CString currentExecutablePath()
{
    static WCHAR buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(0, buffer, MAX_PATH);
    if (!length || (length == MAX_PATH && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        return { };

    String path(buffer, length);
    return path.utf8();
}
#endif

CString currentExecutableName()
{
    auto executablePath = currentExecutablePath();
    if (!executablePath.isNull()) {
        GUniquePtr<char> basename(g_path_get_basename(executablePath.data()));
        return basename.get();
    }

    return g_get_prgname();
}

String userCacheDirectory()
{
    return stringFromFileSystemRepresentation(g_get_user_cache_dir());
}

String userDataDirectory()
{
    return stringFromFileSystemRepresentation(g_get_user_data_dir());
}

} // namespace FileSystemImpl
} // namespace WTF
