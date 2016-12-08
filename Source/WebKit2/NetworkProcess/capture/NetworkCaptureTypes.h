/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(NETWORK_CAPTURE)

#include <WebCore/FileSystem.h>

namespace WebKit {
namespace NetworkCapture {

class FileHandle final {
public:
    FileHandle() = default;

    FileHandle(WebCore::PlatformFileHandle fileHandle)
        : m_fileHandle(fileHandle) { }

    FileHandle(FileHandle&& other)
        : m_fileHandle(std::exchange(other.m_fileHandle, WebCore::invalidPlatformFileHandle)) { }

    ~FileHandle()
    {
        closeFile();
    }

    FileHandle& operator=(WebCore::PlatformFileHandle fileHandle)
    {
        closeFile();
        m_fileHandle = fileHandle;
        return *this;
    }

    FileHandle& operator=(FileHandle&& other)
    {
        closeFile();
        m_fileHandle = std::exchange(other.m_fileHandle, WebCore::invalidPlatformFileHandle);
        return *this;
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    operator WebCore::PlatformFileHandle() const { return m_fileHandle; }
    explicit operator bool() const { return WebCore::isHandleValid(m_fileHandle); }

    static FileHandle openFile(const String& path, WebCore::FileOpenMode mode)
    {
        return FileHandle(WebCore::openFile(path, mode));
    }

    int readFromFile(void* data, int length) const
    {
        return WebCore::readFromFile(m_fileHandle, static_cast<char*>(data), length);
    }

    int writeToFile(const void* data, int length) const
    {
        return WebCore::writeToFile(m_fileHandle, static_cast<const char*>(data), length);
    }

    void closeFile()
    {
        WebCore::closeFile(m_fileHandle);
    }

private:
    WebCore::PlatformFileHandle m_fileHandle { WebCore::invalidPlatformFileHandle };
};

} // namespace NetworkCapture
} // namespace WebKit

#endif // ENABLE(NETWORK_CAPTURE)
