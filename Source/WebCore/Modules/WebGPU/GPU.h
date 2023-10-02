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

#pragma once

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"
#include "GPUTextureFormat.h"
#include "JSDOMPromiseDeferredForward.h"
#include "WebGPU.h"
#include <optional>
#include <wtf/Deque.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class GPUCompositorIntegration;
class GPUPresentationContext;
struct GPUPresentationContextDescriptor;
class GraphicsContext;
class NativeImage;

class GPU : public RefCounted<GPU> {
public:
    static Ref<GPU> create(Ref<WebGPU::GPU>&& backing)
    {
        return adoptRef(*new GPU(WTFMove(backing)));
    }
    ~GPU();

    using RequestAdapterPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<GPUAdapter>>>;
    void requestAdapter(const std::optional<GPURequestAdapterOptions>&, RequestAdapterPromise&&);

    GPUTextureFormat getPreferredCanvasFormat();

    Ref<GPUPresentationContext> createPresentationContext(const GPUPresentationContextDescriptor&);

    Ref<GPUCompositorIntegration> createCompositorIntegration();

    void paintToCanvas(NativeImage&, const IntSize&, GraphicsContext&);

private:
    GPU(Ref<WebGPU::GPU>&&);

    struct PendingRequestAdapterArguments;
    Deque<PendingRequestAdapterArguments> m_pendingRequestAdapterArguments;
    Ref<WebGPU::GPU> m_backing;
};

}
