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
#include "RenderObject.h"

#if ENABLE(SVG)
#include "RenderSVGContainer.h"
#endif

namespace WebCore {

IntSize ImageBuffer::size() const
{
    return m_size;
}

void ImageBuffer::renderSubtreeToImage(ImageBuffer* image, RenderObject* item)
{
    ASSERT(item && image && image->context());
    RenderObject::PaintInfo info(image->context(), IntRect(), PaintPhaseForeground, 0, 0, 0);

#if ENABLE(SVG)
    RenderSVGContainer* svgContainer = 0;
    if (item && item->isSVGContainer())
         svgContainer = static_cast<RenderSVGContainer*>(item);

    bool drawsContents = svgContainer ? svgContainer->drawsContents() : false;
    if (svgContainer && !drawsContents)
        svgContainer->setDrawsContents(true);
#endif

    item->paint(info, 0, 0);

#if ENABLE(SVG)
    if (svgContainer && !drawsContents)
        svgContainer->setDrawsContents(false);
#endif
}

}
