/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include "DeferrableTask.h"
#include "GPUBufferUsage.h"
#include <wtf/Function.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#if USE(METAL)
OBJC_PROTOCOL(MTLBuffer);
OBJC_PROTOCOL(MTLCommandBuffer);
#endif

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class GPUDevice;

struct GPUBufferDescriptor;

#if USE(METAL)
using PlatformBuffer = MTLBuffer;
#else
using PlatformBuffer = void;
#endif
using PlatformBufferSmartPtr = RetainPtr<PlatformBuffer>;

class GPUBuffer : public RefCounted<GPUBuffer> {
public:
    enum class State {
        Mapped,
        Unmapped,
        Destroyed
    };

    ~GPUBuffer();

    static RefPtr<GPUBuffer> tryCreate(Ref<GPUDevice>&&, GPUBufferDescriptor&&);

    PlatformBuffer *platformBuffer() const { return m_platformBuffer.get(); }
    unsigned long byteLength() const { return m_byteLength; }
    bool isTransferSource() const { return m_usage.contains(GPUBufferUsage::Flags::TransferSource); }
    bool isTransferDestination() const { return m_usage.contains(GPUBufferUsage::Flags::TransferDestination); }
    bool isVertex() const { return m_usage.contains(GPUBufferUsage::Flags::Vertex); }
    bool isUniform() const { return m_usage.contains(GPUBufferUsage::Flags::Uniform); }
    bool isStorage() const { return m_usage.contains(GPUBufferUsage::Flags::Storage); }
    bool isReadOnly() const;
    bool isMappable() const { return m_usage.containsAny({ GPUBufferUsage::Flags::MapWrite, GPUBufferUsage::Flags::MapRead }); }
    State state() const;

#if USE(METAL)
    void commandBufferCommitted(MTLCommandBuffer *);
    void commandBufferCompleted();

    void reuseSubDataBuffer(RetainPtr<MTLBuffer>&&);
#endif

    void setSubData(unsigned long, const JSC::ArrayBuffer&);
    using MappingCallback = WTF::Function<void(JSC::ArrayBuffer*)>;
    void registerMappingCallback(MappingCallback&&, bool);
    void unmap();
    void destroy();

private:
    struct PendingMappingCallback : public RefCounted<PendingMappingCallback> {
        static Ref<PendingMappingCallback> create(MappingCallback&& callback)
        {
            return adoptRef(*new PendingMappingCallback(WTFMove(callback)));
        }

        MappingCallback callback;

    private:
        PendingMappingCallback(MappingCallback&&);
    };

    static bool validateBufferUsage(const GPUDevice&, OptionSet<GPUBufferUsage::Flags>);

    GPUBuffer(PlatformBufferSmartPtr&&, const GPUBufferDescriptor&, OptionSet<GPUBufferUsage::Flags>, Ref<GPUDevice>&&);

    JSC::ArrayBuffer* stagingBufferForRead();
    JSC::ArrayBuffer* stagingBufferForWrite();
    void runMappingCallback();

    bool isMapWrite() const { return m_usage.contains(GPUBufferUsage::Flags::MapWrite); }
    bool isMapRead() const { return m_usage.contains(GPUBufferUsage::Flags::MapRead); }
    bool isMapWriteable() const { return isMapWrite() && state() == State::Unmapped; }
    bool isMapReadable() const { return isMapRead() && state() == State::Unmapped; }

    PlatformBufferSmartPtr m_platformBuffer;
    Ref<GPUDevice> m_device;

#if USE(METAL)
    Vector<RetainPtr<MTLBuffer>> m_subDataBuffers;
#endif

    RefPtr<JSC::ArrayBuffer> m_stagingBuffer;
    RefPtr<PendingMappingCallback> m_mappingCallback;
    DeferrableTask<Timer> m_mappingCallbackTask;

    unsigned long m_byteLength;
    OptionSet<GPUBufferUsage::Flags> m_usage;
    unsigned m_numScheduledCommandBuffers { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
