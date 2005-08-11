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

#ifndef KDOM_CSSStyleDeclaration_H
#define KDOM_CSSStyleDeclaration_H

#include <kdom/css/kdomcss.h>
#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class CSSRule;
	class CSSValue;
	class DOMString;
	class CSSStyleDeclarationImpl;
	class CSSStyleDeclaration 
	{
	public:
		CSSStyleDeclaration();
		explicit CSSStyleDeclaration(CSSStyleDeclarationImpl *i);
		CSSStyleDeclaration(const CSSStyleDeclaration &other);
		~CSSStyleDeclaration();

		// Operators
		CSSStyleDeclaration &operator=(const CSSStyleDeclaration &other);
		bool operator==(const CSSStyleDeclaration &other) const;
		bool operator!=(const CSSStyleDeclaration &other) const;

		// 'CSSStyleDeclaration' functions
		void setCssText(const DOMString &cssText);
		DOMString cssText() const;

		DOMString getPropertyValue(const DOMString &propertyName) const;	
		CSSValue getPropertyCSSValue(const DOMString &propertyName) const;
		
		DOMString removeProperty(const DOMString &propertyName);
		DOMString getPropertyPriority(const DOMString &propertyName) const;

		void setProperty(const DOMString &propertyName, const DOMString &value, const DOMString &priority);

		unsigned long length() const;
		DOMString item(unsigned long index) const;

		CSSRule parentRule() const;

		// Internal
		KDOM_INTERNAL_BASE(CSSStyleDeclaration)

	private:
		CSSStyleDeclarationImpl *d;

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);		
	};
};

KDOM_DEFINE_PROTOTYPE(CSSStyleDeclarationProto)
KDOM_IMPLEMENT_PROTOFUNC(CSSStyleDeclarationProtoFunc, CSSStyleDeclaration)

#endif

// vim:ts=4:noet
