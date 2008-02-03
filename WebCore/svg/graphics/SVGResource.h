/*
 * Copyright (C) 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGResource_h
#define SVGResource_h

#if ENABLE(SVG)
#include "PlatformString.h"
#include "StringHash.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebCore {

    class AtomicString; 
    class Document;
    class SVGStyledElement;
    class TextStream;

    enum SVGResourceType {
        // Painting mode
        ClipperResourceType = 0,
        MarkerResourceType,
        ImageResourceType,
        FilterResourceType,
        MaskerResourceType,
        PaintServerResourceType,
        
        // For resource tracking we need to know how many types of resource there are
        _ResourceTypeCount
    };

    // The SVGResource file represent various graphics resources:
    // - Filter resource
    // - Clipper resource
    // - Masker resource
    // - Marker resource
    // - Pattern resource
    // - Linear/Radial gradient resource
    //
    // SVG creates/uses these resources.

    class SVGResource : public RefCounted<SVGResource> {
    public:
        SVGResource();
        virtual ~SVGResource();
      
        virtual void invalidate();

        void addClient(SVGStyledElement*);
        virtual SVGResourceType resourceType() const = 0;
        
        bool isPaintServer() const { return resourceType() == PaintServerResourceType; }
        bool isFilter() const { return resourceType() == FilterResourceType; }
        bool isClipper() const { return resourceType() == ClipperResourceType; }
        bool isMarker() const { return resourceType() == MarkerResourceType; }
        bool isMasker() const { return resourceType() == MaskerResourceType; }

        virtual TextStream& externalRepresentation(TextStream&) const;

        static void invalidateClients(HashSet<SVGStyledElement*>);
        static void removeClient(SVGStyledElement*);

    private:
        HashSet<SVGStyledElement*> m_clients;
    };

    SVGResource* getResourceById(Document*, const AtomicString&);
    
    TextStream& operator<<(TextStream&, const SVGResource&);

} // namespace WebCore

#endif
#endif // SVGResource_h
