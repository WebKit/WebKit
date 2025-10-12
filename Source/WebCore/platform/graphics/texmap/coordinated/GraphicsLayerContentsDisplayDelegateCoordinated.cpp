/*
 * Copyright (C) 2025 Igalia S.L.
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

#include "config.h"
#include "GraphicsLayerContentsDisplayDelegateCoordinated.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedPlatformLayerBuffer.h"

namespace WebCore {

GraphicsLayerContentsDisplayDelegateCoordinated::GraphicsLayerContentsDisplayDelegateCoordinated() = default;

GraphicsLayerContentsDisplayDelegateCoordinated::~GraphicsLayerContentsDisplayDelegateCoordinated() = default;

void GraphicsLayerContentsDisplayDelegateCoordinated::setDisplayBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& displayBuffer)
{
    m_displayBuffer = WTFMove(displayBuffer);
}

bool GraphicsLayerContentsDisplayDelegateCoordinated::display(CoordinatedPlatformLayer& layer, std::optional<Damage>&& dirtyRegion)
{
    if (!m_displayBuffer)
        return false;

    layer.setContentsBuffer(WTFMove(m_displayBuffer), WTFMove(dirtyRegion));
    return true;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
