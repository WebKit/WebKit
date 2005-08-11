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

#ifndef KSVG_SVGPathSegCurvetoQuadratic_H
#define KSVG_SVGPathSegCurvetoQuadratic_H

#include <ksvg2/dom/SVGPathSeg.h>

namespace KSVG
{
	class SVGPathSegCurvetoQuadraticAbsImpl;
	class SVGPathSegCurvetoQuadraticAbs : public SVGPathSeg 
	{ 
	public:
		SVGPathSegCurvetoQuadraticAbs();
		explicit SVGPathSegCurvetoQuadraticAbs(SVGPathSegCurvetoQuadraticAbsImpl *);
		SVGPathSegCurvetoQuadraticAbs(const SVGPathSegCurvetoQuadraticAbs &);
		SVGPathSegCurvetoQuadraticAbs(const SVGPathSeg &);
		virtual ~SVGPathSegCurvetoQuadraticAbs();

		// Operators
		SVGPathSegCurvetoQuadraticAbs &operator=(const SVGPathSegCurvetoQuadraticAbs &other);
		SVGPathSegCurvetoQuadraticAbs &operator=(const SVGPathSeg &other);

		void setX(float);
		float x() const;

		void setY(float);
		float y() const;

		void setX1(float);
		float x1() const;

		void setY1(float);
		float y1() const;

		// Internal
		KSVG_INTERNAL(SVGPathSegCurvetoQuadraticAbs)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};

	class SVGPathSegCurvetoQuadraticRelImpl;
	class SVGPathSegCurvetoQuadraticRel : public SVGPathSeg 
	{ 
	public:
		SVGPathSegCurvetoQuadraticRel();
		explicit SVGPathSegCurvetoQuadraticRel(SVGPathSegCurvetoQuadraticRelImpl *);
		SVGPathSegCurvetoQuadraticRel(const SVGPathSegCurvetoQuadraticRel &);
		SVGPathSegCurvetoQuadraticRel(const SVGPathSeg &);
		virtual ~SVGPathSegCurvetoQuadraticRel();

		// Operators
		SVGPathSegCurvetoQuadraticRel &operator=(const SVGPathSegCurvetoQuadraticRel &other);
		SVGPathSegCurvetoQuadraticRel &operator=(const SVGPathSeg &other);

		void setX(float);
		float x() const;

		void setY(float);
		float y() const;

		void setX1(float);
		float x1() const;

		void setY1(float);
		float y1() const;

		// Internal
		KSVG_INTERNAL(SVGPathSegCurvetoQuadraticRel)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};
};

#endif

// vim:ts=4:noet
