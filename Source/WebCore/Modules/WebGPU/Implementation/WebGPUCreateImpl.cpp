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
#include "WebGPUCreateImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "ProcessIdentity.h"
#include "WebGPUImpl.h"
#include <WebCore/WebGPUCppAPI.h>

#if PLATFORM(COCOA)
#include <wtf/darwin/WeakLinking.h>

namespace WebGPU {
WTF_WEAK_LINK_FORCE_IMPORT(createInstance);
}
#endif

namespace WebCore {

RefPtr<WebGPUIntegration> createWebGPUIntegration(WebGPUScheduleWorkFunction&& scheduleWorkFunction, const ProcessIdentity* webProcessIdentity)
{
#if !HAVE(TASK_IDENTITY_TOKEN)
    UNUSED_PARAM(webProcessIdentity);
#endif
    ::WebGPU::InstanceDescriptor descriptor {
        .scheduleWork = WTF::move(scheduleWorkFunction),
#if HAVE(TASK_IDENTITY_TOKEN)
        .webProcessResourceOwner = webProcessIdentity ? std::optional { webProcessIdentity->taskId() } : std::nullopt,
#elif PLATFORM(COCOA)
        .webProcessResourceOwner = std::nullopt,
#endif
    };

    if (!&::WebGPU::createInstance)
        return nullptr;
    RefPtr instance = ::WebGPU::createInstance(WTF::move(descriptor));
    if (!instance)
        return nullptr;
    return WebGPUIntegrationImpl::create(instance.releaseNonNull());
}

} // namespace WebCore

#endif // HAVE(WEBGPU_IMPLEMENTATION)
