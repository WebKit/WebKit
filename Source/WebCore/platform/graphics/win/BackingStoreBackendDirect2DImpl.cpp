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
    IntRect sourceRect = scrollRect;
    sourceRect.move(-scrollOffset);
    sourceRect.intersect(scrollRect);

    if (!m_scrollSurface || scrollRect.size() != m_scrollSurfaceSize) {
#ifndef _NDEBUG
        ASSERT(m_size.width() >= scrollRect.size().width());
        ASSERT(m_size.height() >= scrollRect.size().height());
#endif
        m_scrollSurfaceSize = sourceRect.size();
        m_scrollSurface = Direct2D::createBitmap(m_renderTarget.get(), m_scrollSurfaceSize);
    }

    auto sourceRectLocation = IntSize(sourceRect.x(), sourceRect.y());
    auto destRectLocation = IntSize(); // Top left corner of scroll surface
    Direct2D::copyRectFromOneSurfaceToAnother(m_surface.get(), m_scrollSurface.get(), sourceRectLocation, sourceRect, destRectLocation);

    IntRect destRect = scrollRect;
    destRect.setHeight(sourceRect.height());
    destRect.setWidth(sourceRect.width());

    IntSize destPosition;
    if (scrollOffset.width() > 0 || scrollOffset.height() > 0) {
        destPosition.setWidth(std::min(scrollRect.width(), scrollOffset.width()));
        destPosition.setHeight(std::min(scrollRect.height(), scrollOffset.height()));
    }

    auto sourceScrollSurfaceLocation = IntSize(); // Top left corner of scroll surface
    Direct2D::copyRectFromOneSurfaceToAnother(m_scrollSurface.get(), m_surface.get(), sourceScrollSurfaceLocation, destRect, destPosition);

    m_scrolledHysteresis.impulse();
}

ID2D1BitmapBrush* BackingStoreBackendDirect2DImpl::bitmapBrush()
{
    if (!m_renderTarget || !m_surface)
        return nullptr;

    if (!m_bitmapBrush) {
        auto bitmapBrushProperties = D2D1::BitmapBrushProperties();
        auto brushProperties = D2D1::BrushProperties();

        HRESULT hr = m_renderTarget->CreateBitmapBrush(m_surface.get(), &bitmapBrushProperties, &brushProperties, &m_bitmapBrush);
        ASSERT(SUCCEEDED(hr));
    }

    return m_bitmapBrush.get();
}

} // namespace WebCore

#endif // USE(DIRECT2D)
