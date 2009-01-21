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

#ifndef SVGPaintServer_h
#define SVGPaintServer_h

#if ENABLE(SVG)

#include "DashArray.h"
#include "SVGResource.h"

#if PLATFORM(CG)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace WebCore {

    enum SVGPaintServerType {
        // Painting mode
        SolidPaintServer = 0,
        PatternPaintServer = 1,
        LinearGradientPaintServer = 2,
        RadialGradientPaintServer = 3
    };

    enum SVGPaintTargetType {
        // Target mode
        ApplyToFillTargetType = 1,
        ApplyToStrokeTargetType = 2
    };

    class GraphicsContext;
    class RenderObject;
    class RenderStyle;
    class SVGPaintServerSolid;

    class SVGPaintServer : public SVGResource {
    public:
        virtual ~SVGPaintServer();

        virtual SVGResourceType resourceType() const { return PaintServerResourceType; }

        virtual SVGPaintServerType type() const = 0;
        virtual TextStream& externalRepresentation(TextStream&) const = 0;

        // To be implemented in platform specific code.
        virtual void draw(GraphicsContext*&, const RenderObject*, SVGPaintTargetType) const;
        virtual void teardown(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool isPaintingText = false) const;
        virtual void renderPath(GraphicsContext*&, const RenderObject*, SVGPaintTargetType) const;

        virtual bool setup(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool isPaintingText = false) const = 0;

        static SVGPaintServer* strokePaintServer(const RenderStyle*, const RenderObject*);
        static SVGPaintServer* fillPaintServer(const RenderStyle*, const RenderObject*);
        static SVGPaintServerSolid* sharedSolidPaintServer();

    protected:
        SVGPaintServer();        
    };

    TextStream& operator<<(TextStream&, const SVGPaintServer&);

    SVGPaintServer* getPaintServerById(Document*, const AtomicString&);

    void applyStrokeStyleToContext(GraphicsContext*, RenderStyle*, const RenderObject*);
    DashArray dashArrayFromRenderingStyle(const RenderStyle* style);
} // namespace WebCore

#endif

#endif // SVGPaintServer_h
