/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "FileSystem.h"

#include "NotImplemented.h"
#include "PathWalker.h"
#include "PlatformString.h"
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

#include <windows.h>
#include <winbase.h>
#include <shlobj.h>
#include <shlwapi.h>

namespace WebCore {

static bool statFile(String path, struct _stat64& st)
{
    ASSERT_ARG(path, !path.isNull());
    return !_wstat64(path.charactersWithNullTermination(), &st) && (st.st_mode & _S_IFMT) == _S_IFREG;
}

bool getFileSize(const String& path, long long& result)
{
    struct _stat64 sb;
    if (!statFile(path, sb))
        return false;
    result = sb.st_size;
    return true;
}

bool getFileModificationTime(const String& path, time_t& result)
{
    struct _stat64 st;
    if (!statFile(path, st))
        return false;
    result = st.st_mtime;
    return true;
}

bool fileExists(const String& path) 
{
    struct _stat64 st;
    return statFile(path, st);
}

bool deleteFile(const String& path)
{
    String filename = path;
    return !!DeleteFileW(filename.charactersWithNullTermination());
}

bool deleteEmptyDirectory(const String& path)
{
    String filename = path;
    return !!RemoveDirectoryW(filename.charactersWithNullTermination());
}

String pathByAppendingComponent(const String& path, const String& component)
{
    Vector<UChar> buffer(MAX_PATH);

    if (path.length() + 1 > buffer.size())
        return String();

    memcpy(buffer.data(), path.characters(), path.length() * sizeof(UChar));
    buffer[path.length()] = '\0';

    String componentCopy = component;
    if (!PathAppendW(buffer.data(), componentCopy.charactersWithNullTermination()))
        return String();

    buffer.resize(wcslen(buffer.data()));

    return String::adopt(buffer);
}

CString fileSystemRepresentation(const String&)
{
    return "";
}

bool makeAllDirectories(const String& path)
{
    String fullPath = path;
    if (SHCreateDirectoryEx(0, fullPath.charactersWithNullTermination(), 0) != ERROR_SUCCESS) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            LOG_ERROR("Failed to create path %s", path.ascii().data());
            return false;
        }
    }
    return true;
}

String homeDirectoryPath()
{
    notImplemented();
    return "";
}

String pathGetFileName(const String& path)
{
    return String(::PathFindFileName(String(path).charactersWithNullTermination()));
}

String directoryName(const String& path)
{
    String name = path.left(path.length() - pathGetFileName(path).length());
    if (name.characterStartingAt(name.length() - 1) == '\\') {
        // Remove any trailing "\".
        name.truncate(name.length() - 1);
    }
    return name;
}

static String bundleName()
{
    static bool initialized;
    static String name = "WebKit";

    if (!initialized) {
        initialized = true;

        if (CFBundleRef bundle = CFBundleGetMainBundle())
            if (CFTypeRef bundleExecutable = CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleExecutableKey))
                if (CFGetTypeID(bundleExecutable) == CFStringGetTypeID())
                    name = reinterpret_cast<CFStringRef>(bundleExecutable);
    }

    return name;
}

static String storageDirectory(DWORD pathIdentifier)
{
    Vector<UChar> buffer(MAX_PATH);
    if (FAILED(SHGetFolderPathW(0, pathIdentifier | CSIDL_FLAG_CREATE, 0, 0, buffer.data())))
        return String();
    buffer.resize(wcslen(buffer.data()));
    String directory = String::adopt(buffer);

    static const String companyNameDirectory = "Apple Computer\\";
    directory = pathByAppendingComponent(directory, companyNameDirectory + bundleName());
    if (!makeAllDirectories(directory))
        return String();

    return directory;
}

static String cachedStorageDirectory(DWORD pathIdentifier)
{
    static HashMap<DWORD, String> directories;

    HashMap<DWORD, String>::iterator it = directories.find(pathIdentifier);
    if (it != directories.end())
        return it->second;

    String directory = storageDirectory(pathIdentifier);
    directories.add(pathIdentifier, directory);

    return directory;
}

String openTemporaryFile(const String&, PlatformFileHandle& handle)
{
    handle = INVALID_HANDLE_VALUE;

    char tempPath[MAX_PATH];
    int tempPathLength = ::GetTempPathA(WTF_ARRAY_LENGTH(tempPath), tempPath);
    if (tempPathLength <= 0 || tempPathLength > WTF_ARRAY_LENGTH(tempPath))
        return String();

    HCRYPTPROV hCryptProv = 0;
    if (!CryptAcquireContext(&hCryptProv, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return String();

    char proposedPath[MAX_PATH];
    while (1) {
        char tempFile[] = "XXXXXXXX.tmp"; // Use 8.3 style name (more characters aren't helpful due to 8.3 short file names)
        const int randomPartLength = 8;
        if (!CryptGenRandom(hCryptProv, randomPartLength, reinterpret_cast<BYTE*>(tempFile)))
            break;

        // Limit to valid filesystem characters, also excluding others that could be problematic, like punctuation.
        // don't include both upper and lowercase since Windows file systems are typically not case sensitive.
        const char validChars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        for (int i = 0; i < randomPartLength; ++i)
            tempFile[i] = validChars[tempFile[i] % (sizeof(validChars) - 1)];

        ASSERT(strlen(tempFile) == sizeof(tempFile) - 1);

        if (!PathCombineA(proposedPath, tempPath, tempFile))
            break;
 
        // use CREATE_NEW to avoid overwriting an existing file with the same name
        handle = CreateFileA(proposedPath, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        if (!isHandleValid(handle) && GetLastError() == ERROR_ALREADY_EXISTS)
            continue;

        break;
    }

    CryptReleaseContext(hCryptProv, 0);

    if (!isHandleValid(handle))
        return String();

    return String::fromUTF8(proposedPath);
}

PlatformFileHandle openFile(const String& path, FileOpenMode mode)
{
    DWORD desiredAccess = 0;
    DWORD creationDisposition = 0;
    switch (mode) {
    case OpenForRead:
        desiredAccess = GENERIC_READ;
        creationDisposition = OPEN_EXISTING;
        break;
    case OpenForWrite:
        desiredAccess = GENERIC_WRITE;
        creationDisposition = CREATE_ALWAYS;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    String destination = path;
    return CreateFile(destination.charactersWithNullTermination(), desiredAccess, 0, 0, creationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
}

void closeFile(PlatformFileHandle& handle)
{
    if (isHandleValid(handle)) {
        ::CloseHandle(handle);
        handle = invalidPlatformFileHandle;
    }
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    if (!isHandleValid(handle))
        return -1;

    DWORD bytesWritten;
    bool success = WriteFile(handle, data, length, &bytesWritten, 0);

    if (!success)
        return -1;
    return static_cast<int>(bytesWritten);
}

bool unloadModule(PlatformModule module)
{
    return ::FreeLibrary(module);
}

String localUserSpecificStorageDirectory()
{
    return cachedStorageDirectory(CSIDL_LOCAL_APPDATA);
}

String roamingUserSpecificStorageDirectory()
{
    return cachedStorageDirectory(CSIDL_APPDATA);
}

bool safeCreateFile(const String& path, CFDataRef data)
{
    // Create a temporary file.
    WCHAR tempDirPath[MAX_PATH];
    if (!GetTempPathW(WTF_ARRAY_LENGTH(tempDirPath), tempDirPath))
        return false;

    WCHAR tempPath[MAX_PATH];
    if (!GetTempFileNameW(tempDirPath, L"WEBKIT", 0, tempPath))
        return false;

    HANDLE tempFileHandle = CreateFileW(tempPath, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (tempFileHandle == INVALID_HANDLE_VALUE)
        return false;

    // Write the data to this temp file.
    DWORD written;
    if (!WriteFile(tempFileHandle, CFDataGetBytePtr(data), static_cast<DWORD>(CFDataGetLength(data)), &written, 0))
        return false;

    CloseHandle(tempFileHandle);

    // Copy the temp file to the destination file.
    String destination = path;
    if (!MoveFileExW(tempPath, destination.charactersWithNullTermination(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
        return false;

    return true;
}

Vector<String> listDirectory(const String& directory, const String& filter)
{
    Vector<String> entries;

    PathWalker walker(directory, filter);
    if (!walker.isValid())
        return entries;

    do {
        if (walker.data().dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        entries.append(makeString(directory, "\\", reinterpret_cast<const UChar*>(walker.data().cFileName)));
    } while (walker.step());

    return entries;
}

} // namespace WebCore
