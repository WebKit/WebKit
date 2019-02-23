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
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

OBJC_PROTOCOL(MTLBuffer);

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class GPUDevice;

struct GPUBufferDescriptor;

using PlatformBuffer = MTLBuffer;
using PlatformBufferSmartPtr = RetainPtr<MTLBuffer>;

class GPUBuffer : public RefCounted<GPUBuffer> {
public:
    ~GPUBuffer();

    static RefPtr<GPUBuffer> tryCreate(const GPUDevice&, GPUBufferDescriptor&&);

    PlatformBuffer *platformBuffer() const { return m_platformBuffer.get(); }
    bool isVertex() const { return m_isVertex; }
    bool isUniform() const { return m_isUniform; }
    bool isStorage() const { return m_isStorage; }
    bool isReadOnly() const { return m_isReadOnly; }

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

    GPUBuffer(PlatformBufferSmartPtr&&, const GPUBufferDescriptor&);

    static RefPtr<GPUBuffer> tryCreateSharedBuffer(const GPUDevice&, const GPUBufferDescriptor&);
    JSC::ArrayBuffer* stagingBufferForRead();
    JSC::ArrayBuffer* stagingBufferForWrite();

    bool isMappable() const { return m_isMapWrite || m_isMapRead; }
    bool isMapWriteable() const { return m_isMapWrite && !m_pendingCallback; }
    bool isMapReadable() const { return m_isMapRead && !m_pendingCallback; }

    PlatformBufferSmartPtr m_platformBuffer;

    RefPtr<JSC::ArrayBuffer> m_stagingBuffer;
    RefPtr<PendingMappingCallback> m_pendingCallback;
    DeferrableTask<Timer> m_mappingCallbackTask;

    unsigned long m_byteLength;
    unsigned m_numScheduledCommandBuffers = 0;
    bool m_isMapWrite;
    bool m_isMapRead;
    bool m_isDestroyed = false;
    bool m_isVertex;
    bool m_isUniform;
    bool m_isStorage;
    bool m_isReadOnly;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
