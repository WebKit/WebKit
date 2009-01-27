/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *               2008 Eric Seidel <eric@webkit.org>
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

#ifndef SVGPaintServerGradient_h
#define SVGPaintServerGradient_h

#if ENABLE(SVG)

#include "TransformationMatrix.h"
#include "Color.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "SVGPaintServer.h"

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class ImageBuffer;
    class SVGGradientElement;

    typedef std::pair<float, Color> SVGGradientStop;

    class SVGPaintServerGradient : public SVGPaintServer {
    public:
        virtual ~SVGPaintServerGradient();

        void setGradient(PassRefPtr<Gradient>);
        Gradient* gradient() const;

        // Gradient start and end points are percentages when used in boundingBox mode.
        // For instance start point with value (0,0) is top-left and end point with
        // value (100, 100) is bottom-right. BoundingBox mode is enabled by default.
        bool boundingBoxMode() const;
        void setBoundingBoxMode(bool mode = true);

        TransformationMatrix gradientTransform() const;
        void setGradientTransform(const TransformationMatrix&);

        void setGradientStops(const Vector<SVGGradientStop>& stops) { m_stops = stops; }
        const Vector<SVGGradientStop>& gradientStops() const { return m_stops; }

        virtual TextStream& externalRepresentation(TextStream&) const;

        virtual bool setup(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool isPaintingText) const;
        virtual void teardown(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool isPaintingText) const;

    protected:
        SVGPaintServerGradient(const SVGGradientElement* owner);
        
    private:
        Vector<SVGGradientStop> m_stops;
        RefPtr<Gradient> m_gradient;
        bool m_boundingBoxMode;
        TransformationMatrix m_gradientTransform;
        const SVGGradientElement* m_ownerElement;

#if PLATFORM(CG)
    public:
        mutable GraphicsContext* m_savedContext;
        mutable OwnPtr<ImageBuffer> m_imageBuffer;
#endif
    };

    inline SVGGradientStop makeGradientStop(float offset, const Color& color)
    {
        return std::make_pair(offset, color);
    }

} // namespace WebCore

#endif

#endif // SVGPaintServerGradient_h
