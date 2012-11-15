/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "ImageBuffer.h"

#include "Base64.h"
#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageData.h"
#include "NotImplemented.h"
#include "UnusedParam.h"
#include <wtf/UnusedParam.h>

// see http://trac.wxwidgets.org/ticket/11482
#ifdef __WXMSW__
#   include "wx/msw/winundef.h"
#endif

#include <wx/defs.h>
#include <wx/bitmap.h>
#include <wx/dc.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>
#include <wx/image.h> 
#include <wx/rawbmp.h>


namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize& size)
{
     m_bitmap.Create(size.width(), size.height(), 32);
     {
        wxAlphaPixelData pixData(m_bitmap, wxPoint(0, 0), wxSize(size.width(), size.height()));
        ASSERT(pixData);
        if (pixData) {
            wxAlphaPixelData::Iterator p(pixData);
            for (int y = 0; y < size.height(); y++) {
                wxAlphaPixelData::Iterator rowStart = p;
                for (int x = 0; x < size.width(); x++) {
                        p.Red() = 0;
                        p.Blue() = 0;
                        p.Green() = 0;
                        // FIXME: The below should be transparent but cannot be on GDI/GDK (see wxWidgets bugs #10066 and #2474)
#if wxUSE_CAIRO || defined(__WXMAC__)
                        p.Alpha() = 0;
#endif
                    ++p; 
                }
                p = rowStart;
                p.OffsetY(pixData, 1);
            }
        }
     }
     // http://www.w3.org/TR/2009/WD-html5-20090212/the-canvas-element.html#canvaspixelarray
     // "When the canvas is initialized it must be set to fully transparent black."

    m_memDC = new wxMemoryDC(m_bitmap);
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetCairoRenderer();
    if (!renderer)
        renderer = wxGraphicsRenderer::GetDefaultRenderer();
    m_graphics = renderer->CreateContext(*m_memDC);
    m_gcdc = new wxGCDC(m_graphics);
}

ImageBufferData::~ImageBufferData()
{
    delete m_gcdc;
    delete m_memDC;
}

ImageBuffer::ImageBuffer(const IntSize& size, float resolutionScale, ColorSpace colorSpace, RenderingMode, DeferralMode, bool& success)
    : m_data(size)
    , m_size(size)
    , m_logicalSize(size)
{
    // FIXME: Respect resoutionScale to support high-DPI canvas.
    UNUSED_PARAM(resolutionScale);
    // FIXME: colorSpace is not used
    UNUSED_PARAM(colorSpace);

    if (m_data.m_gcdc->IsOk()) {
        m_context = adoptPtr(new WebCore::GraphicsContext(m_data.m_gcdc));
        success = true;
    } else
        success = false;
}

ImageBuffer::~ImageBuffer()
{

}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, CoordinateSystem) const
{
    notImplemented();
    return 0;
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, CoordinateSystem) const
{
    notImplemented();
    return 0;
}

void ImageBuffer::putByteArray(Multiply multiplied, Uint8ClampedArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem)
{
    notImplemented();
}
    
String ImageBuffer::toDataURL(const String& mimeType, const double*, CoordinateSystem) const
{
    notImplemented();
    return String();
}

PassRefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, ScaleBehavior) const
{
    ASSERT(copyBehavior == CopyBackingStore);
    
    RefPtr<BitmapImage> img = BitmapImage::create(m_data.m_bitmap);
    return img.release();
}

void ImageBuffer::clip(GraphicsContext* context, const FloatRect& rect) const
{
    context->clip(rect);
}

void ImageBuffer::draw(GraphicsContext* context, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, bool useLowQualityScale)
{
    RefPtr<Image> imageCopy = copyImage(CopyBackingStore);
    context->drawImage(imageCopy.get(), styleColorSpace, destRect, srcRect, op, DoNotRespectImageOrientation, useLowQualityScale);
}

void ImageBuffer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const AffineTransform& patternTransform,
                              const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    ASSERT(context);
    RefPtr<Image> imageCopy = copyImage(CopyBackingStore);
    imageCopy->drawPattern(context, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>&)
{
    notImplemented();
}

} // namespace WebCore
