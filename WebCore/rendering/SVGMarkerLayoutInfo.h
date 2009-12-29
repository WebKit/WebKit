/*
    Copyright (C) Research In Motion Limited 2009. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGMarkerLayoutInfo_h
#define SVGMarkerLayoutInfo_h

#if ENABLE(SVG)
#include "RenderObject.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class Path;
class SVGResourceMarker;

struct MarkerData {
    enum Type {
        Unknown = 0,
        Start,
        Mid,
        End
    };

    MarkerData(const Type& _type, SVGResourceMarker* _marker)
        : type(_type)
        , marker(_marker)
    {
    }

    Type type;
    SVGResourceMarker* marker;
    FloatPoint origin;
    FloatPoint subpathStart;
    FloatPoint inslopePoints[2];
    FloatPoint outslopePoints[2];
};

struct MarkerLayout {
    MarkerLayout(SVGResourceMarker* _marker = 0, TransformationMatrix _matrix = TransformationMatrix())
        : marker(_marker)
        , matrix(_matrix)
    {
        ASSERT(marker);
    }

    SVGResourceMarker* marker;
    TransformationMatrix matrix;
};

class SVGMarkerLayoutInfo : public Noncopyable {
public:
    SVGMarkerLayoutInfo();
    ~SVGMarkerLayoutInfo();

    void initialize(SVGResourceMarker* startMarker, SVGResourceMarker* midMarker, SVGResourceMarker* endMarker, float strokeWidth);

    FloatRect calculateBoundaries(const Path&);
    void drawMarkers(RenderObject::PaintInfo&);

private:
    friend void processStartAndMidMarkers(void*, const PathElement*);
    friend void recordMarkerData(SVGMarkerLayoutInfo&);

    SVGResourceMarker* m_startMarker;
    SVGResourceMarker* m_midMarker;
    SVGResourceMarker* m_endMarker;

    // Used while layouting markers
    int m_elementIndex;
    MarkerData m_markerData;
    float m_strokeWidth;

    // Holds the final computed result
    Vector<MarkerLayout> m_layout;
};

}

#endif // ENABLE(SVG)
#endif // SVGMarkerLayoutInfo_h
