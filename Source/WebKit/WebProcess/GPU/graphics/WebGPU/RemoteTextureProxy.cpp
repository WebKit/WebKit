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
#include "RemoteTextureProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteTextureMessages.h"
#include "RemoteTextureViewProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUTextureViewDescriptor.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteTextureProxy);

RemoteTextureProxy::RemoteTextureProxy(Ref<RemoteGPUProxy>&& root, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, bool isCanvasBacking)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(WTFMove(root))
    , m_isCanvasBacking(isCanvasBacking)
{
}

RemoteTextureProxy::~RemoteTextureProxy()
{
    auto sendResult = send(Messages::RemoteTexture::Destruct());
    UNUSED_VARIABLE(sendResult);
}

static bool equalDescriptors(const std::optional<WebCore::WebGPU::TextureViewDescriptor>& a, const std::optional<WebCore::WebGPU::TextureViewDescriptor>& b)
{
    if (!a && !b)
        return true;

    if (!a || !b)
        return false;

    return a->format == b->format
        && a->dimension == b->dimension
        && a->aspect == b->aspect
        && a->baseMipLevel == b->baseMipLevel
        && a->mipLevelCount == b->mipLevelCount
        && a->baseArrayLayer == b->baseArrayLayer
        && a->arrayLayerCount == b->arrayLayerCount;
}

RefPtr<WebCore::WebGPU::TextureView> RemoteTextureProxy::createView(const std::optional<WebCore::WebGPU::TextureViewDescriptor>& descriptor)
{
    if (m_isCanvasBacking && m_lastCreatedView && equalDescriptors(descriptor, m_lastCreatedViewDescriptor))
        return m_lastCreatedView;

    std::optional<TextureViewDescriptor> convertedDescriptor;
    Ref convertToBackingContext = m_convertToBackingContext;

    if (descriptor) {
        convertedDescriptor = convertToBackingContext->convertToBacking(*descriptor);
        if (!convertedDescriptor)
            return nullptr;
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteTexture::CreateView(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteTextureViewProxy::create(*this, convertToBackingContext, identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    if (!m_isCanvasBacking)
        return result;

    m_lastCreatedView = WTFMove(result);
    m_lastCreatedViewDescriptor = descriptor;
    return m_lastCreatedView;
}

void RemoteTextureProxy::destroy()
{
    auto sendResult = send(Messages::RemoteTexture::Destroy());
    UNUSED_VARIABLE(sendResult);
}

void RemoteTextureProxy::undestroy()
{
    auto sendResult = send(Messages::RemoteTexture::Undestroy());
    UNUSED_VARIABLE(sendResult);
}

void RemoteTextureProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteTexture::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
