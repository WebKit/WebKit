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

#ifndef KSVG_SVGLocatable_H
#define KSVG_SVGLocatable_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
	class SVGRect;
	class SVGMatrix;
	class SVGElement;
	class SVGLocatableImpl;
	class SVGLocatable
	{
	public:
		SVGLocatable();
		explicit SVGLocatable(SVGLocatableImpl *i);
		SVGLocatable(const SVGLocatable &other);
		virtual ~SVGLocatable();

		// Operators
		SVGLocatable &operator=(const SVGLocatable &other);
		SVGLocatable &operator=(SVGLocatableImpl *other);

		// 'SVGLocatable' functions
		SVGElement nearestViewportElement() const;
		SVGElement farthestViewportElement() const;

		SVGRect getBBox() const;
		SVGMatrix getCTM() const;
		SVGMatrix getScreenCTM() const;
		SVGMatrix getTransformToElement(const SVGElement &element) const;

		// Internal
		KSVG_INTERNAL_BASE(SVGLocatable)

	protected:
		SVGLocatableImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGLocatableProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGLocatableProtoFunc, SVGLocatable)

#endif

// vim:ts=4:noet
