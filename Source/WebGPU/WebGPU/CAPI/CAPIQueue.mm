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
#import "CAPIFutures.h"
#import "WebGPUCppConversions.h"

NODELETE void wgpuQueueAddRef(WGPUQueue queue)
{
    WebGPU::CAPI::fromAPI(queue).ref();
}

void wgpuQueueRelease(WGPUQueue queue)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(queue).deref();
    }
}

// Work that cannot complete, because the device is lost, is WGPUQueueWorkDoneStatus_Error.
WGPUFuture wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, WGPUQueueWorkDoneCallbackInfo callbackInfo)
{
    @autoreleasepool {
        Ref protectedQueue = WebGPU::CAPI::fromAPI(queue);
        RefPtr device = protectedQueue->device();
        WebGPU::CAPI::Future future { device ? device->instance() : nullptr };
        protectedQueue->onSubmittedWorkDone(CompletionHandler<void(bool)> { [callbackInfo, future](bool success) {
            if (success)
                callbackInfo.callback(WGPUQueueWorkDoneStatus_Success, WebGPU::CAPI::toAPI(""_s), callbackInfo.userdata1, callbackInfo.userdata2);
            else
                callbackInfo.callback(WGPUQueueWorkDoneStatus_Error, WebGPU::CAPI::toAPI("The device is lost"_s), callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        } });
        return future.future();
    }
}

void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer* commands)
{
    @autoreleasepool {
        Vector<Ref<WebGPU::CommandBuffer>> commandsToForward;
        for (auto& command : unsafeMakeSpan(commands, commandCount))
            commandsToForward.append(protect(WebGPU::CAPI::fromAPI(command)));
        protect(WebGPU::CAPI::fromAPI(queue))->submit(WTF::move(commandsToForward));
    }
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size)
{
    @autoreleasepool {
        auto bytes = unsafeMakeSpan(static_cast<const uint8_t*>(data), size);
        protect(WebGPU::CAPI::fromAPI(queue))->writeBuffer(protect(WebGPU::CAPI::fromAPI(buffer)), bufferOffset, spanConstCast<uint8_t>(bytes));
    }
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUTexelCopyTextureInfo* destination, const void* data, size_t dataSize, const WGPUTexelCopyBufferLayout* dataLayout, const WGPUExtent3D* writeSize)
{
    @autoreleasepool {
        auto bytes = unsafeMakeSpan(static_cast<const uint8_t*>(data), dataSize);
        Ref protectedQueue = WebGPU::CAPI::fromAPI(queue);
        auto apiDestination = WebGPU::CAPI::fromAPI(*destination);
        if (!apiDestination) {
            // No error once the device is gone.
            if (RefPtr device = protectedQueue->device())
                device->generateAValidationError("GPUQueue.writeTexture: the destination has a null texture or an invalid aspect"_s);
            return;
        }
        protectedQueue->writeTexture(*apiDestination, bytes, WebGPU::CAPI::fromAPI(*dataLayout), WebGPU::CAPI::fromAPI(*writeSize));
    }
}

void wgpuQueueSetLabel(WGPUQueue queue, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(queue))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
