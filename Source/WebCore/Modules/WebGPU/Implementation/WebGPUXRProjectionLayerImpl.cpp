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

#include "PlatformXR.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUDevice.h"
#include "WebGPUTextureFormat.h"
#include "WebXRRigidTransform.h"
#include <wtf/MachSendRight.h>

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
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    return 1;
#else
    return 2;
#endif
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
#if ENABLE(WEBXR)
    return m_webXRRigidTransform.get();
#else
    return nullptr;
#endif
}

void XRProjectionLayerImpl::setDeltaPose(WebXRRigidTransform* pose)
{
#if ENABLE(WEBXR)
    m_webXRRigidTransform = pose;
#else
    UNUSED_PARAM(pose);
#endif
}

// WebXRLayer
void XRProjectionLayerImpl::startFrame(size_t frameIndex, MachSendRight&& colorBuffer, MachSendRight&& depthBuffer, MachSendRight&& completionSyncEvent, size_t reusableTextureIndex, PlatformXR::RateMapDescription&& rateMap)
{
#if ENABLE(WEBXR)
    wgpuXRProjectionLayerStartFrame(m_backing.get(), frameIndex, WTFMove(colorBuffer), WTFMove(depthBuffer), WTFMove(completionSyncEvent), reusableTextureIndex, rateMap.screenSize.width(), rateMap.screenSize.height(), WTFMove(rateMap.horizontalSamplesLeft), WTFMove(rateMap.horizontalSamplesRight), WTFMove(rateMap.verticalSamples));
#else
    UNUSED_PARAM(frameIndex);
    UNUSED_PARAM(colorBuffer);
    UNUSED_PARAM(depthBuffer);
    UNUSED_PARAM(completionSyncEvent);
    UNUSED_PARAM(reusableTextureIndex);
    UNUSED_PARAM(rateMap);
#endif
}

void XRProjectionLayerImpl::endFrame()
{
    return;
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
