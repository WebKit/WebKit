/*
 * Copyright (C) 2010 Company 100, Inc. All rights reserved.
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

#include <AEEAppGen.h>
#include <AEEFile.h>
#include <AEEStdLib.h>

#include <wtf/RandomNumber.h>
#include <wtf/brew/RefPtrBrew.h>
#include <wtf/brew/ShellBrew.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

bool getFileSize(const String& path, long long& result)
{
    RefPtr<IFileMgr> fileMgr = createRefPtrInstance<IFileMgr>(AEECLSID_FILEMGR);
    FileInfo info;

    if (IFILEMGR_GetInfo(fileMgr.get(), path.utf8().data(), &info) == SUCCESS) {
        result = info.dwSize;
        return true;
    }

    return false;
}

bool getFileModificationTime(const String& path, time_t& result)
{
    // There is no way to get file modification time in BREW. IFILEMGR_GetInfoEx() returns
    // only file creation time.
    return false;
}

bool fileExists(const String& path)
{
    RefPtr<IFileMgr> fileMgr = createRefPtrInstance<IFileMgr>(AEECLSID_FILEMGR);

    return (IFILEMGR_Test(fileMgr.get(), path.utf8().data()) == SUCCESS);
}

bool deleteFile(const String& path)
{
    RefPtr<IFileMgr> fileMgr = createRefPtrInstance<IFileMgr>(AEECLSID_FILEMGR);

    return (IFILEMGR_Remove(fileMgr.get(), path.utf8().data()) == SUCCESS);
}

bool deleteEmptyDirectory(const String& path)
{
    RefPtr<IFileMgr> fileMgr = createRefPtrInstance<IFileMgr>(AEECLSID_FILEMGR);

    return (IFILEMGR_RmDir(fileMgr.get(), path.utf8().data()) == SUCCESS);
}

String pathByAppendingComponent(const String& path, const String& component)
{
    if (component.isEmpty())
        return path;

    Vector<UChar, 1024> buffer;

    buffer.append(path.characters(), path.length());

    if (buffer.last() != L'/' && component[0] != L'/')
        buffer.append(L'/');

    buffer.append(component.characters(), component.length());

    return String(buffer.data(), buffer.size());
}

CString fileSystemRepresentation(const String& path)
{
    return path.utf8();
}

static String canonicalPath(const String& path)
{
    RefPtr<IFileMgr> fileMgr = createRefPtrInstance<IFileMgr>(AEECLSID_FILEMGR);

    // Get the buffer size required to resolve the path.
    int canonPathLen;
    IFILEMGR_ResolvePath(fileMgr.get(), path.utf8().data(), 0, &canonPathLen);

    // Resolve the path to the canonical path.
    Vector<char> canonPathBuffer(canonPathLen);
    IFILEMGR_ResolvePath(fileMgr.get(), path.utf8().data(), canonPathBuffer.data(), &canonPathLen);

    String canonPath(canonPathBuffer.data());

    // Remove the trailing '/'.
    int lastDivPos = canonPath.reverseFind('/');
    int endPos = canonPath.length();
    if (lastDivPos == endPos - 1)
        canonPath = canonPath.substring(0, canonPath.length() - 1);

    return canonPath;
}

static bool makeAllDirectories(IFileMgr* fileManager, const String& path)
{
    if (path == canonicalPath(AEEFS_HOME_DIR))
        return true;

    int lastDivPos = path.reverseFind('/');
    int endPos = path.length();
    if (lastDivPos == path.length() - 1) {
        endPos -= 1;
        lastDivPos = path.reverseFind('/', lastDivPos);
    }

    if (lastDivPos > 0) {
        if (!makeAllDirectories(fileManager, path.substring(0, lastDivPos)))
            return false;
    }

    String folder(path.substring(0, endPos));

    // IFILEMGR_MkDir return SUCCESS when the file is successfully created or if file already exists.
    // So we need to check fileinfo.attrib.
    IFILEMGR_MkDir(fileManager, folder.utf8().data());

    FileInfo fileInfo;
    if (IFILEMGR_GetInfo(fileManager, folder.utf8().data(), &fileInfo) != SUCCESS)
        return false;

    return fileInfo.attrib & _FA_DIR;
}

bool makeAllDirectories(const String& path)
{
    RefPtr<IFileMgr> fileMgr = createRefPtrInstance<IFileMgr>(AEECLSID_FILEMGR);

    return makeAllDirectories(fileMgr.get(), canonicalPath(path));
}

String homeDirectoryPath()
{
    const int webViewClassId = 0x010aa04c;
    return String::format("fs:/~0X%08X/", webViewClassId);
}

String pathGetFileName(const String& path)
{
    return path.substring(path.reverseFind('/') + 1);
}

String directoryName(const String& path)
{
    String fileName = pathGetFileName(path);
    String dirName = String(path);
    dirName.truncate(dirName.length() - pathGetFileName(path).length());
    return dirName;
}

String openTemporaryFile(const String& prefix, PlatformFileHandle& handle)
{
    // BREW does not have a system-wide temporary directory,
    // use "fs:/~/tmp" as our temporary directory.
    String tempPath("fs:/~/tmp");

    RefPtr<IFileMgr> fileMgr = createRefPtrInstance<IFileMgr>(AEECLSID_FILEMGR);

    // Create the temporary directory if it does not exist.
    IFILEMGR_MkDir(fileMgr.get(), tempPath.utf8().data());

    // Loop until we find a temporary filename that does not exist.
    int number = static_cast<int>(randomNumber() * 10000);
    String filename;
    do {
        StringBuilder builder;
        builder.append(tempPath);
        builder.append('/');
        builder.append(prefix);
        builder.append(String::number(number));
        filename = builder.toString();
        number++;
    } while (IFILEMGR_Test(fileMgr.get(), filename.utf8().data()) == SUCCESS);

    IFile* tempFile = IFILEMGR_OpenFile(fileMgr.get(), filename.utf8().data(), _OFM_CREATE);
    if (tempFile) {
        handle = tempFile;
        return filename;
    }

    return String();
}

void closeFile(PlatformFileHandle& handle)
{
    if (isHandleValid(handle)) {
        IFILE_Release(handle);
        handle = invalidPlatformFileHandle;
    }
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    if (!isHandleValid(handle))
        return -1;

    int bytesWritten = IFILE_Write(handle, data, length);
    if (!bytesWritten)
        return -1;
    return bytesWritten;
}

bool unloadModule(PlatformModule module)
{
    notImplemented();

    return false;
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;

    // OK to not implement listDirectory, because it's only used for plug-ins and
    // Brew MP does not support the plug-in at the moment.
    notImplemented();

    return entries;
}

}
