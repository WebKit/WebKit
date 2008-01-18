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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef CSSStyleSelector_h
#define CSSStyleSelector_h

#include "CSSFontSelector.h"
#include "DeprecatedString.h"
#include "RenderStyle.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSMutableStyleDeclaration;
class CSSPrimitiveValue;
class CSSProperty;
class CSSFontFace;
class CSSFontFaceRule;
class CSSRuleData;
class CSSRuleDataList;
class CSSRuleList;
class CSSRuleSet;
class CSSSelector;
class CSSStyleRule;
class CSSStyleSheet;
class CSSValue;
class Document;
class Element;
class Frame;
class FrameView;
class KURL;
class MediaQueryEvaluator;
class Node;
class Settings;
class StyleSheet;
class StyleSheetList;
class StyledElement;

    /**
     * this class selects a RenderStyle for a given Element based on the
     * collection of styleshets it contains. This is just a vrtual base class
     * for specific implementations of the Selector. At the moment only CSSStyleSelector
     * exists, but someone may wish to implement XSL.
     */
    class StyleSelector {
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
    class CSSStyleSelector : public StyleSelector {
    public:
        CSSStyleSelector(Document*, const String& userStyleSheet, StyleSheetList*, CSSStyleSheet*, bool strictParsing, bool matchAuthorAndUserStyles);
        ~CSSStyleSelector();

        static void loadDefaultStyle();

        void initElementAndPseudoState(Element*);
        void initForStyleResolve(Element*, RenderStyle* parentStyle);
        RenderStyle* styleForElement(Element*, RenderStyle* parentStyle = 0, bool allowSharing = true, bool resolveForRootDefault = false);
        RenderStyle* pseudoStyleForElement(RenderStyle::PseudoId, Element*, RenderStyle* parentStyle = 0);

        RenderStyle* locateSharedStyle();
        Node* locateCousinList(Element* parent, unsigned depth = 1);
        bool canShareStyleWithElement(Node* n);

        // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
        RefPtr<CSSRuleList> styleRulesForElement(Element*, bool authorOnly);
        RefPtr<CSSRuleList> pseudoStyleRulesForElement(Element*, StringImpl* pseudoStyle, bool authorOnly);

        bool strictParsing;

        struct Encodedurl {
            DeprecatedString host; //also contains protocol
            DeprecatedString path;
            DeprecatedString file;
        } m_encodedURL;

        void setEncodedURL(const KURL& url);

        // Given a CSS keyword in the range (xx-small to -webkit-xxx-large), this function will return
        // the correct font size scaled relative to the user's default (medium).
        float fontSizeForKeyword(int keyword, bool quirksMode, bool monospace) const;

        // When the CSS keyword "larger" is used, this function will attempt to match within the keyword
        // table, and failing that, will simply multiply by 1.2.
        float largerFontSize(float size, bool quirksMode) const;

        // Like the previous function, but for the keyword "smaller".
        float smallerFontSize(float size, bool quirksMode) const;

        void setFontSize(FontDescription&, float size);
        float getComputedSizeFromSpecifiedSize(bool isAbsoluteSize, float specifiedSize);

        Color getColorFromPrimitiveValue(CSSPrimitiveValue*);

        bool hasSelectorForAttribute(const AtomicString&);
 
        CSSFontSelector* fontSelector() { return m_fontSelector.get(); }

        /* checks if a compound selector (which can consist of multiple simple selectors)
           matches the given Element */
        bool checkSelector(CSSSelector*);

    protected:
        enum SelectorMatch {
            SelectorMatches = 0,
            SelectorFailsLocally,
            SelectorFailsCompletely
        };

        SelectorMatch checkSelector(CSSSelector*, Element *, bool isAncestor, bool isSubSelector);

        /* checks if the selector matches the given Element */
        bool checkOneSelector(CSSSelector*, Element*, bool isAncestor, bool isSubSelector = false);

        /* This function fixes up the default font size if it detects that the
           current generic font family has changed. -dwh */
        void checkForGenericFamilyChange(RenderStyle* style, RenderStyle* parentStyle);
        void checkForTextSizeAdjust();

        void adjustRenderStyle(RenderStyle*, Element*);

        void addMatchedRule(CSSRuleData* rule) { m_matchedRules.append(rule); }
        void addMatchedDeclaration(CSSMutableStyleDeclaration* decl) { m_matchedDecls.append(decl); }

        void matchRules(CSSRuleSet*, int& firstRuleIndex, int& lastRuleIndex);
        void matchRulesForList(CSSRuleDataList* rules, int& firstRuleIndex, int& lastRuleIndex);
        void sortMatchedRules(unsigned start, unsigned end);

        void applyDeclarations(bool firstPass, bool important, int startIndex, int endIndex);

        static CSSStyleSheet* m_defaultSheet;
        static CSSStyleSheet* m_quirksSheet;
        static CSSStyleSheet* m_viewSourceSheet;
#if ENABLE(SVG)
        static CSSStyleSheet* m_svgSheet;
#endif

        static CSSRuleSet* m_defaultStyle;
        static CSSRuleSet* m_defaultQuirksStyle;
        static CSSRuleSet* m_defaultPrintStyle;
        static CSSRuleSet* m_defaultViewSourceStyle;

        CSSRuleSet* m_authorStyle;
        CSSRuleSet* m_userStyle;
        RefPtr<CSSStyleSheet> m_userSheet;

        bool m_hasUAAppearance;
        BorderData m_borderData;
        BackgroundLayer m_backgroundData;
        Color m_backgroundColor;

    public:
        static RenderStyle* m_styleNotYetAvailable;

    private:
        void init();

        void matchUARules(int& firstUARule, int& lastUARule);
        void updateFont();
        void cacheBorderAndBackground();

        void mapBackgroundAttachment(BackgroundLayer*, CSSValue*);
        void mapBackgroundClip(BackgroundLayer*, CSSValue*);
        void mapBackgroundComposite(BackgroundLayer*, CSSValue*);
        void mapBackgroundOrigin(BackgroundLayer*, CSSValue*);
        void mapBackgroundImage(BackgroundLayer*, CSSValue*);
        void mapBackgroundRepeat(BackgroundLayer*, CSSValue*);
        void mapBackgroundSize(BackgroundLayer*, CSSValue*);
        void mapBackgroundXPosition(BackgroundLayer*, CSSValue*);
        void mapBackgroundYPosition(BackgroundLayer*, CSSValue*);

        void mapTransitionDuration(Transition*, CSSValue*);
        void mapTransitionRepeatCount(Transition*, CSSValue*);
        void mapTransitionTimingFunction(Transition*, CSSValue*);
        void mapTransitionProperty(Transition*, CSSValue*);

        // We collect the set of decls that match in |m_matchedDecls|.  We then walk the
        // set of matched decls four times, once for those properties that others depend on (like font-size),
        // and then a second time for all the remaining properties.  We then do the same two passes
        // for any !important rules.
        Vector<CSSMutableStyleDeclaration*> m_matchedDecls;

        // A buffer used to hold the set of matched rules for an element, and a temporary buffer used for
        // merge sorting.
        Vector<CSSRuleData*> m_matchedRules;

        CSSRuleList* m_ruleList;
        bool m_collectRulesOnly;

        MediaQueryEvaluator* m_medium;
        RenderStyle* m_rootDefaultStyle;

        RenderStyle::PseudoId dynamicPseudo;

        Document* m_document; // back pointer to owner document
        RenderStyle* m_style;
        RenderStyle* m_parentStyle;
        Element* m_element;
        StyledElement* m_styledElement;
        Node* m_parentNode;
        RenderStyle::PseudoId m_pseudoStyle;
        CSSValue* m_lineHeightValue;
        bool m_fontDirty;
        bool m_isXMLDoc;
        bool m_matchAuthorAndUserStyles;

        RefPtr<CSSFontSelector> m_fontSelector;

        HashSet<AtomicStringImpl*> m_selectorAttrs;
        
        Vector<CSSMutableStyleDeclaration*> m_additionalAttributeStyleDecls;
        
        void applyProperty(int id, CSSValue*);

#if ENABLE(SVG)
        void applySVGProperty(int id, CSSValue*);
#endif

        friend class CSSRuleSet;
        friend class Node;
    };

    class CSSRuleData {
    public:
        CSSRuleData(unsigned pos, CSSStyleRule* r, CSSSelector* sel, CSSRuleData* prev = 0)
            : m_position(pos)
            , m_rule(r)
            , m_selector(sel)
            , m_next(0)
        {
            if (prev)
                prev->m_next = this;
        }

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
            : m_first(new CSSRuleData(pos, rule, sel))
            , m_last(m_first)
        {
        }

        ~CSSRuleDataList() { delete m_first; }

        CSSRuleData* first() { return m_first; }
        CSSRuleData* last() { return m_last; }

        void append(unsigned pos, CSSStyleRule* rule, CSSSelector* sel) { m_last = new CSSRuleData(pos, rule, sel, m_last); }

    private:
        CSSRuleData* m_first;
        CSSRuleData* m_last;
    };
    
} // namespace WebCore

#endif // CSSStyleSelector_h
