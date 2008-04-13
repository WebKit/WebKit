/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "MediaQueryExp.h"
#include "RenderStyle.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/HashMap.h>
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
class StyleImage;
class StyleSheet;
class StyleSheetList;
class StyledElement;

class MediaQueryResult {
public:
    MediaQueryResult(const MediaQueryExp& expr, bool result)
        : m_expression(expr)
        , m_result(result)
    {
    }

    MediaQueryExp m_expression;
    bool m_result;
};

    // This class selects a RenderStyle for a given element based on a collection of stylesheets.
    class CSSStyleSelector : Noncopyable {
    public:
        CSSStyleSelector(Document*, const String& userStyleSheet, StyleSheetList*, CSSStyleSheet*, bool strictParsing, bool matchAuthorAndUserStyles);
        ~CSSStyleSelector();

        void initElementAndPseudoState(Element*);
        void initForStyleResolve(Element*, RenderStyle* parentStyle);
        RenderStyle* styleForElement(Element*, RenderStyle* parentStyle = 0, bool allowSharing = true, bool resolveForRootDefault = false);

        RenderStyle* pseudoStyleForElement(RenderStyle::PseudoId, Element*, RenderStyle* parentStyle = 0);

    private:
        RenderStyle* locateSharedStyle();
        Node* locateCousinList(Element* parent, unsigned depth = 1);
        bool canShareStyleWithElement(Node*);

    public:
        // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
        RefPtr<CSSRuleList> styleRulesForElement(Element*, bool authorOnly);
        RefPtr<CSSRuleList> pseudoStyleRulesForElement(Element*, const String& pseudoStyle, bool authorOnly);

        // Given a CSS keyword in the range (xx-small to -webkit-xxx-large), this function will return
        // the correct font size scaled relative to the user's default (medium).
        float fontSizeForKeyword(int keyword, bool quirksMode, bool monospace) const;

    private:
        // When the CSS keyword "larger" is used, this function will attempt to match within the keyword
        // table, and failing that, will simply multiply by 1.2.
        float largerFontSize(float size, bool quirksMode) const;

        // Like the previous function, but for the keyword "smaller".
        float smallerFontSize(float size, bool quirksMode) const;

    public:
        void setStyle(RenderStyle* s) { m_style = s; } // Used by the document when setting up its root style.
        void setFontSize(FontDescription&, float size);

    private:
        float getComputedSizeFromSpecifiedSize(bool isAbsoluteSize, float specifiedSize);

    public:
        Color getColorFromPrimitiveValue(CSSPrimitiveValue*);

        bool hasSelectorForAttribute(const AtomicString&);
 
        CSSFontSelector* fontSelector() { return m_fontSelector.get(); }

        // Checks if a compound selector (which can consist of multiple simple selectors) matches the current element.
        bool checkSelector(CSSSelector*);

        void addViewportDependentMediaQueryResult(const MediaQueryExp*, bool result);

        bool affectedByViewportChange() const;

        void allVisitedStateChanged();
        void visitedStateChanged(unsigned visitedHash);

    private:
        enum SelectorMatch { SelectorMatches, SelectorFailsLocally, SelectorFailsCompletely };
        SelectorMatch checkSelector(CSSSelector*, Element*, bool isAncestor, bool isSubSelector);

        // Checks if the selector matches the given Element.
        bool checkOneSelector(CSSSelector*, Element*, bool isAncestor, bool isSubSelector = false);

        // This function fixes up the default font size if it detects that the current generic font family has changed. -dwh
        void checkForGenericFamilyChange(RenderStyle*, RenderStyle* parentStyle);
        void checkForTextSizeAdjust();

        void adjustRenderStyle(RenderStyle*, Element*);

        void addMatchedRule(CSSRuleData* rule) { m_matchedRules.append(rule); }
        void addMatchedDeclaration(CSSMutableStyleDeclaration* decl) { m_matchedDecls.append(decl); }

        void matchRules(CSSRuleSet*, int& firstRuleIndex, int& lastRuleIndex);
        void matchRulesForList(CSSRuleDataList*, int& firstRuleIndex, int& lastRuleIndex);
        void sortMatchedRules(unsigned start, unsigned end);

        void applyDeclarations(bool firstPass, bool important, int startIndex, int endIndex);

        bool m_strictParsing;

        CSSRuleSet* m_authorStyle;
        CSSRuleSet* m_userStyle;
        RefPtr<CSSStyleSheet> m_userSheet;

        bool m_hasUAAppearance;
        BorderData m_borderData;
        BackgroundLayer m_backgroundData;
        Color m_backgroundColor;

    public:
        static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    private:
        static RenderStyle* s_styleNotYetAvailable;

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

        void applyProperty(int id, CSSValue*);
#if ENABLE(SVG)
        void applySVGProperty(int id, CSSValue*);
#endif

        PassRefPtr<StyleImage> createStyleImage(CSSValue* value);
        
        PseudoState checkPseudoState(Element*, bool checkVisited = true);

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
        Vector<MediaQueryResult*> m_viewportDependentMediaQueryResults;

        HashSet<unsigned, AlreadyHashed> m_linksCheckedForVisitedState;
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
