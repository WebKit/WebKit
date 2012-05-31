/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FileSystem.h"

#include "FileMetadata.h"
#include "NotImplemented.h"
#include "PlatformString.h"

#include <public/Platform.h>
#include <public/WebFileInfo.h>
#include <public/WebFileUtilities.h>

namespace WebCore {

bool deleteFile(const String& path)
{
    return WebKit::Platform::current()->fileUtilities()->deleteFile(path);
}

bool deleteEmptyDirectory(const String& path)
{
    return WebKit::Platform::current()->fileUtilities()->deleteEmptyDirectory(path);
}

bool getFileSize(const String& path, long long& result)
{
    FileMetadata metadata;
    if (!getFileMetadata(path, metadata))
        return false;
    result = metadata.length;
    return true;
}

bool getFileModificationTime(const String& path, time_t& result)
{
    FileMetadata metadata;
    if (!getFileMetadata(path, metadata))
        return false;
    result = metadata.modificationTime;
    return true;
}

bool getFileMetadata(const String& path, FileMetadata& metadata)
{
    WebKit::WebFileInfo webFileInfo;
    if (!WebKit::Platform::current()->fileUtilities()->getFileInfo(path, webFileInfo))
        return false;
    metadata.modificationTime = webFileInfo.modificationTime;
    metadata.length = webFileInfo.length;
    metadata.type = static_cast<FileMetadata::Type>(webFileInfo.type);
    return true;
}

String directoryName(const String& path)
{
    return WebKit::Platform::current()->fileUtilities()->directoryName(path);
}

String pathByAppendingComponent(const String& path, const String& component)
{
    return WebKit::Platform::current()->fileUtilities()->pathByAppendingComponent(path, component);
}

bool makeAllDirectories(const String& path)
{
    return WebKit::Platform::current()->fileUtilities()->makeAllDirectories(path);
}

bool fileExists(const String& path)
{
    return WebKit::Platform::current()->fileUtilities()->fileExists(path);
}

PlatformFileHandle openFile(const String& path, FileOpenMode mode)
{
    return WebKit::Platform::current()->fileUtilities()->openFile(path, mode);
}

void closeFile(PlatformFileHandle& handle)
{
    WebKit::Platform::current()->fileUtilities()->closeFile(handle);
}

long long seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    return WebKit::Platform::current()->fileUtilities()->seekFile(handle, offset, origin);
}

bool truncateFile(PlatformFileHandle handle, long long offset)
{
    return WebKit::Platform::current()->fileUtilities()->truncateFile(handle, offset);
}

int readFromFile(PlatformFileHandle handle, char* data, int length)
{
    return WebKit::Platform::current()->fileUtilities()->readFromFile(handle, data, length);
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    return WebKit::Platform::current()->fileUtilities()->writeToFile(handle, data, length);
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    notImplemented();

    return Vector<String>();
}

} // namespace WebCore
