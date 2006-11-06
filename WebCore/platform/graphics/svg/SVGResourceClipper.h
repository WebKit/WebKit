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

#ifndef SVGResourceClipper_H
#define SVGResourceClipper_H

#ifdef SVG_SUPPORT

#include "SVGResource.h"
#include "Path.h"

namespace WebCore {

    struct ClipData {
        Path path;
        WindRule windRule;
        bool bboxUnits : 1;
    };

    // FIXME: Step away from DeprecatedValueList
    class ClipDataList : public DeprecatedValueList<ClipData> {
    public:
        inline void addPath(const Path& pathData, WindRule windRule, bool bboxUnits)
        {
            ClipData clipData;

            clipData.path = pathData;
            clipData.windRule = windRule;
            clipData.bboxUnits = bboxUnits;
        
            append(clipData);
        }
    };

    class SVGResourceClipper : public SVGResource {
    public:
        SVGResourceClipper();
        virtual ~SVGResourceClipper();
      
        void resetClipData();
        void addClipData(const Path&, WindRule, bool bboxUnits);

        const ClipDataList& clipData() const;

        virtual bool isClipper() const { return true; }
        virtual TextStream& externalRepresentation(TextStream&) const;

        // To be implemented by the specific rendering devices
        void applyClip(const FloatRect& boundingBox) const;

    private:
        ClipDataList m_clipData;
    };

    TextStream& operator<<(TextStream&, WindRule);
    TextStream& operator<<(TextStream&, const ClipData&);

    SVGResourceClipper* getClipperById(Document*, const AtomicString&);

} // namespace WebCore

#endif

#endif // SVGResourceClipper_H
