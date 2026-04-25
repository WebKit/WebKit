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

#pragma once

#include <optional>
#include <wtf/FileLockMode.h>
#include <wtf/Forward.h>
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>

#if OS(WINDOWS)
#include <wtf/win/Win32Handle.h>
#elif OS(UNIX)
#include <sys/types.h>
#endif

namespace WTF {

namespace FileSystemImpl {

class MappedFileData;

enum class FileOpenMode : uint8_t;
enum class MappedFileMode : bool;

#if OS(WINDOWS)
typedef HANDLE PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = INVALID_HANDLE_VALUE;
typedef FILE_ID_128 PlatformFileID;

struct Win32HandleMarkableTraits {
    static bool isEmptyValue(HANDLE value) { return value == invalidPlatformFileHandle; }
    static HANDLE emptyValue() { return invalidPlatformFileHandle; }
};
using PlatformHandleTraits = Win32HandleMarkableTraits;
#else
typedef int PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = -1;
typedef ino_t PlatformFileID;
using PlatformHandleTraits = IntegralMarkableTraits<int, invalidPlatformFileHandle>;
#endif

enum class FileSeekOrigin {
    Beginning,
    Current,
    End,
};

class FileHandle {
public:
    WTF_EXPORT_PRIVATE FileHandle();

    static FileHandle adopt(PlatformFileHandle handle, OptionSet<FileLockMode> lockMode = { })
    {
        return FileHandle { handle, lockMode };
    }

    WTF_EXPORT_PRIVATE FileHandle(FileHandle&&);
    WTF_EXPORT_PRIVATE FileHandle& operator=(FileHandle&&);

    WTF_EXPORT_PRIVATE ~FileHandle();

    PlatformFileHandle platformHandle() const { return m_handle.unsafeValue(); }
    bool isValid() const { return !!m_handle; }
    explicit operator bool() const { return isValid(); }

    WTF_EXPORT_PRIVATE std::optional<uint64_t> write(std::span<const uint8_t>);
    WTF_EXPORT_PRIVATE std::optional<uint64_t> read(std::span<uint8_t>);
    WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> readAll();
    WTF_EXPORT_PRIVATE bool truncate(int64_t offset);
    WTF_EXPORT_PRIVATE std::optional<uint64_t> size();

    WTF_EXPORT_PRIVATE std::optional<uint64_t> seek(int64_t offset, FileSeekOrigin);

    WTF_EXPORT_PRIVATE bool flush();
    WTF_EXPORT_PRIVATE std::optional<PlatformFileID> id();

    // Appends the contents of the file found at 'path' to the FileHandle.
    // Returns true if the write was successful, false if it was not.
    WTF_EXPORT_PRIVATE bool appendFileContents(const String& path);

    WTF_EXPORT_PRIVATE std::optional<MappedFileData> map(MappedFileMode);
    WTF_EXPORT_PRIVATE std::optional<MappedFileData> map(MappedFileMode, FileOpenMode);

private:
    WTF_EXPORT_PRIVATE FileHandle(PlatformFileHandle, OptionSet<FileLockMode>);

#if USE(FILE_LOCK)
    bool lock(OptionSet<FileLockMode>);
#endif
    void close();

    Markable<PlatformFileHandle, PlatformHandleTraits> m_handle;
};

} // namespace FileSystemImpl

} // namespace WTF

namespace FileSystem = WTF::FileSystemImpl;
