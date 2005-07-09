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

#ifndef KSVG_SVGLength_H
#define KSVG_SVGLength_H

#include <kdom/DOMString.h>

#include <ksvg2/ecma/SVGLookup.h>

class KCanvasItem;

namespace KSVG
{
	class SVGLengthImpl;
	class SVGLength
	{
	public:
		SVGLength();
		explicit SVGLength(SVGLengthImpl *i);
		SVGLength(const SVGLength &other);
		virtual ~SVGLength();

		// Operators
		SVGLength &operator=(const SVGLength &other);
		bool operator==(const SVGLength &other) const;
		bool operator!=(const SVGLength &other) const;

		// 'SVGLength' functions
		unsigned short unitType() const;

		float value() const;
		void setValue(float value);

		float valueInSpecifiedUnits() const;
		void setValueInSpecifiedUnits(float valueInSpecifiedUnits);

		KDOM::DOMString valueAsString() const;
		void setValueAsString(const KDOM::DOMString &valueAsString);

		void newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits);
		void convertToSpecifiedUnits(unsigned short unitType);

		// Internal
		KSVG_INTERNAL_BASE(SVGLength)

	protected:
		SVGLengthImpl *impl;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT
		KDOM_CAST

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);
	};
};

KSVG_DEFINE_PROTOTYPE(SVGLengthProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGLengthProtoFunc, SVGLength)

#endif

// vim:ts=4:noet
