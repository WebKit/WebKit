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

#ifndef KDOM_CSSValue_H
#define KDOM_CSSValue_H

#include <kdom/kdom.h>
#include <kdom/css/kdomcss.h>
#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class CSSValueImpl;
	class CSSValue 
	{
	public:
		CSSValue();
		explicit CSSValue(CSSValueImpl *i);
		CSSValue(const CSSValue &other);
		virtual ~CSSValue();

		// Operators
		CSSValue &operator=(const CSSValue &other);
		bool operator==(const CSSValue &other) const;
		bool operator!=(const CSSValue &other) const;

		// 'CSSValue' functions
		void setCssText(const DOMString &cssText);
		DOMString cssText() const;

		unsigned short cssValueType() const;

		// Internal
		KDOM_INTERNAL_BASE(CSSValue)

		bool isCSSValueList() const;
		bool isCSSPrimitiveValue() const;

	protected:
		CSSValueImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);		
	};

// template for easing assigning of CSSValues to derived classes.
// VALUE_TYPE is used to make sure the type is correct, otherwise
// the d pointer is set to 0.
#define KDOM_CSSVALUE_DERIVED_ASSIGN_OP(T, VALUE_TYPE) \
T &T::operator=(const KDOM::CSSValue &other) \
{ \
	KDOM::CSSValueImpl *ohandle = static_cast<KDOM::CSSValueImpl *>(other.handle()); \
	if(d != ohandle) { \
		if(!ohandle || ohandle->cssValueType() != VALUE_TYPE) { \
			if(d) d->deref(); \
			d = 0; \
		} \
		else \
			KDOM::CSSValue::operator=(other); \
	} \
	return *this; \
}

};

#endif

// vim:ts=4:noet
