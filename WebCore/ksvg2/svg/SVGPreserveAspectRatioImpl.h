/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGPreserveAspectRatioImpl_H
#define KSVG_SVGPreserveAspectRatioImpl_H
#if SVG_SUPPORT

#include "Shared.h"

namespace KDOM
{
    class DOMStringImpl;
};

namespace KSVG
{
    class SVGMatrixImpl;
    class SVGStyledElementImpl;
    class SVGPreserveAspectRatioImpl : public Shared<SVGPreserveAspectRatioImpl>
    { 
    public:
        SVGPreserveAspectRatioImpl(const SVGStyledElementImpl *context);
        virtual ~SVGPreserveAspectRatioImpl();

        void setAlign(unsigned short);
        unsigned short align() const;

        void setMeetOrSlice(unsigned short);
        unsigned short meetOrSlice() const;
        
        SVGMatrixImpl *getCTM(float logicX, float logicY,
                              float logicWidth, float logicHeight,
                              float physX, float physY, float physWidth,
                              float physHeight);

        // Helper
        void parsePreserveAspectRatio(KDOM::DOMStringImpl *string);

    protected:
        unsigned short m_align;
        unsigned short m_meetOrSlice;

        const SVGStyledElementImpl *m_context;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
