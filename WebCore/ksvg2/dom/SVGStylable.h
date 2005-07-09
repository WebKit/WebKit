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

#ifndef KSVG_SVGStylable_H
#define KSVG_SVGStylable_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KDOM
{
	class CSSValue;
	class DOMString;
	class CSSStyleDeclaration;
};

namespace KSVG
{
	class SVGElement;
	class SVGStylableImpl;
	class SVGAnimatedString;

	class SVGStylable
	{
	public:
		SVGStylable();
		explicit SVGStylable(SVGStylableImpl *i);
		SVGStylable(const SVGStylable &other);
		virtual ~SVGStylable();

		// Operators
		SVGStylable &operator=(const SVGStylable &other);
		SVGStylable &operator=(SVGStylableImpl *other);

		// 'SVGStylable' functions
		SVGAnimatedString className() const;
		KDOM::CSSStyleDeclaration style() const;
		KDOM::CSSValue getPresentationAttribute(const KDOM::DOMString &name);

		// Internal
		KSVG_INTERNAL_BASE(SVGStylable)

	protected:
		SVGStylableImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGStylableProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGStylableProtoFunc, SVGStylable)

#endif

// vim:ts=4:noet
