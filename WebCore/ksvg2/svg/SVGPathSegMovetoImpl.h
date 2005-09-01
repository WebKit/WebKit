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

#ifndef KSVG_SVGPathSegMovetoImpl_H
#define KSVG_SVGPathSegMovetoImpl_H

#include <ksvg2/svg/SVGPathSegImpl.h>

namespace KSVG
{
    class SVGPathSegMovetoAbsImpl : public SVGPathSegImpl
    { 
    public:
        SVGPathSegMovetoAbsImpl(const SVGStyledElementImpl *context = 0);
        virtual ~SVGPathSegMovetoAbsImpl();

        virtual unsigned short pathSegType() const { return PATHSEG_MOVETO_ABS; }
        virtual KDOM::DOMStringImpl *pathSegTypeAsLetter() const { return new KDOM::DOMStringImpl("M"); }
        virtual QString toString() const { return QString::fromLatin1("M %1 %2").arg(m_x).arg(m_y); }

        void setX(double);
        double x() const;

        void setY(double);
        double y() const;

    private:
        double m_x;
        double m_y;
    };

    class SVGPathSegMovetoRelImpl : public SVGPathSegImpl
    { 
    public:
        SVGPathSegMovetoRelImpl(const SVGStyledElementImpl *context = 0);
        virtual ~SVGPathSegMovetoRelImpl();

        virtual unsigned short pathSegType() const { return PATHSEG_MOVETO_REL; }
        virtual KDOM::DOMStringImpl *pathSegTypeAsLetter() const { return new KDOM::DOMStringImpl("m"); }
        virtual QString toString() const { return QString::fromLatin1("m %1 %2").arg(m_x).arg(m_y); }

        void setX(double);
        double x() const;

        void setY(double);
        double y() const;

    private:
        double m_x;
        double m_y;
    };
};

#endif

// vim:ts=4:noet
