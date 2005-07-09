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

#ifndef KSVG_SVGTransform_H
#define KSVG_SVGTransform_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGMatrix;
	class SVGTransformImpl;
	class SVGTransform
	{ 
	public:
		SVGTransform();
		explicit SVGTransform(SVGTransformImpl *i);
		SVGTransform(const SVGTransform &other);
		virtual ~SVGTransform();

		// Operators
		SVGTransform &operator=(const SVGTransform &other);
		bool operator==(const SVGTransform &other) const;
		bool operator!=(const SVGTransform &other) const;

		// 'SVGTransform' functions
		unsigned short type() const;
		SVGMatrix matrix() const;
		double angle() const;

		void setMatrix(const SVGMatrix &);
		void setTranslate(double, double);
		void setScale(double, double);
		void setRotate(double, double, double);
		void setSkewX(double);
		void setSkewY(double);

		// Internal
		KSVG_INTERNAL_BASE(SVGTransform)

	private:
		SVGTransformImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_CAST

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGTransformProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGTransformProtoFunc, SVGTransform)

#endif

// vim:ts=4:noet
