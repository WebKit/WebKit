/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "WebGPUObjectRegistry.h"
#include <functional>
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class RemoteAdapter;
class RemoteBindGroup;
class RemoteBindGroupLayout;
class RemoteBuffer;
class RemoteCommandBuffer;
class RemoteCommandEncoder;
class RemoteComputePassEncoder;
class RemoteComputePipeline;
class RemoteDevice;
class RemoteExternalTexture;
class RemoteGPU;
class RemotePipelineLayout;
class RemoteQuerySet;
class RemoteQueue;
class RemoteRenderBundleEncoder;
class RemoteRenderBundle;
class RemoteRenderPassEncoder;
class RemoteRenderPipeline;
class RemoteSampler;
class RemoteShaderModule;
class RemoteTexture;
class RemoteTextureView;
}

namespace WebKit::WebGPU {

class ObjectHeap final : public CanMakeWeakPtr<ObjectHeap> {
public:
    ObjectHeap();
    ~ObjectHeap();

    void addObject(RemoteAdapter&);
    void addObject(RemoteBindGroup&);
    void addObject(RemoteBindGroupLayout&);
    void addObject(RemoteBuffer&);
    void addObject(RemoteCommandBuffer&);
    void addObject(RemoteCommandEncoder&);
    void addObject(RemoteComputePassEncoder&);
    void addObject(RemoteComputePipeline&);
    void addObject(RemoteDevice&);
    void addObject(RemoteExternalTexture&);
    void addObject(RemoteGPU&);
    void addObject(RemotePipelineLayout&);
    void addObject(RemoteQuerySet&);
    void addObject(RemoteQueue&);
    void addObject(RemoteRenderBundleEncoder&);
    void addObject(RemoteRenderBundle&);
    void addObject(RemoteRenderPassEncoder&);
    void addObject(RemoteRenderPipeline&);
    void addObject(RemoteSampler&);
    void addObject(RemoteShaderModule&);
    void addObject(RemoteTexture&);
    void addObject(RemoteTextureView&);

    void removeObject(RemoteAdapter&);
    void removeObject(RemoteBindGroup&);
    void removeObject(RemoteBindGroupLayout&);
    void removeObject(RemoteBuffer&);
    void removeObject(RemoteCommandBuffer&);
    void removeObject(RemoteCommandEncoder&);
    void removeObject(RemoteComputePassEncoder&);
    void removeObject(RemoteComputePipeline&);
    void removeObject(RemoteDevice&);
    void removeObject(RemoteExternalTexture&);
    void removeObject(RemoteGPU&);
    void removeObject(RemotePipelineLayout&);
    void removeObject(RemoteQuerySet&);
    void removeObject(RemoteQueue&);
    void removeObject(RemoteRenderBundleEncoder&);
    void removeObject(RemoteRenderBundle&);
    void removeObject(RemoteRenderPassEncoder&);
    void removeObject(RemoteRenderPipeline&);
    void removeObject(RemoteSampler&);
    void removeObject(RemoteShaderModule&);
    void removeObject(RemoteTexture&);
    void removeObject(RemoteTextureView&);

private:
    using Object = std::variant<
        std::monostate,
        Ref<RemoteAdapter>,
        Ref<RemoteBindGroup>,
        Ref<RemoteBindGroupLayout>,
        Ref<RemoteBuffer>,
        Ref<RemoteCommandBuffer>,
        Ref<RemoteCommandEncoder>,
        Ref<RemoteComputePassEncoder>,
        Ref<RemoteComputePipeline>,
        Ref<RemoteDevice>,
        Ref<RemoteExternalTexture>,
        Ref<RemoteGPU>,
        Ref<RemotePipelineLayout>,
        Ref<RemoteQuerySet>,
        Ref<RemoteQueue>,
        Ref<RemoteRenderBundleEncoder>,
        Ref<RemoteRenderBundle>,
        Ref<RemoteRenderPassEncoder>,
        Ref<RemoteRenderPipeline>,
        Ref<RemoteSampler>,
        Ref<RemoteShaderModule>,
        Ref<RemoteTexture>,
        Ref<RemoteTextureView>
    >;
    HashMap<void*, Object> m_objects;

    ObjectRegistry m_objectRegistry;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
