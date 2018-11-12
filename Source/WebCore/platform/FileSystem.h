/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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

#if PLATFORM(WIN)
typedef void *HANDLE;
#endif

#if USE(GLIB)
typedef struct _GFileIOStream GFileIOStream;
#endif

namespace WebCore {

struct FileMetadata;

namespace FileSystem {

// PlatformFileHandle
#if USE(GLIB) && !PLATFORM(WIN)
typedef GFileIOStream* PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = 0;
#elif PLATFORM(WIN)
typedef HANDLE PlatformFileHandle;
// FIXME: -1 is INVALID_HANDLE_VALUE, defined in <winbase.h>. Chromium tries to
// avoid using Windows headers in headers.  We'd rather move this into the .cpp.
const PlatformFileHandle invalidPlatformFileHandle = reinterpret_cast<HANDLE>(-1);
#else
typedef int PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = -1;
#endif

enum class FileOpenMode {
    Read,
    Write,
#if OS(DARWIN)
    EventsOnly,
#endif
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

enum class ShouldFollowSymbolicLinks { No, Yes };

WEBCORE_EXPORT bool fileExists(const String&);
WEBCORE_EXPORT bool deleteFile(const String&);
WEBCORE_EXPORT bool deleteEmptyDirectory(const String&);
WEBCORE_EXPORT bool moveFile(const String& oldPath, const String& newPath);
WEBCORE_EXPORT bool getFileSize(const String&, long long& result);
WEBCORE_EXPORT bool getFileSize(PlatformFileHandle, long long& result);
WEBCORE_EXPORT bool getFileModificationTime(const String&, time_t& result);
WEBCORE_EXPORT std::optional<WallTime> getFileModificationTime(const String&);
WEBCORE_EXPORT bool getFileCreationTime(const String&, time_t& result); // Not all platforms store file creation time.
WEBCORE_EXPORT std::optional<FileMetadata> fileMetadata(const String& path);
WEBCORE_EXPORT std::optional<FileMetadata> fileMetadataFollowingSymlinks(const String& path);
WEBCORE_EXPORT bool fileIsDirectory(const String&, ShouldFollowSymbolicLinks);
WEBCORE_EXPORT String pathByAppendingComponent(const String& path, const String& component);
String pathByAppendingComponents(StringView path, const Vector<StringView>& components);
String lastComponentOfPathIgnoringTrailingSlash(const String& path);
WEBCORE_EXPORT bool makeAllDirectories(const String& path);
String homeDirectoryPath();
WEBCORE_EXPORT String pathGetFileName(const String&);
WEBCORE_EXPORT String directoryName(const String&);
WEBCORE_EXPORT bool getVolumeFreeSpace(const String&, uint64_t&);
WEBCORE_EXPORT std::optional<int32_t> getFileDeviceId(const CString&);
WEBCORE_EXPORT bool createSymbolicLink(const String& targetPath, const String& symbolicLinkPath);

WEBCORE_EXPORT void setMetadataURL(const String& path, const String& urlString, const String& referrer = { });

bool canExcludeFromBackup(); // Returns true if any file can ever be excluded from backup.
bool excludeFromBackup(const String&); // Returns true if successful.

WEBCORE_EXPORT Vector<String> listDirectory(const String& path, const String& filter = String());

WEBCORE_EXPORT CString fileSystemRepresentation(const String&);
String stringFromFileSystemRepresentation(const char*);

inline bool isHandleValid(const PlatformFileHandle& handle) { return handle != invalidPlatformFileHandle; }

inline double invalidFileTime() { return std::numeric_limits<double>::quiet_NaN(); }
inline bool isValidFileTime(double time) { return std::isfinite(time); }

// Prefix is what the filename should be prefixed with, not the full path.
WEBCORE_EXPORT String openTemporaryFile(const String& prefix, PlatformFileHandle&);
WEBCORE_EXPORT PlatformFileHandle openFile(const String& path, FileOpenMode);
WEBCORE_EXPORT void closeFile(PlatformFileHandle&);
// Returns the resulting offset from the beginning of the file if successful, -1 otherwise.
WEBCORE_EXPORT long long seekFile(PlatformFileHandle, long long offset, FileSeekOrigin);
bool truncateFile(PlatformFileHandle, long long offset);
// Returns number of bytes actually read if successful, -1 otherwise.
WEBCORE_EXPORT int writeToFile(PlatformFileHandle, const char* data, int length);
// Returns number of bytes actually written if successful, -1 otherwise.
WEBCORE_EXPORT int readFromFile(PlatformFileHandle, char* data, int length);

WEBCORE_EXPORT PlatformFileHandle openAndLockFile(const String&, FileOpenMode, OptionSet<FileLockMode> = FileLockMode::Exclusive);
WEBCORE_EXPORT void unlockAndCloseFile(PlatformFileHandle);

// Appends the contents of the file found at 'path' to the open PlatformFileHandle.
// Returns true if the write was successful, false if it was not.
bool appendFileContentsToFileHandle(const String& path, PlatformFileHandle&);

// Hard links a file if possible, copies it if not.
bool hardLinkOrCopyFile(const String& source, const String& destination);

#if USE(FILE_LOCK)
WEBCORE_EXPORT bool lockFile(PlatformFileHandle, OptionSet<FileLockMode>);
WEBCORE_EXPORT bool unlockFile(PlatformFileHandle);
#endif

// Encode a string for use within a file name.
WEBCORE_EXPORT String encodeForFileName(const String&);
WEBCORE_EXPORT String decodeFromFilename(const String&);

WEBCORE_EXPORT bool filesHaveSameVolume(const String&, const String&);

#if USE(CF)
RetainPtr<CFURLRef> pathAsURL(const String&);
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
String filenameForDisplay(const String&);
#endif

#if PLATFORM(WIN)
WEBCORE_EXPORT String localUserSpecificStorageDirectory();
String roamingUserSpecificStorageDirectory();
#endif

#if PLATFORM(COCOA)
WEBCORE_EXPORT NSString *createTemporaryDirectory(NSString *directoryPrefix);
WEBCORE_EXPORT bool deleteNonEmptyDirectory(const String&);
#endif

#if PLATFORM(WIN_CAIRO)
WEBCORE_EXPORT String createTemporaryDirectory();
WEBCORE_EXPORT bool deleteNonEmptyDirectory(const String&);
#endif

WEBCORE_EXPORT String realPath(const String&);

class MappedFileData {
public:
    MappedFileData() { }
    MappedFileData(MappedFileData&&);
    WEBCORE_EXPORT MappedFileData(const String& filePath, bool& success);
    WEBCORE_EXPORT ~MappedFileData();
    MappedFileData& operator=(MappedFileData&&);

    explicit operator bool() const { return !!m_fileData; }
    const void* data() const { return m_fileData; }
    unsigned size() const { return m_fileSize; }

private:
    void* m_fileData { nullptr };
    unsigned m_fileSize { 0 };
};

inline MappedFileData::MappedFileData(MappedFileData&& other)
    : m_fileData(std::exchange(other.m_fileData, nullptr))
    , m_fileSize(std::exchange(other.m_fileSize, 0))
{
}

inline MappedFileData& MappedFileData::operator=(MappedFileData&& other)
{
    m_fileData = std::exchange(other.m_fileData, nullptr);
    m_fileSize = std::exchange(other.m_fileSize, 0);
    return *this;
}

} // namespace FileSystem
} // namespace WebCore

