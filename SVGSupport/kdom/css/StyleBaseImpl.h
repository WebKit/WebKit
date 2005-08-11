/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml css code by:
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
                  1999 Waldo Bastian (bastian@kde.org)
                  2002 Apple Computer, Inc.
                  2004 Allan Sandfeld Jensen (kde@carewolf.com)

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

#ifndef KDOM_StyleBaseImpl_H
#define KDOM_StyleBaseImpl_H

#include <qptrlist.h>

#include <kdom/DOMString.h>
#include <kdom/TreeShared.h>
#include <kdom/impl/NodeImpl.h>

namespace KDOM
{
	class CSSParser;
	class CSSProperty;
	class CSSValueImpl;
	class CDFInterface;
	class StyleSheetImpl;

	struct CSSNamespace
	{
		DOMString m_prefix;
		DOMString m_uri;
		CSSNamespace *m_parent;

		CSSNamespace(const DOMString &p, const DOMString &u, CSSNamespace *parent)
		: m_prefix(p), m_uri(u), m_parent(parent) { }
		~CSSNamespace() { delete m_parent; }

		const DOMString &uri() { return m_uri; }
		const DOMString &prefix() { return m_prefix; }

		CSSNamespace* namespaceForPrefix(const DOMString &prefix)
		{
			if(prefix == m_prefix)
				return this;

			if(m_parent)
				return m_parent->namespaceForPrefix(prefix);

			return 0;
		}
	};

	// a style class which has a parent (almost all have)
	class StyleBaseImpl : public TreeShared<StyleBaseImpl>
	{
	public:
		StyleBaseImpl();
		StyleBaseImpl(StyleBaseImpl *p);
		virtual ~StyleBaseImpl();

		// returns the url of the style sheet this object belongs to
		// not const
		KURL baseURL();

		virtual bool isStyleSheet() const { return false; }
		virtual bool isCSSStyleSheet() const { return false; }
		virtual bool isStyleSheetList() const { return false; }
		virtual bool isMediaList() const { return false; }
		virtual bool isRuleList() const { return false; }
		virtual bool isRule() const { return false; }
		virtual bool isStyleRule() const { return false; }
		virtual bool isCharsetRule() const { return false; }
		virtual bool isImportRule() const { return false; }
		virtual bool isMediaRule() const { return false; }
		virtual bool isFontFaceRule() const { return false; }
		virtual bool isPageRule() const { return false; }
		virtual bool isUnknownRule() const { return false; }
		virtual bool isStyleDeclaration() const { return false; }
		virtual bool isValue() const { return false; }
		virtual bool isPrimitiveValue() const { return false; }
		virtual bool isValueList() const { return false; }
		virtual bool isValueCustom() const { return false; }

		void setParent(StyleBaseImpl *parent);

		static void setParsedValue(int propId, const CSSValueImpl *parsedValue,
								   bool important, bool nonCSSHint, QPtrList<CSSProperty> *propList);

		virtual bool parseString(const DOMString &cssString, bool = false);

		virtual void checkLoaded() const;

		void setStrictParsing(bool b);
		bool useStrictParsing() const;

		// not const
		StyleSheetImpl *stylesheet();

		// KDOM extension
		virtual CSSParser *createCSSParser(bool strictParsing = true) const;

	protected:
		bool m_hasInlinedDecl : 1;
		bool m_strictParsing : 1;
		bool m_multiLength : 1;
	};

	// a style class which has a list of children (StyleSheets for example)
	class StyleListImpl : public StyleBaseImpl
	{
	public:
		StyleListImpl();
		StyleListImpl(StyleBaseImpl *parent);
		virtual ~StyleListImpl();

		unsigned long length() const;
		StyleBaseImpl *item(unsigned long num) const;

		void append(StyleBaseImpl *item);

	protected:
		QPtrList<StyleBaseImpl> *m_lstChildren;
	};

	// this class represents a selector for a StyleRule
	class CSSSelector
	{
	public:
		CSSSelector(CDFInterface *interface);
		~CSSSelector();

		/**
		 * Print debug output for this selector
		 */
		void print();

		/**
		 * Re-create selector text from selector's data
		 */
		DOMString selectorText() const;

		// checks if the 2 selectors (including sub selectors) agree.
		bool operator==(const CSSSelector &other) const;

		// tag == -1 means apply to all elements (Selector = *)
		unsigned int specificity() const;

		/* how the attribute value has to match.... Default is Exact */
		enum Match
		{
			None = 0,
			Id,
			Exact,
			Set,
			List,
			Hyphen,
			PseudoClass,
			PseudoElement,
			Contain,   // css3: E[foo*="bar"]
			Begin,     // css3: E[foo^="bar"]
			End        // css3: E[foo$="bar"]
		};

		enum Relation
		{
			Descendant = 0,
			Child,
			DirectAdjacent,
			IndirectAdjacent,
			SubSelector
		};

		enum PseudoType
		{
			PseudoNotParsed = 0,
			PseudoOther,
			PseudoEmpty,
			PseudoFirstChild,
			PseudoLastChild,
			PseudoNthChild,
			PseudoNthLastChild,
			PseudoOnlyChild,
			PseudoFirstOfType,
			PseudoLastOfType,
			PseudoNthOfType,
			PseudoNthLastOfType,
			PseudoOnlyOfType,
			PseudoFirstLine,
			PseudoFirstLetter,
			PseudoLink,
			PseudoVisited,
			PseudoHover,
			PseudoFocus,
			PseudoActive,
			PseudoTarget,
			PseudoBefore,
			PseudoAfter,
			PseudoLang,
			PseudoNot,
			PseudoContains,
			PseudoRoot,
			PseudoSelection,
			PseudoEnabled,
			PseudoDisabled,
			PseudoChecked,
			PseudoIndeterminate

		};

		PseudoType pseudoType() const;

		mutable DOMString value;
		CSSSelector *tagHistory;
		CSSSelector *simpleSelector; // Used by :not.
		DOMString string_arg; // Used by :contains, :lang and :nth-*
		NodeImpl::Id attr;
		NodeImpl::Id tag;

		Relation relation : 3;
		mutable Match match : 4;
		bool nonCSSHint : 1;
		unsigned int pseudoId : 3;
		mutable PseudoType _pseudoType : 5;

	private:
		void extractPseudoType() const;

		CDFInterface *m_interface;
	};
}

#endif

// vim:ts=4:noet
