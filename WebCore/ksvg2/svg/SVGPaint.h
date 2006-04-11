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

#ifndef KSVG_SVGPaintImpl_H
#define KSVG_SVGPaintImpl_H
#if SVG_SUPPORT

#include <SVGColor.h>

namespace WebCore
{
    class SVGPaint : public SVGColor
    {
    public:
        SVGPaint();
        SVGPaint(StringImpl *uri);
        SVGPaint(unsigned short paintType);
        SVGPaint(unsigned short paintType, StringImpl *uri, StringImpl *rgbPaint = 0, StringImpl *iccPaint = 0);
        virtual ~SVGPaint();

        // 'SVGPaint' functions
        unsigned short paintType() const;
        StringImpl *uri() const;

        void setUri(StringImpl *uri);
        void setPaint(unsigned short paintType, StringImpl *uri, StringImpl *rgbPaint, StringImpl *iccPaint);

        virtual String cssText() const;

    private:
        unsigned short m_paintType;
        String m_uri;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
