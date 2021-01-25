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
#include <wtf/FastMalloc.h>

namespace WebKit {

class DisplayListWriterHandle : public SharedDisplayListHandle {
    WTF_MAKE_NONCOPYABLE(DisplayListWriterHandle); WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<DisplayListWriterHandle> create(WebCore::DisplayList::ItemBufferIdentifier identifier, Ref<SharedMemory>&& sharedMemory)
    {
        return adoptRef(*new DisplayListWriterHandle(identifier, WTFMove(sharedMemory)));
    }

    size_t writableOffset() const { return m_writableOffset; }
    size_t availableCapacity() const;

    bool moveWritableOffsetToStartIfPossible();

    size_t advance(size_t amount);
    WebCore::DisplayList::ItemBufferHandle createHandle() const;

    bool tryToResume(SharedDisplayListHandle::ResumeReadingInformation&& info)
    {
        auto& header = this->header();
        header.resumeReadingInfo = WTFMove(info);
        return header.waitingStatus.compareExchangeWeak(SharedDisplayListHandle::WaitingStatus::Waiting, SharedDisplayListHandle::WaitingStatus::Resuming);
    }

private:
    DisplayListWriterHandle(WebCore::DisplayList::ItemBufferIdentifier identifier, Ref<SharedMemory>&& sharedMemory)
        : SharedDisplayListHandle(identifier, WTFMove(sharedMemory))
        , m_writableOffset(SharedDisplayListHandle::headerSize())
    {
    }

    size_t m_writableOffset { 0 };
};

} // namespace WebKit
