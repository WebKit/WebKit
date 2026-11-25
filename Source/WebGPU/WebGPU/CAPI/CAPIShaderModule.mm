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

NODELETE void wgpuShaderModuleAddRef(WGPUShaderModule shaderModule)
{
    WebGPU::CAPI::fromAPI(shaderModule).ref();
}

void wgpuShaderModuleRelease(WGPUShaderModule shaderModule)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(shaderModule).deref();
    }
}

static void getCompilationInfo(WGPUShaderModule shaderModule, CompletionHandler<void(WGPUCompilationInfoRequestStatus, const WGPUCompilationInfo&)>&& callback)
{
    Ref protectedShaderModule = WebGPU::CAPI::fromAPI(shaderModule);
    // WGPUCompilationInfoRequestStatus has no error value: the messages of a shader module that
    // did not compile are reported with WGPUCompilationInfoRequestStatus_Success.
    protectedShaderModule->compilationInfo([callback = WTF::move(callback)](WebGPU::CompilationInfo&& compilationInfo) mutable {
        Vector<UTF8CString> messageStorage;
        auto messages = WebGPU::CAPI::toAPI(compilationInfo, messageStorage);
        callback(WGPUCompilationInfoRequestStatus_Success, WGPUCompilationInfo {
            .nextInChain = nullptr,
            .messageCount = messages.size(),
            .messages = messages.span().data(),
        });
    });
}

WGPUFuture wgpuShaderModuleGetCompilationInfo(WGPUShaderModule shaderModule, WGPUCompilationInfoCallbackInfo callbackInfo)
{
    @autoreleasepool {
        Ref protectedShaderModule = WebGPU::CAPI::fromAPI(shaderModule);
        WebGPU::CAPI::Future future { protect(protectedShaderModule->device())->instance() };
        getCompilationInfo(shaderModule, [callbackInfo, future](WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo& compilationInfo) {
            callbackInfo.callback(status, &compilationInfo, callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        });
        return future.future();
    }
}

void wgpuShaderModuleSetLabel(WGPUShaderModule shaderModule, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(shaderModule))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
