/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(WHLSL_COMPILER)
#include "WHLSLPrepare.h"
#endif

#if USE(METAL)
#include <wtf/RetainPtr.h>
#endif

#if USE(METAL)
OBJC_PROTOCOL(MTLComputePipelineState);
#endif

namespace WebCore {

class GPUDevice;
class GPUErrorScopes;
class GPUPipelineLayout;

struct GPUComputePipelineDescriptor;

#if USE(METAL)
using PlatformComputePipeline = MTLComputePipelineState;
using PlatformComputePipelineSmartPtr = RetainPtr<MTLComputePipelineState>;
#endif

class GPUComputePipeline final : public GPUPipeline {
public:
    virtual ~GPUComputePipeline();

    static RefPtr<GPUComputePipeline> tryCreate(const GPUDevice&, const GPUComputePipelineDescriptor&, GPUErrorScopes&);

    bool isComputePipeline() const { return true; }

    bool recompile(const GPUDevice&, GPUProgrammableStageDescriptor&& computeStage);

    const PlatformComputePipeline* platformComputePipeline() const { return m_platformComputePipeline.get(); }

#if ENABLE(WHLSL_COMPILER)
    WHLSL::ComputeDimensions computeDimensions() const { return m_computeDimensions; }
#endif

private:
#if ENABLE(WHLSL_COMPILER)
    GPUComputePipeline(PlatformComputePipelineSmartPtr&&, WHLSL::ComputeDimensions, const RefPtr<GPUPipelineLayout>&);
#endif

    PlatformComputePipelineSmartPtr m_platformComputePipeline;
#if ENABLE(WHLSL_COMPILER)
    WHLSL::ComputeDimensions m_computeDimensions { 0, 0, 0 };
#endif

    // Preserved for Web Inspector recompilation.
    RefPtr<GPUPipelineLayout> m_layout;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GPUPIPELINE(WebCore::GPUComputePipeline, isComputePipeline())

#endif // ENABLE(WEBGPU)
