/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
#include "PlatformString.h"
#include <wtf/text/CString.h>

#include <windows.h>
#include <wincrypt.h>

namespace WebCore {

static bool getFileInfo(const String& path, BY_HANDLE_FILE_INFORMATION& fileInfo)
{
    String filename = path;
    HANDLE hFile = CreateFile(filename.charactersWithNullTermination(), GENERIC_READ, FILE_SHARE_READ, 0
        , OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);

    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    bool rtn = GetFileInformationByHandle(hFile, &fileInfo) ? true : false;

    CloseHandle(hFile);
    return rtn;
}

bool getFileSize(const String& path, long long& result)
{
    BY_HANDLE_FILE_INFORMATION fileInformation;
    if (!getFileInfo(path, fileInformation))
        return false;

    ULARGE_INTEGER fileSize;
    fileSize.LowPart = fileInformation.nFileSizeLow;
    fileSize.HighPart = fileInformation.nFileSizeHigh;

    result = fileSize.QuadPart;

    return true;
}

bool getFileModificationTime(const String& path, time_t& result)
{
    BY_HANDLE_FILE_INFORMATION fileInformation;
    if (!getFileInfo(path, fileInformation))
        return false;

    ULARGE_INTEGER t;
    memcpy(&t, &fileInformation.ftLastWriteTime, sizeof(t));
    
    result = t.QuadPart * 0.0000001 - 11644473600.0;

    return true;
}

bool fileExists(const String& path) 
{
    String filename = path;
    HANDLE hFile = CreateFile(filename.charactersWithNullTermination(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE
        , 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);

    CloseHandle(hFile);

    return hFile != INVALID_HANDLE_VALUE;
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
    if (component.isEmpty())
        return path;

    Vector<UChar, MAX_PATH> buffer;

    buffer.append(path.characters(), path.length());

    if (buffer.last() != L'\\' && buffer.last() != L'/'
        && component[0] != L'\\' && component[0] != L'/')
        buffer.append(L'\\');

    buffer.append(component.characters(), component.length());

    return String(buffer.data(), buffer.size());
}

CString fileSystemRepresentation(const String&)
{
    return "";
}

bool makeAllDirectories(const String& path)
{
    int lastDivPos = std::max(path.reverseFind('/'), path.reverseFind('\\'));
    int endPos = path.length();
    if (lastDivPos == path.length() - 1) {
        endPos -= 1;
        lastDivPos = std::max(path.reverseFind('/', lastDivPos), path.reverseFind('\\', lastDivPos));
    }

    if (lastDivPos > 0) {
        if (!makeAllDirectories(path.substring(0, lastDivPos)))
            return false;
    }

    String folder(path.substring(0, endPos));
    CreateDirectory(folder.charactersWithNullTermination(), 0);

    DWORD fileAttr = GetFileAttributes(folder.charactersWithNullTermination());
    return fileAttr != 0xFFFFFFFF && (fileAttr & FILE_ATTRIBUTE_DIRECTORY);
}

String homeDirectoryPath()
{
    notImplemented();
    return "";
}

String pathGetFileName(const String& path)
{
    return path.substring(std::max(path.reverseFind('/'), path.reverseFind('\\')) + 1);
}

String directoryName(const String& path)
{
    notImplemented();
    return String();
}

CString openTemporaryFile(const char*, PlatformFileHandle& handle)
{
    handle = INVALID_HANDLE_VALUE;

    wchar_t tempPath[MAX_PATH];
    int tempPathLength = ::GetTempPath(_countof(tempPath), tempPath);
    if (tempPathLength <= 0 || tempPathLength > _countof(tempPath))
        return CString();

    HCRYPTPROV hCryptProv = 0;
    if (!CryptAcquireContext(&hCryptProv, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return CString();

    String proposedPath;
    while (1) {

        wchar_t tempFile[] = L"XXXXXXXX.tmp"; // Use 8.3 style name (more characters aren't helpful due to 8.3 short file names)
        const int randomPartLength = 8;
        if (!CryptGenRandom(hCryptProv, randomPartLength * 2, reinterpret_cast<BYTE*>(tempFile)))
            break;

        // Limit to valid filesystem characters, also excluding others that could be problematic, like punctuation.
        // don't include both upper and lowercase since Windows file systems are typically not case sensitive.
        const char validChars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        for (int i = 0; i < randomPartLength; ++i)
            tempFile[i] = validChars[tempFile[i] % (sizeof(validChars) - 1)];

        ASSERT(wcslen(tempFile) * 2 == sizeof(tempFile) - 2);

        proposedPath = pathByAppendingComponent(String(tempPath), String(tempFile));

        // use CREATE_NEW to avoid overwriting an existing file with the same name
        handle = CreateFile(proposedPath.charactersWithNullTermination(), GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        if (!isHandleValid(handle) && GetLastError() == ERROR_ALREADY_EXISTS)
            continue;

        break;
    }

    CryptReleaseContext(hCryptProv, 0);

    if (!isHandleValid(handle))
        return CString();

    return proposedPath.latin1();
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
    return String(L"\\");
}

String roamingUserSpecificStorageDirectory()
{
    return String(L"\\");
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;

    Vector<UChar, 256> pattern;
    pattern.append(path.characters(), path.length());
    if (pattern.last() != L'/' && pattern.last() != L'\\')
        pattern.append(L'\\');

    String root(pattern.data(), pattern.size());
    pattern.append(filter.characters(), filter.length());
    pattern.append(0);

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(pattern.data(), &findData);
    if (INVALID_HANDLE_VALUE != hFind) {
        do {
            // FIXEME: should we also add the folders? This function
            // is so far only called by PluginDatabase.cpp to list
            // all plugins in a folder, where it's not supposed to list sub-folders.
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                entries.append(root + findData.cFileName);
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }

    return entries;
}

}
