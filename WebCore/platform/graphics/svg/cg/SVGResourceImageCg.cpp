/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#ifdef SVG_SUPPORT
#include <ApplicationServices/ApplicationServices.h>
#include "SVGResourceImage.h"

namespace WebCore {

SVGResourceImage::SVGResourceImage()
    : m_cgLayer(0)
{
}

SVGResourceImage::~SVGResourceImage()
{
    CGLayerRelease(m_cgLayer);
}

void SVGResourceImage::init(const Image&)
{
    // no-op
}

void SVGResourceImage::init(IntSize size)
{
    m_size = size;    
}

IntSize SVGResourceImage::size() const
{
    return m_size;
}

CGLayerRef SVGResourceImage::cgLayer()
{
    return m_cgLayer;
}

void SVGResourceImage::setCGLayer(CGLayerRef layer)
{
    if (m_cgLayer != layer) {
        CGLayerRelease(m_cgLayer);
        m_cgLayer = CGLayerRetain(layer);
    }
}

} // namespace WebCore

#endif // SVG_SUPPORT
