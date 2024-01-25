/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DynamicContentScalingBifurcatedImageBuffer.h"

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)

#import "DynamicContentScalingImageBufferBackend.h"
#import <WebCore/BifurcatedGraphicsContext.h>
#import <WebCore/DynamicContentScalingDisplayList.h>
#import <wtf/MachSendRight.h>

namespace WebKit {

DynamicContentScalingBifurcatedImageBuffer::DynamicContentScalingBifurcatedImageBuffer(Parameters parameters, const WebCore::ImageBufferBackend::Info& backendInfo, const WebCore::ImageBufferCreationContext& creationContext, std::unique_ptr<WebCore::ImageBufferBackend>&& backend, WebCore::RenderingResourceIdentifier renderingResourceIdentifier)
    : ImageBuffer(parameters, backendInfo, creationContext, WTFMove(backend), renderingResourceIdentifier)
    , m_dynamicContentScalingBackend(DynamicContentScalingImageBufferBackend::create(ImageBuffer::backendParameters(parameters), creationContext))
{
}

WebCore::GraphicsContext& DynamicContentScalingBifurcatedImageBuffer::context() const
{
    if (!m_context)
        m_context = makeUnique<WebCore::BifurcatedGraphicsContext>(m_backend->context(), m_dynamicContentScalingBackend->context());
    return *m_context;
}

std::optional<WebCore::DynamicContentScalingDisplayList> DynamicContentScalingBifurcatedImageBuffer::dynamicContentScalingDisplayList()
{
    if (!m_dynamicContentScalingBackend)
        return std::nullopt;
    auto* sharing = static_cast<WebCore::ImageBufferBackend&>(*m_dynamicContentScalingBackend).toBackendSharing();
    if (!is<ImageBufferBackendHandleSharing>(sharing))
        return std::nullopt;
    auto handle = downcast<ImageBufferBackendHandleSharing>(*sharing).takeBackendHandle();
    if (!handle || !std::holds_alternative<WebCore::DynamicContentScalingDisplayList>(*handle))
        return std::nullopt;
    auto& displayList = std::get<WebCore::DynamicContentScalingDisplayList>(*handle);

    // Avoid accumulating display lists; drop the current context and start fresh.
    releaseGraphicsContext();

    return { WTFMove(displayList) };
}

void DynamicContentScalingBifurcatedImageBuffer::releaseGraphicsContext()
{
    ImageBuffer::releaseGraphicsContext();

    m_dynamicContentScalingBackend->releaseGraphicsContext();
    m_context = nullptr;
}

}

#endif // ENABLE(RE_DYNAMIC_CONTENT_SCALING)
