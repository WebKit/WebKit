/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *               2009 Dirk Schulze <krit@webkit.org>
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

#if ENABLE(SVG)
#include "SVGResourceMasker.h"

#include "CanvasPixelArray.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "GraphicsContext.h"
#include "RenderObject.h"
#include "SVGMaskElement.h"
#include "SVGRenderSupport.h"
#include "SVGRenderStyle.h"
#include "TextStream.h"

using namespace std;

namespace WebCore {

SVGResourceMasker::SVGResourceMasker(const SVGMaskElement* ownerElement)
    : SVGResource()
    , m_ownerElement(ownerElement)
    , m_emptyMask(false)
{
}

SVGResourceMasker::~SVGResourceMasker()
{
}

void SVGResourceMasker::invalidate()
{
    SVGResource::invalidate();
    m_mask.clear();
    m_emptyMask = false;
}

FloatRect SVGResourceMasker::maskerBoundingBox(const FloatRect& objectBoundingBox) const
{
    return m_ownerElement->maskBoundingBox(objectBoundingBox);
}

bool SVGResourceMasker::applyMask(GraphicsContext* context, const RenderObject* object)
{
    if (!m_mask && !m_emptyMask)
        m_mask = m_ownerElement->drawMaskerContent(object, m_maskRect, m_emptyMask);

    if (!m_mask)
        return false;

    context->clipToImageBuffer(m_maskRect, m_mask.get());
    return true;
}

TextStream& SVGResourceMasker::externalRepresentation(TextStream& ts) const
{
    ts << "[type=MASKER]";
    return ts;
}

SVGResourceMasker* getMaskerById(Document* document, const AtomicString& id, const RenderObject* object)
{
    SVGResource* resource = getResourceById(document, id, object);
    if (resource && resource->isMasker())
        return static_cast<SVGResourceMasker*>(resource);

    return 0;
}

} // namespace WebCore

#endif
