/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
 * Copyright (C) 2015 Canon Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <time.h>
#include <utility>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/Span.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

#if USE(CF)
typedef const struct __CFData* CFDataRef;
#endif

OBJC_CLASS NSString;

#if OS(WINDOWS)
typedef void *HANDLE;
#endif

#if USE(GLIB)
typedef struct _GFileIOStream GFileIOStream;
#endif

namespace WTF {

namespace FileSystemImpl {

// PlatformFileHandle
#if USE(GLIB) && !OS(WINDOWS)
typedef GFileIOStream* PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = 0;
#elif OS(WINDOWS)
typedef HANDLE PlatformFileHandle;
// FIXME: -1 is INVALID_HANDLE_VALUE, defined in <winbase.h>. Chromium tries to
// avoid using Windows headers in headers. We'd rather move this into the .cpp.
const PlatformFileHandle invalidPlatformFileHandle = reinterpret_cast<HANDLE>(-1);
#else
typedef int PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = -1;
#endif

enum class FileOpenMode {
    Read,
    Write,
    ReadWrite,
#if OS(DARWIN)
    EventsOnly,
#endif
};

enum class FileAccessPermission : bool {
    User,
    All
};

enum class FileSeekOrigin {
    Beginning,
    Current,
    End,
};

enum class FileLockMode {
    Shared = 1 << 0,
    Exclusive = 1 << 1,
    Nonblocking = 1 << 2,
};

enum class MappedFileMode {
    Shared,
    Private,
};

WTF_EXPORT_PRIVATE bool fileExists(const String&);
WTF_EXPORT_PRIVATE bool deleteFile(const String&);
WTF_EXPORT_PRIVATE void deleteAllFilesModifiedSince(const String&, WallTime);
WTF_EXPORT_PRIVATE bool deleteEmptyDirectory(const String&);
WTF_EXPORT_PRIVATE bool moveFile(const String& oldPath, const String& newPath);
WTF_EXPORT_PRIVATE std::optional<uint64_t> fileSize(const String&); // Follows symlinks.
WTF_EXPORT_PRIVATE std::optional<uint64_t> fileSize(PlatformFileHandle);
WTF_EXPORT_PRIVATE std::optional<WallTime> fileModificationTime(const String&);
WTF_EXPORT_PRIVATE bool updateFileModificationTime(const String& path); // Sets modification time to now.
WTF_EXPORT_PRIVATE std::optional<WallTime> fileCreationTime(const String&); // Not all platforms store file creation time.
WTF_EXPORT_PRIVATE bool isHiddenFile(const String&);
WTF_EXPORT_PRIVATE String pathByAppendingComponent(StringView path, StringView component);
WTF_EXPORT_PRIVATE String pathByAppendingComponents(StringView path, const Vector<StringView>& components);
WTF_EXPORT_PRIVATE String lastComponentOfPathIgnoringTrailingSlash(const String& path);
WTF_EXPORT_PRIVATE bool makeAllDirectories(const String& path);
WTF_EXPORT_PRIVATE String pathFileName(const String&);
WTF_EXPORT_PRIVATE String parentPath(const String&);
WTF_EXPORT_PRIVATE std::optional<uint64_t> volumeFreeSpace(const String&);
WTF_EXPORT_PRIVATE std::optional<uint32_t> volumeFileBlockSize(const String&);
WTF_EXPORT_PRIVATE std::optional<int32_t> getFileDeviceId(const String&);
WTF_EXPORT_PRIVATE bool createSymbolicLink(const String& targetPath, const String& symbolicLinkPath);
WTF_EXPORT_PRIVATE String createTemporaryZipArchive(const String& directory);

enum class FileType { Regular, Directory, SymbolicLink };
WTF_EXPORT_PRIVATE std::optional<FileType> fileType(const String&);
WTF_EXPORT_PRIVATE std::optional<FileType> fileTypeFollowingSymlinks(const String&);

WTF_EXPORT_PRIVATE void setMetadataURL(const String& path, const String& urlString, const String& referrer = { });
WTF_EXPORT_PRIVATE bool excludeFromBackup(const String&); // Returns true if successful.

WTF_EXPORT_PRIVATE Vector<String> listDirectory(const String& path); // Returns file names, not full paths.

WTF_EXPORT_PRIVATE CString fileSystemRepresentation(const String&);
WTF_EXPORT_PRIVATE String stringFromFileSystemRepresentation(const char*);

inline bool isHandleValid(const PlatformFileHandle& handle) { return handle != invalidPlatformFileHandle; }

using Salt = std::array<uint8_t, 8>;
WTF_EXPORT_PRIVATE std::optional<Salt> readOrMakeSalt(const String& path);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> readEntireFile(PlatformFileHandle);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> readEntireFile(const String& path);

// Prefix is what the filename should be prefixed with, not the full path.
WTF_EXPORT_PRIVATE String openTemporaryFile(StringView prefix, PlatformFileHandle&, StringView suffix = { });
WTF_EXPORT_PRIVATE PlatformFileHandle openFile(const String& path, FileOpenMode, FileAccessPermission = FileAccessPermission::All, bool failIfFileExists = false);
WTF_EXPORT_PRIVATE void closeFile(PlatformFileHandle&);
// Returns the resulting offset from the beginning of the file if successful, -1 otherwise.
WTF_EXPORT_PRIVATE long long seekFile(PlatformFileHandle, long long offset, FileSeekOrigin);
WTF_EXPORT_PRIVATE bool truncateFile(PlatformFileHandle, long long offset);
WTF_EXPORT_PRIVATE bool flushFile(PlatformFileHandle);
// Returns number of bytes actually read if successful, -1 otherwise.
WTF_EXPORT_PRIVATE int writeToFile(PlatformFileHandle, const void* data, int length);
// Returns number of bytes actually written if successful, -1 otherwise.
WTF_EXPORT_PRIVATE int readFromFile(PlatformFileHandle, void* data, int length);

WTF_EXPORT_PRIVATE PlatformFileHandle openAndLockFile(const String&, FileOpenMode, OptionSet<FileLockMode> = FileLockMode::Exclusive);
WTF_EXPORT_PRIVATE void unlockAndCloseFile(PlatformFileHandle);

// Appends the contents of the file found at 'path' to the open PlatformFileHandle.
// Returns true if the write was successful, false if it was not.
WTF_EXPORT_PRIVATE bool appendFileContentsToFileHandle(const String& path, PlatformFileHandle&);

WTF_EXPORT_PRIVATE bool hardLink(const String& targetPath, const String& linkPath);
// Hard links a file if possible, copies it if not.
WTF_EXPORT_PRIVATE bool hardLinkOrCopyFile(const String& targetPath, const String& linkPath);
WTF_EXPORT_PRIVATE std::optional<uint64_t> hardLinkCount(const String& path);

#if USE(FILE_LOCK)
WTF_EXPORT_PRIVATE bool lockFile(PlatformFileHandle, OptionSet<FileLockMode>);
WTF_EXPORT_PRIVATE bool unlockFile(PlatformFileHandle);
#endif

// Encode a string for use within a file name.
WTF_EXPORT_PRIVATE String encodeForFileName(const String&);
WTF_EXPORT_PRIVATE String decodeFromFilename(const String&);

WTF_EXPORT_PRIVATE bool filesHaveSameVolume(const String&, const String&);

#if USE(CF)
WTF_EXPORT_PRIVATE RetainPtr<CFURLRef> pathAsURL(const String&);
#endif

#if USE(GLIB)
WTF_EXPORT_PRIVATE String filenameForDisplay(const String&);
WTF_EXPORT_PRIVATE CString currentExecutablePath();
WTF_EXPORT_PRIVATE CString currentExecutableName();
WTF_EXPORT_PRIVATE String userCacheDirectory();
WTF_EXPORT_PRIVATE String userDataDirectory();
#endif

#if OS(WINDOWS)
WTF_EXPORT_PRIVATE String localUserSpecificStorageDirectory();
WTF_EXPORT_PRIVATE String roamingUserSpecificStorageDirectory();
WTF_EXPORT_PRIVATE String createTemporaryDirectory();
#endif

#if PLATFORM(COCOA)
WTF_EXPORT_PRIVATE NSString *createTemporaryDirectory(NSString *directoryPrefix);
WTF_EXPORT_PRIVATE NSString *systemDirectoryPath();

// Allow reading cloud files with no local copy.
enum class PolicyScope : uint8_t { Process, Thread };
WTF_EXPORT_PRIVATE bool setAllowsMaterializingDatalessFiles(bool, PolicyScope);
WTF_EXPORT_PRIVATE std::optional<bool> allowsMaterializingDatalessFiles(PolicyScope);
#endif

WTF_EXPORT_PRIVATE bool deleteNonEmptyDirectory(const String&);

WTF_EXPORT_PRIVATE String realPath(const String&);

WTF_EXPORT_PRIVATE bool isSafeToUseMemoryMapForPath(const String&);
WTF_EXPORT_PRIVATE WARN_UNUSED_RETURN bool makeSafeToUseMemoryMapForPath(const String&);

class MappedFileData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MappedFileData() { }
    MappedFileData(MappedFileData&&);
    WTF_EXPORT_PRIVATE MappedFileData(const String& filePath, MappedFileMode, bool& success);
    WTF_EXPORT_PRIVATE MappedFileData(PlatformFileHandle, MappedFileMode, bool& success);
    WTF_EXPORT_PRIVATE MappedFileData(PlatformFileHandle, FileOpenMode, MappedFileMode, bool& success);
    WTF_EXPORT_PRIVATE ~MappedFileData();
    MappedFileData& operator=(MappedFileData&&);

    explicit operator bool() const { return !!m_fileData; }
    const void* data() const { return m_fileData; }
    unsigned size() const { return m_fileSize; }

#if PLATFORM(COCOA)
    void* leakHandle() { return std::exchange(m_fileData, nullptr); }
#endif
#if OS(WINDOWS)
    HANDLE fileMapping() const { return m_fileMapping; }
#endif

private:
    WTF_EXPORT_PRIVATE bool mapFileHandle(PlatformFileHandle, FileOpenMode, MappedFileMode);

    void* m_fileData { nullptr };
    unsigned m_fileSize { 0 };
#if OS(WINDOWS)
    HANDLE m_fileMapping { nullptr };
#endif
};

inline MappedFileData::MappedFileData(PlatformFileHandle handle, MappedFileMode mapMode, bool& success)
{
    success = mapFileHandle(handle, FileOpenMode::Read, mapMode);
}

inline MappedFileData::MappedFileData(PlatformFileHandle handle, FileOpenMode openMode, MappedFileMode mapMode, bool& success)
{
    success = mapFileHandle(handle, openMode, mapMode);
}

inline MappedFileData::MappedFileData(MappedFileData&& other)
    : m_fileData(std::exchange(other.m_fileData, nullptr))
    , m_fileSize(std::exchange(other.m_fileSize, 0))
#if OS(WINDOWS)
    , m_fileMapping(std::exchange(other.m_fileMapping, nullptr))
#endif
{
}

inline MappedFileData& MappedFileData::operator=(MappedFileData&& other)
{
    m_fileData = std::exchange(other.m_fileData, nullptr);
    m_fileSize = std::exchange(other.m_fileSize, 0);
#if OS(WINDOWS)
    m_fileMapping = std::exchange(other.m_fileMapping, nullptr);
#endif
    return *this;
}

// This creates the destination file, maps it, write the provided data to it and returns the mapped file.
// This function fails if there is already a file at the destination path.
WTF_EXPORT_PRIVATE MappedFileData mapToFile(const String& path, size_t bytesSize, Function<void(const Function<bool(Span<const uint8_t>)>&)>&& apply, PlatformFileHandle* = nullptr);

} // namespace FileSystemImpl
} // namespace WTF

namespace FileSystem = WTF::FileSystemImpl;
