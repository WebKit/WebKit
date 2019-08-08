/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "BackingStoreBackendDirect2DImpl.h"

#if USE(DIRECT2D)

#include "Direct2DUtilities.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "ImageDecoderDirect2D.h"
#include "IntRect.h"
#include "IntSize.h"
#include <wincodec.h>

namespace WebCore {

static const Seconds scrollHysteresisDuration { 300_ms };

BackingStoreBackendDirect2DImpl::BackingStoreBackendDirect2DImpl(const IntSize& size, float deviceScaleFactor)
    : BackingStoreBackendDirect2D(size)
    , m_scrolledHysteresis([this](PAL::HysteresisState state) {
        if (state == PAL::HysteresisState::Stopped)
            m_scrollSurface = nullptr;
        }, scrollHysteresisDuration)
{
    m_renderTarget = WebCore::Direct2D::createGDIRenderTarget();

    IntSize scaledSize = m_size;
    scaledSize.scale(deviceScaleFactor);
    m_surface = Direct2D::createBitmap(m_renderTarget.get(), scaledSize);
}

BackingStoreBackendDirect2DImpl::~BackingStoreBackendDirect2DImpl()
{
}

void BackingStoreBackendDirect2DImpl::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    IntRect targetRect = scrollRect;
    targetRect.move(scrollOffset);
    targetRect.shiftMaxXEdgeTo(targetRect.maxX() - scrollOffset.width());
    targetRect.shiftMaxYEdgeTo(targetRect.maxY() - scrollOffset.height());
    if (targetRect.isEmpty())
        return;

    if (!m_scrollSurface) {
        auto floatSize = Direct2D::bitmapSize(m_surface.get());
        IntSize size(floatSize.width(), floatSize.height());
        auto scale = Direct2D::bitmapResolution(m_surface.get());
        ASSERT(scale.x() == scale.y());
        m_scrollSurface = Direct2D::createBitmap(m_renderTarget.get(), size);
    }

    Direct2D::copyRectFromOneSurfaceToAnother(m_surface.get(), m_scrollSurface.get(), scrollOffset, targetRect);
    Direct2D::copyRectFromOneSurfaceToAnother(m_scrollSurface.get(), m_surface.get(), IntSize(), targetRect);

    m_scrolledHysteresis.impulse();
}

} // namespace WebCore

#endif // USE(DIRECT2D)
