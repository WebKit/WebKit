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

#ifndef KSVG_SVGFEGaussianBlurElement_H
#define KSVG_SVGFEGaussianBlurElement_H

#include <ksvg2/dom/SVGElement.h>
#include <ksvg2/dom/SVGFilterPrimitiveStandardAttributes.h>

namespace KSVG
{
	class SVGAnimatedString;
	class SVGAnimatedNumber;
	class SVGFEGaussianBlurElementImpl;

	class SVGFEGaussianBlurElement :  public SVGElement,
									  public SVGFilterPrimitiveStandardAttributes
	{
	public:
		SVGFEGaussianBlurElement();
		explicit SVGFEGaussianBlurElement(SVGFEGaussianBlurElementImpl *i);
		SVGFEGaussianBlurElement(const SVGFEGaussianBlurElement &other);
		SVGFEGaussianBlurElement(const KDOM::Node &other);
		virtual ~SVGFEGaussianBlurElement();

		// Operators
		SVGFEGaussianBlurElement &operator=(const SVGFEGaussianBlurElement &other);
		SVGFEGaussianBlurElement &operator=(const KDOM::Node &other);

		// 'SVGFEGaussianBlurlement' functions
		SVGAnimatedString in1() const;
		SVGAnimatedNumber stdDeviationX() const;
		SVGAnimatedNumber stdDeviationY() const;

		void setStdDeviation(float stdDeviationX, float stdDeviationY);

		// Internal
		KSVG_INTERNAL(SVGFEGaussianBlurElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGFEGaussianBlurElementProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGFEGaussianBlurElementProtoFunc, SVGFEGaussianBlurElement)

#endif

// vim:ts=4:noet
