/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "PlatformCanvas.h"

#include "GraphicsContext.h"

#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "SkColorPriv.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

PlatformCanvas::PlatformCanvas()
    : m_opaque(false)
{
}

PlatformCanvas::~PlatformCanvas()
{
}

void PlatformCanvas::resize(const IntSize& size)
{
    if (m_size == size)
        return;
    m_size = size;
    createBackingCanvas();
}

void PlatformCanvas::setOpaque(bool opaque)
{
    if (opaque == m_opaque)
        return;

    m_opaque = opaque;
    createBackingCanvas();
}

void PlatformCanvas::createBackingCanvas()
{
    if (m_size.isEmpty())
        return;

    m_skiaCanvas = adoptPtr(skia::CreateBitmapCanvas(m_size.width(), m_size.height(), m_opaque));
}

PlatformCanvas::AutoLocker::AutoLocker(PlatformCanvas* canvas)
    : m_canvas(canvas)
{
    if (m_canvas->m_skiaCanvas) {
        m_bitmap = &m_canvas->m_skiaCanvas->getDevice()->accessBitmap(false);
        m_bitmap->lockPixels();
    } else
        m_bitmap = 0;
}

PlatformCanvas::AutoLocker::~AutoLocker()
{
    if (m_bitmap)
        m_bitmap->unlockPixels();
}

const uint8_t* PlatformCanvas::AutoLocker::pixels() const
{
    if (m_bitmap && m_bitmap->config() == SkBitmap::kARGB_8888_Config)
        return static_cast<const uint8_t*>(m_bitmap->getPixels());
    return 0;
}

PlatformCanvas::Painter::Painter(PlatformCanvas* canvas, PlatformCanvas::Painter::TextOption option)
{
    m_skiaContext = adoptPtr(new PlatformContextSkia(canvas->m_skiaCanvas.get()));

    m_skiaContext->setDrawingToImageBuffer(option == GrayscaleText);

    m_context = adoptPtr(new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(m_skiaContext.get())));
    context()->save();
}

PlatformCanvas::Painter::~Painter()
{
    context()->restore();
}

} // namespace WebCore
