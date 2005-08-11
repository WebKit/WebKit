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

#ifndef KDOM_CSSPrimitiveValue_H
#define KDOM_CSSPrimitiveValue_H

#include <kdom/kdom.h>
#include <kdom/css/kdomcss.h>
#include <kdom/css/CSSValue.h>

namespace KDOM
{
	class Rect;
	class Counter;
	class RGBColor;
	class CSSPrimitiveValueImpl;
	class CSSPrimitiveValue : public CSSValue 
	{
	public:
		CSSPrimitiveValue();
		explicit CSSPrimitiveValue(CSSPrimitiveValueImpl *i);
		CSSPrimitiveValue(const CSSPrimitiveValue &other);
		CSSPrimitiveValue(const CSSValue &other);
		virtual ~CSSPrimitiveValue();

		// Operators
		CSSPrimitiveValue &operator=(const CSSPrimitiveValue &other);
		CSSPrimitiveValue &operator=(const CSSValue &other);

		// 'CSSPrimitiveValue' functions
		unsigned short primitiveType() const;
		void setFloatValue(unsigned short unitType, float floatValue);
		float getFloatValue(unsigned short unitType) const;
		void setStringValue(unsigned short stringType, const DOMString &stringValue);
		DOMString getStringValue() const;
		Counter getCounterValue() const;
		Rect getRectValue() const;
		RGBColor getRGBColorValue() const;

		// Internal
		KDOM_INTERNAL(CSSPrimitiveValue)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(CSSPrimitiveValueProto)
KDOM_IMPLEMENT_PROTOFUNC(CSSPrimitiveValueProtoFunc, CSSPrimitiveValue)

#endif

// vim:ts=4:noet
