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

#include "SharedDisplayListHandle.h"
#include <wtf/Expected.h>
#include <wtf/FastMalloc.h>

namespace WebKit {

class DisplayListReaderHandle : public SharedDisplayListHandle {
    WTF_MAKE_NONCOPYABLE(DisplayListReaderHandle); WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<DisplayListReaderHandle> create(WebCore::DisplayList::ItemBufferIdentifier identifier, Ref<SharedMemory>&& sharedMemory)
    {
        if (sharedMemory->size() <= headerSize())
            return nullptr;
        return adoptRef(new DisplayListReaderHandle(identifier, WTFMove(sharedMemory)));
    }

    std::optional<size_t> advance(size_t amount);
    std::unique_ptr<WebCore::DisplayList::DisplayList> displayListForReading(size_t offset, size_t capacity, WebCore::DisplayList::ItemBufferReadingClient&) const;

    void startWaiting()
    {
        header().waitingStatus.store(static_cast<SharedDisplayListHandle::WaitingStatusStorageType>(SharedDisplayListHandle::WaitingStatus::Waiting));
    }

    enum class StopWaitingFailureReason : uint8_t { InvalidWaitingStatus };
    Expected<std::optional<SharedDisplayListHandle::ResumeReadingInformation>, StopWaitingFailureReason> stopWaiting()
    {
        auto& header = this->header();
        auto previousStatus = header.waitingStatus.exchange(static_cast<SharedDisplayListHandle::WaitingStatusStorageType>(SharedDisplayListHandle::WaitingStatus::NotWaiting));
        if (!isValidEnum<SharedDisplayListHandle::WaitingStatus>(previousStatus))
            return makeUnexpected(StopWaitingFailureReason::InvalidWaitingStatus);

        if (static_cast<SharedDisplayListHandle::WaitingStatus>(previousStatus) == SharedDisplayListHandle::WaitingStatus::Resuming)
            return { header.resumeReadingInfo };

        return { std::nullopt };
    }

private:
    DisplayListReaderHandle(WebCore::DisplayList::ItemBufferIdentifier identifier, Ref<SharedMemory>&& sharedMemory)
        : SharedDisplayListHandle(identifier, WTFMove(sharedMemory))
    {
    }
};

} // namespace WebKit
