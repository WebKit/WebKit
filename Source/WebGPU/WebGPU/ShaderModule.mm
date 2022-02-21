/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
#import "ShaderModule.h"

#import "Device.h"

namespace WebGPU {

RefPtr<ShaderModule> Device::createShaderModule(const WGPUShaderModuleDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return ShaderModule::create(nil);
}

ShaderModule::ShaderModule(id <MTLLibrary> library)
    : m_library(library)
{
}

ShaderModule::~ShaderModule() = default;

void ShaderModule::getCompilationInfo(WTF::Function<void(WGPUCompilationInfoRequestStatus, const WGPUCompilationInfo*)>&& callback)
{
    UNUSED_PARAM(callback);
}

void ShaderModule::setLabel(const char* label)
{
    m_library.label = [NSString stringWithCString:label encoding:NSUTF8StringEncoding];
}

} // namespace WebGPU

void wgpuShaderModuleRelease(WGPUShaderModule shaderModule)
{
    delete shaderModule;
}

void wgpuShaderModuleGetCompilationInfo(WGPUShaderModule shaderModule, WGPUCompilationInfoCallback callback, void * userdata)
{
    shaderModule->shaderModule->getCompilationInfo([callback, userdata] (WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo* compilationInfo) {
        callback(status, compilationInfo, userdata);
    });
}

void wgpuShaderModuleSetLabel(WGPUShaderModule shaderModule, const char* label)
{
    shaderModule->shaderModule->setLabel(label);
}

