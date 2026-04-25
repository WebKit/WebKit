/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include <wtf/FileHandle.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FileStream);

FileStream::FileStream() = default;

FileStream::~FileStream() = default;

long long FileStream::getSize(const String& path, std::optional<WallTime> expectedModificationTime)
{
    // Check the modification time for the possible file change.
    auto modificationTime = FileSystem::fileModificationTime(path);
    if (!modificationTime)
        return -1;
    if (expectedModificationTime) {
        if (expectedModificationTime->secondsSinceEpoch().secondsAs<time_t>() != modificationTime->secondsSinceEpoch().secondsAs<time_t>())
            return -1;
    }

    // Now get the file size.
    auto length = FileSystem::fileSize(path);
    if (!length)
        return -1;

    return *length;
}

bool FileStream::openForRead(const String& path, long long offset, long long length)
{
    if (m_handle)
        return true;

    // Open the file.
    m_handle = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    if (!m_handle)
        return false;

    // Jump to the beginning position if the file has been sliced.
    if (offset > 0) {
        if (!m_handle.seek(offset, FileSystem::FileSeekOrigin::Beginning))
            return false;
    }

    m_totalBytesToRead = length;
    m_bytesProcessed = 0;

    return true;
}

void FileStream::close()
{
    m_handle = { };
}

int FileStream::read(std::span<uint8_t> buffer)
{
    if (!m_handle)
        return -1;

    long long remaining = m_totalBytesToRead - m_bytesProcessed;
    int bytesToRead = remaining < static_cast<int>(buffer.size()) ? static_cast<int>(remaining) : static_cast<int>(buffer.size());
    std::optional<uint64_t> bytesRead = 0;
    if (bytesToRead > 0)
        bytesRead = m_handle.read(buffer.first(bytesToRead));
    if (!bytesRead)
        return -1;
    if (*bytesRead > 0)
        m_bytesProcessed += *bytesRead;

    return *bytesRead;
}

} // namespace WebCore
