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

namespace WebKit::WebGPU {

RemoteTextureProxy::RemoteTextureProxy(RemoteGPUProxy& root, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(root)
{
}

RemoteTextureProxy::~RemoteTextureProxy()
{
}

Ref<PAL::WebGPU::TextureView> RemoteTextureProxy::createView(const std::optional<PAL::WebGPU::TextureViewDescriptor>& descriptor)
{
    std::optional<TextureViewDescriptor> convertedDescriptor;
    if (descriptor) {
        convertedDescriptor = m_convertToBackingContext->convertToBacking(*descriptor);
        if (!convertedDescriptor) {
            // FIXME: Implement error handling.
            return RemoteTextureViewProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
        }
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteTexture::CreateView(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteTextureViewProxy::create(*this, m_convertToBackingContext, identifier);
}

void RemoteTextureProxy::destroy()
{
    auto sendResult = send(Messages::RemoteTexture::Destroy());
    UNUSED_VARIABLE(sendResult);
}

void RemoteTextureProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteTexture::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
