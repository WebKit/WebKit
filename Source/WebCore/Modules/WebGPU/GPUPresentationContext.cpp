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

#include "config.h"
#include "GPUPresentationContext.h"

#include "GPUCanvasConfiguration.h"
#include "GPUDevice.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"

namespace WebCore {

bool GPUPresentationContext::configure(const GPUCanvasConfiguration& canvasConfiguration, GPUIntegerCoordinate width, GPUIntegerCoordinate height, bool reportValidationErrors)
{
    m_device = canvasConfiguration.device.get();
    m_currentTexture = nullptr;
    m_textureDescriptor = GPUTextureDescriptor {
        { "canvas backing"_s },
        GPUExtent3DDict { width, height, 1 },
        1,
        1,
        GPUTextureDimension::_2d,
        canvasConfiguration.format,
        canvasConfiguration.usage,
        canvasConfiguration.viewFormats
    };

    if (!m_backing->configure(canvasConfiguration.convertToBacking(reportValidationErrors))) {
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

void GPUPresentationContext::unconfigure()
{
    m_currentTexture = nullptr;
    m_backing->unconfigure();
}

RefPtr<GPUTexture> GPUPresentationContext::getCurrentTexture(uint32_t index)
{
    if ((!m_currentTexture || m_currentTexture->isDestroyed()) && m_device.get()) {
        if (auto currentTexture = m_backing->getCurrentTexture(index))
            m_currentTexture = GPUTexture::create(*currentTexture, m_textureDescriptor, *m_device.get()).ptr();
    }

    return m_currentTexture;
}

void GPUPresentationContext::present(uint32_t frameIndex, bool presentBacking)
{
    m_currentTexture = nullptr;
    if (presentBacking)
        m_backing->present(frameIndex, presentBacking);
}

} // namespace WebCore
