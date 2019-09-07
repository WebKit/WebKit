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
class GPUErrorScopes;

struct GPUBufferDescriptor;

enum class GPUBufferMappedOption;

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

    static RefPtr<GPUBuffer> tryCreate(GPUDevice&, const GPUBufferDescriptor&, GPUBufferMappedOption, GPUErrorScopes&);

    PlatformBuffer *platformBuffer() const { return m_platformBuffer.get(); }
    size_t byteLength() const { return m_byteLength; }
    bool isCopySource() const { return m_usage.contains(GPUBufferUsage::Flags::CopySource); }
    bool isCopyDestination() const { return m_usage.contains(GPUBufferUsage::Flags::CopyDestination); }
    bool isIndex() const { return m_usage.contains(GPUBufferUsage::Flags::Index); }
    bool isVertex() const { return m_usage.contains(GPUBufferUsage::Flags::Vertex); }
    bool isUniform() const { return m_usage.contains(GPUBufferUsage::Flags::Uniform); }
    bool isStorage() const { return m_usage.contains(GPUBufferUsage::Flags::Storage); }
    bool isReadOnly() const;
    bool isMappable() const { return m_usage.containsAny({ GPUBufferUsage::Flags::MapWrite, GPUBufferUsage::Flags::MapRead }); }
    unsigned platformUsage() const { return m_platformUsage; }
    State state() const;

    JSC::ArrayBuffer* mapOnCreation();

#if USE(METAL)
    void commandBufferCommitted(MTLCommandBuffer *);
    void commandBufferCompleted();
#endif

    using MappingCallback = WTF::Function<void(JSC::ArrayBuffer*)>;
    void registerMappingCallback(MappingCallback&&, bool, GPUErrorScopes&);
    void unmap(GPUErrorScopes*);
    void destroy(GPUErrorScopes*);

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

    GPUBuffer(PlatformBufferSmartPtr&&, GPUDevice&, size_t, OptionSet<GPUBufferUsage::Flags>, GPUBufferMappedOption);
    static bool validateBufferUsage(const GPUDevice&, OptionSet<GPUBufferUsage::Flags>, GPUErrorScopes&);

    JSC::ArrayBuffer* stagingBufferForRead();
    JSC::ArrayBuffer* stagingBufferForWrite();
    void runMappingCallback();
    void copyStagingBufferToGPU(GPUErrorScopes*);

    bool isMapWrite() const { return m_usage.contains(GPUBufferUsage::Flags::MapWrite); }
    bool isMapRead() const { return m_usage.contains(GPUBufferUsage::Flags::MapRead); }
    bool isMapWriteable() const { return isMapWrite() && state() == State::Unmapped; }
    bool isMapReadable() const { return isMapRead() && state() == State::Unmapped; }

    PlatformBufferSmartPtr m_platformBuffer;
    Ref<GPUDevice> m_device;

    RefPtr<JSC::ArrayBuffer> m_stagingBuffer;
    RefPtr<PendingMappingCallback> m_mappingCallback;
    DeferrableTask<Timer> m_mappingCallbackTask;

    size_t m_byteLength;
    OptionSet<GPUBufferUsage::Flags> m_usage;
    unsigned m_platformUsage;
    unsigned m_numScheduledCommandBuffers { 0 };
    bool m_isMappedFromCreation { false };
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
