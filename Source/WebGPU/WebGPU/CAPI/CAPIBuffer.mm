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

NODELETE void wgpuBufferAddRef(WGPUBuffer buffer)
{
    WebGPU::CAPI::fromAPI(buffer).ref();
}

void wgpuBufferRelease(WGPUBuffer buffer)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(buffer).deref();
    }
}

void wgpuBufferDestroy(WGPUBuffer buffer)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(buffer))->destroy();
    }
}

WGPUBufferMapState wgpuBufferGetMapState(WGPUBuffer buffer)
{
    @autoreleasepool {
        switch (protect(WebGPU::CAPI::fromAPI(buffer))->mapState()) {
        case WebGPU::BufferMapState::Unmapped:
            return WGPUBufferMapState_Unmapped;
        case WebGPU::BufferMapState::Pending:
            return WGPUBufferMapState_Pending;
        case WebGPU::BufferMapState::Mapped:
            return WGPUBufferMapState_Mapped;
        }
    }
}

void* wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    @autoreleasepool {
        return protect(WebGPU::CAPI::fromAPI(buffer))->persistentMappedRange(offset, WebGPU::CAPI::mapSizeFromAPI(size)).data();
    }
}

const void* wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size)
{
    @autoreleasepool {
        return wgpuBufferGetMappedRange(buffer, offset, size);
    }
}

// mapAsync() validates only the Read and Write bits of the mode, as in the WebGPU specification, so the
// conversion drops unknown bits instead of failing.
static OptionSet<WebGPU::MapMode> mapModeFromAPIIgnoringUnknownBits(WGPUMapMode mode)
{
    return *WebGPU::CAPI::mapModeFromAPI(mode & (WGPUMapMode_Read | WGPUMapMode_Write));
}

WGPUFuture wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallbackInfo callbackInfo)
{
    @autoreleasepool {
        Ref protectedBuffer = WebGPU::CAPI::fromAPI(buffer);
        WebGPU::CAPI::Future future { protect(protectedBuffer->device())->instance() };
        protectedBuffer->mapAsync(mapModeFromAPIIgnoringUnknownBits(mode), offset, WebGPU::CAPI::mapSizeFromAPI(size), [callbackInfo, future](bool success) {
            if (success)
                callbackInfo.callback(WGPUMapAsyncStatus_Success, WebGPU::CAPI::toAPI(""_s), callbackInfo.userdata1, callbackInfo.userdata2);
            else
                callbackInfo.callback(WGPUMapAsyncStatus_Error, WebGPU::CAPI::toAPI("mapAsync failed"_s), callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        });
        return future.future();
    }
}

void wgpuBufferUnmap(WGPUBuffer buffer)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(buffer))->unmap();
    }
}

void wgpuBufferSetLabel(WGPUBuffer buffer, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(buffer))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}

// The size of the descriptor of the buffer.
uint64_t wgpuBufferGetSize(WGPUBuffer buffer)
{
    @autoreleasepool {
        return protect(WebGPU::CAPI::fromAPI(buffer))->size();
    }
}

WGPUBufferUsage wgpuBufferGetUsage(WGPUBuffer buffer)
{
    @autoreleasepool {
        return WebGPU::CAPI::toAPI(protect(WebGPU::CAPI::fromAPI(buffer))->usage());
    }
}
