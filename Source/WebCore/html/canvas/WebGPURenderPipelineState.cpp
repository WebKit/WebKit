/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebGPURenderPipelineState.h"

#if ENABLE(WEBGPU)

#include "GPURenderPipelineState.h"
#include "WebGPURenderPipelineDescriptor.h"
#include "WebGPURenderingContext.h"

namespace WebCore {

Ref<WebGPURenderPipelineState> WebGPURenderPipelineState::create(WebGPURenderingContext* context, WebGPURenderPipelineDescriptor* descriptor)
{
    return adoptRef(*new WebGPURenderPipelineState(context, descriptor));
}

WebGPURenderPipelineState::WebGPURenderPipelineState(WebGPURenderingContext* context, WebGPURenderPipelineDescriptor* descriptor)
    : WebGPUObject(context)
{
    if (!context || !descriptor)
        return;
    m_renderPipelineState = GPURenderPipelineState::create(context->device().get(), descriptor->renderPipelineDescriptor());
}

WebGPURenderPipelineState::~WebGPURenderPipelineState() = default;

String WebGPURenderPipelineState::label() const
{
    if (!m_renderPipelineState)
        return emptyString();

    return m_renderPipelineState->label();
}

void WebGPURenderPipelineState::setLabel(const String& label)
{
    if (!m_renderPipelineState)
        return;

    m_renderPipelineState->setLabel(label);
}

} // namespace WebCore

#endif
