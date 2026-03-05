/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/FileHandle.h>

#include <wtf/FileSystem.h>
#include <wtf/MappedFileData.h>

namespace WTF::FileSystemImpl {

FileHandle::FileHandle() = default;

FileHandle::FileHandle(PlatformFileHandle handle, OptionSet<FileLockMode> lockMode)
    : m_handle(handle)
{
#if USE(FILE_LOCK)
    if (lockMode)
        lock(lockMode);
#else
    UNUSED_PARAM(lockMode);
#endif
}

FileHandle::FileHandle(FileHandle&& other)
    : m_handle(std::exchange(other.m_handle, std::nullopt))
{ }

FileHandle::~FileHandle()
{
    close();
}

FileHandle& FileHandle::operator=(FileHandle&& other)
{
    close();

    m_handle = std::exchange(other.m_handle, std::nullopt);
    return *this;
}

std::optional<Vector<uint8_t>> FileHandle::readAll()
{
    if (!isValid())
        return { };

    auto size = this->size().value_or(0);
    if (!size)
        return std::nullopt;

    size_t bytesToRead;
    if (!WTF::convertSafely(size, bytesToRead))
        return std::nullopt;

    Vector<uint8_t> buffer(bytesToRead);
    size_t totalBytesRead = 0;
    uint64_t bytesRead;

    while ((bytesRead = read(buffer.mutableSpan().subspan(totalBytesRead)).value_or(0)))
        totalBytesRead += bytesRead;

    if (totalBytesRead != bytesToRead)
        return std::nullopt;

    return buffer;
}

bool FileHandle::appendFileContents(const String& path)
{
    auto source = openFile(path, FileOpenMode::Read);
    if (!source)
        return false;

    static size_t bufferSize = 1 << 19;
    Vector<uint8_t> buffer(bufferSize);

    do {
        auto readBytes = source.read(buffer.mutableSpan());

        if (!readBytes)
            return false;

        if (write(buffer.span().first(*readBytes)) != readBytes)
            return false;

        if (*readBytes < bufferSize)
            return true;
    } while (true);

    ASSERT_NOT_REACHED();
}

std::optional<MappedFileData> FileHandle::map(MappedFileMode fileMode)
{
    return map(fileMode, FileOpenMode::Read);
}

} // namespace WTF::FileSystemImpl
