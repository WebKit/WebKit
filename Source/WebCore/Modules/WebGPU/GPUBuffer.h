/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "GPUBufferMapState.h"
#include "GPUIntegralTypes.h"
#include "GPUMapMode.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMPromiseDeferredForward.h"
#include "WebGPUBuffer.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <cstdint>
#include <optional>
#include <wtf/HashSet.h>
#include <wtf/Range.h>
#include <wtf/RangeSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUDevice;

class GPUBuffer : public RefCounted<GPUBuffer> {
public:
    static Ref<GPUBuffer> create(Ref<WebGPU::Buffer>&& backing, size_t bufferSize, GPUBufferUsageFlags usage, bool mappedAtCreation, GPUDevice& device)
    {
        return adoptRef(*new GPUBuffer(WTFMove(backing), bufferSize, usage, mappedAtCreation, device));
    }

    String label() const;
    void setLabel(String&&);

    using MapAsyncPromise = DOMPromiseDeferred<IDLNull>;
    void mapAsync(GPUMapModeFlags, std::optional<GPUSize64> offset, std::optional<GPUSize64> sizeForMap, MapAsyncPromise&&);
    ExceptionOr<Ref<JSC::ArrayBuffer>> getMappedRange(std::optional<GPUSize64> offset, std::optional<GPUSize64> rangeSize);
    void unmap(ScriptExecutionContext&);

    void destroy(ScriptExecutionContext&);

    WebGPU::Buffer& backing() { return m_backing; }
    const WebGPU::Buffer& backing() const { return m_backing; }

    GPUSize64 size() const { return static_cast<GPUSize64>(m_bufferSize); }
    GPUBufferUsageFlags usage() const { return m_usage; }

    GPUBufferMapState mapState() const { return m_mapState; };

    ~GPUBuffer();
private:
    GPUBuffer(Ref<WebGPU::Buffer>&&, size_t, GPUBufferUsageFlags, bool, GPUDevice&);
    void internalUnmap(ScriptExecutionContext&);

    Ref<WebGPU::Buffer> m_backing;
    WebGPU::Buffer::MappedRange m_mappedRange;
    JSC::ArrayBuffer* m_arrayBuffer { nullptr };
    size_t m_bufferSize { 0 };
    size_t m_mappedRangeOffset { 0 };
    size_t m_mappedRangeSize { 0 };
    const GPUBufferUsageFlags m_usage { 0 };
    GPUBufferMapState m_mapState { GPUBufferMapState::Unmapped };
    std::optional<MapAsyncPromise> m_pendingMapPromise;
    GPUDevice& m_device;
    using MappedRanges = WTF::RangeSet<WTF::Range<size_t>>;
    MappedRanges m_mappedRanges;
    HashSet<size_t, DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>> m_mappedPoints;
    bool m_destroyed { false };
    bool m_mappedAtCreation { false };
};

}
