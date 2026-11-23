/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RemoteWebGPUInstanceProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapterProxy.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPURequestAdapterResponse.h"
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteWebGPUInstanceProxy);

RemoteWebGPUInstanceProxy::RemoteWebGPUInstanceProxy(RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteWebGPUInstanceProxy::~RemoteWebGPUInstanceProxy() = default;

void RemoteWebGPUInstanceProxy::requestAdapter(const ::WebGPU::RequestAdapterOptions& options, CompletionHandler<void(RefPtr<::WebGPU::Adapter>&&)>&& callback)
{
    Ref root = this->root();
    if (root->isLost()) {
        callback(nullptr);
        return;
    }

    auto identifier = WebGPUAdapterIdentifier::generate();
    auto sendResult = sendSync(Messages::RemoteGPU::RequestAdapter(options, identifier));
    if (!sendResult.succeeded()) {
        root->gpuProcessConnectionDidClose();
        callback(nullptr);
        return;
    }
    auto [response] = sendResult.takeReply();
    if (!response) {
        callback(nullptr);
        return;
    }

    ::WebGPU::AdapterInfo info {
        .name = WTF::move(response->name),
        .isFallbackAdapter = response->isFallbackAdapter,
        .subgroupMinSize = response->subgroupMinSize,
        .subgroupMaxSize = response->subgroupMaxSize,
        .hasUnifiedMemory = response->hasUnifiedMemory,
    };
    callback(RemoteAdapterProxy::create(WTF::move(response->features), response->limits, WTF::move(info), options.xrCompatible, *this, m_convertToBackingContext, identifier));
}

#if PLATFORM(COCOA)
RefPtr<::WebGPU::PresentationContext> RemoteWebGPUInstanceProxy::createPresentationContext(const ::WebGPU::PresentationContextDescriptor&)
{
    // The descriptor carries registerCompositorIntegration, a callback that the implementation
    // calls in its own process with the IOSurfaces of the render buffers, so it cannot be sent to
    // the GPU Process. The Web Process presents through
    // WebGPUIntegration::createPresentationContext() instead, whose descriptor has a compositor
    // integration proxy.
    return nullptr;
}
#endif

bool RemoteWebGPUInstanceProxy::isValid() const
{
    // The Web Process cannot know.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
