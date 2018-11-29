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

#include "config.h"
#include "WebGPURenderPassEncoder.h"

#if ENABLE(WEBGPU)

#include "GPUProgrammablePassEncoder.h"
#include "GPURenderPassEncoder.h"

namespace WebCore {

Ref<WebGPURenderPassEncoder> WebGPURenderPassEncoder::create(Ref<WebGPUCommandBuffer>&& creator, Ref<GPURenderPassEncoder>&& encoder)
{
    return adoptRef(*new WebGPURenderPassEncoder(WTFMove(creator), WTFMove(encoder)));
}

WebGPURenderPassEncoder::WebGPURenderPassEncoder(Ref<WebGPUCommandBuffer>&& creator, Ref<GPURenderPassEncoder>&& encoder)
    : WebGPUProgrammablePassEncoder(WTFMove(creator))
    , m_passEncoder(WTFMove(encoder))
{
}

void WebGPURenderPassEncoder::draw(unsigned long vertexCount, unsigned long instanceCount, unsigned long firstVertex, unsigned long firstInstance)
{
    // FIXME: What kind of validation do we need to handle here?
    m_passEncoder->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

GPUProgrammablePassEncoder& WebGPURenderPassEncoder::passEncoder() const
{
    return m_passEncoder.get();
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
