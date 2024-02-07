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
#include "GPUAdapter.h"

#include "Exception.h"
#include "JSDOMPromiseDeferred.h"
#include "JSGPUAdapterInfo.h"
#include "JSGPUDevice.h"

namespace WebCore {

String GPUAdapter::name() const
{
    return m_backing->name();
}

Ref<GPUSupportedFeatures> GPUAdapter::features() const
{
    return GPUSupportedFeatures::create(WebGPU::SupportedFeatures::clone(m_backing->features()));
}

Ref<GPUSupportedLimits> GPUAdapter::limits() const
{
    return GPUSupportedLimits::create(WebGPU::SupportedLimits::clone(m_backing->limits()));
}

bool GPUAdapter::isFallbackAdapter() const
{
    return m_backing->isFallbackAdapter();
}

static WebGPU::DeviceDescriptor convertToBacking(const std::optional<GPUDeviceDescriptor>& options)
{
    if (!options)
        return { };

    return options->convertToBacking();
}

void GPUAdapter::requestDevice(ScriptExecutionContext& scriptExecutionContext, const std::optional<GPUDeviceDescriptor>& deviceDescriptor, RequestDevicePromise&& promise)
{
    m_backing->requestDevice(convertToBacking(deviceDescriptor), [deviceDescriptor, promise = WTFMove(promise), scriptExecutionContextRef = Ref { scriptExecutionContext }](RefPtr<WebGPU::Device>&& device) mutable {
        if (!device.get())
            promise.reject(Exception(ExceptionCode::OperationError));
        else if (deviceDescriptor && deviceDescriptor->requiredFeatures.size() > device->features().features().size())
            promise.reject(Exception(ExceptionCode::TypeError));
        else {
            Ref<GPUDevice> gpuDevice = GPUDevice::create(scriptExecutionContextRef.ptr(), device.releaseNonNull());
            gpuDevice->suspendIfNeeded();
            promise.resolve(WTFMove(gpuDevice));
        }
    });
}

void GPUAdapter::requestAdapterInfo(const std::optional<Vector<String>>&, RequestAdapterInfoPromise&& promise)
{
    promise.resolve(GPUAdapterInfo::create(name()));
}

}
