/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef CSS_cssstyleselector_h_
#define CSS_cssstyleselector_h_

#include "css_ruleimpl.h"
#include "PlatformString.h"
#include "render_style.h"

class KHTMLSettings;
class KURL;

namespace WebCore {

class CSSProperty;
class CSSRuleData;
class CSSRuleDataList;
class CSSRuleSet;
class CSSSelector;
class CSSStyleSheetImpl;
class CSSValueImpl;
class DocumentImpl;
class ElementImpl;
class Frame;
class FrameView;
class NodeImpl;
class StyleSheetImpl;
class StyleSheetListImpl;
class StyledElementImpl;

    /**
     * this class selects a RenderStyle for a given Element based on the
     * collection of styleshets it contains. This is just a vrtual base class
     * for specific implementations of the Selector. At the moment only CSSStyleSelector
     * exists, but someone may wish to implement XSL.
     */
    class StyleSelector
    {
    public:
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
        CSSStyleSelector(DocumentImpl* doc, QString userStyleSheet, 
                         StyleSheetListImpl *styleSheets,
                         bool _strictParsing);
        /**
         * same as above but for a single stylesheet.
         */
        CSSStyleSelector(CSSStyleSheetImpl *sheet);
        ~CSSStyleSelector();

        static void loadDefaultStyle();

        void initElementAndPseudoState(ElementImpl* e);
        void initForStyleResolve(ElementImpl* e, RenderStyle* parentStyle);
        RenderStyle *styleForElement(ElementImpl* e, RenderStyle* parentStyle=0, bool allowSharing=true);
        RenderStyle* pseudoStyleForElement(RenderStyle::PseudoId pseudoStyle, 
                                           ElementImpl* e, RenderStyle* parentStyle=0);

        RenderStyle* locateSharedStyle();
        NodeImpl* locateCousinList(ElementImpl* parent);
        bool canShareStyleWithElement(NodeImpl* n);
        
        // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
        RefPtr<CSSRuleListImpl> styleRulesForElement(ElementImpl* e, bool authorOnly);
        RefPtr<CSSRuleListImpl> pseudoStyleRulesForElement(ElementImpl* e, DOMStringImpl* pseudoStyle, bool authorOnly);

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
        
        void setFontSize(FontDescription& FontDescription, float size);
        float getComputedSizeFromSpecifiedSize(bool isAbsoluteSize, float specifiedSize);
        
        Color getColorFromPrimitiveValue(CSSPrimitiveValueImpl* primitiveValue);
        
    protected:

        /* checks if a compound selector (which can consist of multiple simple selectors)
           matches the given Element */
        bool checkSelector(CSSSelector* selector, ElementImpl *e);
        
        /* checks if the selector matches the given Element */
        bool checkOneSelector(CSSSelector *selector, ElementImpl *e);

        /* This function fixes up the default font size if it detects that the
           current generic font family has changed. -dwh */
        void checkForGenericFamilyChange(RenderStyle* aStyle, RenderStyle* aParentStyle);
        void checkForTextSizeAdjust();

        void adjustRenderStyle(RenderStyle* style, ElementImpl *e);
    
        void matchRules(CSSRuleSet* rules, int& firstRuleIndex, int& lastRuleIndex);
        void matchRulesForList(CSSRuleDataList* rules,
                               int& firstRuleIndex, int& lastRuleIndex);
        void sortMatchedRules(uint firstRuleIndex, uint lastRuleIndex);
        void addMatchedRule(CSSRuleData* rule);
        void addMatchedDeclaration(CSSMutableStyleDeclarationImpl* decl);
        void applyDeclarations(bool firstPass, bool important, int startIndex, int endIndex);
        
        static CSSStyleSheetImpl *defaultSheet;
        static CSSStyleSheetImpl *quirksSheet;
#if SVG_SUPPORT
        static CSSStyleSheetImpl *svgSheet;
#endif
        static CSSRuleSet* defaultStyle;
        static CSSRuleSet* defaultQuirksStyle;
        static CSSRuleSet* defaultPrintStyle;
        CSSRuleSet* m_authorStyle;
        CSSRuleSet* m_userStyle;
        CSSStyleSheetImpl* m_userSheet;
        
        bool m_hasUAAppearance;
        BorderData m_borderData;
        BackgroundLayer m_backgroundData;
        Color m_backgroundColor;

public:
        static RenderStyle* styleNotYetAvailable;
 
    private:
        void init();
        
        void mapBackgroundAttachment(BackgroundLayer* layer, CSSValueImpl* value);
        void mapBackgroundClip(BackgroundLayer* layer, CSSValueImpl* value);
        void mapBackgroundOrigin(BackgroundLayer* layer, CSSValueImpl* value);
        void mapBackgroundImage(BackgroundLayer* layer, CSSValueImpl* value);
        void mapBackgroundRepeat(BackgroundLayer* layer, CSSValueImpl* value);
        void mapBackgroundXPosition(BackgroundLayer* layer, CSSValueImpl* value);
        void mapBackgroundYPosition(BackgroundLayer* layer, CSSValueImpl* value);
        
        // We collect the set of decls that match in |m_matchedDecls|.  We then walk the
        // set of matched decls four times, once for those properties that others depend on (like font-size),
        // and then a second time for all the remaining properties.  We then do the same two passes
        // for any !important rules.
        Array<CSSMutableStyleDeclarationImpl*> m_matchedDecls;
        unsigned m_matchedDeclCount;
        
        // A buffer used to hold the set of matched rules for an element, and a temporary buffer used for
        // merge sorting.
        Array<CSSRuleData*> m_matchedRules;
        unsigned m_matchedRuleCount;
        Array<CSSRuleData*> m_tmpRules;
        unsigned m_tmpRuleCount;
        CSSRuleListImpl* m_ruleList;
        bool m_collectRulesOnly;

        QString m_medium;

        RenderStyle::PseudoId dynamicPseudo;
        
        RenderStyle *style;
        RenderStyle *parentStyle;
        ElementImpl *element;
        StyledElementImpl *styledElement;
        NodeImpl *parentNode;
        RenderStyle::PseudoId pseudoStyle;
        FrameView *view;
        Frame *frame;
        const KHTMLSettings *settings;
        bool fontDirty;
        bool isXMLDoc;
        
        void applyProperty(int id, CSSValueImpl *value);
#if SVG_SUPPORT
        void applySVGProperty(int id, CSSValueImpl *value);
#endif
    };

    class CSSRuleData {
    public:
        CSSRuleData(uint pos, CSSStyleRuleImpl* r, CSSSelector* sel, CSSRuleData* prev = 0)
        :m_position(pos), m_rule(r), m_selector(sel), m_next(0) { if (prev) prev->m_next = this; }
        ~CSSRuleData() { delete m_next; }

        uint position() { return m_position; }
        CSSStyleRuleImpl* rule() { return m_rule; }
        CSSSelector* selector() { return m_selector; }
        CSSRuleData* next() { return m_next; }
        
    private:
        uint m_position;
        CSSStyleRuleImpl* m_rule;
        CSSSelector* m_selector;
        CSSRuleData* m_next;
    };

    class CSSRuleDataList {
    public:
        CSSRuleDataList(uint pos, CSSStyleRuleImpl* rule, CSSSelector* sel)
        { m_first = m_last = new CSSRuleData(pos, rule, sel); }
        ~CSSRuleDataList() { delete m_first; }

        CSSRuleData* first() { return m_first; }
        CSSRuleData* last() { return m_last; }
        
        void append(uint pos, CSSStyleRuleImpl* rule, CSSSelector* sel) {
            m_last = new CSSRuleData(pos, rule, sel, m_last);
        }
        
    private:
        CSSRuleData* m_first;
        CSSRuleData* m_last;
    };
    
}

#endif
