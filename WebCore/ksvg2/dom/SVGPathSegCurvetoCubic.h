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

#ifndef KSVG_SVGPathSegCurvetoCubic_H
#define KSVG_SVGPathSegCurvetoCubic_H

#include <ksvg2/dom/SVGPathSeg.h>

namespace KSVG
{
    class SVGPathSegCurvetoCubicAbsImpl;
    class SVGPathSegCurvetoCubicAbs : public SVGPathSeg 
    { 
    public:
        SVGPathSegCurvetoCubicAbs();
        explicit SVGPathSegCurvetoCubicAbs(SVGPathSegCurvetoCubicAbsImpl *);
        SVGPathSegCurvetoCubicAbs(const SVGPathSegCurvetoCubicAbs &);
        SVGPathSegCurvetoCubicAbs(const SVGPathSeg &);
        virtual ~SVGPathSegCurvetoCubicAbs();

        // Operators
        SVGPathSegCurvetoCubicAbs &operator=(const SVGPathSegCurvetoCubicAbs &other);
        SVGPathSegCurvetoCubicAbs &operator=(const SVGPathSeg &other);

        void setX(float);
        float x() const;

        void setY(float);
        float y() const;

        void setX1(float);
        float x1() const;

        void setY1(float);
        float y1() const;

        void setX2(float);
        float x2() const;

        void setY2(float);
        float y2() const;

        // Internal
        KSVG_INTERNAL(SVGPathSegCurvetoCubicAbs)

    public: // EcmaScript section
        KDOM_GET
        KDOM_PUT

        KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
        void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
    };

    class SVGPathSegCurvetoCubicRelImpl;
    class SVGPathSegCurvetoCubicRel : public SVGPathSeg 
    { 
    public:
        SVGPathSegCurvetoCubicRel();
        explicit SVGPathSegCurvetoCubicRel(SVGPathSegCurvetoCubicRelImpl *);
        SVGPathSegCurvetoCubicRel(const SVGPathSegCurvetoCubicRel &);
        SVGPathSegCurvetoCubicRel(const SVGPathSeg &);
        virtual ~SVGPathSegCurvetoCubicRel();

        // Operators
        SVGPathSegCurvetoCubicRel &operator=(const SVGPathSegCurvetoCubicRel &other);
        SVGPathSegCurvetoCubicRel &operator=(const SVGPathSeg &other);

        void setX(float);
        float x() const;

        void setY(float);
        float y() const;

        void setX1(float);
        float x1() const;

        void setY1(float);
        float y1() const;

        void setX2(float);
        float x2() const;

        void setY2(float);
        float y2() const;

        // Internal
        KSVG_INTERNAL(SVGPathSegCurvetoCubicRel)

    public: // EcmaScript section
        KDOM_GET
        KDOM_PUT

        KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
        void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
    };
};

#endif

// vim:ts=4:noet
