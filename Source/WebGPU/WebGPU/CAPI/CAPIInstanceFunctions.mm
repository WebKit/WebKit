/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#import "CAPIConversions.h"
#import "PresentationContextCoreAnimation.h"
#import "WebGPUCppConversions.h"
#import <wtf/Borrow.h>

NODELETE void wgpuInstanceAddRef(WGPUInstance instance)
{
    WebGPU::CAPI::fromAPI(instance).ref();
}

void wgpuInstanceRelease(WGPUInstance instance)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(instance))->waitForCommandBufferCompletions();
        WebGPU::CAPI::metalFromAPI(instance).deref();
    }
}

// The required instance features are not checked: the instance has all of them. The instance has
// no scheduler: its callbacks run in wgpuInstanceProcessEvents() and wgpuInstanceWaitAny().
WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor*)
{
    @autoreleasepool {
        return WebGPU::CAPI::releaseToAPI(WebGPU::Metal::Instance::create({ }));
    }
}

NODELETE WGPUProc wgpuGetProcAddress(WGPUStringView)
{
    return nullptr;
}

// A surface presents into the CAMetalLayer of its WGPUSurfaceSourceMetalLayer. The other surface
// sources are not supported: the surface presents nowhere.
WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, const WGPUSurfaceDescriptor* descriptor)
{
    @autoreleasepool {
        if (auto* metalLayerSource = descriptor ? WebGPU::CAPI::findChainedStruct<WGPUSurfaceSourceMetalLayer>(descriptor->nextInChain) : nullptr) {
            if (CAMetalLayer *layer = (__bridge CAMetalLayer *)metalLayerSource->layer)
                return WebGPU::CAPI::releaseToAPI(WebGPU::Metal::PresentationContextCoreAnimation::create(layer));
        }
        return WebGPU::CAPI::releaseToAPI(protect(WebGPU::CAPI::fromAPI(instance))->createPresentationContext({ }));
    }
}

void wgpuInstanceProcessEvents(WGPUInstance instance)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(instance))->processEvents();
    }
}

// The C API reports an adapter that is not available with WGPURequestAdapterStatus_Unavailable
// and no adapter. Null options are the default ones.
WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options, WGPURequestAdapterCallbackInfo callbackInfo)
{
    @autoreleasepool {
        Ref protectedInstance = WebGPU::CAPI::fromAPI(instance);
        WebGPU::CAPI::Future future { protectedInstance.ptr() };
        auto callback = [callbackInfo, future](WGPURequestAdapterStatus status, WGPUAdapter adapter, ASCIILiteral message) {
            callbackInfo.callback(status, adapter, WebGPU::CAPI::toAPI(message), callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        };
        WGPURequestAdapterOptions defaultOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
        auto apiOptions = WebGPU::CAPI::fromAPI(options ? *options : defaultOptions);
        if (!apiOptions) {
            callback(WGPURequestAdapterStatus_Error, nullptr, "Unknown power preference"_s);
            return future.future();
        }
        protectedInstance->requestAdapter(*apiOptions, [callback = WTF::move(callback)](RefPtr<WebGPU::Adapter>&& adapter) {
            if (!adapter)
                return callback(WGPURequestAdapterStatus_Unavailable, nullptr, "No adapters present"_s);
            callback(WGPURequestAdapterStatus_Success, WebGPU::CAPI::releaseToAPI(WTF::move(adapter)), ""_s);
        });
        return future.future();
    }
}

// A timeout of UINT64_MAX never passes.
WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance instance, size_t futureCount, WGPUFutureWaitInfo* futures, uint64_t timeoutNS)
{
    @autoreleasepool {
        auto waitInfos = unsafeMakeSpan(futures, futureCount);
        auto futureIDs = WTF::map(waitInfos, [](auto& waitInfo) {
            return waitInfo.future.id;
        });
        auto timeout = timeoutNS == UINT64_MAX ? Seconds::infinity() : Seconds::fromNanoseconds(timeoutNS);
        bool completed = protect(WebGPU::CAPI::metalFromAPI(instance))->waitForAnyFuture(borrow(futureIDs)->span(), timeout, [&](size_t index) {
            waitInfos[index].completed = true;
        });
        return completed ? WGPUWaitStatus_Success : WGPUWaitStatus_TimedOut;
    }
}
