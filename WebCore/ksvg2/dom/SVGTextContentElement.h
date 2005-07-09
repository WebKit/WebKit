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

#ifndef KSVG_SVGTextContentElement_H
#define KSVG_SVGTextContentElement_H

#include <ksvg2/ecma/SVGLookup.h>
#include <ksvg2/dom/SVGElement.h>
#include <ksvg2/dom/SVGTests.h>
#include <ksvg2/dom/SVGLangSpace.h>
#include <ksvg2/dom/SVGExternalResourcesRequired.h>
#include <ksvg2/dom/SVGStylable.h>

namespace KSVG
{
	class SVGRect;
	class SVGPoint;
	class SVGAnimatedLength;
	class SVGAnimatedEnumeration;
	class SVGTextContentElementImpl;

	class SVGTextContentElement : public SVGElement,
								  public SVGTests,
								  public SVGLangSpace,
								  public SVGExternalResourcesRequired,
								  public SVGStylable
	{
	public:
		SVGTextContentElement();
		explicit SVGTextContentElement(SVGTextContentElementImpl *i);
		SVGTextContentElement(const SVGTextContentElement &other);
		SVGTextContentElement(const KDOM::Node &other);
		virtual ~SVGTextContentElement();

		// Operators
		SVGTextContentElement &operator=(const SVGTextContentElement &other);
		SVGTextContentElement &operator=(const KDOM::Node &other);

		// 'SVGTextContentElement' functions
		SVGAnimatedLength textLength() const;
		SVGAnimatedEnumeration lengthAdjust() const;

		long getNumberOfChars() const;
		float getComputedTextLength() const;
		float getSubStringLength(unsigned long charnum, unsigned long nchars) const;
		SVGPoint getStartPositionOfChar(unsigned long charnum) const;
		SVGPoint getEndPositionOfChar(unsigned long charnum) const;
		SVGRect getExtentOfChar(unsigned long charnum) const;
		float getRotationOfChar(unsigned long charnum) const;
		long getCharNumAtPosition(const SVGPoint &point) const;
		void selectSubString(unsigned long charnum, unsigned long nchars) const;

		// Internal
		KSVG_INTERNAL(SVGTextContentElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KSVG_DEFINE_PROTOTYPE(SVGTextContentElementProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGTextContentElementProtoFunc, SVGTextContentElement)

#endif

// vim:ts=4:noet
