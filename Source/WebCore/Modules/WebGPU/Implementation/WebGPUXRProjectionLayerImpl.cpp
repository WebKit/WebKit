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

#include "config.h"
#include "WebGPUXRProjectionLayerImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUConvertToBackingContext.h"
#include "WebGPUDevice.h"
#include "WebGPUTextureFormat.h"

namespace WebCore::WebGPU {

XRProjectionLayerImpl::XRProjectionLayerImpl(WebGPUPtr<WGPUXRProjectionLayer>&& projectionLayer, ConvertToBackingContext& convertToBackingContext)
    : m_backing(WTFMove(projectionLayer))
    , m_convertToBackingContext(convertToBackingContext)
{
}

XRProjectionLayerImpl::~XRProjectionLayerImpl() = default;

uint32_t XRProjectionLayerImpl::textureWidth() const
{
    return 0;
}

uint32_t XRProjectionLayerImpl::textureHeight() const
{
    return 0;
}

uint32_t XRProjectionLayerImpl::textureArrayLength() const
{
    return 0;
}

bool XRProjectionLayerImpl::ignoreDepthValues() const
{
    return false;
}

std::optional<float> XRProjectionLayerImpl::fixedFoveation() const
{
    return 0.f;
}

void XRProjectionLayerImpl::setFixedFoveation(std::optional<float>)
{
    return;
}

WebXRRigidTransform* XRProjectionLayerImpl::deltaPose() const
{
    return nullptr;
}

void XRProjectionLayerImpl::setDeltaPose(WebXRRigidTransform*)
{
    return;
}

// WebXRLayer
void XRProjectionLayerImpl::startFrame()
{
    return;
}

void XRProjectionLayerImpl::endFrame()
{
    return;
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
