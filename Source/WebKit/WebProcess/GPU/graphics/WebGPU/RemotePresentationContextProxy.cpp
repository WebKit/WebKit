/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "RemotePresentationContextProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemotePresentationContextMessages.h"
#include "RemoteTextureProxy.h"
#include "WebGPUCanvasConfiguration.h"
#include "WebGPUDescriptorSerializers.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemotePresentationContextProxy);

RemotePresentationContextProxy::RemotePresentationContextProxy(RemoteGPUProxy& parent, WebGPUPresentationContextIdentifier identifier)
    : m_backing(identifier)
    , m_parent(parent)
{
}

RemotePresentationContextProxy::~RemotePresentationContextProxy() = default;

void RemotePresentationContextProxy::configure(const ::WebGPU::CanvasConfiguration& canvasConfiguration)
{
    auto convertedConfiguration = serialize(canvasConfiguration);
    if (!convertedConfiguration)
        return;

    auto sendResult = send(Messages::RemotePresentationContext::Configure(*convertedConfiguration));
    UNUSED_VARIABLE(sendResult);

    m_configuredDevice = downcast<RemoteDeviceProxy>(canvasConfiguration.device);
    m_textureProperties = {
        .size = { canvasConfiguration.width, canvasConfiguration.height, 1 },
        .mipLevelCount = 1,
        .sampleCount = 1,
        .dimension = ::WebGPU::TextureDimension::_2d,
        .format = canvasConfiguration.format,
        .usage = canvasConfiguration.usage,
    };
}

void RemotePresentationContextProxy::unconfigure()
{
    for (size_t i = 0; i < textureCount; ++i)
        m_currentTexture[i] = nullptr;
    m_configuredDevice = nullptr;

    auto sendResult = send(Messages::RemotePresentationContext::Unconfigure());
    UNUSED_VARIABLE(sendResult);
}

RefPtr<::WebGPU::Texture> RemotePresentationContextProxy::getCurrentTexture(uint32_t frameIndex)
{
    if (frameIndex >= textureCount)
        return nullptr;

    if (!m_currentTexture[frameIndex]) {
        // A context that is not configured has no textures.
        RefPtr device = m_configuredDevice;
        if (!device)
            return nullptr;

        auto identifier = WebGPUTextureIdentifier::generate();
        auto sendResult = send(Messages::RemotePresentationContext::GetCurrentTexture(identifier, frameIndex));
        if (sendResult != IPC::Error::NoError)
            return nullptr;

        m_currentTexture[frameIndex] = RemoteTextureProxy::create(*device, identifier, m_textureProperties, true);
    } else
        protect(m_currentTexture[frameIndex])->undestroy();

    return m_currentTexture[frameIndex];
}

void RemotePresentationContextProxy::renderBufferWasReplaced(uint32_t frameIndex)
{
    if (frameIndex < textureCount)
        m_currentTexture[frameIndex] = nullptr;
}

void RemotePresentationContextProxy::present(uint32_t)
{
    RELEASE_ASSERT_NOT_REACHED();
}

Seconds RemotePresentationContextProxy::lastFrameGPUCost() const
{
    // RemoteCompositorIntegrationProxy reports the cost of a frame to the Web Process.
    RELEASE_ASSERT_NOT_REACHED();
}

Seconds RemotePresentationContextProxy::lastFramePresentStall() const
{
    // RemoteCompositorIntegrationProxy reports the present stall of a frame to the Web Process.
    RELEASE_ASSERT_NOT_REACHED();
}

#if PLATFORM(COCOA)
RetainPtr<CGImageRef> RemotePresentationContextProxy::getTextureAsNativeImage(uint32_t, bool&)
{
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

bool RemotePresentationContextProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
