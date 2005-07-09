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

#ifndef KSVG_SVGPoint_H
#define KSVG_SVGPoint_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGMatrix;
	class SVGPointImpl;
	class SVGPoint
	{
	public:
		SVGPoint();
		explicit SVGPoint(SVGPointImpl *i);
		SVGPoint(const SVGPoint &other);
		virtual ~SVGPoint();

		// Operators
		SVGPoint &operator=(const SVGPoint &other);
		bool operator==(const SVGPoint &other) const;
		bool operator!=(const SVGPoint &other) const;

		// 'SVGPoint' functions
		void setX(float x);
		float x() const;

		void setY(float y);
		float y() const;

		SVGPoint matrixTransform(const SVGMatrix &matrix);

		// Internal
		KSVG_INTERNAL_BASE(SVGPoint)

	protected:
		SVGPointImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT
		KDOM_CAST

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);
	};

	KDOM_DEFINE_CAST(SVGPoint)
};

KSVG_DEFINE_PROTOTYPE(SVGPointProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGPointProtoFunc, SVGPoint)

#endif

// vim:ts=4:noet
