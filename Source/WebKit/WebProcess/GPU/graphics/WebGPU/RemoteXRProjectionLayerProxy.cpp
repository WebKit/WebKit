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
#include "RemoteXRProjectionLayerProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteGPUProxy.h"
#include "RemoteXRProjectionLayerMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/PlatformXR.h>
#include <WebCore/WebGPUTextureFormat.h>
#include <wtf/MachSendRight.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteXRProjectionLayerProxy);

RemoteXRProjectionLayerProxy::RemoteXRProjectionLayerProxy(Ref<RemoteGPUProxy>&& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(WTFMove(parent))
{
}

RemoteXRProjectionLayerProxy::~RemoteXRProjectionLayerProxy()
{
    auto sendResult = send(Messages::RemoteXRProjectionLayer::Destruct());
    UNUSED_VARIABLE(sendResult);
}

#if PLATFORM(COCOA)
void RemoteXRProjectionLayerProxy::startFrame(size_t frameIndex, MachSendRight&& colorBuffer, MachSendRight&& depthBuffer, MachSendRight&& completionSyncEvent, size_t reusableTextureIndex)
{
    auto sendResult = send(Messages::RemoteXRProjectionLayer::StartFrame(frameIndex, WTFMove(colorBuffer), WTFMove(depthBuffer), WTFMove(completionSyncEvent), reusableTextureIndex));
    UNUSED_VARIABLE(sendResult);
}
#endif

void RemoteXRProjectionLayerProxy::endFrame()
{
    auto sendResult = send(Messages::RemoteXRProjectionLayer::EndFrame());
    UNUSED_VARIABLE(sendResult);
}

uint32_t RemoteXRProjectionLayerProxy::textureWidth() const
{
    return 0;
}

uint32_t RemoteXRProjectionLayerProxy::textureHeight() const
{
    return 0;
}

uint32_t RemoteXRProjectionLayerProxy::textureArrayLength() const
{
    return 0;
}

bool RemoteXRProjectionLayerProxy::ignoreDepthValues() const
{
    return false;
}

std::optional<float> RemoteXRProjectionLayerProxy::fixedFoveation() const
{
    return 0.f;
}

void RemoteXRProjectionLayerProxy::setFixedFoveation(std::optional<float>)
{
    return;
}

WebCore::WebXRRigidTransform* RemoteXRProjectionLayerProxy::deltaPose() const
{
    return nullptr;
}

void RemoteXRProjectionLayerProxy::setDeltaPose(WebCore::WebXRRigidTransform*)
{
    return;
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
