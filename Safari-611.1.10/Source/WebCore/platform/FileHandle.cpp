/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "FileHandle.h"

namespace WebCore {

FileHandle::FileHandle(const String& path, FileSystem::FileOpenMode mode)
    : m_path { path }
    , m_mode { mode }
{
}

FileHandle::FileHandle(FileHandle&& other)
    : m_path { WTFMove(other.m_path) }
    , m_mode { WTFMove(other.m_mode) }
    , m_fileHandle { std::exchange(other.m_fileHandle, FileSystem::invalidPlatformFileHandle) }
{
}

FileHandle::FileHandle(const String& path, FileSystem::FileOpenMode mode, OptionSet<FileSystem::FileLockMode> lockMode)
    : m_path { path }
    , m_mode { mode }
    , m_lockMode { lockMode }
    , m_shouldLock { true }
{
}

FileHandle::~FileHandle()
{
    close();
}

FileHandle& FileHandle::operator=(FileHandle&& other)
{
    close();
    m_path = WTFMove(other.m_path);
    m_mode = WTFMove(other.m_mode);
    m_fileHandle = std::exchange(other.m_fileHandle, FileSystem::invalidPlatformFileHandle);
    return *this;
}

FileHandle::operator bool() const
{
    return FileSystem::isHandleValid(m_fileHandle);
}

bool FileHandle::open(const String& path, FileSystem::FileOpenMode mode)
{
    if (*this && path == m_path && mode == m_mode)
        return true;

    close();
    m_path = path;
    m_mode = mode;
    return open();
}

bool FileHandle::open()
{
    if (!*this)
        m_fileHandle = m_shouldLock ? FileSystem::openAndLockFile(m_path, m_mode, m_lockMode) :  FileSystem::openFile(m_path, m_mode);
    return static_cast<bool>(*this);
}

int FileHandle::read(void* data, int length)
{
    if (!open())
        return -1;
    return FileSystem::readFromFile(m_fileHandle, static_cast<char*>(data), length);
}

int FileHandle::write(const void* data, int length)
{
    if (!open())
        return -1;
    return FileSystem::writeToFile(m_fileHandle, static_cast<const char*>(data), length);
}

bool FileHandle::printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    va_list preflightArgs;
    va_copy(preflightArgs, args);
    size_t stringLength = vsnprintf(nullptr, 0, format, preflightArgs);
    va_end(preflightArgs);

    Vector<char, 1024> buffer(stringLength + 1);
    vsnprintf(buffer.data(), stringLength + 1, format, args);

    va_end(args);

    return write(buffer.data(), stringLength) >= 0;
}

void FileHandle::close()
{
    if (m_shouldLock && *this) {
        // FileSystem::unlockAndCloseFile requires the file handle to be valid while closeFile does not
        FileSystem::unlockAndCloseFile(m_fileHandle);
    } else
        FileSystem::closeFile(m_fileHandle);
}

} // namespace WebCore
