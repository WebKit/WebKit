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
#include "NavigatorGPU.h"

#if ENABLE(WEBGPU)

#include "Navigator.h"
#include "WebGPU.h"

namespace WebCore {

NavigatorGPU* NavigatorGPU::from(Navigator* navigator)
{
    NavigatorGPU* supplement = static_cast<NavigatorGPU*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorGPU>();
        supplement = newSupplement.get();
        provideTo(navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

const char* NavigatorGPU::supplementName()
{
    return "NavigatorGPU";
}

const WebGPU* NavigatorGPU::gpu(Navigator& navigator)
{
    auto scriptExecutionContext = navigator.scriptExecutionContext();
    if (!scriptExecutionContext)
        return nullptr;

    ASSERT(scriptExecutionContext->settingsValues().webGPUEnabled);
    return NavigatorGPU::from(&navigator)->gpu();
}

const WebGPU* NavigatorGPU::gpu() const
{
    if (!m_gpu)
        m_gpu = WebGPU::create();
    return m_gpu.get();
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
