/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CanvasSurface.h"

#include "AffineTransform.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "ImageBuffer.h"
#include "MIMETypeRegistry.h"

namespace WebCore {

// These values come from the WhatWG spec.
const int CanvasSurface::DefaultWidth = 300;
const int CanvasSurface::DefaultHeight = 150;

// Firefox limits width/height to 32767 pixels, but slows down dramatically before it
// reaches that limit. We limit by area instead, giving us larger maximum dimensions,
// in exchange for a smaller maximum canvas size.
const float CanvasSurface::MaxCanvasArea = 32768 * 8192; // Maximum canvas area in CSS pixels

CanvasSurface::CanvasSurface(float pageScaleFactor)
    : m_size(DefaultWidth, DefaultHeight)
    , m_pageScaleFactor(pageScaleFactor)
    , m_originClean(true)
    , m_hasCreatedImageBuffer(false)
{
}

CanvasSurface::~CanvasSurface()
{
}

void CanvasSurface::setSurfaceSize(const IntSize& size)
{
    m_size = size;
    m_hasCreatedImageBuffer = false;
    m_imageBuffer.clear();
}

String CanvasSurface::toDataURL(const String& mimeType, ExceptionCode& ec)
{
    if (!m_originClean) {
        ec = SECURITY_ERR;
        return String();
    }

    if (m_size.isEmpty() || !buffer())
        return String("data:,");

    // FIXME: Make isSupportedImageMIMETypeForEncoding threadsafe (to allow this method to be used on a worker thread).
    if (mimeType.isNull() || !MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return buffer()->toDataURL("image/png");

    return buffer()->toDataURL(mimeType);
}

void CanvasSurface::willDraw(const FloatRect&)
{
    if (m_imageBuffer)
        m_imageBuffer->clearImage();
}

IntRect CanvasSurface::convertLogicalToDevice(const FloatRect& logicalRect) const
{
    return IntRect(convertLogicalToDevice(logicalRect.location()), convertLogicalToDevice(logicalRect.size()));
}

IntSize CanvasSurface::convertLogicalToDevice(const FloatSize& logicalSize) const
{
    float wf = ceilf(logicalSize.width() * m_pageScaleFactor);
    float hf = ceilf(logicalSize.height() * m_pageScaleFactor);

    if (!(wf >= 1 && hf >= 1 && wf * hf <= MaxCanvasArea))
        return IntSize();

    return IntSize(static_cast<unsigned>(wf), static_cast<unsigned>(hf));
}

IntPoint CanvasSurface::convertLogicalToDevice(const FloatPoint& logicalPos) const
{
    float xf = logicalPos.x() * m_pageScaleFactor;
    float yf = logicalPos.y() * m_pageScaleFactor;

    return IntPoint(static_cast<unsigned>(xf), static_cast<unsigned>(yf));
}

void CanvasSurface::createImageBuffer() const
{
    ASSERT(!m_imageBuffer);

    m_hasCreatedImageBuffer = true;

    FloatSize unscaledSize(width(), height());
    IntSize size = convertLogicalToDevice(unscaledSize);
    if (!size.width() || !size.height())
        return;

    m_imageBuffer = ImageBuffer::create(size);
    // The convertLogicalToDevice MaxCanvasArea check should prevent common cases
    // where ImageBuffer::create() returns 0, however we could still be low on memory.
    if (!m_imageBuffer)
        return;
    m_imageBuffer->context()->scale(FloatSize(size.width() / unscaledSize.width(), size.height() / unscaledSize.height()));
    m_imageBuffer->context()->setShadowsIgnoreTransforms(true);
}

GraphicsContext* CanvasSurface::drawingContext() const
{
    return buffer() ? m_imageBuffer->context() : 0;
}

ImageBuffer* CanvasSurface::buffer() const
{
    if (!m_hasCreatedImageBuffer)
        createImageBuffer();
    return m_imageBuffer.get();
}

AffineTransform CanvasSurface::baseTransform() const
{
    ASSERT(m_hasCreatedImageBuffer);
    FloatSize unscaledSize(width(), height());
    IntSize size = convertLogicalToDevice(unscaledSize);
    AffineTransform transform;
    if (size.width() && size.height())
        transform.scaleNonUniform(size.width() / unscaledSize.width(), size.height() / unscaledSize.height());
    transform.multiply(m_imageBuffer->baseTransform());
    return transform;
}

// FIXME: Everything below here relies on CanvasSurface really being
// a HTMLCanvasElement.
const SecurityOrigin& CanvasSurface::securityOrigin() const
{
    return *(static_cast<const HTMLCanvasElement*>(this)->document()->securityOrigin());
}

RenderBox* CanvasSurface::renderBox() const
{
    return static_cast<const HTMLCanvasElement*>(this)->renderBox();
}

RenderStyle* CanvasSurface::computedStyle()
{
    return static_cast<HTMLCanvasElement*>(this)->computedStyle();
}

CSSStyleSelector* CanvasSurface::styleSelector()
{
    return static_cast<HTMLCanvasElement*>(this)->document()->styleSelector();
}

} // namespace WebCore
