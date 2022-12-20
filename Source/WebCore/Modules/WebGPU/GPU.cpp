/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "GPU.h"

#include "JSGPUAdapter.h"

namespace WebCore {

static PAL::WebGPU::RequestAdapterOptions convertToBacking(const std::optional<GPURequestAdapterOptions>& options)
{
    if (!options)
        return { std::nullopt, false };

    return options->convertToBacking();
}

void GPU::setBacking(PAL::WebGPU::GPU& backing)
{
    m_backing = &backing;
    while (!m_pendingRequestAdapterArguments.isEmpty()) {
        auto arguments = m_pendingRequestAdapterArguments.takeFirst();
        requestAdapter(arguments.options, WTFMove(arguments.promise));
    }
}

void GPU::requestAdapter(const std::optional<GPURequestAdapterOptions>& options, RequestAdapterPromise&& promise)
{
    if (!m_backing) {
        m_pendingRequestAdapterArguments.append({ options, WTFMove(promise) });
        return;
    }

    m_backing->requestAdapter(convertToBacking(options), [promise = WTFMove(promise)] (RefPtr<PAL::WebGPU::Adapter>&& adapter) mutable {
        if (!adapter) {
            promise.reject(nullptr);
            return;
        }
        promise.resolve(GPUAdapter::create(adapter.releaseNonNull()).ptr());
    });
}

GPUTextureFormat GPU::getPreferredCanvasFormat()
{
    return GPUTextureFormat::Bgra8unorm;
}

}
