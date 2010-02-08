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

#if ENABLE(SVG)
#include "SVGResourceMarker.h"

#include "AffineTransform.h"
#include "GraphicsContext.h"
#include "RenderSVGViewportContainer.h"
#include "TextStream.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

SVGResourceMarker::SVGResourceMarker()
    : SVGResource()
    , m_angle(-1) // just like using setAutoAngle()
    , m_renderer(0)
    , m_useStrokeWidth(true)
{
}

SVGResourceMarker::~SVGResourceMarker()
{
}

AffineTransform SVGResourceMarker::markerTransformation(const FloatPoint& origin, float angle, float strokeWidth) const
{
    ASSERT(m_renderer);

    AffineTransform transform;
    transform.translate(origin.x(), origin.y());
    transform.rotate(m_angle == -1 ? angle : m_angle);
    transform = m_renderer->markerContentTransformation(transform, m_referencePoint, m_useStrokeWidth ? strokeWidth : -1);
    return transform;
}

void SVGResourceMarker::draw(RenderObject::PaintInfo& paintInfo, const AffineTransform& transform)
{
    if (!m_renderer)
        return;

    DEFINE_STATIC_LOCAL(HashSet<SVGResourceMarker*>, currentlyDrawingMarkers, ());

    // avoid drawing circular marker references
    if (currentlyDrawingMarkers.contains(this))
        return;

    currentlyDrawingMarkers.add(this);
    ASSERT(!m_renderer->drawsContents());
    RenderObject::PaintInfo info(paintInfo);
    info.context->save();
    applyTransformToPaintInfo(info, transform);
    m_renderer->setDrawsContents(true);
    m_renderer->paint(info, 0, 0);
    m_renderer->setDrawsContents(false);
    info.context->restore();

    currentlyDrawingMarkers.remove(this);
}

TextStream& SVGResourceMarker::externalRepresentation(TextStream& ts) const
{
    ts << "[type=MARKER]"
        << " [angle=";

    if (angle() == -1)
        ts << "auto" << "]";
    else
        ts << angle() << "]";

    ts << " [ref x=" << m_referencePoint.x() << " y=" << m_referencePoint.y() << "]";
    return ts;
}

SVGResourceMarker* getMarkerById(Document* document, const AtomicString& id, const RenderObject* object)
{
    SVGResource* resource = getResourceById(document, id, object);
    if (resource && resource->isMarker())
        return static_cast<SVGResourceMarker*>(resource);

    return 0;
}

} // namespace WebCore

#endif
