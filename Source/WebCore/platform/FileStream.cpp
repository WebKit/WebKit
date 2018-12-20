/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#include "FileStream.h"

#include "FileSystem.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

FileStream::FileStream()
    : m_handle(FileSystem::invalidPlatformFileHandle)
    , m_bytesProcessed(0)
    , m_totalBytesToRead(0)
{
}

FileStream::~FileStream()
{
    close();
}

long long FileStream::getSize(const String& path, Optional<WallTime> expectedModificationTime)
{
    // Check the modification time for the possible file change.
    auto modificationTime = FileSystem::getFileModificationTime(path);
    if (!modificationTime)
        return -1;
    if (expectedModificationTime) {
        if (expectedModificationTime->secondsSinceEpoch().secondsAs<time_t>() != modificationTime->secondsSinceEpoch().secondsAs<time_t>())
            return -1;
    }

    // Now get the file size.
    long long length;
    if (!FileSystem::getFileSize(path, length))
        return -1;

    return length;
}

bool FileStream::openForRead(const String& path, long long offset, long long length)
{
    if (FileSystem::isHandleValid(m_handle))
        return true;

    // Open the file.
    m_handle = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    if (!FileSystem::isHandleValid(m_handle))
        return false;

    // Jump to the beginning position if the file has been sliced.
    if (offset > 0) {
        if (FileSystem::seekFile(m_handle, offset, FileSystem::FileSeekOrigin::Beginning) < 0)
            return false;
    }

    m_totalBytesToRead = length;
    m_bytesProcessed = 0;

    return true;
}

void FileStream::close()
{
    if (FileSystem::isHandleValid(m_handle)) {
        FileSystem::closeFile(m_handle);
        m_handle = FileSystem::invalidPlatformFileHandle;
    }
}

int FileStream::read(char* buffer, int bufferSize)
{
    if (!FileSystem::isHandleValid(m_handle))
        return -1;

    long long remaining = m_totalBytesToRead - m_bytesProcessed;
    int bytesToRead = (remaining < bufferSize) ? static_cast<int>(remaining) : bufferSize;
    int bytesRead = 0;
    if (bytesToRead > 0)
        bytesRead = FileSystem::readFromFile(m_handle, buffer, bytesToRead);
    if (bytesRead < 0)
        return -1;
    if (bytesRead > 0)
        m_bytesProcessed += bytesRead;

    return bytesRead;
}

} // namespace WebCore
