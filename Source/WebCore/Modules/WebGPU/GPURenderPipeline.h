/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "GPUBindGroupLayout.h"
#include "GPUShaderModule.h"
#include "WebGPURenderPipeline.h"
#include <cstdint>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPURenderPipeline : public RefCountedAndCanMakeWeakPtr<GPURenderPipeline> {
public:
    static Ref<GPURenderPipeline> create(Ref<WebGPU::RenderPipeline>&& backing, uint64_t uniqueId, Ref<GPUShaderModule>&& vertexModule, String&& vertexEntryPoint, RefPtr<GPUShaderModule>&& fragmentModule, String&& fragmentEntryPoint)
    {
        return adoptRef(*new GPURenderPipeline(WTF::move(backing), uniqueId, WTF::move(vertexModule), WTF::move(vertexEntryPoint), WTF::move(fragmentModule), WTF::move(fragmentEntryPoint)));
    }

    String NODELETE label() const;
    void setLabel(String&&);

    Ref<GPUBindGroupLayout> getBindGroupLayout(uint32_t index);

    WebGPU::RenderPipeline& backing() { return m_backing; }
    const WebGPU::RenderPipeline& backing() const { return m_backing; }

    // Shader modules that this pipeline was created from. Retained for Web Inspector.
    GPUShaderModule& vertexModule() { return m_vertexModule; }
    const String& vertexEntryPoint() const LIFETIME_BOUND { return m_vertexEntryPoint; }
    GPUShaderModule* fragmentModule() { return m_fragmentModule.get(); }
    const String& fragmentEntryPoint() const LIFETIME_BOUND { return m_fragmentEntryPoint; }

    bool sharesVertexFragmentModule() const { return m_fragmentModule && m_fragmentModule.get() == m_vertexModule.ptr(); }

private:
    GPURenderPipeline(Ref<WebGPU::RenderPipeline>&& backing, uint64_t uniqueId, Ref<GPUShaderModule>&& vertexModule, String&& vertexEntryPoint, RefPtr<GPUShaderModule>&& fragmentModule, String&& fragmentEntryPoint)
        : m_backing(WTF::move(backing))
        , m_uniqueId(uniqueId)
        , m_vertexModule(WTF::move(vertexModule))
        , m_vertexEntryPoint(WTF::move(vertexEntryPoint))
        , m_fragmentModule(WTF::move(fragmentModule))
        , m_fragmentEntryPoint(WTF::move(fragmentEntryPoint))
    {
    }

    const Ref<WebGPU::RenderPipeline> m_backing;
    const uint64_t m_uniqueId;
    const Ref<GPUShaderModule> m_vertexModule;
    const String m_vertexEntryPoint;
    const RefPtr<GPUShaderModule> m_fragmentModule;
    const String m_fragmentEntryPoint;
};

}
