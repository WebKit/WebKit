/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "LocalCurrentGraphicsContextSwitcher.h"

#if PLATFORM(COCOA)

namespace WebCore {

LocalCurrentGraphicsContextSwitcher::LocalCurrentGraphicsContextSwitcher(GraphicsContext& context, const FloatRect& paintRect, float deviceScaleFactor)
    : m_savedGraphicsContext(context)
    , m_paintRect(paintRect)
{
    if (!context.hasPlatformContext()) {
        m_imageBuffer = m_savedGraphicsContext.createImageBuffer(m_paintRect.size(), deviceScaleFactor, DestinationColorSpace::SRGB(), context.renderingMode(), RenderingMethod::Local);
        if (m_imageBuffer) {
            m_localContext = makeUnique<LocalCurrentGraphicsContext>(m_imageBuffer->context());
            return;
        }
    }
    m_localContext = makeUnique<LocalCurrentGraphicsContext>(m_savedGraphicsContext);
}

LocalCurrentGraphicsContextSwitcher::~LocalCurrentGraphicsContextSwitcher()
{
    m_localContext = nullptr;
    if (m_imageBuffer)
        m_savedGraphicsContext.drawConsumingImageBuffer(WTFMove(m_imageBuffer), m_paintRect.location());
}

GraphicsContext& LocalCurrentGraphicsContextSwitcher::context() const
{
    if (m_imageBuffer)
        return m_imageBuffer->context();
    return m_savedGraphicsContext;
}

CGContextRef LocalCurrentGraphicsContextSwitcher::cgContext()
{
    return m_localContext->cgContext();
}

FloatRect LocalCurrentGraphicsContextSwitcher::drawingRect() const
{
    if (m_imageBuffer)
        return FloatRect { { }, m_paintRect.size() };
    return m_paintRect;
}

} // namespace WebCore

#endif // PLATFORM(COCOA)
