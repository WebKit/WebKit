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
#include "KeyframeList.h"
#include "LinkHash.h"
#include "MediaQueryExp.h"
#include "RenderStyle.h"
#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

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
class CSSVariableDependentValue;
class CSSVariablesRule;
class DataGridColumn;
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
class WebKitCSSKeyframesRule;

class MediaQueryResult : public Noncopyable {
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
    class CSSStyleSelector : public Noncopyable {
    public:
        CSSStyleSelector(Document*, StyleSheetList* authorSheets, CSSStyleSheet* mappedElementSheet,
                         CSSStyleSheet* pageUserSheet, const Vector<RefPtr<CSSStyleSheet> >* pageGroupUserSheets,
                         bool strictParsing, bool matchAuthorAndUserStyles);
        ~CSSStyleSelector();

        void initElementAndPseudoState(Element*);
        void initForStyleResolve(Element*, RenderStyle* parentStyle = 0, PseudoId = NOPSEUDO);
        PassRefPtr<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle = 0, bool allowSharing = true, bool resolveForRootDefault = false);
        void keyframeStylesForAnimation(Element*, const RenderStyle*, KeyframeList& list);

        PassRefPtr<RenderStyle> pseudoStyleForElement(PseudoId, Element*, RenderStyle* parentStyle = 0);

#if ENABLE(DATAGRID)
        // Datagrid style computation (uses unique pseudo elements and structures)
        PassRefPtr<RenderStyle> pseudoStyleForDataGridColumn(DataGridColumn*, RenderStyle* parentStyle);
        PassRefPtr<RenderStyle> pseudoStyleForDataGridColumnHeader(DataGridColumn*, RenderStyle* parentStyle);
#endif

    private:
        RenderStyle* locateSharedStyle();
        Node* locateCousinList(Element* parent, unsigned depth = 1);
        bool canShareStyleWithElement(Node*);

        RenderStyle* style() const { return m_style.get(); }

    public:
        // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
        PassRefPtr<CSSRuleList> styleRulesForElement(Element*, bool authorOnly);
        PassRefPtr<CSSRuleList> pseudoStyleRulesForElement(Element*, const String& pseudoStyle, bool authorOnly);

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
        void setStyle(PassRefPtr<RenderStyle> s) { m_style = s; } // Used by the document when setting up its root style.
        void setFontSize(FontDescription&, float size);

        void applyPropertyToStyle(int id, CSSValue*, RenderStyle*);

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

        void allVisitedStateChanged() { m_checker.allVisitedStateChanged(); }
        void visitedStateChanged(LinkHash visitedHash) { m_checker.visitedStateChanged(visitedHash); }

        void addVariables(CSSVariablesRule* variables);
        CSSValue* resolveVariableDependentValue(CSSVariableDependentValue*);
        void resolveVariablesForDeclaration(CSSMutableStyleDeclaration* decl, CSSMutableStyleDeclaration* newDecl, HashSet<String>& usedBlockVariables);

        void addKeyframeStyle(PassRefPtr<WebKitCSSKeyframesRule> rule);

        static bool createTransformOperations(CSSValue* inValue, RenderStyle* inStyle, RenderStyle* rootStyle, TransformOperations& outOperations);

    private:
        enum SelectorMatch { SelectorMatches, SelectorFailsLocally, SelectorFailsCompletely };

        // This function fixes up the default font size if it detects that the current generic font family has changed. -dwh
        void checkForGenericFamilyChange(RenderStyle*, RenderStyle* parentStyle);
        void checkForZoomChange(RenderStyle*, RenderStyle* parentStyle);
        void checkForTextSizeAdjust();

        void adjustRenderStyle(RenderStyle*, Element*);

        void addMatchedRule(CSSRuleData* rule) { m_matchedRules.append(rule); }
        void addMatchedDeclaration(CSSMutableStyleDeclaration* decl);

        void matchRules(CSSRuleSet*, int& firstRuleIndex, int& lastRuleIndex);
        void matchRulesForList(CSSRuleDataList*, int& firstRuleIndex, int& lastRuleIndex);
        void sortMatchedRules(unsigned start, unsigned end);

        void applyDeclarations(bool firstPass, bool important, int startIndex, int endIndex);
        
        CSSRuleSet* m_authorStyle;
        CSSRuleSet* m_userStyle;

        bool m_hasUAAppearance;
        BorderData m_borderData;
        FillLayer m_backgroundData;
        Color m_backgroundColor;

        typedef HashMap<AtomicStringImpl*, RefPtr<WebKitCSSKeyframesRule> > KeyframesRuleMap;
        KeyframesRuleMap m_keyframesRuleMap;

    public:
        static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

        class SelectorChecker : public Noncopyable {
        public:
            SelectorChecker(Document*, bool strictParsing);

            bool checkSelector(CSSSelector*, Element*) const;
            SelectorMatch checkSelector(CSSSelector*, Element*, HashSet<AtomicStringImpl*>* selectorAttrs, PseudoId& dynamicPseudo, bool isAncestor, bool isSubSelector, RenderStyle* = 0, RenderStyle* elementParentStyle = 0) const;
            bool checkOneSelector(CSSSelector*, Element*, HashSet<AtomicStringImpl*>* selectorAttrs, PseudoId& dynamicPseudo, bool isAncestor, bool isSubSelector, RenderStyle*, RenderStyle* elementParentStyle) const;
            PseudoState checkPseudoState(Element*, bool checkVisited = true) const;
            bool checkScrollbarPseudoClass(CSSSelector*, PseudoId& dynamicPseudo) const;

            void allVisitedStateChanged();
            void visitedStateChanged(LinkHash visitedHash);

            Document* m_document;
            bool m_strictParsing;
            bool m_collectRulesOnly;
            PseudoId m_pseudoStyle;
            bool m_documentIsHTML;
            mutable HashSet<LinkHash, LinkHashHash> m_linksCheckedForVisitedState;
        };

    private:
        static RenderStyle* s_styleNotYetAvailable;

        void init();

        void matchUARules(int& firstUARule, int& lastUARule);
        void updateFont();
        void cacheBorderAndBackground();

        void mapFillAttachment(FillLayer*, CSSValue*);
        void mapFillClip(FillLayer*, CSSValue*);
        void mapFillComposite(FillLayer*, CSSValue*);
        void mapFillOrigin(FillLayer*, CSSValue*);
        void mapFillImage(FillLayer*, CSSValue*);
        void mapFillRepeatX(FillLayer*, CSSValue*);
        void mapFillRepeatY(FillLayer*, CSSValue*);
        void mapFillSize(FillLayer*, CSSValue*);
        void mapFillXPosition(FillLayer*, CSSValue*);
        void mapFillYPosition(FillLayer*, CSSValue*);

        void mapAnimationDelay(Animation*, CSSValue*);
        void mapAnimationDirection(Animation*, CSSValue*);
        void mapAnimationDuration(Animation*, CSSValue*);
        void mapAnimationIterationCount(Animation*, CSSValue*);
        void mapAnimationName(Animation*, CSSValue*);
        void mapAnimationPlayState(Animation*, CSSValue*);
        void mapAnimationProperty(Animation*, CSSValue*);
        void mapAnimationTimingFunction(Animation*, CSSValue*);

        void mapNinePieceImage(CSSValue*, NinePieceImage&);

        void applyProperty(int id, CSSValue*);
#if ENABLE(SVG)
        void applySVGProperty(int id, CSSValue*);
#endif

        StyleImage* styleImage(CSSValue* value);

        // We collect the set of decls that match in |m_matchedDecls|.  We then walk the
        // set of matched decls four times, once for those properties that others depend on (like font-size),
        // and then a second time for all the remaining properties.  We then do the same two passes
        // for any !important rules.
        Vector<CSSMutableStyleDeclaration*, 64> m_matchedDecls;

        // A buffer used to hold the set of matched rules for an element, and a temporary buffer used for
        // merge sorting.
        Vector<CSSRuleData*, 32> m_matchedRules;

        RefPtr<CSSRuleList> m_ruleList;

        MediaQueryEvaluator* m_medium;
        RefPtr<RenderStyle> m_rootDefaultStyle;

        PseudoId m_dynamicPseudo;

        SelectorChecker m_checker;

        RefPtr<RenderStyle> m_style;
        RenderStyle* m_parentStyle;
        RenderStyle* m_rootElementStyle;
        Element* m_element;
        StyledElement* m_styledElement;
        Node* m_parentNode;
        CSSValue* m_lineHeightValue;
        bool m_fontDirty;
        bool m_matchAuthorAndUserStyles;

        RefPtr<CSSFontSelector> m_fontSelector;
        HashSet<AtomicStringImpl*> m_selectorAttrs;
        Vector<CSSMutableStyleDeclaration*> m_additionalAttributeStyleDecls;
        Vector<MediaQueryResult*> m_viewportDependentMediaQueryResults;
        
        HashMap<String, CSSVariablesRule*> m_variablesMap;
        HashMap<CSSMutableStyleDeclaration*, RefPtr<CSSMutableStyleDeclaration> > m_resolvedVariablesDeclarations;
    };

    class CSSRuleData : public Noncopyable {
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

        ~CSSRuleData() 
        { 
        }

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

    class CSSRuleDataList : public Noncopyable {
    public:
        CSSRuleDataList(unsigned pos, CSSStyleRule* rule, CSSSelector* sel)
            : m_first(new CSSRuleData(pos, rule, sel))
            , m_last(m_first)
        {
        }

        ~CSSRuleDataList() 
        { 
            CSSRuleData* ptr;
            CSSRuleData* next;
            ptr = m_first;
            while (ptr) {
                next = ptr->next();
                delete ptr;
                ptr = next;
            }
        }

        CSSRuleData* first() { return m_first; }
        CSSRuleData* last() { return m_last; }

        void append(unsigned pos, CSSStyleRule* rule, CSSSelector* sel) { m_last = new CSSRuleData(pos, rule, sel, m_last); }

    private:
        CSSRuleData* m_first;
        CSSRuleData* m_last;
    };
    
} // namespace WebCore

#endif // CSSStyleSelector_h
