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

#ifndef KSVG_SVGAnimationElement_H
#define KSVG_SVGAnimationElement_H

#include <ksvg2/dom/SVGTests.h>
#include <ksvg2/dom/SVGElement.h>
#include <ksvg2/ecma/SVGLookup.h>
#include <ksvg2/dom/SVGExternalResourcesRequired.h>

namespace KSVG
{
	class SVGAnimationElementImpl;
	class SVGAnimationElement : public SVGElement,
								public SVGTests,
								public SVGExternalResourcesRequired
	{
	public:
		SVGAnimationElement();
		explicit SVGAnimationElement(SVGAnimationElementImpl *i);
		SVGAnimationElement(const SVGAnimationElement &other);
		SVGAnimationElement(const KDOM::Node &other);
		virtual ~SVGAnimationElement();

		// Operators
		SVGAnimationElement &operator=(const SVGAnimationElement &other);
		SVGAnimationElement &operator=(const KDOM::Node &other);

		// 'SVGAnimationElement' functions
		SVGElement targetElement() const;

		float getStartTime() const;
		float getCurrentTime() const;
		float getSimpleDuration() const;

		// Internal
		KSVG_INTERNAL(SVGAnimationElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGAnimationElementProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGAnimationElementProtoFunc, SVGAnimationElement)

#endif

// vim:ts=4:noet
