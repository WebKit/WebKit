/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>

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

#ifndef SVGPathSegMoveto_h
#define SVGPathSegMoveto_h

#if ENABLE(SVG)

#include "SVGPathSeg.h"

namespace WebCore {
    class SVGPathSegMovetoAbs : public SVGPathSegSingleCoord { 
    public:
        static PassRefPtr<SVGPathSegMovetoAbs> create(float x, float y) { return adoptRef(new SVGPathSegMovetoAbs(x, y)); }

        virtual unsigned short pathSegType() const { return PATHSEG_MOVETO_ABS; }
        virtual String pathSegTypeAsLetter() const { return "M"; }

    private:
        SVGPathSegMovetoAbs(float x, float y);
    };

    class SVGPathSegMovetoRel : public SVGPathSegSingleCoord { 
    public:
        static PassRefPtr<SVGPathSegMovetoRel> create(float x, float y) { return adoptRef(new SVGPathSegMovetoRel(x, y)); }

        virtual unsigned short pathSegType() const { return PATHSEG_MOVETO_REL; }
        virtual String pathSegTypeAsLetter() const { return "m"; }

    private:
        SVGPathSegMovetoRel(float x, float y);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
