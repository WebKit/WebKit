/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGPathSegCurvetoQuadraticImpl_H
#define KSVG_SVGPathSegCurvetoQuadraticImpl_H

#include "SVGPathSegImpl.h"

namespace KSVG
{
	class SVGPathSegCurvetoQuadraticAbsImpl : public SVGPathSegImpl
	{ 
	public:
		SVGPathSegCurvetoQuadraticAbsImpl(const SVGStyledElementImpl *context = 0);
		virtual ~SVGPathSegCurvetoQuadraticAbsImpl();

		virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_QUADRATIC_ABS; }
		virtual KDOM::DOMString pathSegTypeAsLetter() const { return "Q"; }
		virtual QString toString() const { return QString::fromLatin1("Q %1 %2 %3 %4").arg(m_x1).arg(m_y1).arg(m_x).arg(m_y); }

		void setX(double);
		double x() const;

		void setY(double);
		double y() const;

		void setX1(double);
		double x1() const;

		void setY1(double);
		double y1() const;

	private:
		double m_x;
		double m_y;
		double m_x1;
		double m_y1;
	};

	class SVGPathSegCurvetoQuadraticRelImpl : public SVGPathSegImpl 
	{ 
	public:
		SVGPathSegCurvetoQuadraticRelImpl(const SVGStyledElementImpl *context = 0);
		virtual ~SVGPathSegCurvetoQuadraticRelImpl();

		virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_QUADRATIC_REL; }
		virtual KDOM::DOMString pathSegTypeAsLetter() const { return "q"; }
		virtual QString toString() const { return QString::fromLatin1("q %1 %2 %3 %4").arg(m_x1).arg(m_y1).arg(m_x).arg(m_y); }
 
		void setX(double);
		double x() const;

		void setY(double);
		double y() const;

		void setX1(double);
		double x1() const;

		void setY1(double);
		double y1() const;

	private:
		double m_x;
		double m_y;
		double m_x1;
		double m_y1;
	};
};

#endif

// vim:ts=4:noet
