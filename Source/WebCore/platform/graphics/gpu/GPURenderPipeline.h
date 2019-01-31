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

#include "GPURenderPipelineDescriptor.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if USE(METAL)
OBJC_PROTOCOL(MTLDepthStencilState);
OBJC_PROTOCOL(MTLRenderPipelineState);
#endif // USE(METAL)

namespace WebCore {

class GPUDevice;

using PlatformRenderPipeline = MTLRenderPipelineState;
using PlatformRenderPipelineSmartPtr = RetainPtr<MTLRenderPipelineState>;
using PrimitiveTopology = GPURenderPipelineDescriptor::PrimitiveTopology;

class GPURenderPipeline : public RefCounted<GPURenderPipeline> {
public:
    static RefPtr<GPURenderPipeline> create(const GPUDevice&, GPURenderPipelineDescriptor&&);

#if USE(METAL)
    MTLDepthStencilState *depthStencilState() const { return m_depthStencilState.get(); }
#endif
    PlatformRenderPipeline* platformRenderPipeline() const { return m_platformRenderPipeline.get(); }
    PrimitiveTopology primitiveTopology() const { return m_primitiveTopology; }

private:
#if USE(METAL)
    GPURenderPipeline(RetainPtr<MTLDepthStencilState>&&, PlatformRenderPipelineSmartPtr&&, GPURenderPipelineDescriptor&&);

    RetainPtr<MTLDepthStencilState> m_depthStencilState;
#endif // USE(METAL)
    PlatformRenderPipelineSmartPtr m_platformRenderPipeline;
    RefPtr<GPUPipelineLayout> m_layout;
    PrimitiveTopology m_primitiveTopology;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
