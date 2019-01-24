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

#pragma once

#include <wtf/Assertions.h>
#include <wtf/FileSystem.h>

namespace WebCore {

class WEBCORE_EXPORT FileHandle final {
public:
    FileHandle() = default;
    FileHandle(const String& path, FileSystem::FileOpenMode);
    FileHandle(const String& path, FileSystem::FileOpenMode, OptionSet<FileSystem::FileLockMode>);
    FileHandle(const FileHandle& other) = delete;
    FileHandle(FileHandle&& other);

    ~FileHandle();

    FileHandle& operator=(const FileHandle& other) = delete;
    FileHandle& operator=(FileHandle&& other);

    explicit operator bool() const;

    bool open(const String& path, FileSystem::FileOpenMode);
    bool open();
    int read(void* data, int length);
    int write(const void* data, int length);
    bool printf(const char* format, ...) WTF_ATTRIBUTE_PRINTF(2, 3);
    void close();

private:
    String m_path;
    FileSystem::FileOpenMode m_mode { FileSystem::FileOpenMode::Read };
    FileSystem::PlatformFileHandle m_fileHandle { FileSystem::invalidPlatformFileHandle };
    OptionSet<FileSystem::FileLockMode> m_lockMode;
    bool m_shouldLock { false };
};

} // namespace WebCore
