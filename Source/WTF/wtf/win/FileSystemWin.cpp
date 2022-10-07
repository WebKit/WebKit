/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
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

#include "config.h"
#include <wtf/FileSystem.h>

#include <io.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sys/stat.h>
#include <windows.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/win/WCharStringExtras.h>
#include <wtf/win/PathWalker.h>

namespace WTF {

namespace FileSystemImpl {

static const ULONGLONG kSecondsFromFileTimeToTimet = 11644473600;

static bool getFindData(String path, WIN32_FIND_DATAW& findData)
{
    HANDLE handle = FindFirstFileW(path.wideCharacters().data(), &findData);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    FindClose(handle);
    return true;
}

static bool getFileSizeFromFindData(const WIN32_FIND_DATAW& findData, long long& size)
{
    ULARGE_INTEGER fileSize;
    fileSize.HighPart = findData.nFileSizeHigh;
    fileSize.LowPart = findData.nFileSizeLow;

    if (fileSize.QuadPart > static_cast<ULONGLONG>(std::numeric_limits<long long>::max()))
        return false;

    size = fileSize.QuadPart;
    return true;
}

static std::optional<uint64_t> getFileSizeFromByHandleFileInformationStructure(const BY_HANDLE_FILE_INFORMATION& fileInformation)
{
    ULARGE_INTEGER fileSize;
    fileSize.HighPart = fileInformation.nFileSizeHigh;
    fileSize.LowPart = fileInformation.nFileSizeLow;

    if (fileSize.QuadPart > static_cast<ULONGLONG>(std::numeric_limits<long long>::max()))
        return std::nullopt;

    return fileSize.QuadPart;
}

static void fileCreationTimeFromFindData(const WIN32_FIND_DATAW& findData, time_t& time)
{
    ULARGE_INTEGER fileTime;
    fileTime.HighPart = findData.ftCreationTime.dwHighDateTime;
    fileTime.LowPart = findData.ftCreationTime.dwLowDateTime;

    // Information about converting time_t to FileTime is available at http://msdn.microsoft.com/en-us/library/ms724228%28v=vs.85%29.aspx
    time = fileTime.QuadPart / 10000000 - kSecondsFromFileTimeToTimet;
}


static void fileModificationTimeFromFindData(const WIN32_FIND_DATAW& findData, time_t& time)
{
    ULARGE_INTEGER fileTime;
    fileTime.HighPart = findData.ftLastWriteTime.dwHighDateTime;
    fileTime.LowPart = findData.ftLastWriteTime.dwLowDateTime;

    // Information about converting time_t to FileTime is available at http://msdn.microsoft.com/en-us/library/ms724228%28v=vs.85%29.aspx
    time = fileTime.QuadPart / 10000000 - kSecondsFromFileTimeToTimet;
}

std::optional<uint64_t> fileSize(PlatformFileHandle fileHandle)
{
    BY_HANDLE_FILE_INFORMATION fileInformation;
    if (!::GetFileInformationByHandle(fileHandle, &fileInformation))
        return std::nullopt;

    return getFileSizeFromByHandleFileInformationStructure(fileInformation);
}

std::optional<PlatformFileID> fileID(PlatformFileHandle fileHandle)
{
    // FIXME (246118): Implement this function properly.
    return std::nullopt;
}

bool fileIDsAreEqual(std::optional<PlatformFileID> a, std::optional<PlatformFileID> b)
{
    // FIXME (246118): Implement this function properly.
    return true;
}

std::optional<WallTime> fileCreationTime(const String& path)
{
    WIN32_FIND_DATAW findData;
    if (!getFindData(path, findData))
        return std::nullopt;

    time_t time = 0;
    fileCreationTimeFromFindData(findData, time);
    return WallTime::fromRawSeconds(time);
}

#if !USE(CF)

CString fileSystemRepresentation(const String& path)
{
    auto characters = wcharFrom(StringView(path).upconvertedCharacters());
    int size = WideCharToMultiByte(CP_ACP, 0, characters, path.length(), 0, 0, 0, 0) - 1;

    char* buffer;
    CString string = CString::newUninitialized(size, buffer);

    WideCharToMultiByte(CP_ACP, 0, characters, path.length(), buffer, size, 0, 0);

    return string;
}

#endif // !USE(CF)

static String bundleName()
{
    static const NeverDestroyed<String> name = [] {
        String name { "WebKit"_s };

#if USE(CF)
        if (CFBundleRef bundle = CFBundleGetMainBundle()) {
            if (CFTypeRef bundleExecutable = CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleExecutableKey)) {
                if (CFGetTypeID(bundleExecutable) == CFStringGetTypeID())
                    name = reinterpret_cast<CFStringRef>(bundleExecutable);
            }
        }
#endif

        return name;
    }();

    return name;
}

static String storageDirectory(DWORD pathIdentifier)
{
    Vector<UChar> buffer(MAX_PATH);
    if (FAILED(SHGetFolderPathW(nullptr, pathIdentifier | CSIDL_FLAG_CREATE, nullptr, 0, wcharFrom(buffer.data()))))
        return String();

    buffer.shrink(wcslen(wcharFrom(buffer.data())));
    String directory = String::adopt(WTFMove(buffer));

    directory = pathByAppendingComponent(directory, makeString("Apple Computer\\", bundleName()));
    if (!makeAllDirectories(directory))
        return String();

    return directory;
}

static String cachedStorageDirectory(DWORD pathIdentifier)
{
    static HashMap<DWORD, String> directories;

    HashMap<DWORD, String>::iterator it = directories.find(pathIdentifier);
    if (it != directories.end())
        return it->value;

    String directory = storageDirectory(pathIdentifier);
    directories.add(pathIdentifier, directory);

    return directory;
}

static String generateTemporaryPath(const Function<bool(const String&)>& action)
{
    wchar_t tempPath[MAX_PATH];
    int tempPathLength = ::GetTempPathW(WTF_ARRAY_LENGTH(tempPath), tempPath);
    if (tempPathLength <= 0 || tempPathLength > WTF_ARRAY_LENGTH(tempPath))
        return String();

    String proposedPath;
    do {
        wchar_t tempFile[] = L"XXXXXXXX.tmp"; // Use 8.3 style name (more characters aren't helpful due to 8.3 short file names)
        const int randomPartLength = 8;
        cryptographicallyRandomValues(tempFile, randomPartLength * sizeof(wchar_t));

        // Limit to valid filesystem characters, also excluding others that could be problematic, like punctuation.
        // don't include both upper and lowercase since Windows file systems are typically not case sensitive.
        const char validChars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        for (int i = 0; i < randomPartLength; ++i)
            tempFile[i] = validChars[tempFile[i] % (sizeof(validChars) - 1)];

        ASSERT(wcslen(tempFile) == WTF_ARRAY_LENGTH(tempFile) - 1);

        proposedPath = pathByAppendingComponent(String(tempPath), String(tempFile));
        if (proposedPath.isEmpty())
            break;
    } while (!action(proposedPath));

    return proposedPath;
}

String openTemporaryFile(StringView, PlatformFileHandle& handle, StringView suffix)
{
    // FIXME: Suffix is not supported, but OK for now since the code using it is macOS-port-only.
    ASSERT_UNUSED(suffix, suffix.isEmpty());

    handle = INVALID_HANDLE_VALUE;

    String proposedPath = generateTemporaryPath([&handle](const String& proposedPath) {
        // use CREATE_NEW to avoid overwriting an existing file with the same name
        handle = ::CreateFileW(proposedPath.wideCharacters().data(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

        return isHandleValid(handle) || GetLastError() == ERROR_ALREADY_EXISTS;
    });

    if (!isHandleValid(handle))
        return String();

    return proposedPath;
}

PlatformFileHandle openFile(const String& path, FileOpenMode mode, FileAccessPermission, bool failIfFileExists)
{
    DWORD desiredAccess = 0;
    DWORD creationDisposition = 0;
    DWORD shareMode = 0;
    switch (mode) {
    case FileOpenMode::Read:
        desiredAccess = GENERIC_READ;
        creationDisposition = OPEN_EXISTING;
        shareMode = FILE_SHARE_READ;
        break;
    case FileOpenMode::Write:
        desiredAccess = GENERIC_WRITE;
        creationDisposition = CREATE_ALWAYS;
        break;
    case FileOpenMode::ReadWrite:
        desiredAccess = GENERIC_READ | GENERIC_WRITE;
        creationDisposition = OPEN_ALWAYS;
        break;
    }

    if (failIfFileExists)
        creationDisposition = CREATE_NEW;

    String destination = path;
    return CreateFile(destination.wideCharacters().data(), desiredAccess, shareMode, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void closeFile(PlatformFileHandle& handle)
{
    if (isHandleValid(handle)) {
        ::CloseHandle(handle);
        handle = invalidPlatformFileHandle;
    }
}

long long seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    DWORD moveMethod = FILE_BEGIN;

    if (origin == FileSeekOrigin::Current)
        moveMethod = FILE_CURRENT;
    else if (origin == FileSeekOrigin::End)
        moveMethod = FILE_END;

    LARGE_INTEGER largeOffset;
    largeOffset.QuadPart = offset;

    largeOffset.LowPart = SetFilePointer(handle, largeOffset.LowPart, &largeOffset.HighPart, moveMethod);

    if (largeOffset.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return -1;

    return largeOffset.QuadPart;
}

bool truncateFile(PlatformFileHandle handle, long long offset)
{
    FILE_END_OF_FILE_INFO eofInfo;
    eofInfo.EndOfFile.QuadPart = offset;

    return SetFileInformationByHandle(handle, FileEndOfFileInfo, &eofInfo, sizeof(FILE_END_OF_FILE_INFO));
}

bool flushFile(PlatformFileHandle handle)
{
    // Not implemented.
    return false;
}

int writeToFile(PlatformFileHandle handle, const void* data, int length)
{
    if (!isHandleValid(handle))
        return -1;

    DWORD bytesWritten;
    bool success = WriteFile(handle, data, length, &bytesWritten, nullptr);

    if (!success)
        return -1;
    return static_cast<int>(bytesWritten);
}

int readFromFile(PlatformFileHandle handle, void* data, int length)
{
    if (!isHandleValid(handle))
        return -1;

    DWORD bytesRead;
    bool success = ::ReadFile(handle, data, length, &bytesRead, nullptr);

    if (!success)
        return -1;
    return static_cast<int>(bytesRead);
}

String localUserSpecificStorageDirectory()
{
    return cachedStorageDirectory(CSIDL_LOCAL_APPDATA);
}

String roamingUserSpecificStorageDirectory()
{
    return cachedStorageDirectory(CSIDL_APPDATA);
}

std::optional<int32_t> getFileDeviceId(const String& fsFile)
{
    auto handle = openFile(fsFile, FileOpenMode::Read);
    if (!isHandleValid(handle))
        return std::nullopt;

    BY_HANDLE_FILE_INFORMATION fileInformation = { };
    if (!::GetFileInformationByHandle(handle, &fileInformation)) {
        closeFile(handle);
        return std::nullopt;
    }

    closeFile(handle);

    return fileInformation.dwVolumeSerialNumber;
}

String createTemporaryDirectory()
{
    return generateTemporaryPath([](const String& proposedPath) {
        return makeAllDirectories(proposedPath);
    });
}

std::optional<uint32_t> volumeFileBlockSize(const String& path)
{
    DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;
    if (!GetDiskFreeSpaceW(path.wideCharacters().data(), &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters))
        return std::nullopt;

    return sectorsPerCluster * bytesPerSector;
}

MappedFileData::~MappedFileData()
{
    if (m_fileData)
        UnmapViewOfFile(m_fileData);
    if (m_fileMapping)
        CloseHandle(m_fileMapping);
}

bool MappedFileData::mapFileHandle(PlatformFileHandle handle, FileOpenMode openMode, MappedFileMode)
{
    if (!isHandleValid(handle))
        return false;

    auto size = fileSize(handle);
    if (!size || *size > std::numeric_limits<size_t>::max() || *size > std::numeric_limits<decltype(m_fileSize)>::max()) {
        return false;
    }

    if (!*size) {
        return true;
    }

    DWORD pageProtection = PAGE_READONLY;
    DWORD desiredAccess = FILE_MAP_READ;
    switch (openMode) {
    case FileOpenMode::Read:
        pageProtection = PAGE_READONLY;
        desiredAccess = FILE_MAP_READ;
        break;
    case FileOpenMode::Write:
        pageProtection = PAGE_READWRITE;
        desiredAccess = FILE_MAP_WRITE;
        break;
    case FileOpenMode::ReadWrite:
        pageProtection = PAGE_READWRITE;
        desiredAccess = FILE_MAP_WRITE | FILE_MAP_READ;
        break;
    }

    m_fileMapping = CreateFileMapping(handle, nullptr, pageProtection, 0, 0, nullptr);
    if (!m_fileMapping)
        return false;

    m_fileData = MapViewOfFile(m_fileMapping, desiredAccess, 0, 0, *size);
    if (!m_fileData)
        return false;
    m_fileSize = *size;
    return true;
}

} // namespace FileSystemImpl
} // namespace WTF
