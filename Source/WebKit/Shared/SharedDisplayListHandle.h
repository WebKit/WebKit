/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "SharedMemory.h"
#include <WebCore/DisplayList.h>
#include <wtf/Atomics.h>
#include <wtf/RefCounted.h>
#include <wtf/Threading.h>

namespace WebKit {

class SharedDisplayListHandle : public RefCounted<SharedDisplayListHandle> {
public:
    virtual ~SharedDisplayListHandle() = default;

    static constexpr size_t headerSize()
    {
        return roundUpToMultipleOf<alignof(std::max_align_t)>(sizeof(DisplayListSharedMemoryHeader));
    }

    SharedMemory& sharedMemory() { return m_sharedMemory.get(); }
    const SharedMemory& sharedMemory() const { return m_sharedMemory.get(); }

    WebCore::DisplayList::ItemBufferIdentifier identifier() const { return m_identifier; }
    uint8_t* data() const { return reinterpret_cast<uint8_t*>(sharedMemory().data()); }

    uint64_t unreadBytes()
    {
        return header().unreadBytes.load();
    }

    enum class WaitingStatus : uint8_t {
        NotWaiting,
        Waiting,
        Resuming
    };

    struct ResumeReadingInformation {
        uint64_t offset;
        uint64_t destination;
    };

protected:
    SharedDisplayListHandle(WebCore::DisplayList::ItemBufferIdentifier identifier, Ref<SharedMemory>&& sharedMemory)
        : m_identifier(identifier)
        , m_sharedMemory(WTFMove(sharedMemory))
    {
    }

    struct DisplayListSharedMemoryHeader {
        DisplayListSharedMemoryHeader() = delete;
        ~DisplayListSharedMemoryHeader() = delete;

        Atomic<uint64_t> unreadBytes;
        ResumeReadingInformation resumeReadingInfo;
        Atomic<WaitingStatus> waitingStatus;
    };

    const DisplayListSharedMemoryHeader& header() const { return *reinterpret_cast<const DisplayListSharedMemoryHeader*>(data()); }
    DisplayListSharedMemoryHeader& header() { return *reinterpret_cast<DisplayListSharedMemoryHeader*>(data()); }

private:
    WebCore::DisplayList::ItemBufferIdentifier m_identifier;
    Ref<SharedMemory> m_sharedMemory;
};

} // namespace WebKit
