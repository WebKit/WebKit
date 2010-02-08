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

#ifndef SVGResourceMarker_h
#define SVGResourceMarker_h

#if ENABLE(SVG)
#include "FloatPoint.h"
#include "FloatRect.h"
#include "RenderObject.h"
#include "SVGResource.h"

namespace WebCore {

    class RenderSVGViewportContainer;
    class AffineTransform;

    class SVGResourceMarker : public SVGResource {
    public:
        static PassRefPtr<SVGResourceMarker> create() { return adoptRef(new SVGResourceMarker); }
        virtual ~SVGResourceMarker();

        RenderSVGViewportContainer* renderer() const { return m_renderer; }
        void setRenderer(RenderSVGViewportContainer* marker) { m_renderer = marker; }

        void setReferencePoint(const FloatPoint& point) { m_referencePoint = point; }
        FloatPoint referencePoint() const { return m_referencePoint; }

        void setAngle(float angle) { m_angle = angle; }
        void setAutoAngle() { m_angle = -1; }
        float angle() const { return m_angle; }

        void setUseStrokeWidth(bool useStrokeWidth = true) { m_useStrokeWidth = useStrokeWidth; }
        bool useStrokeWidth() const { return m_useStrokeWidth; }

        AffineTransform markerTransformation(const FloatPoint& origin, float angle, float strokeWidth) const;
        void draw(RenderObject::PaintInfo&, const AffineTransform&);

        virtual SVGResourceType resourceType() const { return MarkerResourceType; }
        virtual TextStream& externalRepresentation(TextStream&) const;

    private:
        SVGResourceMarker();

        FloatPoint m_referencePoint;
        float m_angle;
        RenderSVGViewportContainer* m_renderer;
        bool m_useStrokeWidth;
    };

    SVGResourceMarker* getMarkerById(Document*, const AtomicString&, const RenderObject*);

} // namespace WebCore

#endif

#endif // SVGResourceMarker_h
