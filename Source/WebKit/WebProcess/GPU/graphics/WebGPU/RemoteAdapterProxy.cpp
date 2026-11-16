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
#include "RemoteAdapterProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapterMessages.h"
#include "RemoteDeviceProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAdapterProxy);

RemoteAdapterProxy::RemoteAdapterProxy(Vector<::WebGPU::FeatureName>&& features, const ::WebGPU::Limits& limits, ::WebGPU::AdapterInfo&& info, bool xrCompatible, RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_features(WTF::move(features))
    , m_limits(limits)
    , m_info(WTF::move(info))
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
    , m_xrCompatible(xrCompatible)
{
}

RemoteAdapterProxy::~RemoteAdapterProxy()
{
    auto sendResult = send(Messages::RemoteAdapter::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteAdapterProxy::requestDevice(const ::WebGPU::DeviceDescriptor& descriptor, CompletionHandler<void(RefPtr<::WebGPU::Device>&&)>&& callback)
{
    Ref convertToBackingContext = m_convertToBackingContext;
    auto convertedDescriptor = convertToBackingContext->convertToBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return callback(nullptr);

    auto identifier = WebGPUIdentifier::generate();
    auto queueIdentifier = WebGPUIdentifier::generate();
    auto sendResult = sendSync(Messages::RemoteAdapter::RequestDevice(*convertedDescriptor, identifier, queueIdentifier));
    if (!sendResult.succeeded())
        return callback(nullptr);

    auto [features, supportedLimits] = sendResult.takeReply();
    if (!supportedLimits.maxTextureDimension2D) {
        callback(nullptr);
        return;
    }

    auto result = RemoteDeviceProxy::create(WTF::move(features), supportedLimits, *this, convertToBackingContext, identifier, queueIdentifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    callback(WTF::move(result));
}

bool RemoteAdapterProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
