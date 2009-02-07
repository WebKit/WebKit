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

#include "TransformationMatrix.h"
#include "GraphicsContext.h"
#include "RenderSVGViewportContainer.h"
#include "TextStream.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

SVGResourceMarker::SVGResourceMarker()
    : SVGResource()
    , m_refX(0.0)
    , m_refY(0.0)
    , m_angle(-1) // just like using setAutoAngle()
    , m_marker(0)
    , m_useStrokeWidth(true)
{
}

SVGResourceMarker::~SVGResourceMarker()
{
}

void SVGResourceMarker::setMarker(RenderSVGViewportContainer* marker)
{
    m_marker = marker;
}

void SVGResourceMarker::setRef(double refX, double refY)
{
    m_refX = refX;
    m_refY = refY;
}

void SVGResourceMarker::draw(GraphicsContext* context, const FloatRect& rect, double x, double y, double strokeWidth, double angle)
{
    if (!m_marker)
        return;

    DEFINE_STATIC_LOCAL(HashSet<SVGResourceMarker*>, currentlyDrawingMarkers, ());

    // avoid drawing circular marker references
    if (currentlyDrawingMarkers.contains(this))
        return;

    currentlyDrawingMarkers.add(this);

    TransformationMatrix transform;
    transform.translate(x, y);
    transform.rotate(m_angle > -1 ? m_angle : angle);

    // refX and refY are given in coordinates relative to the viewport established by the marker, yet they affect
    // the translation performed on the viewport itself.
    TransformationMatrix viewportTransform;
    if (m_useStrokeWidth)
        viewportTransform.scaleNonUniform(strokeWidth, strokeWidth);
    viewportTransform *= m_marker->viewportTransform();
    double refX, refY;
    viewportTransform.map(m_refX, m_refY, refX, refY);
    transform.translate(-refX, -refY);

    if (m_useStrokeWidth)
        transform.scaleNonUniform(strokeWidth, strokeWidth);

    // FIXME: PaintInfo should be passed into this method instead of being created here
    // FIXME: bounding box fractions are lost
    RenderObject::PaintInfo info(context, enclosingIntRect(rect), PaintPhaseForeground, 0, 0, 0);

    context->save();
    context->concatCTM(transform);
    m_marker->setDrawsContents(true);
    m_marker->paint(info, 0, 0);
    m_marker->setDrawsContents(false);
    context->restore();

    m_cachedBounds = transform.mapRect(m_marker->absoluteClippedOverflowRect());

    currentlyDrawingMarkers.remove(this);
}

FloatRect SVGResourceMarker::cachedBounds() const
{
    return m_cachedBounds;
}

TextStream& SVGResourceMarker::externalRepresentation(TextStream& ts) const
{
    ts << "[type=MARKER]"
        << " [angle=";

    if (angle() == -1)
        ts << "auto" << "]";
    else
        ts << angle() << "]";

    ts << " [ref x=" << refX() << " y=" << refY() << "]";
    return ts;
}

SVGResourceMarker* getMarkerById(Document* document, const AtomicString& id)
{
    SVGResource* resource = getResourceById(document, id);
    if (resource && resource->isMarker())
        return static_cast<SVGResourceMarker*>(resource);

    return 0;
}

} // namespace WebCore

#endif
