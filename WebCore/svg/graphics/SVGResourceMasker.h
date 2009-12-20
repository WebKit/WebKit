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

#ifndef SVGResourceMasker_h
#define SVGResourceMasker_h

#if ENABLE(SVG)

#include "GraphicsContext.h"
#include "SVGResource.h"

#include <memory>

#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    class FloatRect;
    class ImageBuffer;
    class SVGMaskElement;

    class SVGResourceMasker : public SVGResource {
    public:
        static PassRefPtr<SVGResourceMasker> create(const SVGMaskElement* ownerElement) { return adoptRef(new SVGResourceMasker(ownerElement)); }
        virtual ~SVGResourceMasker();
        
        virtual void invalidate();
        
        virtual SVGResourceType resourceType() const { return MaskerResourceType; }
        virtual TextStream& externalRepresentation(TextStream&) const;

        // To be implemented by the specific rendering devices
        void applyMask(GraphicsContext*, const FloatRect& boundingBox);

    private:
        SVGResourceMasker(const SVGMaskElement*);

        const SVGMaskElement* m_ownerElement;
        
        OwnPtr<ImageBuffer> m_mask;
        FloatRect m_maskRect;
    };

    SVGResourceMasker* getMaskerById(Document*, const AtomicString&);

} // namespace WebCore

#endif

#endif // SVGResourceMasker_h
