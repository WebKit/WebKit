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

#ifndef KSVG_SVGLinearGradientElement_H
#define KSVG_SVGLinearGradientElement_H

#include <SVGGradientElement.h>

namespace KSVG
{
	class SVGAnimatedLength;
	class SVGLinearGradientElementImpl;
	class SVGLinearGradientElement : public SVGGradientElement
	{
	public:
		SVGLinearGradientElement();
		explicit SVGLinearGradientElement(SVGLinearGradientElementImpl *i);
		SVGLinearGradientElement(const SVGLinearGradientElement &other);
		SVGLinearGradientElement(const KDOM::Node &other);
		virtual ~SVGLinearGradientElement();

		// Operators
		SVGLinearGradientElement &operator=(const SVGLinearGradientElement &other);
		SVGLinearGradientElement &operator=(const KDOM::Node &other);

		// 'SVGLinearGradientElement' functions
		SVGAnimatedLength x1() const;
		SVGAnimatedLength y1() const;
		SVGAnimatedLength x2() const;
		SVGAnimatedLength y2() const;

		// Internal
		KSVG_INTERNAL(SVGLinearGradientElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
