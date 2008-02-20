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

#include "FloatRect.h"
#include "SVGResource.h"

namespace WebCore {

    class GraphicsContext;
    class RenderSVGViewportContainer;

    class SVGResourceMarker : public SVGResource {
    public:
        static PassRefPtr<SVGResourceMarker> create() { return adoptRef(new SVGResourceMarker); }
        virtual ~SVGResourceMarker();

        void setMarker(RenderSVGViewportContainer*);

        void setRef(double refX, double refY);
        double refX() const { return m_refX; }
        double refY() const { return m_refY; }

        void setAngle(float angle) { m_angle = angle; }
        void setAutoAngle() { m_angle = -1; }
        float angle() const { return m_angle; }

        void setUseStrokeWidth(bool useStrokeWidth = true) { m_useStrokeWidth = useStrokeWidth; }
        bool useStrokeWidth() const { return m_useStrokeWidth; }

        FloatRect cachedBounds() const;
        void draw(GraphicsContext*, const FloatRect&, double x, double y, double strokeWidth = 1, double angle = 0);
        
        virtual SVGResourceType resourceType() const { return MarkerResourceType; }
        virtual TextStream& externalRepresentation(TextStream&) const;

    private:
        SVGResourceMarker();
        double m_refX, m_refY;
        FloatRect m_cachedBounds;
        float m_angle;
        RenderSVGViewportContainer* m_marker;
        bool m_useStrokeWidth;
    };

    SVGResourceMarker* getMarkerById(Document*, const AtomicString&);

} // namespace WebCore

#endif

#endif // SVGResourceMarker_h
