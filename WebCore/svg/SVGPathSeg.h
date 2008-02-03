/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGPathSeg_h
#define SVGPathSeg_h

#if ENABLE(SVG)
#include "PlatformString.h"
#include "SVGNames.h"

#include <wtf/RefCounted.h>

namespace WebCore
{
    class SVGPathElement;
    class SVGStyledElement;

    class SVGPathSeg : public RefCounted<SVGPathSeg>
    {
    public:
        SVGPathSeg() { }
        virtual ~SVGPathSeg() { }

        enum SVGPathSegType {
            PATHSEG_UNKNOWN                         = 0,
            PATHSEG_CLOSEPATH                       = 1,
            PATHSEG_MOVETO_ABS                      = 2,
            PATHSEG_MOVETO_REL                      = 3,
            PATHSEG_LINETO_ABS                      = 4,
            PATHSEG_LINETO_REL                      = 5,
            PATHSEG_CURVETO_CUBIC_ABS               = 6,
            PATHSEG_CURVETO_CUBIC_REL               = 7,
            PATHSEG_CURVETO_QUADRATIC_ABS           = 8,
            PATHSEG_CURVETO_QUADRATIC_REL           = 9,
            PATHSEG_ARC_ABS                         = 10,
            PATHSEG_ARC_REL                         = 11,
            PATHSEG_LINETO_HORIZONTAL_ABS           = 12,
            PATHSEG_LINETO_HORIZONTAL_REL           = 13,
            PATHSEG_LINETO_VERTICAL_ABS             = 14,
            PATHSEG_LINETO_VERTICAL_REL             = 15,
            PATHSEG_CURVETO_CUBIC_SMOOTH_ABS        = 16,
            PATHSEG_CURVETO_CUBIC_SMOOTH_REL        = 17,
            PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS    = 18,
            PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL    = 19
        };

        virtual unsigned short pathSegType() const { return PATHSEG_UNKNOWN; }
        virtual String pathSegTypeAsLetter() const { return ""; }
        virtual String toString() const { return ""; }

        const QualifiedName& associatedAttributeName() const { return SVGNames::dAttr; }
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
