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

#ifndef KSVG_SVGRadialGradientElement_H
#define KSVG_SVGRadialGradientElement_H

#include <SVGGradientElement.h>

namespace KSVG
{
	class SVGAnimatedLength;
	class SVGRadialGradientElementImpl;
	class SVGRadialGradientElement : public SVGGradientElement
	{
	public:
		SVGRadialGradientElement();
		explicit SVGRadialGradientElement(SVGRadialGradientElementImpl *i);
		SVGRadialGradientElement(const SVGRadialGradientElement &other);
		SVGRadialGradientElement(const KDOM::Node &other);
		virtual ~SVGRadialGradientElement();

		// Operators
		SVGRadialGradientElement &operator=(const SVGRadialGradientElement &other);
		SVGRadialGradientElement &operator=(const KDOM::Node &other);

		// 'SVGRadialGradientElement' functions
		SVGAnimatedLength cx() const;
		SVGAnimatedLength cy() const;
		SVGAnimatedLength fx() const;
		SVGAnimatedLength fy() const;
		SVGAnimatedLength r() const;

		// Internal
		KSVG_INTERNAL(SVGRadialGradientElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
