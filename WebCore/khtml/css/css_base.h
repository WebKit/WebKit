/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _CSS_BASE_H
#define _CSS_BASE_H

#include "dom/dom_string.h"
#include "dom/dom_misc.h"
#include "xml/dom_nodeimpl.h"
#include "misc/shared.h"
#include <qdatetime.h>
#include <qptrlist.h>

namespace DOM {

    class StyleSheetImpl;
    class MediaList;

    class CSSSelector;
    class CSSProperty;
    class CSSValueImpl;
    class CSSPrimitiveValueImpl;
    class CSSRuleImpl;
    class CSSStyleRuleImpl;

    class DocumentImpl;

    struct CSSNamespace {
        DOMString m_prefix;
        DOMString m_uri;
        CSSNamespace* m_parent;

        MAIN_THREAD_ALLOCATED;

        CSSNamespace(const DOMString& p, const DOMString& u, CSSNamespace* parent) 
            :m_prefix(p), m_uri(u), m_parent(parent) {}
        ~CSSNamespace() { delete m_parent; }
        
        const DOMString& uri() { return m_uri; }
        const DOMString& prefix() { return m_prefix; }
        
        CSSNamespace* namespaceForPrefix(const DOMString& prefix) {
            if (prefix == m_prefix)
                return this;
            if (m_parent)
                return m_parent->namespaceForPrefix(prefix);
            return 0;
        }
    };
    
// this class represents a selector for a StyleRule
    class CSSSelector
    {
    public:
	CSSSelector()
	    : tagHistory(0), simpleSelector(0), nextSelector(0), attr(0), tag(anyQName),
              relation( Descendant ), match( None ),
              pseudoId( 0 ), _pseudoType(PseudoNotParsed)
        {}

	~CSSSelector() {
	    delete tagHistory;
            delete simpleSelector;
            delete nextSelector;
	}

        MAIN_THREAD_ALLOCATED;

        void append(CSSSelector* n) {
            if (!nextSelector) nextSelector = n; else nextSelector->append(n);
        }
        CSSSelector* next() { return nextSelector; }

	/**
	 * Print debug output for this selector
	 */
	void print();

	/**
	 * Re-create selector text from selector's data
	 */
	DOMString selectorText() const;

	// checks if the 2 selectors (including sub selectors) agree.
	bool operator == ( const CSSSelector &other );

	// tag == -1 means apply to all elements (Selector = *)

	unsigned int specificity();

	/* how the attribute value has to match.... Default is Exact */
	enum Match
	{
	    None = 0,
	    Id,
            Class,
	    Exact,
	    Set,
	    List,
	    Hyphen,
	    Pseudo,
	    Contain,   // css3: E[foo*="bar"]
	    Begin,     // css3: E[foo^="bar"]
	    End        // css3: E[foo$="bar"]
	};

	enum Relation
	{
	    Descendant = 0,
	    Child,
	    Sibling,
	    SubSelector
	};

	enum PseudoType
	{
	    PseudoNotParsed = 0,
	    PseudoOther,
	    PseudoEmpty,
	    PseudoFirstChild,
            PseudoLastChild,
            PseudoOnlyChild,
	    PseudoFirstLine,
	    PseudoFirstLetter,
	    PseudoLink,
	    PseudoVisited,
            PseudoAnyLink,
	    PseudoHover,
	    PseudoDrag,
	    PseudoFocus,
	    PseudoActive,
            PseudoTarget,
	    PseudoBefore,
	    PseudoAfter,
            PseudoLang,
            PseudoNot,
            PseudoRoot,
            PseudoSelection
	};

	inline PseudoType pseudoType() const
	    {
		if (_pseudoType == PseudoNotParsed)
		    extractPseudoType();
		return _pseudoType;
	    }

	mutable DOM::AtomicString value;
	CSSSelector* tagHistory;
        CSSSelector* simpleSelector; // Used for :not.
        CSSSelector* nextSelector; // used for ,-chained selectors
	Q_UINT32     attr;
	Q_UINT32     tag;

        Relation relation     : 2;
	Match 	 match         : 4;
	unsigned int pseudoId : 3;
	mutable PseudoType _pseudoType : 5;

    private:
	void extractPseudoType() const;
    };

    // a style class which has a parent (almost all have)
    class StyleBaseImpl : public khtml::TreeShared<StyleBaseImpl>
    {
    public:
	StyleBaseImpl()  { m_parent = 0; strictParsing = true; multiLength = false; }
	StyleBaseImpl(StyleBaseImpl *p) {
	    m_parent = p;
	    strictParsing = (m_parent ? m_parent->useStrictParsing() : true);
	    multiLength = false;
	}

	virtual ~StyleBaseImpl() {}

        MAIN_THREAD_ALLOCATED;

	// returns the url of the style sheet this object belongs to
	DOMString baseURL();

	virtual bool isStyleSheet() const { return false; }
	virtual bool isCSSStyleSheet() const { return false; }
        virtual bool isXSLStyleSheet() const { return false; }
	virtual bool isStyleSheetList() const { return false; }
	virtual bool isMediaList() { return false; }
	virtual bool isRuleList() { return false; }
	virtual bool isRule() { return false; }
	virtual bool isStyleRule() { return false; }
	virtual bool isCharetRule() { return false; }
	virtual bool isImportRule() { return false; }
	virtual bool isMediaRule() { return false; }
	virtual bool isFontFaceRule() { return false; }
	virtual bool isPageRule() { return false; }
	virtual bool isUnknownRule() { return false; }
	virtual bool isStyleDeclaration() { return false; }
	virtual bool isValue() { return false; }
	virtual bool isPrimitiveValue() const { return false; }
	virtual bool isValueList() { return false; }
	virtual bool isValueCustom() { return false; }

	void setParent(StyleBaseImpl *parent) { m_parent = parent; }

	virtual bool parseString(const DOMString &/*cssString*/, bool = false) { return false; }

	virtual void checkLoaded();

	void setStrictParsing( bool b ) { strictParsing = b; }
	bool useStrictParsing() const { return strictParsing; }

	StyleSheetImpl* stylesheet();

    protected:
	bool strictParsing : 1;
	bool multiLength : 1;
    };

    // a style class which has a list of children (StyleSheets for example)
    class StyleListImpl : public StyleBaseImpl
    {
    public:
	StyleListImpl() : StyleBaseImpl() { m_lstChildren = 0; }
	StyleListImpl(StyleBaseImpl *parent) : StyleBaseImpl(parent) { m_lstChildren = 0; }

	virtual ~StyleListImpl();

	unsigned long length() { return m_lstChildren->count(); }
	StyleBaseImpl *item(unsigned long num) { return m_lstChildren->at(num); }

	void append(StyleBaseImpl *item) { m_lstChildren->append(item); }

    protected:
	QPtrList<StyleBaseImpl> *m_lstChildren;
    };

    int getPropertyID(const char *tagStr, int len);

}

#endif
