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

#ifndef KSVG_SVGMarkerElement_H
#define KSVG_SVGMarkerElement_H

#include <SVGElement.h>
#include <SVGLangSpace.h>
#include <SVGStylable.h>
#include <SVGExternalResourcesRequired.h>
#include <SVGFitToViewBox.h>

namespace KSVG
{
	class SVGAngle;
	class SVGAnimatedAngle;
	class SVGAnimatedLength;
	class SVGAnimatedEnumeration;
	class SVGMarkerElementImpl;

	class SVGMarkerElement : public SVGElement,
							 public SVGLangSpace,
							 public SVGExternalResourcesRequired,
							 public SVGStylable,
							 public SVGFitToViewBox
	{
	public:
		SVGMarkerElement();
		explicit SVGMarkerElement(SVGMarkerElementImpl *i);
		SVGMarkerElement(const SVGMarkerElement &other);
		SVGMarkerElement(const KDOM::Node &other);
		virtual ~SVGMarkerElement();

		// Operators
		SVGMarkerElement &operator=(const SVGMarkerElement &other);
		SVGMarkerElement &operator=(const KDOM::Node &other);

		// 'SVGMarkerElement' functions
		SVGAnimatedLength refX() const;
		SVGAnimatedLength refY() const;
		SVGAnimatedEnumeration markerUnits() const;
		SVGAnimatedLength markerWidth() const;
		SVGAnimatedLength markerHeight() const;
		SVGAnimatedEnumeration orientType() const;
		SVGAnimatedAngle orientAngle() const;

		void setOrientToAuto();
		void setOrientToAngle(const SVGAngle &angle);

		// Internal
		KSVG_INTERNAL(SVGMarkerElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGMarkerElementProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGMarkerElementProtoFunc, SVGMarkerElement)

#endif

// vim:ts=4:noet
