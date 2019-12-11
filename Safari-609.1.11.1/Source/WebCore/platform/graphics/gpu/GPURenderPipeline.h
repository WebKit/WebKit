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

#include "GPUPipeline.h"
#include "GPUProgrammableStageDescriptor.h"
#include "GPURenderPipelineDescriptor.h"
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if USE(METAL)
OBJC_PROTOCOL(MTLDepthStencilState);
OBJC_PROTOCOL(MTLRenderPipelineState);
#endif // USE(METAL)

namespace WebCore {

class GPUDevice;
class GPUErrorScopes;

using PlatformRenderPipeline = MTLRenderPipelineState;
using PlatformRenderPipelineSmartPtr = RetainPtr<MTLRenderPipelineState>;

class GPURenderPipeline final : public GPUPipeline {
public:
    virtual ~GPURenderPipeline();

    static RefPtr<GPURenderPipeline> tryCreate(const GPUDevice&, const GPURenderPipelineDescriptor&, GPUErrorScopes&);

    bool isRenderPipeline() const { return true; }

    bool recompile(const GPUDevice&, GPUProgrammableStageDescriptor&& vertexStage, Optional<GPUProgrammableStageDescriptor>&& fragmentStage);

#if USE(METAL)
    MTLDepthStencilState *depthStencilState() const { return m_depthStencilState.get(); }
#endif
    PlatformRenderPipeline* platformRenderPipeline() const { return m_platformRenderPipeline.get(); }
    GPUPrimitiveTopology primitiveTopology() const { return m_primitiveTopology; }
    Optional<GPUIndexFormat> indexFormat() const { return m_indexFormat; }

private:
#if USE(METAL)
    GPURenderPipeline(RetainPtr<MTLDepthStencilState>&&, PlatformRenderPipelineSmartPtr&&, GPUPrimitiveTopology, Optional<GPUIndexFormat>, const RefPtr<GPUPipelineLayout>&, const GPURenderPipelineDescriptorBase&);

    RetainPtr<MTLDepthStencilState> m_depthStencilState;
#endif // USE(METAL)
    PlatformRenderPipelineSmartPtr m_platformRenderPipeline;
    GPUPrimitiveTopology m_primitiveTopology;
    Optional<GPUIndexFormat> m_indexFormat;

    // Preserved for Web Inspector recompilation.
    RefPtr<GPUPipelineLayout> m_layout;
    GPURenderPipelineDescriptorBase m_renderDescriptorBase;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GPUPIPELINE(WebCore::GPURenderPipeline, isRenderPipeline())

#endif // ENABLE(WEBGPU)
