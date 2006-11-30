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

#ifndef SVGResource_H
#define SVGResource_H

#ifdef SVG_SUPPORT

#include "Shared.h"
#include "PlatformString.h"

namespace WebCore {

    class Document;
    class RenderPath;
    class TextStream;
    class AtomicString;

    typedef Vector<const RenderPath*> RenderPathList;

    enum SVGResourceType {
        // Painting mode
        ClipperResourceType = 0,
        MarkerResourceType = 1,
        ImageResourceType = 2,
        FilterResourceType = 3,
        MaskerResourceType = 4
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

    class SVGResource : public Shared<SVGResource> {
    public:
        SVGResource();
        virtual ~SVGResource();
      
        virtual void invalidate();
        void addClient(const RenderPath*);

        const RenderPathList& clients() const;

        virtual bool isPaintServer() const { return false; }
        virtual bool isFilter() const { return false; }
        virtual bool isClipper() const { return false; }
        virtual bool isMarker() const { return false; }
        virtual bool isMasker() const { return false; }

        virtual TextStream& externalRepresentation(TextStream&) const;

    private:
        RenderPathList m_clients;
    };

    // Helper class notifying about changes in the resources
    class SVGResourceListener
    {
    public:
        virtual ~SVGResourceListener() { }
        virtual void resourceNotification() const = 0;
    };

    SVGResource* getResourceById(Document*, const AtomicString&);

    TextStream& operator<<(TextStream&, const SVGResource&);

} // namespace WebCore

#endif

#endif // SVGResource_H
