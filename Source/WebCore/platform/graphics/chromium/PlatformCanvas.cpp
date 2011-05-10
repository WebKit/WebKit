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

#if USE(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "SkColorPriv.h"
#include "skia/ext/platform_canvas.h"
#elif USE(CG)
#include <CoreGraphics/CGBitmapContext.h>
#endif

namespace WebCore {

PlatformCanvas::PlatformCanvas()
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
#if USE(SKIA)
    m_skiaCanvas = adoptPtr(skia::CreateBitmapCanvas(size.width(), size.height(), false));
#elif USE(CG)
    size_t bufferSize = size.width() * size.height() * 4;
    m_pixelData = adoptArrayPtr(new uint8_t[bufferSize]);
    memset(m_pixelData.get(), 0, bufferSize);
#endif
}

PlatformCanvas::AutoLocker::AutoLocker(PlatformCanvas* canvas)
    : m_canvas(canvas)
    , m_pixels(0)
{
#if USE(SKIA)
    if (m_canvas->m_skiaCanvas) {
        m_bitmap = &m_canvas->m_skiaCanvas->getDevice()->accessBitmap(false);
        m_bitmap->lockPixels();

        if (m_bitmap->config() == SkBitmap::kARGB_8888_Config)
            m_pixels = static_cast<uint8_t*>(m_bitmap->getPixels());
    } else
        m_bitmap = 0;
#elif USE(CG)
    if (canvas->m_pixelData)
        m_pixels = &canvas->m_pixelData[0];
#endif
}

PlatformCanvas::AutoLocker::~AutoLocker()
{
#if USE(SKIA)
    if (m_bitmap)
        m_bitmap->unlockPixels();
#endif
}

PlatformCanvas::Painter::Painter(PlatformCanvas* canvas, PlatformCanvas::Painter::TextOption option)
{
#if USE(SKIA)
    m_skiaContext = adoptPtr(new PlatformContextSkia(canvas->m_skiaCanvas.get()));

    m_skiaContext->setDrawingToImageBuffer(option == GrayscaleText);

    m_context = adoptPtr(new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(m_skiaContext.get())));
#elif USE(CG)

    m_colorSpace = CGColorSpaceCreateDeviceRGB();
    size_t rowBytes = canvas->size().width() * 4;
    m_contextCG = CGBitmapContextCreate(canvas->m_pixelData.get(),
                                        canvas->size().width(), canvas->size().height(), 8, rowBytes,
                                        m_colorSpace.get(),
                                        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGContextTranslateCTM(m_contextCG.get(), 0, canvas->size().height());
    CGContextScaleCTM(m_contextCG.get(), 1, -1);
    m_context = adoptPtr(new GraphicsContext(m_contextCG.get()));
#endif
    context()->save();
}

PlatformCanvas::Painter::~Painter()
{
    context()->restore();
}

} // namespace WebCore
