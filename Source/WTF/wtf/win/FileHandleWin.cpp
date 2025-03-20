/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
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
#include "FileSystem.h"

#include <io.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sys/stat.h>
#include <windows.h>

namespace WTF::FileSystemImpl {

static std::optional<uint64_t> getFileSizeFromByHandleFileInformationStructure(const BY_HANDLE_FILE_INFORMATION& fileInformation)
{
    ULARGE_INTEGER fileSize;
    fileSize.HighPart = fileInformation.nFileSizeHigh;
    fileSize.LowPart = fileInformation.nFileSizeLow;

    if (fileSize.QuadPart > static_cast<ULONGLONG>(std::numeric_limits<int64_t>::max()))
        return std::nullopt;

    return fileSize.QuadPart;
}

std::optional<uint64_t> FileHandle::read(std::span<uint8_t> data)
{
    if (!m_handle)
        return { };

    DWORD bytesRead;
    bool success = ::ReadFile(*m_handle, data.data(), data.size(), &bytesRead, nullptr);

    if (!success)
        return { };
    return static_cast<uint64_t>(bytesRead);
}

std::optional<uint64_t> FileHandle::write(std::span<const uint8_t> data)
{
    if (!m_handle)
        return { };

    DWORD bytesWritten;
    bool success = WriteFile(*m_handle, data.data(), data.size(), &bytesWritten, nullptr);

    if (!success)
        return { };
    return static_cast<uint64_t>(bytesWritten);
}

bool FileHandle::flush()
{
    // Not implemented.
    return false;
}

bool FileHandle::truncate(int64_t offset)
{
    if (!m_handle)
        return false;

    FILE_END_OF_FILE_INFO eofInfo;
    eofInfo.EndOfFile.QuadPart = offset;

    return SetFileInformationByHandle(*m_handle, FileEndOfFileInfo, &eofInfo, sizeof(FILE_END_OF_FILE_INFO));
}

std::optional<uint64_t> FileHandle::seek(int64_t offset, FileSeekOrigin origin)
{
    if (!m_handle)
        return { };

    DWORD moveMethod = FILE_BEGIN;

    if (origin == FileSeekOrigin::Current)
        moveMethod = FILE_CURRENT;
    else if (origin == FileSeekOrigin::End)
        moveMethod = FILE_END;

    LARGE_INTEGER largeOffset;
    largeOffset.QuadPart = offset;

    largeOffset.LowPart = SetFilePointer(*m_handle, largeOffset.LowPart, &largeOffset.HighPart, moveMethod);

    if (largeOffset.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return { };

    return largeOffset.QuadPart;
}

std::optional<PlatformFileID> FileHandle::id()
{
    // FIXME (246118): Implement this function properly.
    return std::nullopt;
}

void FileHandle::close()
{
    if (auto handle = std::exchange(m_handle, std::nullopt))
        ::CloseHandle(*handle);
}

std::optional<uint64_t> FileHandle::size()
{
    if (!m_handle)
        return std::nullopt;

    BY_HANDLE_FILE_INFORMATION fileInformation;
    if (!::GetFileInformationByHandle(*m_handle, &fileInformation))
        return std::nullopt;

    return getFileSizeFromByHandleFileInformationStructure(fileInformation);
}

} // WTF::FileSystemImpl
