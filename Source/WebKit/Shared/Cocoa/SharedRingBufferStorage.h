/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if USE(MEDIATOOLBOX)

#include <wtf/Atomics.h>
#include "SharedMemory.h"
#include <WebCore/CARingBuffer.h>
#include <wtf/Function.h>

namespace WebCore {
class CARingBuffer;
}

namespace WebKit {

class ReadOnlySharedRingBufferStorage : public WebCore::CARingBufferStorage {
public:
    ReadOnlySharedRingBufferStorage() = default;
    explicit ReadOnlySharedRingBufferStorage(const SharedMemory::Handle&);

    SharedMemory* storage() const { return m_storage.get(); }

protected:
    // WebCore::CARingBufferStorage
    void allocate(size_t, const WebCore::CAAudioStreamDescription& format, size_t frameCount) override { }
    void deallocate() override { }
    void* data() final;
    void getCurrentFrameBounds(uint64_t& startTime, uint64_t& endTime) final;
    void setCurrentFrameBounds(uint64_t startFrame, uint64_t endFrame) override { }
    uint64_t currentStartFrame() const final { return m_startFrame; }
    uint64_t currentEndFrame() const final { return m_endFrame; }
    void updateFrameBounds() final;
    void flush() final;

    static constexpr unsigned boundsBufferSize { 32 };
    struct FrameBounds {
        std::pair<uint64_t, uint64_t> boundsBuffer[boundsBufferSize];
        Atomic<unsigned> boundsBufferIndex { 0 };
    };
    const FrameBounds* sharedFrameBounds() const;
    FrameBounds* sharedFrameBounds();

    RefPtr<SharedMemory> m_storage;
    uint64_t m_startFrame { 0 };
    uint64_t m_endFrame { 0 };
};

class SharedRingBufferStorage final : public ReadOnlySharedRingBufferStorage {
public:
    SharedRingBufferStorage(Function<void(SharedMemory*, const WebCore::CAAudioStreamDescription& format, size_t frameCount)>&& storageChangedHandler)
        : m_storageChangedHandler(WTFMove(storageChangedHandler))
    {
    }

    void invalidate() { m_storageChangedHandler = nullptr; }

private:
    // WebCore::CARingBufferStorage
    void allocate(size_t, const WebCore::CAAudioStreamDescription& format, size_t frameCount) final;
    void deallocate() final;
    void setCurrentFrameBounds(uint64_t startFrame, uint64_t endFrame) final;

    void setStorage(RefPtr<SharedMemory>&&, const WebCore::CAAudioStreamDescription& format, size_t frameCount);

    Function<void(SharedMemory*, const WebCore::CAAudioStreamDescription& format, size_t frameCount)> m_storageChangedHandler;
};

}

#endif
