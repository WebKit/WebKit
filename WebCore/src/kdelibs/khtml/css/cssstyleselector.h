/**
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
 * $Id$
 */
#ifndef _CSS_cssstyleselector_h_
#define _CSS_cssstyleselector_h_

#include <qlist.h>

#include "rendering/render_style.h"
#include "dom/dom_string.h"

class KHTMLSettings;

namespace DOM {
    class ElementImpl;
    class DocumentImpl;
    class HTMLDocumentImpl;
    class StyleSheetImpl;
    class CSSStyleRuleImpl;
    class CSSStyleSheetImpl;
    class CSSSelector;
    class CSSStyleDeclarationImpl;
    class CSSProperty;
}

namespace khtml
{
    class CSSStyleSelectorList;
    class CSSOrderedRule;
    class CSSOrderedProperty;
    class CSSOrderedPropertyList;
    class RenderStyle;

    // independent of classes. Applies on styleDeclaration to the RenderStyle style
    void applyRule(khtml::RenderStyle *style, DOM::CSSProperty *prop,
		   DOM::ElementImpl *e);

    /*
     * to remember the source where a rule came from. Differntiates between
     * important and not important rules. This is ordered in the order they have to be applied 
     * to the RenderStyle.
     */
    enum Source {
	Default = 0,
	User = 1,
	NonCSSHint = 2,
	Author = 3,
	Inline = 4,
	AuthorImportant = 5,
	InlineImportant = 6,
	UserImportant =7
    };

    /**
     * this class selects a RenderStyle for a given Element based on the
     * collection of styleshets it contains. This is just a vrtual base class
     * for specific implementations of the Selector. At the moment only CSSStyleSelector
     * exists, but someone may wish to implement XSL...
     */
    class StyleSelector
    {
    public:
	StyleSelector() {};
	virtual ~StyleSelector() {};
	
	virtual RenderStyle *styleForElement(DOM::ElementImpl *e, int = None) = 0;
	
	enum State {
	    None = 0x00,
	    Hover = 0x01,
	    Focus = 0x02,
	    Active = 0x04
	};
    };


    /**
     * the StyleSelector implementation for CSS.
     */
    class CSSStyleSelector : public StyleSelector
    {
    public:
	/**
	 * creates a new StyleSelector for a Document.
	 * goes through all StyleSheets defined in the document and
	 * creates a list of rules it needs to apply to objects
	 *
	 * Also takes into account special cases for HTML documents,
	 * including the defaultStyle (which is html only)
	 */
	CSSStyleSelector(DOM::DocumentImpl *doc);
	/**
	 * same as above but for a single stylesheet.
	 */
	CSSStyleSelector(DOM::StyleSheetImpl *sheet);

	virtual ~CSSStyleSelector();
	
	void addSheet(DOM::StyleSheetImpl *sheet);
        
	static void loadDefaultStyle(const KHTMLSettings *s = 0);
	static void clear();
	
	virtual RenderStyle *styleForElement(DOM::ElementImpl *e, int state = None );
	
	bool strictParsing;
	struct Encodedurl {
	    QString host; //also contains protocol
	    QString path;
	    QString file;
	} encodedurl;
    protected:

	/* checks if the complete selector (which can be build up from a few CSSSelector's
	    with given relationships matches the given Element */
	void checkSelector(int selector, DOM::ElementImpl *e);
	/* checks if the selector matches the given Element */
	bool checkOneSelector(DOM::CSSSelector *selector, DOM::ElementImpl *e);

	/* builds up the selectors and properties lists from the CSSStyleSelectorList's */
	void buildLists();
	void clearLists();

	void addInlineDeclarations(DOM::CSSStyleDeclarationImpl *decl,
				   CSSOrderedPropertyList *list );
	
	static DOM::CSSStyleSheetImpl *defaultSheet;
	static CSSStyleSelectorList *defaultStyle;
	CSSStyleSelectorList *authorStyle;
        CSSStyleSelectorList *userStyle;
        DOM::CSSStyleSheetImpl *userSheet;
	
    public: // we need to make the enum public for SelectorCache
	enum SelectorState {
	    Unknown = 0,
	    Applies,
	    AppliesPseudo,
	    Invalid
	};
    protected:

        struct SelectorCache {
            SelectorState state;
            unsigned int props_size;
            int *props;
        };
            
	unsigned int selectors_size;
	DOM::CSSSelector **selectors;
	SelectorCache *selectorCache;
	unsigned int properties_size;
	CSSOrderedProperty **properties;
	QArray<CSSOrderedProperty> inlineProps;
    };

    /*
     * List of properties that get applied to the Element. We need to collect them first
     * and then apply them one by one, because we have to change the apply order.
     * Some properties depend on other one already being applied (for example all properties spezifying
     * some length need to have already the correct font size. Same applies to color
     *
     * While sorting them, we have to take care not to mix up the original order.
     */
    class CSSOrderedProperty
    {
    public:	
	CSSOrderedProperty(DOM::CSSProperty *_prop, uint _selector, 
			   bool first, Source source, unsigned int specificity, 
			   unsigned int _position )
	    : prop ( _prop ), pseudoId( RenderStyle::NOPSEUDO ), selector( _selector ),
	      position( _position )
	{
	    priority = (!first << 30) | (source << 24) | specificity;
	}

	DOM::CSSProperty *prop;
	RenderStyle::PseudoId pseudoId;
	unsigned int selector;
	unsigned int position;
	
	Q_UINT32 priority;
    };

    /*
     * This is the list we will collect all properties we need to apply in.
     * It will get sorted once before applying.
     */
    class CSSOrderedPropertyList : public QList<CSSOrderedProperty>
    {
    public:
	virtual int compareItems(QCollection::Item i1, QCollection::Item i2);
	void append(DOM::CSSStyleDeclarationImpl *decl, uint selector, uint specificity, 
		    Source regular, Source important );
    };

    class CSSOrderedRule
    {
    public:
	CSSOrderedRule(DOM::CSSStyleRuleImpl *r, DOM::CSSSelector *s, int _index);
	~CSSOrderedRule();
	
	DOM::CSSSelector *selector;
	DOM::CSSStyleRuleImpl *rule;
	int index;
    };

    class CSSStyleSelectorList : public QList<CSSOrderedRule>
    {
    public:
	CSSStyleSelectorList();
	virtual ~CSSStyleSelectorList();
	
	void append(DOM::StyleSheetImpl *sheet);
	
	void collect( QList<DOM::CSSSelector> *selectorList, CSSOrderedPropertyList *propList,
		      Source regular, Source important );
    };

};
#endif
