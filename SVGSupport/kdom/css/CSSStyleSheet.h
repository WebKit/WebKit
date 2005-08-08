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

#ifndef KDOM_CSSStyleSheet_H
#define KDOM_CSSStyleSheet_H

#include <kdom/css/StyleSheet.h>
#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class CSSRule;
	class CSSRuleList;
	class CSSStyleSheetImpl;
	class CSSStyleSheet : public StyleSheet 
	{
	public:
		CSSStyleSheet();
		explicit CSSStyleSheet(CSSStyleSheetImpl *i);
		CSSStyleSheet(const CSSStyleSheet &other);
		CSSStyleSheet(const StyleSheet &other);
		virtual ~CSSStyleSheet();

		// Operators
		CSSStyleSheet &operator=(const CSSStyleSheet &other);
		CSSStyleSheet &operator=(const StyleSheet &other);

		// 'CSSStyleSheet' functions
		CSSRule ownerRule() const;
		CSSRuleList cssRules() const;
		unsigned long insertRule(const DOMString &rule, unsigned long index);
		void deleteRule(unsigned long index);

		// 'StyleSheet' functions
		virtual DOMString type() const;

		// Internal
		KDOM_INTERNAL(CSSStyleSheet)

		// KDOM extension
		void setCssText(const DOMString &cssText);

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(CSSStyleSheetProto)
KDOM_IMPLEMENT_PROTOFUNC(CSSStyleSheetProtoFunc, CSSStyleSheet)

#endif

// vim:ts=4:noet
