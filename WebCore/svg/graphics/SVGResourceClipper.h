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

#ifndef SVGResourceClipper_h
#define SVGResourceClipper_h

#if ENABLE(SVG)

#include "SVGResource.h"
#include "Path.h"

namespace WebCore {

    struct ClipData {
        Path path;
        WindRule windRule;
        bool bboxUnits : 1;
    };

    class ClipDataList { 
    public:
        void addPath(const Path& pathData, WindRule windRule, bool bboxUnits)
        {
            ClipData clipData;
            
            clipData.path = pathData;
            clipData.windRule = windRule;
            clipData.bboxUnits = bboxUnits;
            
            m_clipData.append(clipData);
        }
        
        void clear() { m_clipData.clear(); }
        const Vector<ClipData>& clipData() const { return m_clipData; }
        bool isEmpty() const { return m_clipData.isEmpty(); }
    private:
        Vector<ClipData> m_clipData;
    };  

    class GraphicsContext;

    class SVGResourceClipper : public SVGResource {
    public:
        static PassRefPtr<SVGResourceClipper> create() { return adoptRef(new SVGResourceClipper); }
        virtual ~SVGResourceClipper();
      
        void resetClipData();
        void addClipData(const Path&, WindRule, bool bboxUnits);

        const ClipDataList& clipData() const;
        
        virtual SVGResourceType resourceType() const { return ClipperResourceType; }
        virtual TextStream& externalRepresentation(TextStream&) const;

        // To be implemented by the specific rendering devices
        void applyClip(GraphicsContext*, const FloatRect& boundingBox) const;
    private:
        SVGResourceClipper();
        ClipDataList m_clipData;
    };

    TextStream& operator<<(TextStream&, WindRule);
    TextStream& operator<<(TextStream&, const ClipData&);

    SVGResourceClipper* getClipperById(Document*, const AtomicString&);

} // namespace WebCore

#endif

#endif // SVGResourceClipper_h
