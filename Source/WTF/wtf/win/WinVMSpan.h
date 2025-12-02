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

#include <wtf/AllocSpanMixin.h>
#include <wtf/win/Win32Handle.h>

namespace WTF {

// RAII class for allocating and holding Windows virtual memory with std::span access.
template<typename T> class WinVMSpan : public AllocSpanMixin<T> {
public:
    static WinVMSpan mapViewOfFileEx(HANDLE fileMappingObject, DWORD desiredAccess, DWORD fileOffsetHigh, DWORD fileOffsetLow, SIZE_T numberOfBytesToMap, LPVOID baseAddress)
    {
        void* data = ::MapViewOfFileEx(fileMappingObject, desiredAccess, fileOffsetHigh, fileOffsetLow, numberOfBytesToMap, baseAddress);
        if (!data)
            return { };
        return WinVMSpan { data, numberOfBytesToMap };
    }

    WinVMSpan() = default;
    WinVMSpan(WinVMSpan&& other)
        : AllocSpanMixin<T>(WTFMove(other))
    {
    }

    template<typename U>
    WinVMSpan(WinVMSpan<U>&& other) requires (std::is_same_v<T, uint8_t>)
        : AllocSpanMixin<T>(asWritableBytes(other.leakSpan()))
    {
    }

    ~WinVMSpan()
    {
        auto data = this->mutableSpan();
        if (!data.data())
            return;
        ::UnmapViewOfFile(m_data.data());
    }

    WinVMSpan& operator=(WinVMSpan&& other)
    {
        WinVMSpan ptr { WTFMove(other) };
        this->swap(ptr);
        return *this;
    }

private:
    using AllocSpanMixin<T>::AllocSpanMixin;
};

} // namespace WTF

using WTF::WinVMSpan;
