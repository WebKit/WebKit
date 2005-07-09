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

#ifndef KDOM_CSSStyleSheetImpl_H
#define KDOM_CSSStyleSheetImpl_H

#include <kdom/css/impl/StyleSheetImpl.h>

namespace KDOM
{
	class NodeImpl;
	class CSSRuleImpl;
	class CSSRuleList;
	class DocumentImpl;

	class CSSStyleSheetImpl : public StyleSheetImpl 
	{
	public:
		CSSStyleSheetImpl(NodeImpl *parentNode, const DOMString &href = DOMString(), bool _implicit = false);
		CSSStyleSheetImpl(CSSStyleSheetImpl *parentSheet, const DOMString &href = DOMString());
		CSSStyleSheetImpl(CSSRuleImpl *ownerRule, const DOMString &href = DOMString());
		
		// clone from a cached version of the sheet
		CSSStyleSheetImpl(NodeImpl *parentNode, CSSStyleSheetImpl *orig);
		CSSStyleSheetImpl(CSSRuleImpl *ownerRule, CSSStyleSheetImpl *orig);
		
		virtual ~CSSStyleSheetImpl();

		virtual bool isCSSStyleSheet() const { return true; }
		virtual DOMString type() const { return "text/css"; }

		// 'CSSStyleSheetImpl' functions
		CSSRuleImpl *ownerRule() const;
		CSSRuleList cssRules();

		unsigned long insertRule(const DOMString &rule, unsigned long index);
		void deleteRule(unsigned long index);

		void addNamespace(CSSParser *p, const DOMString &prefix, const DOMString &uri);
		void determineNamespace(Q_UINT32 &id, const DOMString &prefix);

		virtual bool parseString(const DOMString &string, bool strict = true);

		bool isLoading() const;
		void setNonCSSHints();

		virtual void checkLoaded() const;

		DocumentImpl *doc() const { return m_doc; }
		bool implicit() const { return m_implicit; }

	protected:
		DocumentImpl *m_doc;
		bool m_implicit : 1;
		CSSNamespace *m_namespaces;
	};
};

#endif

// vim:ts=4:noet
