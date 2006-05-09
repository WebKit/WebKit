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
#include "render_style.h"

class KHTMLSettings;
class KURL;

namespace WebCore {

class CSSProperty;
class CSSRuleData;
class CSSRuleDataList;
class CSSRuleSet;
class CSSSelector;
class CSSStyleSheet;
class CSSValue;
class Document;
class Element;
class Frame;
class FrameView;
class Node;
class StyleSheet;
class StyleSheetList;
class StyledElement;

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
        CSSStyleSelector(Document*, const String& userStyleSheet, StyleSheetList *styleSheets, bool strictParsing);
        /**
         * same as above but for a single stylesheet.
         */
        CSSStyleSelector(CSSStyleSheet *sheet);
        ~CSSStyleSelector();

        static void loadDefaultStyle();

        void initElementAndPseudoState(Element* e);
        void initForStyleResolve(Element* e, RenderStyle* parentStyle);
        RenderStyle *styleForElement(Element*, RenderStyle* parentStyle=0, bool allowSharing=true);
        RenderStyle* pseudoStyleForElement(RenderStyle::PseudoId, Element*, RenderStyle* parentStyle=0);

        RenderStyle* locateSharedStyle();
        Node* locateCousinList(Element* parent);
        bool canShareStyleWithElement(Node* n);
        
        // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
        RefPtr<CSSRuleList> styleRulesForElement(Element* e, bool authorOnly);
        RefPtr<CSSRuleList> pseudoStyleRulesForElement(Element* e, StringImpl* pseudoStyle, bool authorOnly);

        bool strictParsing;
        
        struct Encodedurl {
            DeprecatedString host; //also contains protocol
            DeprecatedString path;
            DeprecatedString file;
        } encodedurl;
        void setEncodedURL(const KURL& url);
        
        // Given a CSS keyword in the range (xx-small to -webkit-xxx-large), this function will return
        // the correct font size scaled relative to the user's default (medium).
        float fontSizeForKeyword(int keyword, bool quirksMode) const;
        
        // When the CSS keyword "larger" is used, this function will attempt to match within the keyword
        // table, and failing that, will simply multiply by 1.2.
        float largerFontSize(float size, bool quirksMode) const;
        
        // Like the previous function, but for the keyword "smaller".
        float smallerFontSize(float size, bool quirksMode) const;
        
        void setFontSize(FontDescription& FontDescription, float size);
        float getComputedSizeFromSpecifiedSize(bool isAbsoluteSize, float specifiedSize);
        
        Color getColorFromPrimitiveValue(CSSPrimitiveValue* primitiveValue);
        
    protected:

        /* checks if a compound selector (which can consist of multiple simple selectors)
           matches the given Element */
        bool checkSelector(CSSSelector* selector, Element *e);
        
        /* checks if the selector matches the given Element */
        bool checkOneSelector(CSSSelector*, Element*, bool isSubSelector = false);

        /* This function fixes up the default font size if it detects that the
           current generic font family has changed. -dwh */
        void checkForGenericFamilyChange(RenderStyle* aStyle, RenderStyle* aParentStyle);
        void checkForTextSizeAdjust();

        void adjustRenderStyle(RenderStyle* style, Element *e);
    
        void matchRules(CSSRuleSet* rules, int& firstRuleIndex, int& lastRuleIndex);
        void matchRulesForList(CSSRuleDataList* rules,
                               int& firstRuleIndex, int& lastRuleIndex);
        void sortMatchedRules(unsigned firstRuleIndex, unsigned lastRuleIndex);
        void addMatchedRule(CSSRuleData* rule);
        void addMatchedDeclaration(CSSMutableStyleDeclaration* decl);
        void applyDeclarations(bool firstPass, bool important, int startIndex, int endIndex);
        
        static CSSStyleSheet *defaultSheet;
        static CSSStyleSheet *quirksSheet;
#if SVG_SUPPORT
        static CSSStyleSheet *svgSheet;
#endif
        static CSSRuleSet* defaultStyle;
        static CSSRuleSet* defaultQuirksStyle;
        static CSSRuleSet* defaultPrintStyle;
        CSSRuleSet* m_authorStyle;
        CSSRuleSet* m_userStyle;
        CSSStyleSheet* m_userSheet;
        
        bool m_hasUAAppearance;
        BorderData m_borderData;
        BackgroundLayer m_backgroundData;
        Color m_backgroundColor;

public:
        static RenderStyle* styleNotYetAvailable;
 
    private:
        void init();
        
        void mapBackgroundAttachment(BackgroundLayer* layer, CSSValue* value);
        void mapBackgroundClip(BackgroundLayer* layer, CSSValue* value);
        void mapBackgroundOrigin(BackgroundLayer* layer, CSSValue* value);
        void mapBackgroundImage(BackgroundLayer* layer, CSSValue* value);
        void mapBackgroundRepeat(BackgroundLayer* layer, CSSValue* value);
        void mapBackgroundSize(BackgroundLayer* layer, CSSValue* value);
        void mapBackgroundXPosition(BackgroundLayer* layer, CSSValue* value);
        void mapBackgroundYPosition(BackgroundLayer* layer, CSSValue* value);
        
        // We collect the set of decls that match in |m_matchedDecls|.  We then walk the
        // set of matched decls four times, once for those properties that others depend on (like font-size),
        // and then a second time for all the remaining properties.  We then do the same two passes
        // for any !important rules.
        DeprecatedArray<CSSMutableStyleDeclaration*> m_matchedDecls;
        unsigned m_matchedDeclCount;
        
        // A buffer used to hold the set of matched rules for an element, and a temporary buffer used for
        // merge sorting.
        DeprecatedArray<CSSRuleData*> m_matchedRules;
        unsigned m_matchedRuleCount;
        DeprecatedArray<CSSRuleData*> m_tmpRules;
        unsigned m_tmpRuleCount;
        CSSRuleList* m_ruleList;
        bool m_collectRulesOnly;

        String m_mediaType;

        RenderStyle::PseudoId dynamicPseudo;
        
        RenderStyle *style;
        RenderStyle *parentStyle;
        Element *element;
        StyledElement *styledElement;
        Node *parentNode;
        RenderStyle::PseudoId pseudoStyle;
        FrameView *view;
        Frame *frame;
        const KHTMLSettings *settings;
        bool fontDirty;
        bool isXMLDoc;
        
        void applyProperty(int id, CSSValue *value);
#if SVG_SUPPORT
        void applySVGProperty(int id, CSSValue *value);
#endif
    };

    class CSSRuleData {
    public:
        CSSRuleData(unsigned pos, CSSStyleRule* r, CSSSelector* sel, CSSRuleData* prev = 0)
        :m_position(pos), m_rule(r), m_selector(sel), m_next(0) { if (prev) prev->m_next = this; }
        ~CSSRuleData() { delete m_next; }

        unsigned position() { return m_position; }
        CSSStyleRule* rule() { return m_rule; }
        CSSSelector* selector() { return m_selector; }
        CSSRuleData* next() { return m_next; }
        
    private:
        unsigned m_position;
        CSSStyleRule* m_rule;
        CSSSelector* m_selector;
        CSSRuleData* m_next;
    };

    class CSSRuleDataList {
    public:
        CSSRuleDataList(unsigned pos, CSSStyleRule* rule, CSSSelector* sel)
        { m_first = m_last = new CSSRuleData(pos, rule, sel); }
        ~CSSRuleDataList() { delete m_first; }

        CSSRuleData* first() { return m_first; }
        CSSRuleData* last() { return m_last; }
        
        void append(unsigned pos, CSSStyleRule* rule, CSSSelector* sel) {
            m_last = new CSSRuleData(pos, rule, sel, m_last);
        }
        
    private:
        CSSRuleData* m_first;
        CSSRuleData* m_last;
    };
    
}

#endif
