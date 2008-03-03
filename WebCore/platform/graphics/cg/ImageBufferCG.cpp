/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple, Inc
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

#include "GraphicsContext.h"
#include "ImageData.h"

#include <ApplicationServices/ApplicationServices.h>
#include <wtf/Assertions.h>

using namespace std;

namespace WebCore {

auto_ptr<ImageBuffer> ImageBuffer::create(const IntSize& size, bool grayScale)
{
    if (size.width() < 0 || size.height() < 0)
        return auto_ptr<ImageBuffer>();        
    unsigned int bytesPerRow = size.width();
    if (!grayScale) {
        // Protect against overflow
        if (bytesPerRow > 0x3FFFFFFF)
            return auto_ptr<ImageBuffer>();            
        bytesPerRow *= 4;
    }

    void* imageBuffer = fastCalloc(size.height(), bytesPerRow);
    if (!imageBuffer)
        return auto_ptr<ImageBuffer>();
    
    CGColorSpaceRef colorSpace = grayScale ? CGColorSpaceCreateDeviceGray() : CGColorSpaceCreateDeviceRGB();
    CGContextRef cgContext = CGBitmapContextCreate(imageBuffer, size.width(), size.height(), 8, bytesPerRow,
        colorSpace, grayScale ? kCGImageAlphaNone : kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorSpace);
    if (!cgContext) {
        fastFree(imageBuffer);
        return auto_ptr<ImageBuffer>();
    }

    auto_ptr<GraphicsContext> context(new GraphicsContext(cgContext));
    CGContextRelease(cgContext);
    
    return auto_ptr<ImageBuffer>(new ImageBuffer(imageBuffer, size, context));
}


ImageBuffer::ImageBuffer(void* imageData, const IntSize& size, auto_ptr<GraphicsContext> context)
    : m_data(imageData)
    , m_size(size)
    , m_context(context.release())
    , m_cgImage(0)
{
    ASSERT((reinterpret_cast<size_t>(imageData) & 2) == 0);
}

ImageBuffer::~ImageBuffer()
{
    fastFree(m_data);
    CGImageRelease(m_cgImage);
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

CGImageRef ImageBuffer::cgImage() const
{
    // It's assumed that if cgImage() is called, the actual rendering to the
    // contained GraphicsContext must be done, as we create the CGImageRef here.
    if (!m_cgImage) {
        ASSERT(context());
        m_cgImage = CGBitmapContextCreateImage(context()->platformContext());
    }

    return m_cgImage;
}

PassRefPtr<ImageData> ImageBuffer::getImageData(const IntRect& rect) const
{
    if (!m_data)
        return 0;
    
    PassRefPtr<ImageData> result = ImageData::create(rect.width(), rect.height());
    unsigned char* data = result->data()->data().data();
    
    if (rect.x() < 0 || rect.y() < 0 || (rect.x() + rect.width()) > m_size.width() || (rect.y() + rect.height()) > m_size.height())
        memset(data, 0, result->data()->length());

    int originx = rect.x();
    int destx = 0;
    if (originx < 0) {
        destx = -originx;
        originx = 0;
    }
    int endx = rect.x() + rect.width();
    if (endx > m_size.width())
        endx = m_size.width();
    int numColumns = endx - originx;
    
    int originy = rect.y();
    int desty = 0;
    if (originy < 0) {
        desty = -originy;
        originy = 0;
    }
    int endy = rect.y() + rect.height();
    if (endy > m_size.height())
        endy = m_size.height();
    int numRows = endy - originy;
    
    unsigned srcBytesPerRow = 4 * m_size.width();
    unsigned destBytesPerRow = 4 * rect.width();
    
    // m_size.height() - originy to handle the accursed flipped y axis in CG backing store
    unsigned char* srcRows = reinterpret_cast<unsigned char*>(m_data) + (m_size.height() - originy - 1) * srcBytesPerRow + originx * 4;
    unsigned char* destRows = data + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            int basex = x * 4;
            if (unsigned char alpha = srcRows[basex + 3]) {
                destRows[0] = (srcRows[basex] * 255) / alpha;
                destRows[1] = (srcRows[basex + 1] * 255) / alpha;
                destRows[2] = (srcRows[basex + 2] * 255) / alpha;
                destRows[3] = alpha;
            } else {
                reinterpret_cast<uint32_t*>(destRows)[0] = reinterpret_cast<uint32_t*>(srcRows)[0];
            }
            destRows += 4;
        }
        srcRows -= srcBytesPerRow;
    }
    return result;
}

void ImageBuffer::putImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originx = sourceRect.x();
    int destx = destPoint.x() + sourceRect.x();
    ASSERT(destx >= 0);
    ASSERT(destx < m_size.width());
    ASSERT(originx >= 0);
    ASSERT(originx <= sourceRect.right());

    int endx = destPoint.x() + sourceRect.right();
    ASSERT(endx <= m_size.width());

    int numColumns = endx - destx;
    
    int originy = sourceRect.y();
    int desty = destPoint.y() + sourceRect.y();
    ASSERT(desty >= 0);
    ASSERT(desty < m_size.height());
    ASSERT(originy >= 0);
    ASSERT(originy <= sourceRect.bottom());

    int endy = destPoint.y() + sourceRect.bottom();
    ASSERT(endy <= m_size.height());
    int numRows = endy - desty;

    unsigned srcBytesPerRow = 4 * source->width();
    unsigned destBytesPerRow = 4 * m_size.width();
    
    unsigned char* srcRows = source->data()->data().data() + originy * srcBytesPerRow + originx * 4;

    // -desty to handle the accursed flipped y axis
    unsigned char* destRows = reinterpret_cast<unsigned char*>(m_data) + (m_size.height() - desty - 1) * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            unsigned char alpha = srcRows[x * 4 + 3];
            if (alpha != 255) {
                destRows[x * 4 + 0] = (srcRows[0] * alpha) / 255;
                destRows[x * 4 + 1] = (srcRows[1] * alpha) / 255;
                destRows[x * 4 + 2] = (srcRows[2] * alpha) / 255;
                destRows[x * 4 + 3] = alpha;
            } else {
                reinterpret_cast<uint32_t*>(destRows + x * 4)[0] = reinterpret_cast<uint32_t*>(srcRows + x * 4)[0];
            }
        }
        destRows -= destBytesPerRow;
        srcRows += srcBytesPerRow;
    }
}

}
