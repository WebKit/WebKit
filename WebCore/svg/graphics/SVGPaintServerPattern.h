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

#ifndef SVGPaintServerPattern_h
#define SVGPaintServerPattern_h

#if ENABLE(SVG)

#include "TransformationMatrix.h"
#include "FloatRect.h"
#include "Pattern.h"
#include "SVGPaintServer.h"

#include <memory>

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

    class GraphicsContext;
    class ImageBuffer;
    class SVGPatternElement;

    class SVGPaintServerPattern : public SVGPaintServer {
    public:
        static PassRefPtr<SVGPaintServerPattern> create(const SVGPatternElement* owner) { return adoptRef(new SVGPaintServerPattern(owner)); }

        virtual ~SVGPaintServerPattern();

        virtual SVGPaintServerType type() const { return PatternPaintServer; }

        // Pattern boundaries
        void setPatternBoundaries(const FloatRect&);
        FloatRect patternBoundaries() const;

        ImageBuffer* tile() const;
        void setTile(PassOwnPtr<ImageBuffer>);

        TransformationMatrix patternTransform() const;
        void setPatternTransform(const TransformationMatrix&);

        virtual TextStream& externalRepresentation(TextStream&) const;

        virtual bool setup(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool isPaintingText) const;
        virtual void teardown(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool isPaintingText) const;

    private:
        SVGPaintServerPattern(const SVGPatternElement*);
        
        OwnPtr<ImageBuffer> m_tile;
        const SVGPatternElement* m_ownerElement;
        TransformationMatrix m_patternTransform;
        FloatRect m_patternBoundaries;

        mutable RefPtr<Pattern> m_pattern;
    };

} // namespace WebCore

#endif

#endif // SVGPaintServerPattern_h
