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

#if !OS(WINDOWS)
#include <sys/mman.h>
#endif

namespace JSC {

CachePayload CachePayload::makeMappedPayload(FileSystem::MappedFileData&& data)
{
    return CachePayload(true, data.leakHandle(), data.size());
}

CachePayload CachePayload::makeMallocPayload(MallocPtr<uint8_t, VMMalloc>&& data, size_t size)
{
    return CachePayload(false, data.leakPtr(), size);
}

CachePayload CachePayload::makeEmptyPayload()
{
    return CachePayload(true, nullptr, 0);
}

CachePayload::CachePayload(CachePayload&& other)
{
    m_mapped = other.m_mapped;
    m_size = other.m_size;
    m_data = other.m_data;
    other.m_mapped = false;
    other.m_data = nullptr;
    other.m_size = 0;
}

CachePayload::~CachePayload()
{
    freeData();
}

CachePayload& CachePayload::operator=(CachePayload&& other)
{
    ASSERT(&other != this);
    freeData();
    return *new (this) CachePayload(WTFMove(other));
}

void CachePayload::freeData()
{
    if (!m_data)
        return;
    if (m_mapped) {
        FileSystem::unmapViewOfFile(m_data, m_size);
    } else
        fastFree(m_data);
}

} // namespace JSC
