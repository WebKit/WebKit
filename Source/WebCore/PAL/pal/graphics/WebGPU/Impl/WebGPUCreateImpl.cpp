/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "WebGPUAdapterImpl.h"
#include "WebGPUDowncastConvertToBackingContext.h"
#include "WebGPUImpl.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>

#if PLATFORM(COCOA)
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(wgpuCreateInstance);
#endif

namespace PAL::WebGPU {

RefPtr<GPU> create(ScheduleWorkFunction&& scheduleWorkFunction)
{
    auto scheduleWorkBlock = makeBlockPtr([scheduleWorkFunction = WTFMove(scheduleWorkFunction)](WGPUWorkItem workItem)
    {
        scheduleWorkFunction(CompletionHandler<void(void)>(makeBlockPtr(WTFMove(workItem)), CompletionHandlerCallThread::AnyThread));
    });
    WGPUInstanceCocoaDescriptor cocoaDescriptor {
        {
            nullptr,
            static_cast<WGPUSType>(WGPUSTypeExtended_InstanceCocoaDescriptor),
        },
        scheduleWorkBlock.get(),
    };
    WGPUInstanceDescriptor descriptor = { &cocoaDescriptor.chain };

    if (!&wgpuCreateInstance)
        return nullptr;
    auto instance = wgpuCreateInstance(&descriptor);
    if (!instance)
        return nullptr;
    auto convertToBackingContext = DowncastConvertToBackingContext::create();
    return GPUImpl::create(instance, convertToBackingContext);
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
