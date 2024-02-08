/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#include "GPUTexture.h"
#include "WebGPUPresentationContext.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h>
#endif

namespace WebCore {

struct GPUCanvasConfiguration;
class GPUDevice;
class GPUTexture;

class GPUPresentationContext : public RefCounted<GPUPresentationContext> {
public:
    static Ref<GPUPresentationContext> create(Ref<WebGPU::PresentationContext>&& backing)
    {
        return adoptRef(*new GPUPresentationContext(WTFMove(backing)));
    }

    void configure(const GPUCanvasConfiguration&);
    void unconfigure();

    RefPtr<GPUTexture> getCurrentTexture();
    void present();

    WebGPU::PresentationContext& backing() { return m_backing; }
    const WebGPU::PresentationContext& backing() const { return m_backing; }

private:
    GPUPresentationContext(Ref<WebGPU::PresentationContext>&& backing)
        : m_backing(WTFMove(backing))
    {
    }

    Ref<WebGPU::PresentationContext> m_backing;
    RefPtr<GPUTexture> m_currentTexture;
    RefPtr<const GPUDevice> m_device;
};

}
