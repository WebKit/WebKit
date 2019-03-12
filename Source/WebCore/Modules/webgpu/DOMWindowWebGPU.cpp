/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "DOMWindowWebGPU.h"

#if ENABLE(WEBGPU)

#include "DOMWindow.h"
#include "RuntimeEnabledFeatures.h"
#include "WebGPU.h"

namespace WebCore {

DOMWindowWebGPU::DOMWindowWebGPU(DOMWindow* window)
    : DOMWindowProperty(window)
{
}

const char* DOMWindowWebGPU::supplementName()
{
    return "DOMWindowWebGPU";
}

DOMWindowWebGPU* DOMWindowWebGPU::from(DOMWindow* window)
{
    DOMWindowWebGPU* supplement = static_cast<DOMWindowWebGPU*>(Supplement<DOMWindow>::from(window, supplementName()));
    if (!supplement) {
        auto newSupplement = std::make_unique<DOMWindowWebGPU>(window);
        supplement = newSupplement.get();
        provideTo(window, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

WebGPU* DOMWindowWebGPU::gpu(DOMWindow& window)
{
    return DOMWindowWebGPU::from(&window)->gpu();
}

WebGPU* DOMWindowWebGPU::gpu() const
{
    ASSERT(RuntimeEnabledFeatures::sharedFeatures().webGPUEnabled());

    if (!m_gpu && frame())
        m_gpu = WebGPU::create();
    return m_gpu.get();
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
