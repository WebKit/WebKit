/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "SharedMemory.h"

#include <wtf/RefPtr.h>

namespace WebCore {

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    auto handle = Win32Handle::adopt(::CreateFileMappingW(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, size, 0));
    if (!handle)
        return nullptr;
    auto data = WinVMSpan::mapViewOfFile(handle.get(), FILE_MAP_ALL_ACCESS, 0, 0, size, nullptr);
    ASSERT_WITH_MESSAGE(data, "::MapViewOfFileEx failed with error %lu %p", ::GetLastError(), handle.m_handle.get());
    if (!data)
        return nullptr;
    return adoptRef(*new SharedMemory(WTFMove(data), WTFMove(handle.m_handle), Protection::ReadWrite));
}

static DWORD accessRights(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::Protection::ReadOnly:
        return FILE_MAP_READ;
    case SharedMemory::Protection::ReadWrite:
        return FILE_MAP_READ | FILE_MAP_WRITE;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

RefPtr<SharedMemory> SharedMemory::map(Handle&& handleIn, Protection protection)
{
    auto data = WinVMSpan::mapViewOfFile(handleIn.m_handle.get(), accessRights(protection), 0, 0, size, nullptr);
    ASSERT_WITH_MESSAGE(data, "::MapViewOfFileEx failed with error %lu %p", ::GetLastError(), handleIn.m_handle.get());
    if (!data)
        return nullptr;
    auto handle = WTFMove(handleIn);
    return adoptRef(*new SharedMemory(WTFMove(data), WTFMove(handle.m_handle), protection));
}

SharedMemory::~SharedMemory() = default;

auto SharedMemory::createHandle(Protection protection) -> std::optional<Handle>
{
    HANDLE processHandle = ::GetCurrentProcess();

    HANDLE duplicatedHandle;
    if (!::DuplicateHandle(processHandle, m_handle.get(), processHandle, &duplicatedHandle, accessRights(protection), FALSE, 0))
        return std::nullopt;

    return { Handle(Win32Handle::adopt(duplicatedHandle), m_size) };
}

} // namespace WebCore
