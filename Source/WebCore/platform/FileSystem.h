/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#ifndef FileSystem_h
#define FileSystem_h

#include <time.h>
#include <utility>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

#if USE(CF)
typedef struct __CFBundle* CFBundleRef;
typedef const struct __CFData* CFDataRef;
#endif

#if OS(WINDOWS)
// These are to avoid including <winbase.h> in a header for Chromium
typedef void *HANDLE;
// Assuming STRICT
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;
#endif

#if USE(GLIB)
typedef struct _GFileIOStream GFileIOStream;
typedef struct _GModule GModule;
#endif

namespace WebCore {

// PlatformModule
#if OS(WINDOWS)
typedef HMODULE PlatformModule;
#elif PLATFORM(EFL)
typedef Eina_Module* PlatformModule;
#elif USE(GLIB)
typedef GModule* PlatformModule;
#elif USE(CF)
typedef CFBundleRef PlatformModule;
#else
typedef void* PlatformModule;
#endif

// PlatformModuleVersion
#if OS(WINDOWS)
struct PlatformModuleVersion {
    unsigned leastSig;
    unsigned mostSig;

    PlatformModuleVersion(unsigned)
        : leastSig(0)
        , mostSig(0)
    {
    }

    PlatformModuleVersion(unsigned lsb, unsigned msb)
        : leastSig(lsb)
        , mostSig(msb)
    {
    }

};
#else
typedef unsigned PlatformModuleVersion;
#endif

// PlatformFileHandle
#if USE(GLIB) && !PLATFORM(EFL) && !PLATFORM(WIN)
typedef GFileIOStream* PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = 0;
#elif OS(WINDOWS)
typedef HANDLE PlatformFileHandle;
// FIXME: -1 is INVALID_HANDLE_VALUE, defined in <winbase.h>. Chromium tries to
// avoid using Windows headers in headers.  We'd rather move this into the .cpp.
const PlatformFileHandle invalidPlatformFileHandle = reinterpret_cast<HANDLE>(-1);
#else
typedef int PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = -1;
#endif

enum FileOpenMode {
    OpenForRead = 0,
    OpenForWrite
};

enum FileSeekOrigin {
    SeekFromBeginning = 0,
    SeekFromCurrent,
    SeekFromEnd
};

enum FileLockMode {
    LockShared = 1,
    LockExclusive = 2,
    LockNonBlocking = 4
};

#if OS(WINDOWS)
static const char PlatformFilePathSeparator = '\\';
#else
static const char PlatformFilePathSeparator = '/';
#endif

struct FileMetadata;

WEBCORE_EXPORT bool fileExists(const String&);
WEBCORE_EXPORT bool deleteFile(const String&);
WEBCORE_EXPORT bool deleteEmptyDirectory(const String&);
WEBCORE_EXPORT bool moveFile(const String& oldPath, const String& newPath);
WEBCORE_EXPORT bool getFileSize(const String&, long long& result);
WEBCORE_EXPORT bool getFileSize(PlatformFileHandle, long long& result);
WEBCORE_EXPORT bool getFileModificationTime(const String&, time_t& result);
WEBCORE_EXPORT bool getFileCreationTime(const String&, time_t& result); // Not all platforms store file creation time.
bool getFileMetadata(const String&, FileMetadata&);
WEBCORE_EXPORT String pathByAppendingComponent(const String& path, const String& component);
WEBCORE_EXPORT bool makeAllDirectories(const String& path);
String homeDirectoryPath();
WEBCORE_EXPORT String pathGetFileName(const String&);
WEBCORE_EXPORT String directoryName(const String&);

WEBCORE_EXPORT void setMetadataURL(String& URLString, const String& referrer, const String& path);

bool canExcludeFromBackup(); // Returns true if any file can ever be excluded from backup.
bool excludeFromBackup(const String&); // Returns true if successful.

WEBCORE_EXPORT Vector<String> listDirectory(const String& path, const String& filter = String());

WEBCORE_EXPORT CString fileSystemRepresentation(const String&);

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
int readFromFile(PlatformFileHandle, char* data, int length);

// Appends the contents of the file found at 'path' to the open PlatformFileHandle.
// Returns true if the write was successful, false if it was not.
bool appendFileContentsToFileHandle(const String& path, PlatformFileHandle&);

// Hard links a file if possible, copies it if not.
bool hardLinkOrCopyFile(const String& source, const String& destination);

#if USE(FILE_LOCK)
bool lockFile(PlatformFileHandle, FileLockMode);
bool unlockFile(PlatformFileHandle);
#endif

// Functions for working with loadable modules.
bool unloadModule(PlatformModule);

// Encode a string for use within a file name.
WEBCORE_EXPORT String encodeForFileName(const String&);

#if USE(CF)
RetainPtr<CFURLRef> pathAsURL(const String&);
#endif

#if PLATFORM(GTK)
String filenameToString(const char*);
String filenameForDisplay(const String&);
CString applicationDirectoryPath();
CString sharedResourcesPath();
#endif
#if USE(SOUP)
uint64_t getVolumeFreeSizeForPath(const char*);
#endif

#if PLATFORM(WIN)
WEBCORE_EXPORT String localUserSpecificStorageDirectory();
String roamingUserSpecificStorageDirectory();
#endif

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

} // namespace WebCore

#endif // FileSystem_h
