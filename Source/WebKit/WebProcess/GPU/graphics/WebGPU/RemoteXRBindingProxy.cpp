/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "RemoteXRBindingProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteDeviceProxy.h"
#include "RemoteGPUProxy.h"
#include "RemoteXRBindingMessages.h"
#include "RemoteXRProjectionLayerProxy.h"
#include "RemoteXRSubImageProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/WebGPUCppAPI.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteXRBindingProxy);

RemoteXRBindingProxy::RemoteXRBindingProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteXRBindingProxy::~RemoteXRBindingProxy()
{
    auto sendResult = send(Messages::RemoteXRBinding::Destruct());
    UNUSED_VARIABLE(sendResult);
}

RefPtr<::WebGPU::XRProjectionLayer> RemoteXRBindingProxy::createProjectionLayer(const ::WebGPU::XRProjectionLayerDescriptor& descriptor)
{
    auto identifier = WebGPUIdentifier::generate();

    auto sendResult = send(Messages::RemoteXRBinding::CreateProjectionLayer(descriptor.colorFormat, descriptor.depthStencilFormat, descriptor.textureUsage, descriptor.scaleFactor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteXRProjectionLayerProxy::create(protect(root()), m_convertToBackingContext, identifier);
    return result;
}

RefPtr<::WebGPU::XRSubImage> RemoteXRBindingProxy::getViewSubImage(::WebGPU::XRProjectionLayer& projectionLayer)
{
    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteXRBinding::GetViewSubImage(downcast<RemoteXRProjectionLayerProxy>(projectionLayer).backing(), identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteXRSubImageProxy::create(protect(root()), m_convertToBackingContext, identifier);
    return result;
}

bool RemoteXRBindingProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
