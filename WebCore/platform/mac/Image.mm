/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "Image.h"
#import "IntSize.h"
#import "IntRect.h"

#import "KWQFoundationExtras.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreImageRendererFactory.h"

namespace WebCore {

Image * Image::loadResource(const char *name)
{
    Image *p = new Image([[WebCoreImageRendererFactory sharedFactory] imageRendererWithName:[NSString stringWithCString:name]]);
    return p;
}

bool Image::supportsType(const QString& type)
{
    return [[[WebCoreImageRendererFactory sharedFactory] supportedMIMETypes] containsObject:type.getNSString()];
}

Image::Image()
{
    m_imageRenderer = nil;
    m_needCopyOnWrite = false;
}

Image::Image(WebCoreImageRendererPtr r)
{
    m_imageRenderer = KWQRetain(r);
    m_needCopyOnWrite = false;
}

Image::Image(const QString& type)
{
    m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithMIMEType:type.getNSString()]);
    m_needCopyOnWrite = false;
}

Image::Image(const IntSize& sz)
{
    m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithSize:sz]);
    m_needCopyOnWrite = false;
}

Image::Image(const ByteArray& bytes, const QString& type)
{
    m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithBytes:bytes.data() length:bytes.size() MIMEType:type.getNSString()]);
    m_needCopyOnWrite = false;
}

Image::Image(int w, int h)
{
    m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithSize:NSMakeSize(w, h)]);
    m_needCopyOnWrite = false;
}

Image::Image(const Image& copyFrom)
{
    m_imageRenderer = KWQRetainNSRelease([copyFrom.m_imageRenderer copyWithZone:NULL]);;
    m_needCopyOnWrite = false;
}

Image::~Image()
{
    KWQRelease(m_imageRenderer);
}

CGImageRef Image::imageRef() const
{
    return [m_imageRenderer imageRef];
}

void Image::resetAnimation()
{
    if (m_imageRenderer)
        [m_imageRenderer resetAnimation];
}

void Image::setAnimationRect(const IntRect& rect) const
{
    [m_imageRenderer setAnimationRect:NSMakeRect(rect.x(), rect.y(), rect.width(), rect.height())];
}

bool Image::decode(const ByteArray& bytes, bool allDataReceived)
{
    return [m_imageRenderer incrementalLoadWithBytes:bytes.data() length:bytes.size() complete:allDataReceived callback:nil];
}

bool Image::isNull() const
{
    return !m_imageRenderer || [m_imageRenderer isNull];
}

IntSize Image::size() const
{
    if (!m_imageRenderer)
        return IntSize(0, 0);
    return IntSize([m_imageRenderer size]);
}

IntRect Image::rect() const
{
    return IntRect(IntPoint(0, 0), size());
}

int Image::width() const
{
    return size().width();
}

int Image::height() const
{
    return size().height();
}

void Image::resize(const IntSize& sz)
{
    resize(sz.width(), sz.height());
}

void Image::resize(int w, int h)
{
    if (m_needCopyOnWrite) {
        id <WebCoreImageRenderer> newImageRenderer = KWQRetainNSRelease([m_imageRenderer copyWithZone:NULL]);
        KWQRelease(m_imageRenderer);
        m_imageRenderer = newImageRenderer;
        m_needCopyOnWrite = false;
    }
    [m_imageRenderer resize:NSMakeSize(w, h)];
}

Image& Image::operator=(const Image& assignFrom)
{
    id <WebCoreImageRenderer> oldImageRenderer = m_imageRenderer;
    m_imageRenderer = KWQRetainNSRelease([assignFrom.m_imageRenderer retainOrCopyIfNeeded]);
    KWQRelease(oldImageRenderer);
    assignFrom.m_needCopyOnWrite = true;
    m_needCopyOnWrite = true;
    return *this;
}

void Image::increaseUseCount() const
{
    [m_imageRenderer increaseUseCount];
}

void Image::decreaseUseCount() const
{
    [m_imageRenderer decreaseUseCount];
}

void Image::stopAnimations()
{
    [m_imageRenderer stopAnimation];
}

void Image::flushRasterCache()
{
    [m_imageRenderer flushRasterCache];
}

}
