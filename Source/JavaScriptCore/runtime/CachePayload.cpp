/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CachePayload.h"

namespace JSC {

CachePayload CachePayload::makeMappedPayload(FileSystem::MappedFileData&& data)
{
    return CachePayload(WTFMove(data));
}

CachePayload CachePayload::makeMallocPayload(MallocPtr<uint8_t, VMMalloc>&& data, size_t size)
{
    return CachePayload(std::pair { WTFMove(data), size });
}

CachePayload CachePayload::makeEmptyPayload()
{
    return CachePayload(std::pair { nullptr, 0 });
}

CachePayload::CachePayload(CachePayload&& other)
{
    m_data = WTFMove(other.m_data);
    other.m_data = std::pair { nullptr, 0 };
}

CachePayload::CachePayload(std::variant<FileSystem::MappedFileData, std::pair<MallocPtr<uint8_t, VMMalloc>, size_t>>&& data)
    : m_data(WTFMove(data))
{
}

CachePayload::~CachePayload() = default;

const uint8_t* CachePayload::data() const
{
    return WTF::switchOn(m_data,
        [](const FileSystem::MappedFileData& data) {
            return static_cast<const uint8_t*>(data.data());
        }, [](const std::pair<MallocPtr<uint8_t, VMMalloc>, size_t>& data) -> const uint8_t* {
            return data.first.get();
        }
    );
}

size_t CachePayload::size() const
{
    return WTF::switchOn(m_data,
        [](const FileSystem::MappedFileData& data) -> size_t {
            return data.size();
        }, [](const std::pair<MallocPtr<uint8_t, VMMalloc>, size_t>& data) -> size_t {
            return data.second;
        }
    );
}

} // namespace JSC
