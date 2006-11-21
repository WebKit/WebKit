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

#ifndef SVGPaintServer_H
#define SVGPaintServer_H

#ifdef SVG_SUPPORT

#include "SVGResource.h"

#if PLATFORM(MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

#if PLATFORM(QT)
class QPen;
#endif
    
namespace WebCore {
    
    enum SVGPaintServerType {
        // Painting mode
        PS_SOLID = 0,
        PS_PATTERN = 1,
        PS_LINEAR_GRADIENT = 2,
        PS_RADIAL_GRADIENT = 3
    };

    enum SVGPaintTargetType {
        // Target mode
        APPLY_TO_FILL = 1,
        APPLY_TO_STROKE = 2
    };

    class RenderObject;
    class RenderPath;
    class RenderStyle;
    class KRenderingDeviceContext; // FIXME: This is gone soon!

    class SVGPaintServer : public SVGResource {
    public:
        SVGPaintServer();
        virtual ~SVGPaintServer();

        virtual bool isPaintServer() const { return true; }

        const RenderPath* activeClient() const;
        void setActiveClient(const RenderPath*);

        bool isPaintingText() const;
        void setPaintingText(bool);

        virtual SVGPaintServerType type() const = 0;
        virtual TextStream& externalRepresentation(TextStream&) const = 0;

        // To be implemented in platform specific code.
        virtual void draw(KRenderingDeviceContext*, const RenderPath*, SVGPaintTargetType) const;
        virtual void teardown(KRenderingDeviceContext*, const RenderObject*, SVGPaintTargetType) const;
        virtual void renderPath(KRenderingDeviceContext*, const RenderPath*, SVGPaintTargetType) const;

        virtual bool setup(KRenderingDeviceContext*, const RenderObject*, SVGPaintTargetType) const = 0;

    protected:
#if PLATFORM(MAC)
        void strokePath(CGContextRef, const RenderPath*) const;
        void clipToStrokePath(CGContextRef, const RenderPath*) const;
        void fillPath(CGContextRef, const RenderPath*) const;
        void clipToFillPath(CGContextRef, const RenderPath*) const;
#endif

#if PLATFORM(QT)
        void setPenProperties(const RenderObject*, const RenderStyle*, QPen&) const;
#endif

    private:
        const RenderPath* m_activeClient;
        bool m_paintingText;
    };

    TextStream& operator<<(TextStream&, const SVGPaintServer&);

    SVGPaintServer* getPaintServerById(Document*, const AtomicString&);

} // namespace WebCore

#endif

#endif // SVGPaintServer_H
