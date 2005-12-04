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

#ifndef CSS_BASE_H
#define CSS_BASE_H

#include "dom/dom_string.h"
#include "xml/dom_nodeimpl.h"
#include "dom_qname.h"
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
        AtomicString m_prefix;
        AtomicString m_uri;
        CSSNamespace* m_parent;

        CSSNamespace(const AtomicString& p, const AtomicString& u, CSSNamespace* parent) 
            :m_prefix(p), m_uri(u), m_parent(parent) {}
        ~CSSNamespace() { delete m_parent; }
        
        const AtomicString& uri() { return m_uri; }
        const AtomicString& prefix() { return m_prefix; }
        
        CSSNamespace* namespaceForPrefix(const AtomicString& prefix) {
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
            : tagHistory(0), simpleSelector(0), nextSelector(0), attr(anyQName()), tag(anyQName()),
              relation( Descendant ), match( None ),
              pseudoId( 0 ), _pseudoType(PseudoNotParsed)
        {}
        
        CSSSelector(const QualifiedName& qName)
            : tagHistory(0), simpleSelector(0), nextSelector(0), attr(anyQName()), tag(qName),
              relation( Descendant ), match( None ),
              pseudoId( 0 ), _pseudoType(PseudoNotParsed)
        {}

        ~CSSSelector() {
            delete tagHistory;
            delete simpleSelector;
            delete nextSelector;
        }

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
            PseudoFirstOfType,
            PseudoLastChild,
            PseudoLastOfType,
            PseudoOnlyChild,
            PseudoOnlyOfType,
            PseudoFirstLine,
            PseudoFirstLetter,
            PseudoLink,
            PseudoVisited,
            PseudoAnyLink,
            PseudoHover,
            PseudoDrag,
            PseudoFocus,
            PseudoActive,
            PseudoChecked,
            PseudoEnabled,
            PseudoDisabled,
            PseudoIndeterminate,
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

        bool hasTag() const { return tag != anyQName(); }
        bool hasAttribute() const { return attr != anyQName(); }

        mutable DOM::AtomicString value;
        CSSSelector* tagHistory;
        CSSSelector* simpleSelector; // Used for :not.
        CSSSelector* nextSelector; // used for ,-chained selectors
        
        QualifiedName attr;
        QualifiedName tag;
        
        Relation relation              : 3;
        mutable Match  match           : 4;
        unsigned int pseudoId          : 3;
        mutable PseudoType _pseudoType : 5;

    private:
        void extractPseudoType() const;
    };

    // a style class which has a parent (almost all have)
    class StyleBaseImpl : public khtml::TreeShared<StyleBaseImpl>
    {
    public:
        typedef khtml::TreeShared<StyleBaseImpl> TreeShared;
        StyleBaseImpl()  { strictParsing = true; multiLength = false; }
        StyleBaseImpl(StyleBaseImpl *p) : TreeShared(p) {
            strictParsing = (p ? p->useStrictParsing() : true);
            multiLength = false;
        }

        virtual ~StyleBaseImpl() {}

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

        unsigned length() { return m_lstChildren->count(); }
        StyleBaseImpl *item(unsigned num) { return m_lstChildren->at(num); }

        void append(StyleBaseImpl *item) { m_lstChildren->append(item); }

    protected:
        QPtrList<StyleBaseImpl> *m_lstChildren;
    };

    int getPropertyID(const char *tagStr, int len);

}

#endif
