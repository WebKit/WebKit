/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

}
