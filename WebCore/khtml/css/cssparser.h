/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 *
 */
#ifndef _CSS_cssparser_h_
#define _CSS_cssparser_h_

#include "dom/dom_string.h"
#include "dom/dom_misc.h"
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
    class CSSStyleDeclarationImpl;
    class CSSRuleImpl;
    class CSSStyleRuleImpl;

    class DocumentImpl;

    int getPropertyID(const char *tagStr, int len);

// this class represents a selector for a StyleRule
class CSSSelector
{
public:
    CSSSelector()
	: tagHistory(0), attr(0), tag(0), relation( Descendant ),
    match( None ), nonCSSHint( false ), pseudoId( 0 ), _pseudoType(PseudoNotParsed) {}

    ~CSSSelector() {
	delete tagHistory;
    }

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
        PseudoFirstLine,
        PseudoFirstLetter,
        PseudoLink,
        PseudoVisited,
        PseudoHover,
        PseudoFocus,
        PseudoActive,
        PseudoBefore,
        PseudoAfter
    };

    inline PseudoType pseudoType() const
    {
        if (_pseudoType == PseudoNotParsed)
            extractPseudoType();
        return _pseudoType;
    }

    mutable DOM::DOMString value;
    CSSSelector *tagHistory;
    int          attr;
    int          tag;

    Relation relation     : 2;
    Match 	 match         : 4;
    bool	nonCSSHint : 1;
    unsigned int pseudoId : 3;
    
    mutable PseudoType _pseudoType;
    
private:
    void extractPseudoType() const;
};

    // a style class which has a parent (almost all have)
    class StyleBaseImpl : public khtml::TreeShared<StyleBaseImpl>
    {
    public:
	StyleBaseImpl()  { m_parent = 0; hasInlinedDecl = false; strictParsing = true; sawDescendantRule = false; }
	StyleBaseImpl(StyleBaseImpl *p) { m_parent = p; hasInlinedDecl = false; strictParsing = (m_parent ? m_parent->useStrictParsing() : true); sawDescendantRule = false; }

	virtual ~StyleBaseImpl() {}

	// returns the url of the style sheet this object belongs to
	DOMString baseURL();

	virtual bool isStyleSheet() const { return false; }
	virtual bool isCSSStyleSheet() const { return false; }
	virtual bool isStyleSheetList() const { return false; }
	virtual bool isMediaList() { return false; }
	virtual bool isRuleList() { return false; }
	virtual bool isRule() { return false; }
	virtual bool isStyleRule() { return false; }
	virtual bool isCharetRule() { return false; }
	virtual bool isImportRule() { return false; }
	virtual bool isMediaRule() { return false; }
	virtual bool isQuirksRule() { return false; }
	virtual bool isFontFaceRule() { return false; }
	virtual bool isPageRule() { return false; }
	virtual bool isUnknownRule() { return false; }
	virtual bool isStyleDeclaration() { return false; }
	virtual bool isValue() { return false; }
	virtual bool isPrimitiveValue() const { return false; }
	virtual bool isValueList() { return false; }
	virtual bool isValueCustom() { return false; }

	void setParent(StyleBaseImpl *parent) { m_parent = parent; }

	const QString preprocess(const QString &str, bool justOneRule = false);
	const QChar *parseSpace(const QChar *curP, const QChar *endP);
	const QChar *parseToChar(const QChar *curP, const QChar *endP,
				 QChar c, bool chkws, bool endAtBlock = false);

	CSSSelector *parseSelector2(const QChar *curP, const QChar *endP, CSSSelector::Relation relation);
	CSSSelector *parseSelector1(const QChar *curP, const QChar *endP);
	QPtrList<CSSSelector> *parseSelector(const QChar *curP, const QChar *endP);

	void parseProperty(const QChar *curP, const QChar *endP);
	QPtrList<CSSProperty> *parseProperties(const QChar *curP, const QChar *endP);

	/* parses generic CSSValues, return true, if it found a valid value */
	bool parseValue(const QChar *curP, const QChar *endP, int propId);
	bool parseValue(const QChar *curP, const QChar *endP, int propId,
			bool important, bool nonCSSHint, QPtrList<CSSProperty> *propList);
	bool parseFont(const QChar *curP, const QChar *endP);
	bool parse4Values(const QChar *curP, const QChar *endP, const int *properties);
	bool parseShortHand(const QChar *curP, const QChar *endP, const int *properties, int num);
	void setParsedValue(int propId, const CSSValueImpl *parsedValue);
	void setParsedValue(int propId, const CSSValueImpl *parsedValue,
			    bool important, bool nonCSSHint, QPtrList<CSSProperty> *propList);
	bool isHexadecimal( const QChar &c );
	QPtrList<QChar> splitShorthandProperties(const QChar *curP, const QChar *endP);
	bool parseBackgroundPosition(const QChar *curP, const QChar *&nextP, const QChar *endP);

	/* define CSS_AURAL in cssparser.cpp if you want to parse CSS2 Aural properties */
	bool parse2Values(const QChar *curP, const QChar *endP, const int *properties);
	bool parseAuralValue(const QChar *curP, const QChar *endP, int propId);

        CSSValueImpl* parseContent(const QChar *curP, const QChar *endP);

	// defines units allowed for a certain property, used in parseUnit
	enum Units
	{
	    UNKNOWN   = 0x0000,
	    INTEGER   = 0x0001,
	    NUMBER    = 0x0002,  // real numbers
	    PERCENT   = 0x0004,
	    LENGTH    = 0x0008,
	    ANGLE     = 0x0010,
	    TIME      = 0x0020,
	    FREQUENCY = 0x0040,
	    NONNEGATIVE = 0x0080,
        COLLAPSIBLE = 0x010
	};

	/* called by parseValue, parses numbers+units */
	CSSPrimitiveValueImpl *parseUnit(const QChar * curP, const QChar *endP, int allowedUnits);

	CSSRuleImpl *parseAtRule(const QChar *&curP, const QChar *endP);

	// the caller is responible for deleting the returned lists in the next to methods
	CSSRuleImpl *parseStyleRule(const QChar *&curP, const QChar *endP);
	CSSRuleImpl *parseRule(const QChar *&curP, const QChar *endP);

	virtual bool parseString(const DOMString &/*cssString*/, bool = false) { return false; }

	virtual void checkLoaded();

	void setStrictParsing( bool b ) { strictParsing = b; }
	bool useStrictParsing() const { return strictParsing; }

    protected:
	bool hasInlinedDecl : 1;
	bool strictParsing : 1;
    bool sawDescendantRule : 1;
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

}; // namespace

#endif
