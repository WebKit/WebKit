/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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

#ifndef SVGPreserveAspectRatio_h
#define SVGPreserveAspectRatio_h

#if ENABLE(SVG)
#include "PlatformString.h"
#include "SVGNames.h"

#include <wtf/RefCounted.h>

namespace WebCore {

    class String;
    class TransformationMatrix;
    class SVGStyledElement;

    class SVGPreserveAspectRatio : public RefCounted<SVGPreserveAspectRatio> { 
    public:
        static PassRefPtr<SVGPreserveAspectRatio> create() { return adoptRef(new SVGPreserveAspectRatio); }

        enum SVGPreserveAspectRatioType {
            SVG_PRESERVEASPECTRATIO_UNKNOWN     = 0,
            SVG_PRESERVEASPECTRATIO_NONE        = 1,
            SVG_PRESERVEASPECTRATIO_XMINYMIN    = 2,
            SVG_PRESERVEASPECTRATIO_XMIDYMIN    = 3,
            SVG_PRESERVEASPECTRATIO_XMAXYMIN    = 4,
            SVG_PRESERVEASPECTRATIO_XMINYMID    = 5,
            SVG_PRESERVEASPECTRATIO_XMIDYMID    = 6,
            SVG_PRESERVEASPECTRATIO_XMAXYMID    = 7,
            SVG_PRESERVEASPECTRATIO_XMINYMAX    = 8,
            SVG_PRESERVEASPECTRATIO_XMIDYMAX    = 9,
            SVG_PRESERVEASPECTRATIO_XMAXYMAX    = 10
        };

        enum SVGMeetOrSliceType {
            SVG_MEETORSLICE_UNKNOWN    = 0,
            SVG_MEETORSLICE_MEET       = 1,
            SVG_MEETORSLICE_SLICE      = 2
        };

        virtual ~SVGPreserveAspectRatio();

        void setAlign(unsigned short);
        unsigned short align() const;

        void setMeetOrSlice(unsigned short);
        unsigned short meetOrSlice() const;
        
        TransformationMatrix getCTM(double logicX, double logicY,
                               double logicWidth, double logicHeight,
                               double physX, double physY,
                               double physWidth, double physHeight);

        // Helper
        bool parsePreserveAspectRatio(const UChar*& currParam, const UChar* end, bool validate = true);
        String valueAsString() const;

        const QualifiedName& associatedAttributeName() const { return SVGNames::preserveAspectRatioAttr; }

    private:
        SVGPreserveAspectRatio();
        
        unsigned short m_align;
        unsigned short m_meetOrSlice;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGPreserveAspectRatio_h

