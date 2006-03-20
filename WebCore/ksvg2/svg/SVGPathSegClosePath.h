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

#ifndef KSVG_SVGPathSegClosePathImpl_H
#define KSVG_SVGPathSegClosePathImpl_H
#if SVG_SUPPORT

#include <ksvg2/svg/SVGPathSeg.h>

namespace WebCore
{
    class SVGPathSegClosePath : public SVGPathSeg
    {
    public:
        SVGPathSegClosePath();
        virtual ~SVGPathSegClosePath();

        virtual unsigned short pathSegType() const { return PATHSEG_CLOSEPATH; }
        virtual StringImpl *pathSegTypeAsLetter() const { return new StringImpl("Z"); }
        virtual DeprecatedString toString() const { return DeprecatedString::fromLatin1("Z"); }
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
