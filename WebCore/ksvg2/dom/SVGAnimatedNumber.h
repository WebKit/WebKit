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

#ifndef KSVG_SVGAnimatedNumber_H
#define KSVG_SVGAnimatedNumber_H

#include <ksvg2/dom/SVGNumber.h>

namespace KSVG
{
	class SVGAnimatedNumberImpl;
	class SVGAnimatedNumber
	{
	public:
		SVGAnimatedNumber();
		explicit SVGAnimatedNumber(SVGAnimatedNumberImpl *i);
		SVGAnimatedNumber(const SVGAnimatedNumber &other);
		virtual ~SVGAnimatedNumber();

		// Operators
		SVGAnimatedNumber &operator=(const SVGAnimatedNumber &other);
		bool operator==(const SVGAnimatedNumber &other) const;
		bool operator!=(const SVGAnimatedNumber &other) const;

		// 'SVGAnimatedNumber' functions
		float baseVal() const;
		float animVal() const;

		// Internal
		KSVG_INTERNAL_BASE(SVGAnimatedNumber)

	protected:
		SVGAnimatedNumberImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);
	};
};

#endif

// vim:ts=4:noet
