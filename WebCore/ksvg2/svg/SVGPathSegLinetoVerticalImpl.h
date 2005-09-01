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

#ifndef KSVG_SVGPathSegLinetoVerticalImpl_H
#define KSVG_SVGPathSegLinetoVerticalImpl_H

#include <ksvg2/impl/SVGPathSegImpl.h>

namespace KSVG
{
    class SVGPathSegLinetoVerticalAbsImpl : public SVGPathSegImpl
    {
    public:
        SVGPathSegLinetoVerticalAbsImpl(const SVGStyledElementImpl *context = 0);
        virtual~SVGPathSegLinetoVerticalAbsImpl();

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_VERTICAL_ABS; }
        virtual KDOM::DOMStringImpl *pathSegTypeAsLetter() const { return new KDOM::DOMStringImpl("V"); }
        virtual QString toString() const { return QString::fromLatin1("V %1").arg(m_y); }

        void setY(double);
        double y() const;

    private:
        double m_y;
    };

    class SVGPathSegLinetoVerticalRelImpl : public SVGPathSegImpl
    {
    public:
        SVGPathSegLinetoVerticalRelImpl(const SVGStyledElementImpl *context = 0);
        virtual ~SVGPathSegLinetoVerticalRelImpl();

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_VERTICAL_REL; }
        virtual KDOM::DOMStringImpl *pathSegTypeAsLetter() const { return new KDOM::DOMStringImpl("v"); }
        virtual QString toString() const { return QString::fromLatin1("v %1").arg(m_y); }

        void setY(double);
        double y() const;

    private:
        double m_y;
    };
};

#endif

// vim:ts=4:noet
