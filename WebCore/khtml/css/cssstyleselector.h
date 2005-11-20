/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
#ifndef _CSS_cssstyleselector_h_
#define _CSS_cssstyleselector_h_

#include <qptrvector.h>

#include "rendering/render_style.h"
#include "dom/dom_string.h"
#include "xml/dom_stringimpl.h"
#include "css/css_ruleimpl.h"

class KHTMLSettings;
class KHTMLView;
class KHTMLPart;
class KURL;

namespace DOM {
    class DocumentImpl;
    class NodeImpl;
    class ElementImpl;
    class StyledElementImpl;
    class StyleSheetImpl;
    class CSSStyleSheetImpl;
    class CSSSelector;
    class CSSProperty;
    class StyleSheetListImpl;
    class CSSValueImpl;
}

namespace khtml
{
    class CSSRuleData;
    class CSSRuleDataList;
    class CSSRuleSet;
    class RenderStyle;

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

	/* as noone has implemented a second style selector up to now comment out
	   the virtual methods until then, so the class has no vptr.
	*/
// 	virtual ~StyleSelector() {};
// 	virtual RenderStyle *styleForElement(DOM::ElementImpl *e) = 0;

	enum State {
	    None = 0x00,
	    Hover = 0x01,
	    Focus = 0x02,
	    Active = 0x04,
	    Drag = 0x08
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
	 */
	CSSStyleSelector(DOM::DocumentImpl* doc, QString userStyleSheet, 
                         DOM::StyleSheetListImpl *styleSheets,
                         bool _strictParsing);
	/**
	 * same as above but for a single stylesheet.
	 */
	CSSStyleSelector(DOM::CSSStyleSheetImpl *sheet);
	~CSSStyleSelector();

	static void loadDefaultStyle(const KHTMLSettings *s = 0);

        void initElementAndPseudoState(DOM::ElementImpl* e);
        void initForStyleResolve(DOM::ElementImpl* e, RenderStyle* parentStyle);
	RenderStyle *styleForElement(DOM::ElementImpl* e, RenderStyle* parentStyle=0, bool allowSharing=true);
        RenderStyle* pseudoStyleForElement(RenderStyle::PseudoId pseudoStyle, 
                                           DOM::ElementImpl* e, RenderStyle* parentStyle=0);

        RenderStyle* locateSharedStyle();
        DOM::NodeImpl* locateCousinList(DOM::ElementImpl* parent);
        bool canShareStyleWithElement(DOM::NodeImpl* n);
        
	bool strictParsing;
	
        struct Encodedurl {
	    QString host; //also contains protocol
	    QString path;
	    QString file;
	} encodedurl;
        void setEncodedURL(const KURL& url);
        
        // Given a CSS keyword in the range (xx-small to -khtml-xxx-large), this function will return
        // the correct font size scaled relative to the user's default (medium).
        float fontSizeForKeyword(int keyword, bool quirksMode) const;
        
        // When the CSS keyword "larger" is used, this function will attempt to match within the keyword
        // table, and failing that, will simply multiply by 1.2.
        float largerFontSize(float size, bool quirksMode) const;
        
        // Like the previous function, but for the keyword "smaller".
        float smallerFontSize(float size, bool quirksMode) const;
        
        void setFontSize(FontDef& fontDef, float size);
        float getComputedSizeFromSpecifiedSize(bool isAbsoluteSize, float specifiedSize);
        
        QColor getColorFromPrimitiveValue(DOM::CSSPrimitiveValueImpl* primitiveValue);
        
        QPaintDeviceMetrics* paintMetrics() const { return paintDeviceMetrics; }

    protected:

	/* checks if a compound selector (which can consist of multiple simple selectors)
           matches the given Element */
        bool checkSelector(DOM::CSSSelector* selector, DOM::ElementImpl *e);
        
	/* checks if the selector matches the given Element */
	bool checkOneSelector(DOM::CSSSelector *selector, DOM::ElementImpl *e);

	/* This function fixes up the default font size if it detects that the
	   current generic font family has changed. -dwh */
	void checkForGenericFamilyChange(RenderStyle* aStyle, RenderStyle* aParentStyle);
#if APPLE_CHANGES
        void checkForTextSizeAdjust();
#endif

        void adjustRenderStyle(RenderStyle* style, DOM::ElementImpl *e);
    
        void matchRules(CSSRuleSet* rules, int& firstRuleIndex, int& lastRuleIndex);
        void matchRulesForList(CSSRuleDataList* rules,
                               int& firstRuleIndex, int& lastRuleIndex);
        void sortMatchedRules(uint firstRuleIndex, uint lastRuleIndex);
        void addMatchedRule(CSSRuleData* rule);
        void addMatchedDeclaration(DOM::CSSMutableStyleDeclarationImpl* decl);
        void applyDeclarations(bool firstPass, bool important, int startIndex, int endIndex);
        
	static DOM::CSSStyleSheetImpl *defaultSheet;
        static DOM::CSSStyleSheetImpl *quirksSheet;
	static CSSRuleSet* defaultStyle;
        static CSSRuleSet* defaultQuirksStyle;
	static CSSRuleSet* defaultPrintStyle;
	CSSRuleSet* m_authorStyle;
        CSSRuleSet* m_userStyle;
        DOM::CSSStyleSheetImpl* m_userSheet;
        
        bool m_hasUAAppearance;
        BorderData m_borderData;
        BackgroundLayer m_backgroundData;
        QColor m_backgroundColor;

public:
	static RenderStyle* styleNotYetAvailable;
 
    private:
        void init();
        
        void mapBackgroundAttachment(BackgroundLayer* layer, DOM::CSSValueImpl* value);
        void mapBackgroundClip(BackgroundLayer* layer, DOM::CSSValueImpl* value);
        void mapBackgroundOrigin(BackgroundLayer* layer, DOM::CSSValueImpl* value);
        void mapBackgroundImage(BackgroundLayer* layer, DOM::CSSValueImpl* value);
        void mapBackgroundRepeat(BackgroundLayer* layer, DOM::CSSValueImpl* value);
        void mapBackgroundXPosition(BackgroundLayer* layer, DOM::CSSValueImpl* value);
        void mapBackgroundYPosition(BackgroundLayer* layer, DOM::CSSValueImpl* value);
        
    protected:
        // We collect the set of decls that match in |m_matchedDecls|.  We then walk the
        // set of matched decls four times, once for those properties that others depend on (like font-size),
        // and then a second time for all the remaining properties.  We then do the same two passes
        // for any !important rules.
        QMemArray<DOM::CSSMutableStyleDeclarationImpl*> m_matchedDecls;
        unsigned m_matchedDeclCount;
        
        // A buffer used to hold the set of matched rules for an element, and a temporary buffer used for
        // merge sorting.
        QMemArray<CSSRuleData*> m_matchedRules;
        unsigned m_matchedRuleCount;
        QMemArray<CSSRuleData*> m_tmpRules;
        unsigned m_tmpRuleCount;
        
        QString m_medium;

	RenderStyle::PseudoId dynamicPseudo;
	
	RenderStyle *style;
	RenderStyle *parentStyle;
	DOM::ElementImpl *element;
        DOM::StyledElementImpl *styledElement;
	DOM::NodeImpl *parentNode;
        RenderStyle::PseudoId pseudoStyle;
	KHTMLView *view;
	KHTMLPart *part;
	const KHTMLSettings *settings;
	QPaintDeviceMetrics *paintDeviceMetrics;
        bool fontDirty;
        bool isXMLDoc;
        
	void applyProperty(int id, DOM::CSSValueImpl *value);
    };

    class CSSRuleData {
    public:
        CSSRuleData(uint pos, DOM::CSSStyleRuleImpl* r, DOM::CSSSelector* sel, CSSRuleData* prev = 0)
        :m_position(pos), m_rule(r), m_selector(sel), m_next(0) { if (prev) prev->m_next = this; }
        ~CSSRuleData() { delete m_next; }

        uint position() { return m_position; }
        DOM::CSSStyleRuleImpl* rule() { return m_rule; }
        DOM::CSSSelector* selector() { return m_selector; }
        CSSRuleData* next() { return m_next; }
        
    private:
        uint m_position;
        DOM::CSSStyleRuleImpl* m_rule;
        DOM::CSSSelector* m_selector;
        CSSRuleData* m_next;
    };

    class CSSRuleDataList {
    public:
        CSSRuleDataList(uint pos, DOM::CSSStyleRuleImpl* rule, DOM::CSSSelector* sel)
        { m_first = m_last = new CSSRuleData(pos, rule, sel); }
        ~CSSRuleDataList() { delete m_first; }

        CSSRuleData* first() { return m_first; }
        CSSRuleData* last() { return m_last; }
        
        void append(uint pos, DOM::CSSStyleRuleImpl* rule, DOM::CSSSelector* sel) {
            m_last = new CSSRuleData(pos, rule, sel, m_last);
        }
        
    private:
        CSSRuleData* m_first;
        CSSRuleData* m_last;
    };
    
}
#endif
