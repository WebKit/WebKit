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

#ifndef KSVG_SVGComponentTransferFunctionElement_H
#define KSVG_SVGComponentTransferFunctionElement_H

#include <ksvg2/dom/SVGElement.h>

namespace KSVG
{
	class SVGAnimatedNumber;
	class SVGAnimatedEnumeration;
	class SVGAnimatedNumberList;
	class SVGComponentTransferFunctionElementImpl;

	class SVGComponentTransferFunctionElement :  public SVGElement
	{
	public:
		SVGComponentTransferFunctionElement();
		explicit SVGComponentTransferFunctionElement(SVGComponentTransferFunctionElementImpl *i);
		SVGComponentTransferFunctionElement(const SVGComponentTransferFunctionElement &other);
		SVGComponentTransferFunctionElement(const KDOM::Node &other);
		virtual ~SVGComponentTransferFunctionElement();

		// Operators
		SVGComponentTransferFunctionElement &operator=(const SVGComponentTransferFunctionElement &other);
		SVGComponentTransferFunctionElement &operator=(const KDOM::Node &other);

		// 'SVGComponentTransferFunctionlement' functions
		SVGAnimatedEnumeration type() const;
		SVGAnimatedNumberList tableValues() const;
		SVGAnimatedNumber slope() const;
		SVGAnimatedNumber intercept() const;
		SVGAnimatedNumber amplitude() const;
		SVGAnimatedNumber exponent() const;
		SVGAnimatedNumber offset() const;

		// Internal
		KSVG_INTERNAL(SVGComponentTransferFunctionElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
