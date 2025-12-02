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

#include <span>
#include <wtf/Forward.h>

#if HAVE(MMAP)
#include <wtf/MallocSpan.h>
#include <wtf/MmapSpan.h>
#endif

#if OS(WINDOWS)
#include <wtf/win/Win32Handle.h>
#endif

namespace WTF {

namespace FileSystemImpl {

enum class MappedFileMode : bool {
    Shared,
    Private,
};

class MappedFileData {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MappedFileData);
public:
    MappedFileData() = default;
    WTF_EXPORT_PRIVATE ~MappedFileData();

    MappedFileData(MappedFileData&&) = default;
    MappedFileData& operator=(MappedFileData&&) = default;

#if HAVE(MMAP)
    explicit MappedFileData(MmapSpan<uint8_t>&&);

    std::span<uint8_t> leakHandle() WARN_UNUSED_RETURN { return m_fileData.leakSpan(); }
    explicit operator bool() const { return !!m_fileData; }
    size_t size() const { return m_fileData.span().size(); }
    std::span<const uint8_t> span() const LIFETIME_BOUND { return m_fileData.span(); }
    std::span<uint8_t> mutableSpan() LIFETIME_BOUND { return m_fileData.mutableSpan(); }
#elif OS(WINDOWS)
    MappedFileData(std::span<uint8_t>, Win32Handle&&);

    const Win32Handle& fileMapping() const { return m_fileMapping; }
    explicit operator bool() const { return !!m_fileData.data(); }
    size_t size() const { return m_fileData.size(); }
    std::span<const uint8_t> span() const { return m_fileData; }
    std::span<uint8_t> mutableSpan() { return m_fileData; }
#endif

private:
#if HAVE(MMAP)
    MmapSpan<uint8_t> m_fileData;
#elif OS(WINDOWS)
    std::span<uint8_t> m_fileData;
    Win32Handle m_fileMapping;
#endif
};

} // namespace FileSystemImpl

} // namespace WTF

namespace FileSystem = WTF::FileSystemImpl;
