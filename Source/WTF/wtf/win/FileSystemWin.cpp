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
#include <wtf/FileHandle.h>
#include <wtf/HashMap.h>
#include <wtf/MappedFileData.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/win/WCharStringExtras.h>
#include <wtf/win/PathWalker.h>

namespace WTF {

namespace FileSystemImpl {

static const ULONGLONG kSecondsFromFileTimeToTimet = 11644473600;

static bool getFindData(String path, WIN32_FIND_DATAW& findData)
{
    HANDLE handle = FindFirstFileW(path.wideCharacters().span().data(), &findData);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    FindClose(handle);
    return true;
}

static void fileCreationTimeFromFindData(const WIN32_FIND_DATAW& findData, time_t& time)
{
    ULARGE_INTEGER fileTime;
    fileTime.HighPart = findData.ftCreationTime.dwHighDateTime;
    fileTime.LowPart = findData.ftCreationTime.dwLowDateTime;

    // Information about converting time_t to FileTime is available at http://msdn.microsoft.com/en-us/library/ms724228%28v=vs.85%29.aspx
    time = fileTime.QuadPart / 10000000 - kSecondsFromFileTimeToTimet;
}

bool fileIDsAreEqual(std::optional<PlatformFileID>, std::optional<PlatformFileID>)
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

CString fileSystemRepresentation(const String& path)
{
    auto characters = StringView(path).upconvertedCharacters();
    int size = WideCharToMultiByte(CP_ACP, 0, wcharFrom(characters), path.length(), 0, 0, 0, 0);

    std::span<char> buffer;
    CString string = CString::newUninitialized(size, buffer);

    WideCharToMultiByte(CP_ACP, 0, wcharFrom(characters), path.length(), buffer.data(), buffer.size(), 0, 0);

    return string;
}

static String storageDirectory(DWORD pathIdentifier)
{
    Vector<char16_t> buffer(MAX_PATH);
    if (FAILED(SHGetFolderPathW(nullptr, pathIdentifier | CSIDL_FLAG_CREATE, nullptr, 0, wcharFrom(buffer.mutableSpan().data()))))
        return String();

    buffer.shrink(wcslen(wcharFrom(buffer.span().data())));
    String directory = String::adopt(WTFMove(buffer));

    directory = pathByAppendingComponent(directory, "Apple Computer\\WebKit"_s);
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
    int tempPathLength = ::GetTempPathW(std::size(tempPath), tempPath);
    if (tempPathLength <= 0 || static_cast<size_t>(tempPathLength) >= std::size(tempPath))
        return String();

    String proposedPath;
    do {
        wchar_t tempFile[] = L"XXXXXXXX.tmp"; // Use 8.3 style name (more characters aren't helpful due to 8.3 short file names)
        const int randomPartLength = 8;
        cryptographicallyRandomValues({ reinterpret_cast<uint8_t*>(tempFile), randomPartLength * sizeof(wchar_t) });

        // Limit to valid filesystem characters, also excluding others that could be problematic, like punctuation.
        // don't include both upper and lowercase since Windows file systems are typically not case sensitive.
        const char validChars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        for (int i = 0; i < randomPartLength; ++i)
            tempFile[i] = validChars[tempFile[i] % (sizeof(validChars) - 1)];

        ASSERT(wcslen(tempFile) == std::size(tempFile) - 1);

        proposedPath = pathByAppendingComponent(String(tempPath), String(tempFile));
        if (proposedPath.isEmpty())
            break;
    } while (!action(proposedPath));

    return proposedPath;
}

std::pair<String, FileHandle> openTemporaryFile(StringView, StringView suffix)
{
    // FIXME: Suffix is not supported, but OK for now since the code using it is macOS-port-only.
    ASSERT_UNUSED(suffix, suffix.isEmpty());

    FileHandle handle;

    String proposedPath = generateTemporaryPath([&handle](const String& proposedPath) {
        // use CREATE_NEW to avoid overwriting an existing file with the same name
        handle = FileHandle::adopt(::CreateFileW(proposedPath.wideCharacters().span().data(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));

        return handle || GetLastError() == ERROR_ALREADY_EXISTS;
    });

    if (!handle)
        return { String(), FileHandle() };

    return { proposedPath, WTFMove(handle) };
}

FileHandle openFile(const String& path, FileOpenMode mode, FileAccessPermission, OptionSet<FileLockMode> lockMode, bool failIfFileExists)
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
    case FileOpenMode::Truncate:
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
    return FileHandle::adopt(CreateFile(destination.wideCharacters().span().data(), desiredAccess, shareMode, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr), lockMode);
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
    if (!handle)
        return std::nullopt;

    BY_HANDLE_FILE_INFORMATION fileInformation = { };
    if (!::GetFileInformationByHandle(handle.platformHandle(), &fileInformation))
        return std::nullopt;

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
    if (!GetDiskFreeSpaceW(path.wideCharacters().span().data(), &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters))
        return std::nullopt;

    return sectorsPerCluster * bytesPerSector;
}

} // namespace FileSystemImpl
} // namespace WTF
