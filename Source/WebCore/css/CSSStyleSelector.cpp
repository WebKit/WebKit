/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 */

#include "config.h"
#include "CSSStyleSelector.h"

#include "Attribute.h"
#include "CachedImage.h"
#include "ContentData.h"
#include "Counter.h"
#include "CounterContent.h"
#include "CSSBorderImage.h"
#include "CSSCursorImageValue.h"
#include "CSSFontFaceRule.h"
#include "CSSFontSelector.h"
#include "CSSImportRule.h"
#include "CSSLineBoxContainValue.h"
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSStyleApplyProperty.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CursorList.h"
#include "FontFamilyValue.h"
#include "FontFeatureValue.h"
#include "FontValue.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLProgressElement.h"
#include "HTMLTextAreaElement.h"
#include "InspectorInstrumentation.h"
#include "KeyframeList.h"
#include "LinkHash.h"
#include "LocaleToScriptMapping.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageGroup.h"
#include "Pair.h"
#include "PerspectiveTransformOperation.h"
#include "QuotesData.h"
#include "Rect.h"
#include "RenderRegion.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarTheme.h"
#include "RenderStyleConstants.h"
#include "RenderTheme.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ShadowData.h"
#include "ShadowValue.h"
#include "SkewTransformOperation.h"
#include "StyleCachedImage.h"
#include "StylePendingImage.h"
#include "StyleGeneratedImage.h"
#include "StyleSheetList.h"
#include "Text.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"
#include "UserAgentStyleSheets.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSRegionRule.h"
#include "WebKitCSSTransformValue.h"
#include "WebKitFontFamilyNames.h"
#include "XMLNames.h"
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if ENABLE(CSS_FILTERS)
#include "FilterOperation.h"
#include "WebKitCSSFilterValue.h"
#endif

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

#if ENABLE(CSS_SHADERS)
#include "CustomFilterNumberParameter.h"
#include "CustomFilterOperation.h"
#include "CustomFilterParameter.h"
#include "StyleCachedShader.h"
#include "StyleCustomFilterProgram.h"
#include "StylePendingShader.h"
#include "StyleShader.h"
#include "WebKitCSSShaderValue.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) { \
    m_style->set##Prop(m_parentStyle->prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    m_style->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(prop, Prop, Value) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    m_style->set##Prop(RenderStyle::initial##Value());\
    return;\
}

#define HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(prop, Prop) \
HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
if (primitiveValue) \
    m_style->set##Prop(*primitiveValue);

#define HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(prop, Prop, Value) \
HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(prop, Prop, Value) \
if (primitiveValue) \
    m_style->set##Prop(*primitiveValue);

class RuleData {
public:
    RuleData(CSSStyleRule*, CSSSelector*, unsigned position);

    unsigned position() const { return m_position; }
    CSSStyleRule* rule() const { return m_rule; }
    CSSSelector* selector() const { return m_selector; }

    bool hasFastCheckableSelector() const { return m_hasFastCheckableSelector; }
    bool hasMultipartSelector() const { return m_hasMultipartSelector; }
    bool hasRightmostSelectorMatchingHTMLBasedOnRuleHash() const { return m_hasRightmostSelectorMatchingHTMLBasedOnRuleHash; }
    bool containsUncommonAttributeSelector() const { return m_containsUncommonAttributeSelector; }
    unsigned specificity() const { return m_specificity; }
    unsigned linkMatchType() const { return m_linkMatchType; }

    // Try to balance between memory usage (there can be lots of RuleData objects) and good filtering performance.
    static const unsigned maximumIdentifierCount = 4;
    const unsigned* descendantSelectorIdentifierHashes() const { return m_descendantSelectorIdentifierHashes; }

private:
    CSSStyleRule* m_rule;
    CSSSelector* m_selector;
    unsigned m_specificity;
    // This number was picked fairly arbitrarily. We can probably lower it if we need to.
    // Some simple testing showed <100,000 RuleData's on large sites.
    unsigned m_position : 26;
    unsigned m_hasFastCheckableSelector : 1;
    unsigned m_hasMultipartSelector : 1;
    unsigned m_hasRightmostSelectorMatchingHTMLBasedOnRuleHash : 1;
    unsigned m_containsUncommonAttributeSelector : 1;
    unsigned m_linkMatchType : 2; //  SelectorChecker::LinkMatchMask
    // Use plain array instead of a Vector to minimize memory overhead.
    unsigned m_descendantSelectorIdentifierHashes[maximumIdentifierCount];
};
    
struct SameSizeAsRuleData {
    void* a;
    void* b;
    unsigned c;
    unsigned d;
    unsigned e[4];
};

COMPILE_ASSERT(sizeof(RuleData) == sizeof(SameSizeAsRuleData), RuleData_should_stay_small);

class RuleSet {
    WTF_MAKE_NONCOPYABLE(RuleSet);
public:
    RuleSet();

    typedef HashMap<AtomicStringImpl*, OwnPtr<Vector<RuleData> > > AtomRuleMap;

    void addRulesFromSheet(CSSStyleSheet*, const MediaQueryEvaluator&, CSSStyleSelector* = 0);

    void addStyleRule(CSSStyleRule*);
    void addRule(CSSStyleRule*, CSSSelector*);
    void addPageRule(CSSPageRule*);
    void addToRuleSet(AtomicStringImpl* key, AtomRuleMap&, const RuleData&);
    void addRegionRule(WebKitCSSRegionRule*);
    void shrinkToFit();
    void disableAutoShrinkToFit() { m_autoShrinkToFitEnabled = false; }

    const CSSStyleSelector::Features& features() const { return m_features; }

    const Vector<RuleData>* idRules(AtomicStringImpl* key) const { return m_idRules.get(key); }
    const Vector<RuleData>* classRules(AtomicStringImpl* key) const { return m_classRules.get(key); }
    const Vector<RuleData>* tagRules(AtomicStringImpl* key) const { return m_tagRules.get(key); }
    const Vector<RuleData>* shadowPseudoElementRules(AtomicStringImpl* key) const { return m_shadowPseudoElementRules.get(key); }
    const Vector<RuleData>* linkPseudoClassRules() const { return &m_linkPseudoClassRules; }
    const Vector<RuleData>* focusPseudoClassRules() const { return &m_focusPseudoClassRules; }
    const Vector<RuleData>* universalRules() const { return &m_universalRules; }
    const Vector<RuleData>* pageRules() const { return &m_pageRules; }

public:
    AtomRuleMap m_idRules;
    AtomRuleMap m_classRules;
    AtomRuleMap m_tagRules;
    AtomRuleMap m_shadowPseudoElementRules;
    Vector<RuleData> m_linkPseudoClassRules;
    Vector<RuleData> m_focusPseudoClassRules;
    Vector<RuleData> m_universalRules;
    Vector<RuleData> m_pageRules;
    unsigned m_ruleCount;
    bool m_autoShrinkToFitEnabled;
    CSSStyleSelector::Features m_features;

    struct RuleSetSelectorPair {
        RuleSetSelectorPair(CSSSelector* selector, RuleSet* ruleSet) : selector(selector), ruleSet(adoptPtr(ruleSet)) { }
        RuleSetSelectorPair(const RuleSetSelectorPair& rs) : selector(rs.selector), ruleSet(const_cast<RuleSetSelectorPair*>(&rs)->ruleSet.release()) { }
        CSSSelector* selector;
        OwnPtr<RuleSet> ruleSet;
    };

    Vector<RuleSetSelectorPair> m_regionSelectorsAndRuleSets;
};

static RuleSet* defaultStyle;
static RuleSet* defaultQuirksStyle;
static RuleSet* defaultPrintStyle;
static RuleSet* defaultViewSourceStyle;
static CSSStyleSheet* simpleDefaultStyleSheet;

RenderStyle* CSSStyleSelector::s_styleNotYetAvailable;

static void loadFullDefaultStyle();
static void loadSimpleDefaultStyle();
// FIXME: It would be nice to use some mechanism that guarantees this is in sync with the real UA stylesheet.
static const char* simpleUserAgentStyleSheet = "html,body,div{display:block}head{display:none}body{margin:8px}div:focus,span:focus{outline:auto 5px -webkit-focus-ring-color}a:-webkit-any-link{color:-webkit-link;text-decoration:underline}a:-webkit-any-link:active{color:-webkit-activelink}";

static inline bool elementCanUseSimpleDefaultStyle(Element* e)
{
    return e->hasTagName(htmlTag) || e->hasTagName(headTag) || e->hasTagName(bodyTag) || e->hasTagName(divTag) || e->hasTagName(spanTag) || e->hasTagName(brTag) || e->hasTagName(aTag);
}

static const MediaQueryEvaluator& screenEval()
{
    DEFINE_STATIC_LOCAL(const MediaQueryEvaluator, staticScreenEval, ("screen"));
    return staticScreenEval;
}

static const MediaQueryEvaluator& printEval()
{
    DEFINE_STATIC_LOCAL(const MediaQueryEvaluator, staticPrintEval, ("print"));
    return staticPrintEval;
}

static CSSMutableStyleDeclaration* leftToRightDeclaration()
{
    DEFINE_STATIC_LOCAL(RefPtr<CSSMutableStyleDeclaration>, leftToRightDecl, (CSSMutableStyleDeclaration::create()));
    if (!leftToRightDecl->length())
        leftToRightDecl->setProperty(CSSPropertyDirection, CSSValueLtr);
    return leftToRightDecl.get();
}

static CSSMutableStyleDeclaration* rightToLeftDeclaration()
{
    DEFINE_STATIC_LOCAL(RefPtr<CSSMutableStyleDeclaration>, rightToLeftDecl, (CSSMutableStyleDeclaration::create()));
    if (!rightToLeftDecl->length())
        rightToLeftDecl->setProperty(CSSPropertyDirection, CSSValueRtl);
    return rightToLeftDecl.get();
}

CSSStyleSelector::CSSStyleSelector(Document* document, StyleSheetList* styleSheets, CSSStyleSheet* mappedElementSheet,
                                   CSSStyleSheet* pageUserSheet, const Vector<RefPtr<CSSStyleSheet> >* pageGroupUserSheets, const Vector<RefPtr<CSSStyleSheet> >* documentUserSheets,
                                   bool strictParsing, bool matchAuthorAndUserStyles)
    : m_hasUAAppearance(false)
    , m_backgroundData(BackgroundFillLayer)
    , m_matchedDeclarationCacheAdditionsSinceLastSweep(0)
    , m_checker(document, strictParsing)
    , m_parentStyle(0)
    , m_rootElementStyle(0)
    , m_element(0)
    , m_styledElement(0)
    , m_regionForStyling(0)
    , m_elementLinkState(NotInsideLink)
    , m_parentNode(0)
    , m_lineHeightValue(0)
    , m_fontDirty(false)
    , m_matchAuthorAndUserStyles(matchAuthorAndUserStyles)
    , m_sameOriginOnly(false)
    , m_fontSelector(CSSFontSelector::create(document))
    , m_applyPropertyToRegularStyle(true)
    , m_applyPropertyToVisitedLinkStyle(false)
    , m_applyProperty(CSSStyleApplyProperty::sharedCSSStyleApplyProperty())
#if ENABLE(CSS_SHADERS)
    , m_hasPendingShaders(false)
#endif
{
    Element* root = document->documentElement();

    if (!defaultStyle) {
        if (!root || elementCanUseSimpleDefaultStyle(root))
            loadSimpleDefaultStyle();
        else {
            loadFullDefaultStyle();
        }
    }

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = document->view();
    if (view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType()));
    else
        m_medium = adoptPtr(new MediaQueryEvaluator("all"));

    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, false, true); // don't ref, because the RenderStyle is allocated from global heap

    if (m_rootDefaultStyle && view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType(), view->frame(), m_rootDefaultStyle.get()));

    m_authorStyle = adoptPtr(new RuleSet);
    // Adding rules from multiple sheets, shrink at the end.
    m_authorStyle->disableAutoShrinkToFit();

    // FIXME: This sucks! The user sheet is reparsed every time!
    OwnPtr<RuleSet> tempUserStyle = adoptPtr(new RuleSet);
    if (pageUserSheet)
        tempUserStyle->addRulesFromSheet(pageUserSheet, *m_medium, this);
    if (pageGroupUserSheets) {
        unsigned length = pageGroupUserSheets->size();
        for (unsigned i = 0; i < length; i++) {
            if (pageGroupUserSheets->at(i)->isUserStyleSheet())
                tempUserStyle->addRulesFromSheet(pageGroupUserSheets->at(i).get(), *m_medium, this);
            else
                m_authorStyle->addRulesFromSheet(pageGroupUserSheets->at(i).get(), *m_medium, this);
        }
    }
    if (documentUserSheets) {
        unsigned length = documentUserSheets->size();
        for (unsigned i = 0; i < length; i++) {
            if (documentUserSheets->at(i)->isUserStyleSheet())
                tempUserStyle->addRulesFromSheet(documentUserSheets->at(i).get(), *m_medium, this);
            else
                m_authorStyle->addRulesFromSheet(documentUserSheets->at(i).get(), *m_medium, this);
        }
    }

    if (tempUserStyle->m_ruleCount > 0 || tempUserStyle->m_pageRules.size() > 0)
        m_userStyle = tempUserStyle.release();

    // Add rules from elements like SVG's <font-face>
    if (mappedElementSheet)
        m_authorStyle->addRulesFromSheet(mappedElementSheet, *m_medium, this);

    // add stylesheets from document
    appendAuthorStylesheets(0, styleSheets->vector());
}
    
static PassOwnPtr<RuleSet> makeRuleSet(const Vector<CSSStyleSelector::RuleSelectorPair>& rules)
{
    size_t size = rules.size();
    if (!size)
        return nullptr;
    OwnPtr<RuleSet> ruleSet = adoptPtr(new RuleSet);
    for (size_t i = 0; i < size; ++i)
        ruleSet->addRule(rules[i].rule, rules[i].selector);
    return ruleSet.release();
}

void CSSStyleSelector::collectFeatures()
{
    m_features.clear();
    // Collect all ids and rules using sibling selectors (:first-child and similar)
    // in the current set of stylesheets. Style sharing code uses this information to reject
    // sharing candidates.
    m_features.add(defaultStyle->features());
    m_features.add(m_authorStyle->features());
    if (m_userStyle)
        m_features.add(m_userStyle->features());

    m_siblingRuleSet = makeRuleSet(m_features.siblingRules);
    m_uncommonAttributeRuleSet = makeRuleSet(m_features.uncommonAttributeRules);
}

void CSSStyleSelector::appendAuthorStylesheets(unsigned firstNew, const Vector<RefPtr<StyleSheet> >& stylesheets)
{
    // This handles sheets added to the end of the stylesheet list only. In other cases the style resolver
    // needs to be reconstructed. To handle insertions too the rule order numbers would need to be updated.
    unsigned size = stylesheets.size();
    for (unsigned i = firstNew; i < size; ++i) {
        if (!stylesheets[i]->isCSSStyleSheet() || stylesheets[i]->disabled())
            continue;
        m_authorStyle->addRulesFromSheet(static_cast<CSSStyleSheet*>(stylesheets[i].get()), *m_medium, this);
    }
    m_authorStyle->shrinkToFit();
    collectFeatures();
    
    if (document()->renderer() && document()->renderer()->style())
        document()->renderer()->style()->font().update(fontSelector());
}

// This is a simplified style setting function for keyframe styles
void CSSStyleSelector::addKeyframeStyle(PassRefPtr<WebKitCSSKeyframesRule> rule)
{
    AtomicString s(rule->name());
    m_keyframesRuleMap.set(s.impl(), rule);
}

CSSStyleSelector::~CSSStyleSelector()
{
    m_fontSelector->clearDocument();
}

void CSSStyleSelector::sweepMatchedDeclarationCache()
{
    // Look for cache entries containing a style declaration with a single ref and remove them.
    // This may happen when an element attribute mutation causes it to swap out its Attribute::decl()
    // for another CSSMappedAttributeDeclaration, potentially leaving this cache with the last ref.
    Vector<unsigned, 16> toRemove;
    MatchedStyleDeclarationCache::iterator it = m_matchedStyleDeclarationCache.begin();
    MatchedStyleDeclarationCache::iterator end = m_matchedStyleDeclarationCache.end();
    for (; it != end; ++it) {
        Vector<MatchedStyleDeclaration>& matchedStyleDeclarations = it->second.matchedStyleDeclarations;
        for (size_t i = 0; i < matchedStyleDeclarations.size(); ++i) {
            if (matchedStyleDeclarations[i].styleDeclaration->hasOneRef()) {
                toRemove.append(it->first);
                break;
            }
        }
    }
    for (size_t i = 0; i < toRemove.size(); ++i)
        m_matchedStyleDeclarationCache.remove(toRemove[i]);
}

CSSStyleSelector::Features::Features()
    : usesFirstLineRules(false)
    , usesBeforeAfterRules(false)
    , usesLinkRules(false)
{
}

CSSStyleSelector::Features::~Features()
{
}
    
void CSSStyleSelector::Features::add(const CSSStyleSelector::Features& other)
{
    HashSet<AtomicStringImpl*>::iterator end = other.idsInRules.end();
    for (HashSet<AtomicStringImpl*>::iterator it = other.idsInRules.begin(); it != end; ++it)
        idsInRules.add(*it);
    end = other.attrsInRules.end();
    for (HashSet<AtomicStringImpl*>::iterator it = other.attrsInRules.begin(); it != end; ++it)
        attrsInRules.add(*it);
    siblingRules.append(other.siblingRules);
    uncommonAttributeRules.append(other.uncommonAttributeRules);
    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    usesBeforeAfterRules = usesBeforeAfterRules || other.usesBeforeAfterRules;
    usesLinkRules = usesLinkRules || other.usesLinkRules;
}

void CSSStyleSelector::Features::clear()
{
    idsInRules.clear();
    attrsInRules.clear();
    siblingRules.clear();
    uncommonAttributeRules.clear();
    usesFirstLineRules = false;
    usesBeforeAfterRules = false;
    usesLinkRules = false;
}

static CSSStyleSheet* parseUASheet(const String& str)
{
    CSSStyleSheet* sheet = CSSStyleSheet::create().leakRef(); // leak the sheet on purpose
    sheet->parseString(str);
    return sheet;
}

static CSSStyleSheet* parseUASheet(const char* characters, unsigned size)
{
    return parseUASheet(String(characters, size));
}

static void loadFullDefaultStyle()
{
    if (simpleDefaultStyleSheet) {
        ASSERT(defaultStyle);
        ASSERT(defaultPrintStyle == defaultStyle);
        delete defaultStyle;
        simpleDefaultStyleSheet->deref();
        defaultStyle = new RuleSet;
        defaultPrintStyle = new RuleSet;
        simpleDefaultStyleSheet = 0;
    } else {
        ASSERT(!defaultStyle);
        defaultStyle = new RuleSet;
        defaultPrintStyle = new RuleSet;
        defaultQuirksStyle = new RuleSet;
    }

    // Strict-mode rules.
    String defaultRules = String(htmlUserAgentStyleSheet, sizeof(htmlUserAgentStyleSheet)) + RenderTheme::defaultTheme()->extraDefaultStyleSheet();
    CSSStyleSheet* defaultSheet = parseUASheet(defaultRules);
    defaultStyle->addRulesFromSheet(defaultSheet, screenEval());
    defaultPrintStyle->addRulesFromSheet(defaultSheet, printEval());

    // Quirks-mode rules.
    String quirksRules = String(quirksUserAgentStyleSheet, sizeof(quirksUserAgentStyleSheet)) + RenderTheme::defaultTheme()->extraQuirksStyleSheet();
    CSSStyleSheet* quirksSheet = parseUASheet(quirksRules);
    defaultQuirksStyle->addRulesFromSheet(quirksSheet, screenEval());
}

static void loadSimpleDefaultStyle()
{
    ASSERT(!defaultStyle);
    ASSERT(!simpleDefaultStyleSheet);

    defaultStyle = new RuleSet;
    // There are no media-specific rules in the simple default style.
    defaultPrintStyle = defaultStyle;
    defaultQuirksStyle = new RuleSet;

    simpleDefaultStyleSheet = parseUASheet(simpleUserAgentStyleSheet, strlen(simpleUserAgentStyleSheet));
    defaultStyle->addRulesFromSheet(simpleDefaultStyleSheet, screenEval());

    // No need to initialize quirks sheet yet as there are no quirk rules for elements allowed in simple default style.
}

static void loadViewSourceStyle()
{
    ASSERT(!defaultViewSourceStyle);
    defaultViewSourceStyle = new RuleSet;
    defaultViewSourceStyle->addRulesFromSheet(parseUASheet(sourceUserAgentStyleSheet, sizeof(sourceUserAgentStyleSheet)), screenEval());
}

static void ensureDefaultStyleSheetsForElement(Element* element)
{
    if (simpleDefaultStyleSheet && !elementCanUseSimpleDefaultStyle(element))
        loadFullDefaultStyle();

#if ENABLE(SVG)
    static bool loadedSVGUserAgentSheet;
    if (element->isSVGElement() && !loadedSVGUserAgentSheet) {
        // SVG rules.
        loadedSVGUserAgentSheet = true;
        CSSStyleSheet* svgSheet = parseUASheet(svgUserAgentStyleSheet, sizeof(svgUserAgentStyleSheet));
        defaultStyle->addRulesFromSheet(svgSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(svgSheet, printEval());
    }
#endif

    static bool loadedMathMLUserAgentSheet;
#if ENABLE(MATHML)
    if (element->isMathMLElement() && !loadedMathMLUserAgentSheet) {
        // MathML rules.
        loadedMathMLUserAgentSheet = true;
        CSSStyleSheet* mathMLSheet = parseUASheet(mathmlUserAgentStyleSheet, sizeof(mathmlUserAgentStyleSheet));
        defaultStyle->addRulesFromSheet(mathMLSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(mathMLSheet, printEval());
    }
#endif

#if ENABLE(VIDEO)
    static bool loadedMediaStyleSheet;
    if (!loadedMediaStyleSheet && (element->hasTagName(videoTag) || element->hasTagName(audioTag))) {
        loadedMediaStyleSheet = true;
        String mediaRules = String(mediaControlsUserAgentStyleSheet, sizeof(mediaControlsUserAgentStyleSheet)) + RenderTheme::themeForPage(element->document()->page())->extraMediaControlsStyleSheet();
        CSSStyleSheet* mediaControlsSheet = parseUASheet(mediaRules);
        defaultStyle->addRulesFromSheet(mediaControlsSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(mediaControlsSheet, printEval());
    }
#endif

#if ENABLE(FULLSCREEN_API)
    static bool loadedFullScreenStyleSheet;
    if (!loadedFullScreenStyleSheet && element->document()->webkitIsFullScreen()) {
        loadedFullScreenStyleSheet = true;
        String fullscreenRules = String(fullscreenUserAgentStyleSheet, sizeof(fullscreenUserAgentStyleSheet)) + RenderTheme::defaultTheme()->extraFullScreenStyleSheet();
        CSSStyleSheet* fullscreenSheet = parseUASheet(fullscreenRules);
        defaultStyle->addRulesFromSheet(fullscreenSheet, screenEval());
        defaultQuirksStyle->addRulesFromSheet(fullscreenSheet, screenEval());
    }
#endif

    ASSERT(defaultStyle->features().idsInRules.isEmpty());
    ASSERT_UNUSED(loadedMathMLUserAgentSheet, loadedMathMLUserAgentSheet || defaultStyle->features().siblingRules.isEmpty());
}

void CSSStyleSelector::addMatchedDeclaration(CSSMutableStyleDeclaration* styleDeclaration, unsigned linkMatchType)
{
    m_matchedDecls.grow(m_matchedDecls.size() + 1);
    MatchedStyleDeclaration& newDeclaration = m_matchedDecls.last();
    newDeclaration.styleDeclaration = styleDeclaration;
    newDeclaration.linkMatchType = linkMatchType;
}

void CSSStyleSelector::collectMatchingRules(RuleSet* rules, int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules)
{
    ASSERT(rules);
    ASSERT(m_element);

    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    if (m_element->hasID())
        collectMatchingRulesForList(rules->idRules(m_element->idForStyleResolution().impl()), firstRuleIndex, lastRuleIndex, includeEmptyRules);
    if (m_element->hasClass()) {
        ASSERT(m_styledElement);
        const SpaceSplitString& classNames = m_styledElement->classNames();
        size_t size = classNames.size();
        for (size_t i = 0; i < size; ++i)
            collectMatchingRulesForList(rules->classRules(classNames[i].impl()), firstRuleIndex, lastRuleIndex, includeEmptyRules);
    }
    const AtomicString& pseudoId = m_element->shadowPseudoId();
    if (!pseudoId.isEmpty()) {
        ASSERT(m_styledElement);
        collectMatchingRulesForList(rules->shadowPseudoElementRules(pseudoId.impl()), firstRuleIndex, lastRuleIndex, includeEmptyRules);
    }
    if (m_element->isLink())
        collectMatchingRulesForList(rules->linkPseudoClassRules(), firstRuleIndex, lastRuleIndex, includeEmptyRules);
    if (m_checker.matchesFocusPseudoClass(m_element))
        collectMatchingRulesForList(rules->focusPseudoClassRules(), firstRuleIndex, lastRuleIndex, includeEmptyRules);
    collectMatchingRulesForList(rules->tagRules(m_element->localName().impl()), firstRuleIndex, lastRuleIndex, includeEmptyRules);
    collectMatchingRulesForList(rules->universalRules(), firstRuleIndex, lastRuleIndex, includeEmptyRules);
}

void CSSStyleSelector::collectMatchingRulesForRegion(RuleSet* rules, int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules)
{
    if (!m_regionForStyling)
        return;

    unsigned size = rules->m_regionSelectorsAndRuleSets.size();
    for (unsigned i = 0; i < size; ++i) {
        CSSSelector* regionSelector = rules->m_regionSelectorsAndRuleSets.at(i).selector;
        if (checkRegionSelector(regionSelector, static_cast<Element*>(m_regionForStyling->node()))) {
            RuleSet* regionRules = rules->m_regionSelectorsAndRuleSets.at(i).ruleSet.get();
            ASSERT(regionRules);
            collectMatchingRules(regionRules, firstRuleIndex, lastRuleIndex, includeEmptyRules);
        }
    }
}

void CSSStyleSelector::matchRules(RuleSet* rules, int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules)
{
    m_matchedRules.clear();

    if (!rules || !m_element)
        return;

    collectMatchingRules(rules, firstRuleIndex, lastRuleIndex, includeEmptyRules);
    collectMatchingRulesForRegion(rules, firstRuleIndex, lastRuleIndex, includeEmptyRules);

    if (m_matchedRules.isEmpty())
        return;

    sortMatchedRules();

    if (m_checker.isCollectingRulesOnly()) {
        if (!m_ruleList)
            m_ruleList = CSSRuleList::create();
        for (unsigned i = 0; i < m_matchedRules.size(); ++i)
            m_ruleList->append(m_matchedRules[i]->rule());
        return;
    }

    // Now transfer the set of matched rules over to our list of declarations.
    // FIXME: This sucks, the inspector should get the style from the visited style itself.
    bool swapVisitedUnvisited = InspectorInstrumentation::forcePseudoState(m_element, CSSSelector::PseudoVisited);
    for (unsigned i = 0; i < m_matchedRules.size(); i++) {
        if (m_style && m_matchedRules[i]->containsUncommonAttributeSelector())
            m_style->setAffectedByUncommonAttributeSelectors();
        unsigned linkMatchType = m_matchedRules[i]->linkMatchType();
        if (swapVisitedUnvisited && linkMatchType && linkMatchType != SelectorChecker::MatchAll)
            linkMatchType = (linkMatchType == SelectorChecker::MatchVisited) ? SelectorChecker::MatchLink : SelectorChecker::MatchVisited;
        addMatchedDeclaration(m_matchedRules[i]->rule()->declaration(), linkMatchType);
    }
}

class MatchingUARulesScope {
public:
    MatchingUARulesScope();
    ~MatchingUARulesScope();

    static bool isMatchingUARules();

private:
    static bool m_matchingUARules;
};

MatchingUARulesScope::MatchingUARulesScope()
{
    ASSERT(!m_matchingUARules);
    m_matchingUARules = true;
}

MatchingUARulesScope::~MatchingUARulesScope()
{
    m_matchingUARules = false;
}

inline bool MatchingUARulesScope::isMatchingUARules()
{
    return m_matchingUARules;
}

bool MatchingUARulesScope::m_matchingUARules = false;

inline static bool matchesInTreeScope(TreeScope* treeScope, bool ruleReachesIntoShadowDOM)
{
    return MatchingUARulesScope::isMatchingUARules() || treeScope->applyAuthorSheets() || ruleReachesIntoShadowDOM;
}

void CSSStyleSelector::collectMatchingRulesForList(const Vector<RuleData>* rules, int& firstRuleIndex, int& lastRuleIndex, bool includeEmptyRules)
{
    if (!rules)
        return;
    // In some cases we may end up looking up style for random elements in the middle of a recursive tree resolve.
    // Ancestor identifier filter won't be up-to-date in that case and we can't use the fast path.
    bool canUseFastReject = m_checker.parentStackIsConsistent(m_parentNode);

    unsigned size = rules->size();
    for (unsigned i = 0; i < size; ++i) {
        const RuleData& ruleData = rules->at(i);
        if (canUseFastReject && m_checker.fastRejectSelector<RuleData::maximumIdentifierCount>(ruleData.descendantSelectorIdentifierHashes()))
            continue;

        CSSStyleRule* rule = ruleData.rule();
        InspectorInstrumentationCookie cookie = InspectorInstrumentation::willMatchRule(document(), rule);
        if (checkSelector(ruleData)) {
            if (!matchesInTreeScope(m_element->treeScope(), m_checker.hasUnknownPseudoElements())) {
                InspectorInstrumentation::didMatchRule(cookie, false);
                continue;
            }
            // If the rule has no properties to apply, then ignore it in the non-debug mode.
            CSSMutableStyleDeclaration* decl = rule->declaration();
            if (!decl || (!decl->length() && !includeEmptyRules)) {
                InspectorInstrumentation::didMatchRule(cookie, false);
                continue;
            }
            if (m_sameOriginOnly && !m_checker.document()->securityOrigin()->canRequest(rule->baseURL())) {
                InspectorInstrumentation::didMatchRule(cookie, false);
                continue;
            }
            // If we're matching normal rules, set a pseudo bit if
            // we really just matched a pseudo-element.
            if (m_dynamicPseudo != NOPSEUDO && m_checker.pseudoStyle() == NOPSEUDO) {
                if (m_checker.isCollectingRulesOnly()) {
                    InspectorInstrumentation::didMatchRule(cookie, false);
                    continue;
                }
                if (m_dynamicPseudo < FIRST_INTERNAL_PSEUDOID)
                    m_style->setHasPseudoStyle(m_dynamicPseudo);
            } else {
                // Update our first/last rule indices in the matched rules array.
                lastRuleIndex = m_matchedDecls.size() + m_matchedRules.size();
                if (firstRuleIndex == -1)
                    firstRuleIndex = lastRuleIndex;

                // Add this rule to our list of matched rules.
                addMatchedRule(&ruleData);
                InspectorInstrumentation::didMatchRule(cookie, true);
                continue;
            }
        }
        InspectorInstrumentation::didMatchRule(cookie, false);
    }
}

static inline bool compareRules(const RuleData* r1, const RuleData* r2)
{
    unsigned specificity1 = r1->specificity();
    unsigned specificity2 = r2->specificity();
    return (specificity1 == specificity2) ? r1->position() < r2->position() : specificity1 < specificity2;
}

void CSSStyleSelector::sortMatchedRules()
{
    std::sort(m_matchedRules.begin(), m_matchedRules.end(), compareRules);
}

void CSSStyleSelector::matchAllRules(MatchResult& result)
{
    matchUARules(result);

    // Now we check user sheet rules.
    if (m_matchAuthorAndUserStyles)
        matchRules(m_userStyle.get(), result.ranges.firstUserRule, result.ranges.lastUserRule, false);
        
    // Now check author rules, beginning first with presentational attributes mapped from HTML.
    if (m_styledElement) {
        if (const NamedNodeMap* map = m_styledElement->attributeMap()) {
            // Walk the element's attribute map and add all mapped attribute declarations.
            for (unsigned i = 0; i < map->length(); ++i) {
                Attribute* attribute = map->attributeItem(i);
                if (!attribute->decl())
                    continue;
                ASSERT(attribute->isMappedAttribute());
                result.ranges.lastAuthorRule = m_matchedDecls.size();
                if (result.ranges.firstAuthorRule == -1)
                    result.ranges.firstAuthorRule = result.ranges.lastAuthorRule;
                addMatchedDeclaration(attribute->decl());
            }
        }

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        if (m_styledElement->canHaveAdditionalAttributeStyleDecls()) {
            Vector<CSSMutableStyleDeclaration*> additionalAttributeStyleDecls;
            m_styledElement->additionalAttributeStyleDecls(additionalAttributeStyleDecls);
            if (!additionalAttributeStyleDecls.isEmpty()) {
                unsigned additionalDeclsSize = additionalAttributeStyleDecls.size();
                if (result.ranges.firstAuthorRule == -1)
                    result.ranges.firstAuthorRule = m_matchedDecls.size();
                result.ranges.lastAuthorRule = m_matchedDecls.size() + additionalDeclsSize - 1;
                for (unsigned i = 0; i < additionalDeclsSize; ++i)
                    addMatchedDeclaration(additionalAttributeStyleDecls[i]);
                result.isCacheable = false;
            }
        }
        if (m_styledElement->isHTMLElement()) {
            bool isAuto;
            TextDirection textDirection = toHTMLElement(m_styledElement)->directionalityIfhasDirAutoAttribute(isAuto);
            if (isAuto)
                addMatchedDeclaration(textDirection == LTR ? leftToRightDeclaration() : rightToLeftDeclaration());
        }
    }
    
    // Check the rules in author sheets next.
    if (m_matchAuthorAndUserStyles)
        matchRules(m_authorStyle.get(), result.ranges.firstAuthorRule, result.ranges.lastAuthorRule, false);

    // Now check our inline style attribute.
    if (m_matchAuthorAndUserStyles && m_styledElement) {
        CSSMutableStyleDeclaration* inlineDecl = m_styledElement->inlineStyleDecl();
        if (inlineDecl) {
            result.ranges.lastAuthorRule = m_matchedDecls.size();
            if (result.ranges.firstAuthorRule == -1)
                result.ranges.firstAuthorRule = result.ranges.lastAuthorRule;
            addMatchedDeclaration(inlineDecl);
            result.isCacheable = false;
        }
    }
}
    
inline void CSSStyleSelector::initElement(Element* e)
{
    if (m_element != e) {
        m_element = e;
        m_styledElement = m_element && m_element->isStyledElement() ? static_cast<StyledElement*>(m_element) : 0;
        m_elementLinkState = m_checker.determineLinkState(m_element);
        if (e && e == e->document()->documentElement()) {
            e->document()->setDirectionSetOnDocumentElement(false);
            e->document()->setWritingModeSetOnDocumentElement(false);
        }
    }
}

inline void CSSStyleSelector::initForStyleResolve(Element* e, RenderStyle* parentStyle, PseudoId pseudoID)
{
    m_checker.setPseudoStyle(pseudoID);

    m_parentNode = e ? e->parentNodeForRenderingAndStyle() : 0;

    if (parentStyle)
        m_parentStyle = parentStyle;
    else
        m_parentStyle = m_parentNode ? m_parentNode->renderStyle() : 0;

    Node* docElement = e ? e->document()->documentElement() : 0;
    RenderStyle* docStyle = m_checker.document()->renderStyle();
    m_rootElementStyle = docElement && e != docElement ? docElement->renderStyle() : docStyle;

    m_style = 0;

    m_matchedDecls.clear();

    m_pendingImageProperties.clear();

    m_ruleList = 0;

    m_fontDirty = false;
}

static const unsigned cStyleSearchThreshold = 10;
static const unsigned cStyleSearchLevelThreshold = 10;

Node* CSSStyleSelector::locateCousinList(Element* parent, unsigned& visitedNodeCount) const
{
    if (visitedNodeCount >= cStyleSearchThreshold * cStyleSearchLevelThreshold)
        return 0;
    if (!parent || !parent->isStyledElement())
        return 0;
    StyledElement* p = static_cast<StyledElement*>(parent);
    if (p->inlineStyleDecl())
        return 0;
    if (p->hasID() && m_features.idsInRules.contains(p->idForStyleResolution().impl()))
        return 0;

    RenderStyle* parentStyle = p->renderStyle();
    unsigned subcount = 0;
    Node* thisCousin = p;
    Node* currentNode = p->previousSibling();

    // Reserve the tries for this level. This effectively makes sure that the algorithm
    // will never go deeper than cStyleSearchLevelThreshold levels into recursion.
    visitedNodeCount += cStyleSearchThreshold;
    while (thisCousin) {
        while (currentNode) {
            ++subcount;
            if (currentNode->renderStyle() == parentStyle && currentNode->lastChild()) {
                // Adjust for unused reserved tries.
                visitedNodeCount -= cStyleSearchThreshold - subcount;
                return currentNode->lastChild();
            }
            if (subcount >= cStyleSearchThreshold)
                return 0;
            currentNode = currentNode->previousSibling();
        }
        currentNode = locateCousinList(thisCousin->parentElement(), visitedNodeCount);
        thisCousin = currentNode;
    }

    return 0;
}

bool CSSStyleSelector::matchesRuleSet(RuleSet* ruleSet)
{
    int firstSiblingRule = -1, lastSiblingRule = -1;
    matchRules(ruleSet, firstSiblingRule, lastSiblingRule, false);
    if (m_matchedDecls.isEmpty())
        return false;
    m_matchedDecls.clear();
    return true;
}

bool CSSStyleSelector::canShareStyleWithControl(StyledElement* element) const
{
#if ENABLE(PROGRESS_TAG)
    if (element->hasTagName(progressTag)) {
        if (!m_element->hasTagName(progressTag))
            return false;

        HTMLProgressElement* thisProgressElement = static_cast<HTMLProgressElement*>(element);
        HTMLProgressElement* otherProgressElement = static_cast<HTMLProgressElement*>(m_element);
        if (thisProgressElement->isDeterminate() != otherProgressElement->isDeterminate())
            return false;

        return true;
    }
#endif

    HTMLInputElement* thisInputElement = element->toInputElement();
    HTMLInputElement* otherInputElement = m_element->toInputElement();

    if (!thisInputElement || !otherInputElement)
        return false;

    if (thisInputElement->isAutofilled() != otherInputElement->isAutofilled())
        return false;
    if (thisInputElement->shouldAppearChecked() != otherInputElement->shouldAppearChecked())
        return false;
    if (thisInputElement->isIndeterminate() != otherInputElement->isIndeterminate())
        return false;
    if (thisInputElement->required() != otherInputElement->required())
        return false;

    if (element->isEnabledFormControl() != m_element->isEnabledFormControl())
        return false;

    if (element->isDefaultButtonForForm() != m_element->isDefaultButtonForForm())
        return false;

    if (!m_element->document()->containsValidityStyleRules())
        return false;

    bool willValidate = element->willValidate();

    if (willValidate != m_element->willValidate())
        return false;

    if (willValidate && (element->isValidFormControlElement() != m_element->isValidFormControlElement()))
        return false;

    if (element->isInRange() != m_element->isInRange())
        return false;

    if (element->isOutOfRange() != m_element->isOutOfRange())
        return false;

    return true;
}

static inline bool mappedAttributesEquivalent(NamedNodeMap* a, NamedNodeMap* b)
{
    ASSERT(a->mappedAttributeCount() == b->mappedAttributeCount());

    for (size_t i = 0; i < a->length(); ++i) {
        Attribute* attribute = a->attributeItem(i);
        if (!attribute->decl())
            continue;
        ASSERT(attribute->isMappedAttribute());
        Attribute* otherAttribute = b->getAttributeItem(attribute->name());
        if (!otherAttribute || attribute->value() != otherAttribute->value() || attribute->decl() != otherAttribute->decl())
            return false;
    }
    return true;
}

bool CSSStyleSelector::canShareStyleWithElement(StyledElement* element) const
{
    RenderStyle* style = element->renderStyle();

    if (!style)
        return false;
    if (style->unique())
        return false;
    if (element->tagQName() != m_element->tagQName())
        return false;
    if (element->hasClass() != m_element->hasClass())
        return false;
    if (element->inlineStyleDecl())
        return false;
    size_t mappedAttributeCount = element->mappedAttributeCount();
    if (mappedAttributeCount != m_styledElement->mappedAttributeCount())
        return false;
    if (element->isLink() != m_element->isLink())
        return false;
    if (style->affectedByUncommonAttributeSelectors())
        return false;
    if (element->hovered() != m_element->hovered())
        return false;
    if (element->active() != m_element->active())
        return false;
    if (element->focused() != m_element->focused())
        return false;
    if (element->shadowPseudoId() != m_element->shadowPseudoId())
        return false;
    if (element == element->document()->cssTarget())
        return false;
    if (m_element == m_element->document()->cssTarget())
        return false;
    if (element->getAttribute(typeAttr) != m_element->getAttribute(typeAttr))
        return false;
    if (element->fastGetAttribute(XMLNames::langAttr) != m_element->fastGetAttribute(XMLNames::langAttr))
        return false;
    if (element->fastGetAttribute(langAttr) != m_element->fastGetAttribute(langAttr))
        return false;
    if (element->fastGetAttribute(readonlyAttr) != m_element->fastGetAttribute(readonlyAttr))
        return false;
    if (element->fastGetAttribute(cellpaddingAttr) != m_element->fastGetAttribute(cellpaddingAttr))
        return false;

    if (element->hasID() && m_features.idsInRules.contains(element->idForStyleResolution().impl()))
        return false;

    bool isControl = element->isFormControlElement();

    if (isControl != m_element->isFormControlElement())
        return false;

    if (isControl && !canShareStyleWithControl(element))
        return false;

    if (style->transitions() || style->animations())
        return false;

#if USE(ACCELERATED_COMPOSITING)
    // Turn off style sharing for elements that can gain layers for reasons outside of the style system.
    // See comments in RenderObject::setStyle().
    if (element->hasTagName(iframeTag) || element->hasTagName(frameTag) || element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag)
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        // With proxying, the media elements are backed by a RenderEmbeddedObject.
        || element->hasTagName(videoTag) || element->hasTagName(audioTag)
#endif
        )
        return false;
#endif

    if (equalIgnoringCase(element->fastGetAttribute(dirAttr), "auto") || equalIgnoringCase(m_element->fastGetAttribute(dirAttr), "auto"))
        return false;

    if (element->hasClass() && m_element->getAttribute(classAttr) != element->getAttribute(classAttr))
        return false;

    if (mappedAttributeCount && !mappedAttributesEquivalent(element->attributeMap(), m_styledElement->attributeMap()))
        return false;

    if (element->isLink() && m_elementLinkState != style->insideLink())
        return false;

    return true;
}

inline StyledElement* CSSStyleSelector::findSiblingForStyleSharing(Node* node, unsigned& count) const
{
    for (; node; node = node->previousSibling()) {
        if (!node->isStyledElement())
            continue;
        if (canShareStyleWithElement(static_cast<StyledElement*>(node)))
            break;
        if (count++ == cStyleSearchThreshold)
            return 0;
    }
    return static_cast<StyledElement*>(node);
}

static inline bool parentStylePreventsSharing(const RenderStyle* parentStyle)
{
    return parentStyle->childrenAffectedByPositionalRules()
        || parentStyle->childrenAffectedByFirstChildRules()
        || parentStyle->childrenAffectedByLastChildRules() 
        || parentStyle->childrenAffectedByDirectAdjacentRules();
}

RenderStyle* CSSStyleSelector::locateSharedStyle()
{
    if (!m_styledElement || !m_parentStyle)
        return 0;
    // If the element has inline style it is probably unique.
    if (m_styledElement->inlineStyleDecl())
        return 0;
    // Ids stop style sharing if they show up in the stylesheets.
    if (m_styledElement->hasID() && m_features.idsInRules.contains(m_styledElement->idForStyleResolution().impl()))
        return 0;
    if (parentStylePreventsSharing(m_parentStyle))
        return 0;

    // Check previous siblings and their cousins.
    unsigned count = 0;
    unsigned visitedNodeCount = 0;
    StyledElement* shareElement = 0;
    Node* cousinList = m_styledElement->previousSibling();
    while (cousinList) {
        shareElement = findSiblingForStyleSharing(cousinList, count);
        if (shareElement)
            break;
        cousinList = locateCousinList(cousinList->parentElement(), visitedNodeCount);
    }

    // If we have exhausted all our budget or our cousins.
    if (!shareElement)
        return 0;

    // Can't share if sibling rules apply. This is checked at the end as it should rarely fail.
    if (matchesRuleSet(m_siblingRuleSet.get()))
        return 0;
    // Can't share if attribute rules apply.
    if (matchesRuleSet(m_uncommonAttributeRuleSet.get()))
        return 0;
    // Tracking child index requires unique style for each node. This may get set by the sibling rule match above.
    if (parentStylePreventsSharing(m_parentStyle))
        return 0;
    return shareElement->renderStyle();
}

void CSSStyleSelector::matchUARules(MatchResult& result)
{
    MatchingUARulesScope scope;

    // First we match rules from the user agent sheet.
    if (simpleDefaultStyleSheet)
        result.isCacheable = false;
    RuleSet* userAgentStyleSheet = m_medium->mediaTypeMatchSpecific("print")
        ? defaultPrintStyle : defaultStyle;
    matchRules(userAgentStyleSheet, result.ranges.firstUARule, result.ranges.lastUARule, false);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (!m_checker.strictParsing())
        matchRules(defaultQuirksStyle, result.ranges.firstUARule, result.ranges.lastUARule, false);

    // If document uses view source styles (in view source mode or in xml viewer mode), then we match rules from the view source style sheet.
    if (m_checker.document()->isViewSource()) {
        if (!defaultViewSourceStyle)
            loadViewSourceStyle();
        matchRules(defaultViewSourceStyle, result.ranges.firstUARule, result.ranges.lastUARule, false);
    }
}

PassRefPtr<RenderStyle> CSSStyleSelector::styleForDocument(Document* document, CSSFontSelector* fontSelector)
{
    Frame* frame = document->frame();

    RefPtr<RenderStyle> documentStyle = RenderStyle::create();
    documentStyle->setDisplay(BLOCK);
    documentStyle->setRTLOrdering(document->visuallyOrdered() ? VisualOrder : LogicalOrder);
    documentStyle->setZoom(frame ? frame->pageZoomFactor() : 1);
    documentStyle->setPageScaleTransform(frame ? frame->frameScaleFactor() : 1);
    documentStyle->setUserModify(document->inDesignMode() ? READ_WRITE : READ_ONLY);

    Element* docElement = document->documentElement();
    RenderObject* docElementRenderer = docElement ? docElement->renderer() : 0;
    if (docElementRenderer) {
        // Use the direction and writing-mode of the body to set the
        // viewport's direction and writing-mode unless the property is set on the document element.
        // If there is no body, then use the document element.
        RenderObject* bodyRenderer = document->body() ? document->body()->renderer() : 0;
        if (bodyRenderer && !document->writingModeSetOnDocumentElement())
            documentStyle->setWritingMode(bodyRenderer->style()->writingMode());
        else
            documentStyle->setWritingMode(docElementRenderer->style()->writingMode());
        if (bodyRenderer && !document->directionSetOnDocumentElement())
            documentStyle->setDirection(bodyRenderer->style()->direction());
        else
            documentStyle->setDirection(docElementRenderer->style()->direction());
    }

    if (frame) {
        if (Page* page = frame->page()) {
            const Page::Pagination& pagination = page->pagination();
            if (pagination.mode != Page::Pagination::Unpaginated) {
                documentStyle->setColumnAxis(pagination.mode == Page::Pagination::HorizontallyPaginated ? HorizontalColumnAxis : VerticalColumnAxis);
                documentStyle->setColumnGap(pagination.gap);
            }
        }
    }

    FontDescription fontDescription;
    fontDescription.setUsePrinterFont(document->printing());
    if (Settings* settings = document->settings()) {
        fontDescription.setRenderingMode(settings->fontRenderingMode());
        const AtomicString& stdfont = settings->standardFontFamily();
        if (!stdfont.isEmpty()) {
            fontDescription.setGenericFamily(FontDescription::StandardFamily);
            fontDescription.firstFamily().setFamily(stdfont);
            fontDescription.firstFamily().appendFamily(0);
        }
        fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        int size = CSSStyleSelector::fontSizeForKeyword(document, CSSValueMedium, false);
        fontDescription.setSpecifiedSize(size);
        bool useSVGZoomRules = document->isSVGDocument();
        fontDescription.setComputedSize(CSSStyleSelector::getComputedSizeFromSpecifiedSize(document, documentStyle.get(), fontDescription.isAbsoluteSize(), size, useSVGZoomRules));
    }

    documentStyle->setFontDescription(fontDescription);
    documentStyle->font().update(fontSelector);

    return documentStyle.release();
}

static inline bool isAtShadowBoundary(Element* element)
{
    if (!element)
        return false;
    ContainerNode* parentNode = element->parentNode();
    return parentNode && parentNode->isShadowRoot();
}

// If resolveForRootDefault is true, style based on user agent style sheet only. This is used in media queries, where
// relative units are interpreted according to document root element style, styled only with UA stylesheet

PassRefPtr<RenderStyle> CSSStyleSelector::styleForElement(Element* element, RenderStyle* defaultParent, bool allowSharing, bool resolveForRootDefault, RenderRegion* regionForStyling)
{
    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (allowSharing && !element->document()->haveStylesheetsLoaded() && !element->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = RenderStyle::create().leakRef();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(m_fontSelector);
        }
        element->document()->setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    initElement(element);
    initForStyleResolve(element, defaultParent);
    m_regionForStyling = regionForStyling;
    if (allowSharing) {
        RenderStyle* sharedStyle = locateSharedStyle();
        if (sharedStyle)
            return sharedStyle;
    }

    m_style = RenderStyle::create();

    if (m_parentStyle)
        m_style->inheritFrom(m_parentStyle);
    else {
        m_parentStyle = style();
        // Make sure our fonts are initialized if we don't inherit them from our parent style.
        m_style->font().update(0);
    }

    // Even if surrounding content is user-editable, shadow DOM should act as a single unit, and not necessarily be editable
    if (isAtShadowBoundary(element))
        m_style->setUserModify(RenderStyle::initialUserModify());

    if (element->isLink()) {
        m_style->setIsLink(true);
        m_style->setInsideLink(m_elementLinkState);
    }

    ensureDefaultStyleSheetsForElement(element);

    MatchResult matchResult;
    if (resolveForRootDefault)
        matchUARules(matchResult);
    else
        matchAllRules(matchResult);

    applyMatchedDeclarations(matchResult);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style(), m_parentStyle, element);

    initElement(0); // Clear out for the next resolve.

    // Now return the style.
    return m_style.release();
}

PassRefPtr<RenderStyle> CSSStyleSelector::styleForKeyframe(const RenderStyle* elementStyle, const WebKitCSSKeyframeRule* keyframeRule, KeyframeValue& keyframe)
{
    if (keyframeRule->style())
        addMatchedDeclaration(keyframeRule->style());

    ASSERT(!m_style);

    // Create the style
    m_style = RenderStyle::clone(elementStyle);

    m_lineHeightValue = 0;

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    bool inheritedOnly = false;
    if (keyframeRule->style())
        applyDeclarations<true>(false, 0, m_matchedDecls.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size
    if (m_lineHeightValue)
        applyProperty(CSSPropertyLineHeight, m_lineHeightValue);

    // Now do rest of the properties.
    if (keyframeRule->style())
        applyDeclarations<false>(false, 0, m_matchedDecls.size() - 1, inheritedOnly);

    // If our font got dirtied by one of the non-essential font props,
    // go ahead and update it a second time.
    updateFont();

    // Start loading images referenced by this style.
    loadPendingImages();
    
#if ENABLE(CSS_SHADERS)
    // Start loading the shaders referenced by this style.
    loadPendingShaders();
#endif

    // Add all the animating properties to the keyframe.
    if (keyframeRule->style()) {
        CSSMutableStyleDeclaration::const_iterator end = keyframeRule->style()->end();
        for (CSSMutableStyleDeclaration::const_iterator it = keyframeRule->style()->begin(); it != end; ++it) {
            int property = (*it).id();
            // Timing-function within keyframes is special, because it is not animated; it just
            // describes the timing function between this keyframe and the next.
            if (property != CSSPropertyWebkitAnimationTimingFunction)
                keyframe.addProperty(property);
        }
    }

    return m_style.release();
}

void CSSStyleSelector::keyframeStylesForAnimation(Element* e, const RenderStyle* elementStyle, KeyframeList& list)
{
    list.clear();

    // Get the keyframesRule for this name
    if (!e || list.animationName().isEmpty())
        return;

    m_keyframesRuleMap.checkConsistency();

    KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(list.animationName().impl());
    if (it == m_keyframesRuleMap.end())
        return;

    const WebKitCSSKeyframesRule* rule = it->second.get();

    // Construct and populate the style for each keyframe
    for (unsigned i = 0; i < rule->length(); ++i) {
        // Apply the declaration to the style. This is a simplified version of the logic in styleForElement
        initElement(e);
        initForStyleResolve(e);

        const WebKitCSSKeyframeRule* keyframeRule = rule->item(i);

        KeyframeValue keyframe(0, 0);
        keyframe.setStyle(styleForKeyframe(elementStyle, keyframeRule, keyframe));

        // Add this keyframe style to all the indicated key times
        Vector<float> keys;
        keyframeRule->getKeys(keys);
        for (size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
            keyframe.setKey(keys[keyIndex]);
            list.insert(keyframe);
        }
    }

    // If the 0% keyframe is missing, create it (but only if there is at least one other keyframe)
    int initialListSize = list.size();
    if (initialListSize > 0 && list[0].key() != 0) {
        RefPtr<WebKitCSSKeyframeRule> keyframeRule = WebKitCSSKeyframeRule::create();
        keyframeRule->setKeyText("0%");
        KeyframeValue keyframe(0, 0);
        keyframe.setStyle(styleForKeyframe(elementStyle, keyframeRule.get(), keyframe));
        list.insert(keyframe);
    }

    // If the 100% keyframe is missing, create it (but only if there is at least one other keyframe)
    if (initialListSize > 0 && (list[list.size() - 1].key() != 1)) {
        RefPtr<WebKitCSSKeyframeRule> keyframeRule = WebKitCSSKeyframeRule::create();
        keyframeRule->setKeyText("100%");
        KeyframeValue keyframe(1, 0);
        keyframe.setStyle(styleForKeyframe(elementStyle, keyframeRule.get(), keyframe));
        list.insert(keyframe);
    }
}

PassRefPtr<RenderStyle> CSSStyleSelector::pseudoStyleForElement(PseudoId pseudo, Element* e, RenderStyle* parentStyle)
{
    if (!e)
        return 0;

    initElement(e);

    initForStyleResolve(e, parentStyle, pseudo);
    m_style = RenderStyle::create();

    if (m_parentStyle)
        m_style->inheritFrom(m_parentStyle);

    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.

    // Check UA, user and author rules.
    MatchResult matchResult;
    matchUARules(matchResult);

    if (m_matchAuthorAndUserStyles) {
        matchRules(m_userStyle.get(), matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, false);
        matchRules(m_authorStyle.get(), matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, false);
    }

    if (m_matchedDecls.isEmpty())
        return 0;

    m_style->setStyleType(pseudo);

    applyMatchedDeclarations(matchResult);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style(), parentStyle, 0);

    // Start loading images referenced by this style.
    loadPendingImages();

#if ENABLE(CSS_SHADERS)
    // Start loading the shaders referenced by this style.
    loadPendingShaders();
#endif

    // Now return the style.
    return m_style.release();
}

PassRefPtr<RenderStyle> CSSStyleSelector::styleForPage(int pageIndex)
{
    initForStyleResolve(m_checker.document()->documentElement()); // m_rootElementStyle will be set to the document style.

    m_style = RenderStyle::create();
    m_style->inheritFrom(m_rootElementStyle);

    const bool isLeft = isLeftPage(pageIndex);
    const bool isFirst = isFirstPage(pageIndex);
    const String page = pageName(pageIndex);
    matchPageRules(defaultPrintStyle, isLeft, isFirst, page);
    matchPageRules(m_userStyle.get(), isLeft, isFirst, page);
    matchPageRules(m_authorStyle.get(), isLeft, isFirst, page);
    m_lineHeightValue = 0;
    bool inheritedOnly = false;
    applyDeclarations<true>(false, 0, m_matchedDecls.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size.
    if (m_lineHeightValue)
        applyProperty(CSSPropertyLineHeight, m_lineHeightValue);

    applyDeclarations<false>(false, 0, m_matchedDecls.size() - 1, inheritedOnly);

    // Start loading images referenced by this style.
    loadPendingImages();

#if ENABLE(CSS_SHADERS)
    // Start loading the shaders referenced by this style.
    loadPendingShaders();
#endif

    // Now return the style.
    return m_style.release();
}

static void addIntrinsicMargins(RenderStyle* style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = 2 * style->effectiveZoom();

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    // FIXME: Using "quirk" to decide the margin wasn't set is kind of lame.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(intrinsicMargin, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(intrinsicMargin, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(intrinsicMargin, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(intrinsicMargin, Fixed));
    }
}

void CSSStyleSelector::adjustRenderStyle(RenderStyle* style, RenderStyle* parentStyle, Element *e)
{
    // Cache our original display.
    style->setOriginalDisplay(style->display());

    if (style->display() != NONE) {
        // If we have a <td> that specifies a float property, in quirks mode we just drop the float
        // property.
        // Sites also commonly use display:inline/block on <td>s and <table>s.  In quirks mode we force
        // these tags to retain their display types.
        if (!m_checker.strictParsing() && e) {
            if (e->hasTagName(tdTag)) {
                style->setDisplay(TABLE_CELL);
                style->setFloating(NoFloat);
            }
            else if (e->hasTagName(tableTag))
                style->setDisplay(style->isDisplayInlineType() ? INLINE_TABLE : TABLE);
        }

        if (e && (e->hasTagName(tdTag) || e->hasTagName(thTag))) {
            if (style->whiteSpace() == KHTML_NOWRAP) {
                // Figure out if we are really nowrapping or if we should just
                // use normal instead.  If the width of the cell is fixed, then
                // we don't actually use NOWRAP.
                if (style->width().isFixed())
                    style->setWhiteSpace(NORMAL);
                else
                    style->setWhiteSpace(NOWRAP);
            }
        }

        // Tables never support the -webkit-* values for text-align and will reset back to the default.
        if (e && e->hasTagName(tableTag) && (style->textAlign() == WEBKIT_LEFT || style->textAlign() == WEBKIT_CENTER || style->textAlign() == WEBKIT_RIGHT))
            style->setTextAlign(TAAUTO);

        // Frames and framesets never honor position:relative or position:absolute.  This is necessary to
        // fix a crash where a site tries to position these objects.  They also never honor display.
        if (e && (e->hasTagName(frameTag) || e->hasTagName(framesetTag))) {
            style->setPosition(StaticPosition);
            style->setDisplay(BLOCK);
        }

        // Table headers with a text-align of auto will change the text-align to center.
        if (e && e->hasTagName(thTag) && style->textAlign() == TAAUTO)
            style->setTextAlign(CENTER);

        if (e && e->hasTagName(legendTag))
            style->setDisplay(BLOCK);

        // Mutate the display to BLOCK or TABLE for certain cases, e.g., if someone attempts to
        // position or float an inline, compact, or run-in.  Cache the original display, since it
        // may be needed for positioned elements that have to compute their static normal flow
        // positions.  We also force inline-level roots to be block-level.
        if (style->display() != BLOCK && style->display() != TABLE && style->display() != BOX &&
            (style->position() == AbsolutePosition || style->position() == FixedPosition || style->isFloating() ||
             (e && e->document()->documentElement() == e))) {
            if (style->display() == INLINE_TABLE)
                style->setDisplay(TABLE);
            else if (style->display() == INLINE_BOX)
                style->setDisplay(BOX);
            else if (style->display() == LIST_ITEM) {
                // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk,
                // but only in quirks mode.
                if (!m_checker.strictParsing() && style->isFloating())
                    style->setDisplay(BLOCK);
            }
            else
                style->setDisplay(BLOCK);
        }

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style->display() == INLINE && style->styleType() == NOPSEUDO && parentStyle && style->writingMode() != parentStyle->writingMode())
            style->setDisplay(INLINE_BLOCK);

        // After performing the display mutation, check table rows.  We do not honor position:relative on
        // table rows or cells.  This has been established in CSS2.1 (and caused a crash in containingBlock()
        // on some sites).
        if ((style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_ROW_GROUP
             || style->display() == TABLE_FOOTER_GROUP || style->display() == TABLE_ROW) &&
             style->position() == RelativePosition)
            style->setPosition(StaticPosition);

        // writing-mode does not apply to table row groups, table column groups, table rows, and table columns.
        // FIXME: Table cells should be allowed to be perpendicular or flipped with respect to the table, though.
        if (style->display() == TABLE_COLUMN || style->display() == TABLE_COLUMN_GROUP || style->display() == TABLE_FOOTER_GROUP
            || style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_ROW || style->display() == TABLE_ROW_GROUP
            || style->display() == TABLE_CELL)
            style->setWritingMode(parentStyle->writingMode());

        // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
        // of block-flow to anything other than TopToBottomWritingMode.
        // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
        if (style->writingMode() != TopToBottomWritingMode && (style->display() == BOX || style->display() == INLINE_BOX))
            style->setWritingMode(TopToBottomWritingMode);
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style->position() == StaticPosition)
        style->setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects.  This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them.  Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style->hasAutoZIndex() && ((e && e->document()->documentElement() == e) || style->opacity() < 1.0f
        || style->hasTransformRelatedProperty() || style->hasMask() || style->boxReflect() || style->hasFilter()))
        style->setZIndex(0);

    // Textarea considers overflow visible as auto.
    if (e && e->hasTagName(textareaTag)) {
        style->setOverflowX(style->overflowX() == OVISIBLE ? OAUTO : style->overflowX());
        style->setOverflowY(style->overflowY() == OVISIBLE ? OAUTO : style->overflowY());
    }

    // Finally update our text decorations in effect, but don't allow text-decoration to percolate through
    // tables, inline blocks, inline tables, run-ins, or shadow DOM.
    if (style->display() == TABLE || style->display() == INLINE_TABLE || style->display() == RUN_IN
        || style->display() == INLINE_BLOCK || style->display() == INLINE_BOX || isAtShadowBoundary(e))
        style->setTextDecorationsInEffect(style->textDecoration());
    else
        style->addToTextDecorationsInEffect(style->textDecoration());

    // If either overflow value is not visible, change to auto.
    if (style->overflowX() == OMARQUEE && style->overflowY() != OMARQUEE)
        style->setOverflowY(OMARQUEE);
    else if (style->overflowY() == OMARQUEE && style->overflowX() != OMARQUEE)
        style->setOverflowX(OMARQUEE);
    else if (style->overflowX() == OVISIBLE && style->overflowY() != OVISIBLE)
        style->setOverflowX(OAUTO);
    else if (style->overflowY() == OVISIBLE && style->overflowX() != OVISIBLE)
        style->setOverflowY(OAUTO);

    // Table rows, sections and the table itself will support overflow:hidden and will ignore scroll/auto.
    // FIXME: Eventually table sections will support auto and scroll.
    if (style->display() == TABLE || style->display() == INLINE_TABLE ||
        style->display() == TABLE_ROW_GROUP || style->display() == TABLE_ROW) {
        if (style->overflowX() != OVISIBLE && style->overflowX() != OHIDDEN)
            style->setOverflowX(OVISIBLE);
        if (style->overflowY() != OVISIBLE && style->overflowY() != OHIDDEN)
            style->setOverflowY(OVISIBLE);
    }

    // Menulists should have visible overflow
    if (style->appearance() == MenulistPart) {
        style->setOverflowX(OVISIBLE);
        style->setOverflowY(OVISIBLE);
    }

    // Cull out any useless layers and also repeat patterns into additional layers.
    style->adjustBackgroundLayers();
    style->adjustMaskLayers();

    // Do the same for animations and transitions.
    style->adjustAnimations();
    style->adjustTransitions();

    // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
    // alter fonts and heights/widths.
    if (e && e->isFormControlElement() && style->fontSize() >= 11) {
        // Don't apply intrinsic margins to image buttons.  The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!e->hasTagName(inputTag) || !static_cast<HTMLInputElement*>(e)->isImageButton())
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style->hasAppearance())
        RenderTheme::defaultTheme()->adjustStyle(this, style, e, m_hasUAAppearance, m_borderData, m_backgroundData, m_backgroundColor);

    // If we have first-letter pseudo style, do not share this style.
    if (style->hasPseudoStyle(FIRST_LETTER))
        style->setUnique();

#if ENABLE(SVG)
    if (e && e->isSVGElement()) {
        // Spec: http://www.w3.org/TR/SVG/masking.html#OverflowProperty
        if (style->overflowY() == OSCROLL)
            style->setOverflowY(OHIDDEN);
        else if (style->overflowY() == OAUTO)
            style->setOverflowY(OVISIBLE);

        if (style->overflowX() == OSCROLL)
            style->setOverflowX(OHIDDEN);
        else if (style->overflowX() == OAUTO)
            style->setOverflowX(OVISIBLE);

        // Only the root <svg> element in an SVG document fragment tree honors css position
        if (!(e->hasTagName(SVGNames::svgTag) && e->parentNode() && !e->parentNode()->isSVGElement()))
            style->setPosition(RenderStyle::initialPosition());
    }
#endif
}

bool CSSStyleSelector::checkRegionStyle(Element* regionElement)
{
    unsigned rulesSize = m_authorStyle->m_regionSelectorsAndRuleSets.size();
    for (unsigned i = 0; i < rulesSize; ++i) {
        ASSERT(m_authorStyle->m_regionSelectorsAndRuleSets.at(i).ruleSet.get());
        if (checkRegionSelector(m_authorStyle->m_regionSelectorsAndRuleSets.at(i).selector, regionElement))
            return true;
    }

    if (m_userStyle) {
        rulesSize = m_userStyle->m_regionSelectorsAndRuleSets.size();
        for (unsigned i = 0; i < rulesSize; ++i) {
            ASSERT(m_userStyle->m_regionSelectorsAndRuleSets.at(i).ruleSet.get());
            if (checkRegionSelector(m_userStyle->m_regionSelectorsAndRuleSets.at(i).selector, regionElement))
                return true;
        }
    }

    return false;
}

void CSSStyleSelector::updateFont()
{
    if (!m_fontDirty)
        return;

    checkForTextSizeAdjust();
    checkForGenericFamilyChange(style(), m_parentStyle);
    checkForZoomChange(style(), m_parentStyle);
    m_style->font().update(m_fontSelector);
    m_fontDirty = false;
}

void CSSStyleSelector::cacheBorderAndBackground()
{
    m_hasUAAppearance = m_style->hasAppearance();
    if (m_hasUAAppearance) {
        m_borderData = m_style->border();
        m_backgroundData = *m_style->backgroundLayers();
        m_backgroundColor = m_style->backgroundColor();
    }
}

PassRefPtr<CSSRuleList> CSSStyleSelector::styleRulesForElement(Element* e, unsigned rulesToInclude)
{
    return pseudoStyleRulesForElement(e, NOPSEUDO, rulesToInclude);
}

PassRefPtr<CSSRuleList> CSSStyleSelector::pseudoStyleRulesForElement(Element* e, PseudoId pseudoId, unsigned rulesToInclude)
{
    if (!e || !e->document()->haveStylesheetsLoaded())
        return 0;

    m_checker.setCollectingRulesOnly(true);

    initElement(e);
    initForStyleResolve(e, 0, pseudoId);

    MatchResult dummy;
    if (rulesToInclude & UAAndUserCSSRules) {
        // First we match rules from the user agent sheet.
        matchUARules(dummy);

        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            matchRules(m_userStyle.get(), dummy.ranges.firstUserRule, dummy.ranges.lastUserRule, rulesToInclude & EmptyCSSRules);
    }

    if (m_matchAuthorAndUserStyles && (rulesToInclude & AuthorCSSRules)) {
        m_sameOriginOnly = !(rulesToInclude & CrossOriginCSSRules);

        // Check the rules in author sheets.
        matchRules(m_authorStyle.get(), dummy.ranges.firstAuthorRule, dummy.ranges.lastAuthorRule, rulesToInclude & EmptyCSSRules);

        m_sameOriginOnly = false;
    }

    m_checker.setCollectingRulesOnly(false);

    return m_ruleList.release();
}

inline bool CSSStyleSelector::checkSelector(const RuleData& ruleData)
{
    m_dynamicPseudo = NOPSEUDO;
    m_checker.clearHasUnknownPseudoElements();

    // Let the slow path handle SVG as it has some additional rules regarding shadow trees.
    if (ruleData.hasFastCheckableSelector() && !m_element->isSVGElement()) {
        // We know this selector does not include any pseudo elements.
        if (m_checker.pseudoStyle() != NOPSEUDO)
            return false;
        // We know a sufficiently simple single part selector matches simply because we found it from the rule hash.
        // This is limited to HTML only so we don't need to check the namespace.
        if (ruleData.hasRightmostSelectorMatchingHTMLBasedOnRuleHash() && m_element->isHTMLElement()) {
            if (!ruleData.hasMultipartSelector())
                return true;
        } else if (!SelectorChecker::tagMatches(m_element, ruleData.selector()))
            return false;
        if (!SelectorChecker::fastCheckRightmostAttributeSelector(m_element, ruleData.selector()))
            return false;
        return m_checker.fastCheckSelector(ruleData.selector(), m_element);
    }

    // Slow path.
    SelectorChecker::SelectorMatch match = m_checker.checkSelector(ruleData.selector(), m_element, m_dynamicPseudo, false, SelectorChecker::VisitedMatchEnabled, style(), m_parentNode ? m_parentNode->renderStyle() : 0);
    if (match != SelectorChecker::SelectorMatches)
        return false;
    if (m_checker.pseudoStyle() != NOPSEUDO && m_checker.pseudoStyle() != m_dynamicPseudo)
        return false;
    return true;
}

bool CSSStyleSelector::checkRegionSelector(CSSSelector* regionSelector, Element* regionElement)
{
    if (!regionSelector || !regionElement)
        return false;

    m_checker.clearHasUnknownPseudoElements();
    m_checker.setPseudoStyle(NOPSEUDO);

    for (CSSSelector* s = regionSelector; s; s = CSSSelectorList::next(s))
        if (m_checker.checkSelector(s, regionElement))
            return true;

    return false;
}
    
bool CSSStyleSelector::determineStylesheetSelectorScopes(CSSStyleSheet* stylesheet, HashSet<AtomicStringImpl*>& idScopes, HashSet<AtomicStringImpl*>& classScopes)
{
    ASSERT(!stylesheet->isLoading());
    
    size_t size = stylesheet->length();
    for (size_t i = 0; i < size; i++) {
        CSSRule* rule = stylesheet->item(i);
        if (rule->isStyleRule()) {
            CSSStyleRule* styleRule = static_cast<CSSStyleRule*>(rule);
            if (!SelectorChecker::determineSelectorScopes(styleRule->selectorList(), idScopes, classScopes))
                return false;
            continue;
        } 
        if (rule->isImportRule()) {
            CSSImportRule* importRule = static_cast<CSSImportRule*>(rule);
            if (importRule->styleSheet()) {
                if (!determineStylesheetSelectorScopes(importRule->styleSheet(), idScopes, classScopes))
                    return false;
            }
            continue;
        }
        // FIXME: Media rules and maybe some others could be allowed.
        return false;
    }
    return true;
}

// -----------------------------------------------------------------

static inline bool isSelectorMatchingHTMLBasedOnRuleHash(const CSSSelector* selector)
{
    const AtomicString& selectorNamespace = selector->tag().namespaceURI();
    if (selectorNamespace != starAtom && selectorNamespace != xhtmlNamespaceURI)
        return false;
    if (selector->m_match == CSSSelector::None)
        return true;
    if (selector->tag() != starAtom)
        return false;
    if (SelectorChecker::isCommonPseudoClassSelector(selector))
        return true;
    return selector->m_match == CSSSelector::Id || selector->m_match == CSSSelector::Class;
}

static inline bool selectorListContainsUncommonAttributeSelector(const CSSSelector* selector)
{
    CSSSelectorList* selectorList = selector->selectorList();
    if (!selectorList)
        return false;
    for (CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
        if (subSelector->isAttributeSelector())
            return true;
    }
    return false;
}

static inline bool isCommonAttributeSelectorAttribute(const QualifiedName& attribute)
{
    // These are explicitly tested for equality in canShareStyleWithElement.
    return attribute == typeAttr || attribute == readonlyAttr;
}

static inline bool containsUncommonAttributeSelector(const CSSSelector* selector)
{
    while (selector) {
        // Allow certain common attributes (used in the default style) in the selectors that match the current element.
        if (selector->isAttributeSelector() && !isCommonAttributeSelectorAttribute(selector->attribute()))
            return true;
        if (selectorListContainsUncommonAttributeSelector(selector))
            return true;
        if (selector->relation() != CSSSelector::SubSelector)
            break;
        selector = selector->tagHistory();
    };

    for (selector = selector->tagHistory(); selector; selector = selector->tagHistory()) {
        if (selector->isAttributeSelector())
            return true;
        if (selectorListContainsUncommonAttributeSelector(selector))
            return true;
    }
    return false;
}

RuleData::RuleData(CSSStyleRule* rule, CSSSelector* selector, unsigned position)
    : m_rule(rule)
    , m_selector(selector)
    , m_specificity(selector->specificity())
    , m_position(position)
    , m_hasFastCheckableSelector(SelectorChecker::isFastCheckableSelector(selector))
    , m_hasMultipartSelector(!!selector->tagHistory())
    , m_hasRightmostSelectorMatchingHTMLBasedOnRuleHash(isSelectorMatchingHTMLBasedOnRuleHash(selector))
    , m_containsUncommonAttributeSelector(WebCore::containsUncommonAttributeSelector(selector))
    , m_linkMatchType(SelectorChecker::determineLinkMatchType(selector))
{
    SelectorChecker::collectIdentifierHashes(m_selector, m_descendantSelectorIdentifierHashes, maximumIdentifierCount);
}

RuleSet::RuleSet()
    : m_ruleCount(0)
    , m_autoShrinkToFitEnabled(true)
{
}

static inline void collectFeaturesFromSelector(CSSStyleSelector::Features& features, const CSSSelector* selector)
{
    if (selector->m_match == CSSSelector::Id)
        features.idsInRules.add(selector->value().impl());
    if (selector->isAttributeSelector())
        features.attrsInRules.add(selector->attribute().localName().impl());
    switch (selector->pseudoType()) {
    case CSSSelector::PseudoFirstLine:
        features.usesFirstLineRules = true;
        break;
    case CSSSelector::PseudoBefore:
    case CSSSelector::PseudoAfter:
        features.usesBeforeAfterRules = true;
        break;
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoVisited:
        features.usesLinkRules = true;
        break;
    default:
        break;
    }
}

static void collectFeaturesFromRuleData(CSSStyleSelector::Features& features, const RuleData& ruleData)
{
    bool foundSiblingSelector = false;
    for (CSSSelector* selector = ruleData.selector(); selector; selector = selector->tagHistory()) {
        collectFeaturesFromSelector(features, selector);
        
        if (CSSSelectorList* selectorList = selector->selectorList()) {
            for (CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                if (!foundSiblingSelector && selector->isSiblingSelector())
                    foundSiblingSelector = true;
                collectFeaturesFromSelector(features, subSelector);
            }
        } else if (!foundSiblingSelector && selector->isSiblingSelector())
            foundSiblingSelector = true;
    }
    if (foundSiblingSelector)
        features.siblingRules.append(CSSStyleSelector::RuleSelectorPair(ruleData.rule(), ruleData.selector()));
    if (ruleData.containsUncommonAttributeSelector())
        features.uncommonAttributeRules.append(CSSStyleSelector::RuleSelectorPair(ruleData.rule(), ruleData.selector()));
}
    
void RuleSet::addToRuleSet(AtomicStringImpl* key, AtomRuleMap& map, const RuleData& ruleData)
{
    if (!key)
        return;
    OwnPtr<Vector<RuleData> >& rules = map.add(key, nullptr).first->second;
    if (!rules)
        rules = adoptPtr(new Vector<RuleData>);
    rules->append(ruleData);
}

void RuleSet::addRule(CSSStyleRule* rule, CSSSelector* selector)
{
    RuleData ruleData(rule, selector, m_ruleCount++);
    collectFeaturesFromRuleData(m_features, ruleData);

    if (selector->m_match == CSSSelector::Id) {
        addToRuleSet(selector->value().impl(), m_idRules, ruleData);
        return;
    }
    if (selector->m_match == CSSSelector::Class) {
        addToRuleSet(selector->value().impl(), m_classRules, ruleData);
        return;
    }
    if (selector->isUnknownPseudoElement()) {
        addToRuleSet(selector->value().impl(), m_shadowPseudoElementRules, ruleData);
        return;
    }
    if (SelectorChecker::isCommonPseudoClassSelector(selector)) {
        switch (selector->pseudoType()) {
        case CSSSelector::PseudoLink:
        case CSSSelector::PseudoVisited:
        case CSSSelector::PseudoAnyLink:
            m_linkPseudoClassRules.append(ruleData);
            return;
        case CSSSelector::PseudoFocus:
            m_focusPseudoClassRules.append(ruleData);
            return;
        default:
            ASSERT_NOT_REACHED();
        }
        return;
    }
    const AtomicString& localName = selector->tag().localName();
    if (localName != starAtom) {
        addToRuleSet(localName.impl(), m_tagRules, ruleData);
        return;
    }
    m_universalRules.append(ruleData);
}

void RuleSet::addPageRule(CSSPageRule* rule)
{
    m_pageRules.append(RuleData(rule, rule->selectorList().first(), m_pageRules.size()));
}

void RuleSet::addRegionRule(WebKitCSSRegionRule* rule)
{
    RuleSet* regionRuleSet = new RuleSet;
    // The region rule set should take into account the position inside the parent rule set.
    // Otherwise, the rules inside region block might be incorrectly positioned before other similar rules from
    // the stylesheet that contains the region block.
    regionRuleSet->m_ruleCount = m_ruleCount;

    // Collect the region rules into a rule set
    CSSRuleList* regionStylingRules = rule->cssRules();
    unsigned rulesSize = regionStylingRules->length();
    for (unsigned i = 0; i < rulesSize; ++i) {
        CSSRule* regionStylingRule = regionStylingRules->item(i);
        if (regionStylingRule->isStyleRule())
            regionRuleSet->addStyleRule(static_cast<CSSStyleRule*>(regionStylingRule));
    }

    m_regionSelectorsAndRuleSets.append(RuleSetSelectorPair(rule->selectorList().first(), regionRuleSet));
}

void RuleSet::addRulesFromSheet(CSSStyleSheet* sheet, const MediaQueryEvaluator& medium, CSSStyleSelector* styleSelector)
{
    ASSERT(sheet);

    // No media implies "all", but if a media list exists it must
    // contain our current medium
    if (sheet->media() && !medium.eval(sheet->media(), styleSelector))
        return; // the style sheet doesn't apply

    int len = sheet->length();

    for (int i = 0; i < len; i++) {
        CSSRule* rule = sheet->item(i);
        if (rule->isStyleRule())
            addStyleRule(static_cast<CSSStyleRule*>(rule));
        else if (rule->isPageRule())
            addPageRule(static_cast<CSSPageRule*>(rule));
        else if (rule->isImportRule()) {
            CSSImportRule* import = static_cast<CSSImportRule*>(rule);
            if (import->styleSheet() && (!import->media() || medium.eval(import->media(), styleSelector)))
                addRulesFromSheet(import->styleSheet(), medium, styleSelector);
        }
        else if (rule->isMediaRule()) {
            CSSMediaRule* r = static_cast<CSSMediaRule*>(rule);
            CSSRuleList* rules = r->cssRules();

            if ((!r->media() || medium.eval(r->media(), styleSelector)) && rules) {
                // Traverse child elements of the @media rule.
                for (unsigned j = 0; j < rules->length(); j++) {
                    CSSRule *childItem = rules->item(j);
                    if (childItem->isStyleRule())
                        addStyleRule(static_cast<CSSStyleRule*>(childItem));
                    else if (childItem->isPageRule())
                        addPageRule(static_cast<CSSPageRule*>(childItem));
                    else if (childItem->isFontFaceRule() && styleSelector) {
                        // Add this font face to our set.
                        const CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(childItem);
                        styleSelector->fontSelector()->addFontFaceRule(fontFaceRule);
                        styleSelector->invalidateMatchedDeclarationCache();
                    } else if (childItem->isKeyframesRule() && styleSelector) {
                        // Add this keyframe rule to our set.
                        styleSelector->addKeyframeStyle(static_cast<WebKitCSSKeyframesRule*>(childItem));
                    }
                }   // for rules
            }   // if rules
        } else if (rule->isFontFaceRule() && styleSelector) {
            // Add this font face to our set.
            const CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(rule);
            styleSelector->fontSelector()->addFontFaceRule(fontFaceRule);
            styleSelector->invalidateMatchedDeclarationCache();
        } else if (rule->isKeyframesRule())
            styleSelector->addKeyframeStyle(static_cast<WebKitCSSKeyframesRule*>(rule));
        else if (rule->isRegionRule() && styleSelector)
            addRegionRule(static_cast<WebKitCSSRegionRule*>(rule));
    }
    if (m_autoShrinkToFitEnabled)
        shrinkToFit();
}

void RuleSet::addStyleRule(CSSStyleRule* rule)
{
    for (CSSSelector* s = rule->selectorList().first(); s; s = CSSSelectorList::next(s))
        addRule(rule, s);
}

static inline void shrinkMapVectorsToFit(RuleSet::AtomRuleMap& map)
{
    RuleSet::AtomRuleMap::iterator end = map.end();
    for (RuleSet::AtomRuleMap::iterator it = map.begin(); it != end; ++it)
        it->second->shrinkToFit();
}

void RuleSet::shrinkToFit()
{
    shrinkMapVectorsToFit(m_idRules);
    shrinkMapVectorsToFit(m_classRules);
    shrinkMapVectorsToFit(m_tagRules);
    shrinkMapVectorsToFit(m_shadowPseudoElementRules);
    m_linkPseudoClassRules.shrinkToFit();
    m_focusPseudoClassRules.shrinkToFit();
    m_universalRules.shrinkToFit();
    m_pageRules.shrinkToFit();
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

static Length convertToLength(CSSPrimitiveValue* primitiveValue, RenderStyle* style, RenderStyle* rootStyle, bool toFloat, double multiplier = 1, bool *ok = 0)
{
    // This function is tolerant of a null style value. The only place style is used is in
    // length measurements, like 'ems' and 'px'. And in those cases style is only used
    // when the units are EMS or EXS. So we will just fail in those cases.
    Length l;
    if (!primitiveValue) {
        if (ok)
            *ok = false;
    } else {
        if (!style && primitiveValue->isFontRelativeLength()) {
            if (ok)
                *ok = false;
        } else if (primitiveValue->isLength()) {
            if (toFloat)
                l = Length(primitiveValue->computeLength<double>(style, rootStyle, multiplier), Fixed);
            else
                l = primitiveValue->computeLength<Length>(style, rootStyle, multiplier);
        } else if (primitiveValue->isPercentage())
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else if (primitiveValue->isNumber())
            l = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
        else if (ok)
            *ok = false;
    }
    return l;
}

static Length convertToIntLength(CSSPrimitiveValue* primitiveValue, RenderStyle* style, RenderStyle* rootStyle, double multiplier = 1, bool *ok = 0)
{
    return convertToLength(primitiveValue, style, rootStyle, false, multiplier, ok);
}

static Length convertToFloatLength(CSSPrimitiveValue* primitiveValue, RenderStyle* style, RenderStyle* rootStyle, double multiplier = 1, bool *ok = 0)
{
    return convertToLength(primitiveValue, style, rootStyle, true, multiplier, ok);
}

static inline bool isInsideRegionRule(CSSMutableStyleDeclaration* styleDeclaration)
{
    ASSERT(styleDeclaration);

    CSSRule* parentRule = styleDeclaration->parentRule();
    while (parentRule) {
        if (parentRule->isRegionRule())
            return true;
        parentRule = parentRule->parentRule();
    }
    return false;
}

template <bool applyFirst>
void CSSStyleSelector::applyDeclaration(CSSMutableStyleDeclaration* styleDeclaration, bool isImportant, bool inheritedOnly)
{
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willProcessRule(document(), styleDeclaration->parentRule());
    CSSMutableStyleDeclaration::const_iterator end = styleDeclaration->end();

    bool styleDeclarationInsideRegionRule = m_regionForStyling ? isInsideRegionRule(styleDeclaration) : false;

    for (CSSMutableStyleDeclaration::const_iterator it = styleDeclaration->begin(); it != end; ++it) {
        const CSSProperty& current = *it;
        if (isImportant != current.isImportant())
            continue;
        if (inheritedOnly && !current.isInherited()) {
            // If the property value is explicitly inherited, we need to apply further non-inherited properties
            // as they might override the value inherited here. For this reason we don't allow declarations with
            // explicitly inherited properties to be cached.
            ASSERT(!current.value()->isInheritedValue());
            continue;
        }
        int property = current.id();

        // Filter the properties that can be applied using region styling.
        if (styleDeclarationInsideRegionRule && !CSSStyleSelector::isValidRegionStyleProperty(property))
            continue;

        if (applyFirst) {
            COMPILE_ASSERT(firstCSSProperty == CSSPropertyColor, CSS_color_is_first_property);
            COMPILE_ASSERT(CSSPropertyZoom == CSSPropertyColor + 18, CSS_zoom_is_end_of_first_prop_range);
            COMPILE_ASSERT(CSSPropertyLineHeight == CSSPropertyZoom + 1, CSS_line_height_is_after_zoom);
            // give special priority to font-xxx, color properties, etc
            if (property > CSSPropertyLineHeight)
                continue;
            // we apply line-height later
            if (property == CSSPropertyLineHeight) {
                m_lineHeightValue = current.value();
                continue;
            }
            applyProperty(current.id(), current.value());
            continue;
        }
        if (property > CSSPropertyLineHeight)
            applyProperty(current.id(), current.value());
    }
    InspectorInstrumentation::didProcessRule(cookie);
}

template <bool applyFirst>
void CSSStyleSelector::applyDeclarations(bool isImportant, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    if (m_style->insideLink() != NotInsideLink) {
        for (int i = startIndex; i <= endIndex; ++i) {
            CSSMutableStyleDeclaration* styleDeclaration = m_matchedDecls[i].styleDeclaration.get();
            unsigned linkMatchType = m_matchedDecls[i].linkMatchType;
            // FIXME: It would be nicer to pass these as arguments but that requires changes in many places.
            m_applyPropertyToRegularStyle = linkMatchType & SelectorChecker::MatchLink;
            m_applyPropertyToVisitedLinkStyle = linkMatchType & SelectorChecker::MatchVisited;

            applyDeclaration<applyFirst>(styleDeclaration, isImportant, inheritedOnly);
        }
        m_applyPropertyToRegularStyle = true;
        m_applyPropertyToVisitedLinkStyle = false;
        return;
    }
    for (int i = startIndex; i <= endIndex; ++i)
        applyDeclaration<applyFirst>(m_matchedDecls[i].styleDeclaration.get(), isImportant, inheritedOnly);
}

unsigned CSSStyleSelector::computeDeclarationHash(MatchedStyleDeclaration* declarations, unsigned size)
{
    return StringHasher::hashMemory(declarations, sizeof(MatchedStyleDeclaration) * size);
}

bool operator==(const CSSStyleSelector::MatchRanges& a, const CSSStyleSelector::MatchRanges& b)
{
    return a.firstUARule == b.firstUARule
        && a.lastUARule == b.lastUARule
        && a.firstAuthorRule == b.firstAuthorRule
        && a.lastAuthorRule == b.lastAuthorRule
        && a.firstUserRule == b.firstUserRule
        && a.lastUserRule == b.lastUserRule;
}

bool operator!=(const CSSStyleSelector::MatchRanges& a, const CSSStyleSelector::MatchRanges& b)
{
    return !(a == b);
}

bool operator==(const CSSStyleSelector::MatchedStyleDeclaration& a, const CSSStyleSelector::MatchedStyleDeclaration& b)
{
    return a.styleDeclaration == b.styleDeclaration && a.linkMatchType == b.linkMatchType;
}

bool operator!=(const CSSStyleSelector::MatchedStyleDeclaration& a, const CSSStyleSelector::MatchedStyleDeclaration& b)
{
    return !(a == b);
}

const CSSStyleSelector::MatchedStyleDeclarationCacheItem* CSSStyleSelector::findFromMatchedDeclarationCache(unsigned hash, const MatchResult& matchResult)
{
    ASSERT(hash);

    MatchedStyleDeclarationCache::iterator it = m_matchedStyleDeclarationCache.find(hash);
    if (it == m_matchedStyleDeclarationCache.end())
        return 0;
    MatchedStyleDeclarationCacheItem& cacheItem = it->second;

    size_t size = m_matchedDecls.size();
    if (size != cacheItem.matchedStyleDeclarations.size())
        return 0;
    for (size_t i = 0; i < size; ++i) {
        if (m_matchedDecls[i] != cacheItem.matchedStyleDeclarations[i])
            return 0;
    }
    if (cacheItem.ranges != matchResult.ranges)
        return 0;
    return &cacheItem;
}

void CSSStyleSelector::addToMatchedDeclarationCache(const RenderStyle* style, const RenderStyle* parentStyle, unsigned hash, const MatchResult& matchResult)
{
    static unsigned matchedDeclarationCacheAdditionsBetweenSweeps = 100;
    if (++m_matchedDeclarationCacheAdditionsSinceLastSweep >= matchedDeclarationCacheAdditionsBetweenSweeps) {
        sweepMatchedDeclarationCache();
        m_matchedDeclarationCacheAdditionsSinceLastSweep = 0;
    }

    ASSERT(hash);
    MatchedStyleDeclarationCacheItem cacheItem;
    cacheItem.matchedStyleDeclarations.append(m_matchedDecls);
    cacheItem.ranges = matchResult.ranges;
    // Note that we don't cache the original RenderStyle instance. It may be further modified.
    // The RenderStyle in the cache is really just a holder for the substructures and never used as-is.
    cacheItem.renderStyle = RenderStyle::clone(style);
    cacheItem.parentRenderStyle = RenderStyle::clone(parentStyle);
    m_matchedStyleDeclarationCache.add(hash, cacheItem);
}
    
void CSSStyleSelector::invalidateMatchedDeclarationCache()
{
    m_matchedStyleDeclarationCache.clear();
}

static bool isCacheableInMatchedDeclarationCache(const RenderStyle* style, const RenderStyle* parentStyle)
{
    if (style->unique() || (style->styleType() != NOPSEUDO && parentStyle->unique()))
        return false;
    if (style->hasAppearance())
        return false;
    if (style->zoom() != RenderStyle::initialZoom())
        return false;
    // The cache assumes static knowledge about which properties are inherited.
    if (parentStyle->hasExplicitlyInheritedProperties())
        return false;
    return true;
}

void CSSStyleSelector::applyMatchedDeclarations(const MatchResult& matchResult)
{
    unsigned cacheHash = matchResult.isCacheable ? computeDeclarationHash(m_matchedDecls.data(), m_matchedDecls.size()) : 0;
    bool applyInheritedOnly = false;
    const MatchedStyleDeclarationCacheItem* cacheItem = 0;
    if (cacheHash && (cacheItem = findFromMatchedDeclarationCache(cacheHash, matchResult))) {
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the 
        // element context. This is fast and saves memory by reusing the style data structures.
        m_style->copyNonInheritedFrom(cacheItem->renderStyle.get());
        if (m_parentStyle->inheritedDataShared(cacheItem->parentRenderStyle.get())) {
            EInsideLink linkStatus = m_style->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            m_style->inheritFrom(cacheItem->renderStyle.get());
            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            m_style->setInsideLink(linkStatus);
            return;
        }
        applyInheritedOnly = true; 
    }
    // Now we have all of the matched rules in the appropriate order. Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    m_lineHeightValue = 0;
    applyDeclarations<true>(false, 0, m_matchedDecls.size() - 1, applyInheritedOnly);
    applyDeclarations<true>(true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyDeclarations<true>(true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyDeclarations<true>(true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    if (cacheItem && cacheItem->renderStyle->effectiveZoom() != m_style->effectiveZoom()) {
        m_fontDirty = true;
        applyInheritedOnly = false;
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size.
    if (m_lineHeightValue)
        applyProperty(CSSPropertyLineHeight, m_lineHeightValue);

    // Many properties depend on the font. If it changes we just apply all properties.
    if (cacheItem && cacheItem->renderStyle->fontDescription() != m_style->fontDescription())
        applyInheritedOnly = false;

    // Now do the normal priority UA properties.
    applyDeclarations<false>(false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);
    
    // Cache our border and background so that we can examine them later.
    cacheBorderAndBackground();
    
    // Now do the author and user normal priority properties and all the !important properties.
    applyDeclarations<false>(false, matchResult.ranges.lastUARule + 1, m_matchedDecls.size() - 1, applyInheritedOnly);
    applyDeclarations<false>(true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyDeclarations<false>(true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyDeclarations<false>(true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);
    
    loadPendingImages();
    
#if ENABLE(CSS_SHADERS)
    loadPendingShaders();
#endif
    
    ASSERT(!m_fontDirty);
    
    if (cacheItem || !cacheHash)
        return;
    if (!isCacheableInMatchedDeclarationCache(m_style.get(), m_parentStyle))
        return;
    addToMatchedDeclarationCache(m_style.get(), m_parentStyle, cacheHash, matchResult);
}

void CSSStyleSelector::matchPageRules(RuleSet* rules, bool isLeftPage, bool isFirstPage, const String& pageName)
{
    m_matchedRules.clear();

    if (!rules)
        return;

    matchPageRulesForList(rules->pageRules(), isLeftPage, isFirstPage, pageName);

    // If we didn't match any rules, we're done.
    if (m_matchedRules.isEmpty())
        return;

    // Sort the set of matched rules.
    sortMatchedRules();

    // Now transfer the set of matched rules over to our list of decls.
    for (unsigned i = 0; i < m_matchedRules.size(); i++)
        addMatchedDeclaration(m_matchedRules[i]->rule()->declaration());
}

void CSSStyleSelector::matchPageRulesForList(const Vector<RuleData>* rules, bool isLeftPage, bool isFirstPage, const String& pageName)
{
    if (!rules)
        return;

    unsigned size = rules->size();
    for (unsigned i = 0; i < size; ++i) {
        const RuleData& ruleData = rules->at(i);
        CSSStyleRule* rule = ruleData.rule();
        const AtomicString& selectorLocalName = ruleData.selector()->tag().localName();
        if (selectorLocalName != starAtom && selectorLocalName != pageName)
            continue;
        CSSSelector::PseudoType pseudoType = ruleData.selector()->pseudoType();
        if ((pseudoType == CSSSelector::PseudoLeftPage && !isLeftPage)
            || (pseudoType == CSSSelector::PseudoRightPage && isLeftPage)
            || (pseudoType == CSSSelector::PseudoFirstPage && !isFirstPage))
            continue;

        // If the rule has no properties to apply, then ignore it.
        CSSMutableStyleDeclaration* decl = rule->declaration();
        if (!decl || !decl->length())
            continue;

        // Add this rule to our list of matched rules.
        addMatchedRule(&ruleData);
    }
}

bool CSSStyleSelector::isLeftPage(int pageIndex) const
{
    bool isFirstPageLeft = false;
    if (!m_rootElementStyle->isLeftToRightDirection())
        isFirstPageLeft = true;

    return (pageIndex + (isFirstPageLeft ? 1 : 0)) % 2;
}

bool CSSStyleSelector::isFirstPage(int pageIndex) const
{
    // FIXME: In case of forced left/right page, page at index 1 (not 0) can be the first page.
    return (!pageIndex);
}

String CSSStyleSelector::pageName(int /* pageIndex */) const
{
    // FIXME: Implement page index to page name mapping.
    return "";
}

void CSSStyleSelector::applyPropertyToStyle(int id, CSSValue* value, RenderStyle* style)
{
    initElement(0);
    initForStyleResolve(0, style);
    m_style = style;
    applyPropertyToCurrentStyle(id, value);
}

void CSSStyleSelector::applyPropertyToCurrentStyle(int id, CSSValue* value)
{
    if (value)
        applyProperty(id, value);
}

inline bool isValidVisitedLinkProperty(int id)
{
    switch(static_cast<CSSPropertyID>(id)) {
        case CSSPropertyBackgroundColor:
        case CSSPropertyBorderLeftColor:
        case CSSPropertyBorderRightColor:
        case CSSPropertyBorderTopColor:
        case CSSPropertyBorderBottomColor:
        case CSSPropertyColor:
        case CSSPropertyOutlineColor:
        case CSSPropertyWebkitColumnRuleColor:
        case CSSPropertyWebkitTextEmphasisColor:
        case CSSPropertyWebkitTextFillColor:
        case CSSPropertyWebkitTextStrokeColor:
        // Also allow shorthands so that inherit/initial still work.
        case CSSPropertyBackground:
        case CSSPropertyBorderLeft:
        case CSSPropertyBorderRight:
        case CSSPropertyBorderTop:
        case CSSPropertyBorderBottom:
        case CSSPropertyOutline:
        case CSSPropertyWebkitColumnRule:
#if ENABLE(SVG)
        case CSSPropertyFill:
        case CSSPropertyStroke:
#endif
            return true;
        default:
            break;
    }

    return false;
}

// http://dev.w3.org/csswg/css3-regions/#the-at-region-style-rule
// FIXME: add incremental support for other region styling properties.
inline bool CSSStyleSelector::isValidRegionStyleProperty(int id)
{
    switch (static_cast<CSSPropertyID>(id)) {
    case CSSPropertyBackgroundColor:
        return true;
    default:
        break;
    }

    return false;
}

// SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
// of each individual length value in the render style / tree. CSSPrimitiveValue::computeLength*()
// multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
// Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
// need to respect the scaling. RenderBox (the parent class of RenderSVGRoot) grabs values like
// width/height/border/padding/... from the RenderStyle -> for SVG these values would never scale,
// if we'd pass a 1.0 zoom factor everyhwere. So we only pass a zoom factor of 1.0 for specific
// properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
bool CSSStyleSelector::useSVGZoomRules()
{
    return m_element && m_element->isSVGElement();
}

#if ENABLE(CSS_GRID_LAYOUT)

static bool createGridTrackBreadth(CSSPrimitiveValue* primitiveValue, CSSStyleSelector* selector, Length& length)
{
    if (primitiveValue->getIdent() == CSSValueAuto) {
        length = Length();
        return true;
    }

    if (primitiveValue->isLength()) {
        length = primitiveValue->computeLength<Length>(selector->style(), selector->rootElementStyle(), selector->style()->effectiveZoom());
        length.setQuirk(primitiveValue->isQuirkValue());
        return true;
    }

    if (primitiveValue->isPercentage()) {
        length = Length(primitiveValue->getDoubleValue(), Percent);
        return true;
    }

    return false;
}

static bool createGridTrackList(CSSValue* value, Vector<Length>& lengths, CSSStyleSelector* selector)
{
    // Handle 'none'.
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (primitiveValue->getIdent() == CSSValueNone) {
            lengths.append(Length(Undefined));
            return true;
        }
        return false;
    }

    if (value->isValueList()) {
        for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
            CSSValue* currValue = i.value();
            if (!currValue->isPrimitiveValue())
                return false;

            Length length;
            if (!createGridTrackBreadth(static_cast<CSSPrimitiveValue*>(currValue), selector, length))
                return false;

            lengths.append(length);
        }
        return true;
    }

    return false;
}
#endif

void CSSStyleSelector::applyProperty(int id, CSSValue *value)
{
    bool isInherit = m_parentNode && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!m_parentNode && value->isInheritedValue());

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit

    if (!applyPropertyToRegularStyle() && (!applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    CSSPropertyID property = static_cast<CSSPropertyID>(id);

    if (isInherit && m_parentStyle && !m_parentStyle->hasExplicitlyInheritedProperties() && !CSSProperty::isInheritedProperty(property))
        m_parentStyle->setHasExplicitlyInheritedProperties();

    // check lookup table for implementations and use when available
    const PropertyHandler& handler = m_applyProperty.propertyHandler(property);
    if (handler.isValid()) {
        if (isInherit)
            handler.applyInheritValue(this);
        else if (isInitial)
            handler.applyInitialValue(this);
        else
            handler.applyValue(this, value);
        return;
    }

    CSSPrimitiveValue* primitiveValue = value->isPrimitiveValue() ? static_cast<CSSPrimitiveValue*>(value) : 0;

    float zoomFactor = m_style->effectiveZoom();

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.  Shorthands (e.g. border, background) occur in this list as well and
    // are only hit when mapping "inherit" or "initial" into front-end values.
    switch (property) {
// ident only properties
    case CSSPropertyBorderCollapse:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(borderCollapse, BorderCollapse)
        return;
    case CSSPropertyCaptionSide:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(captionSide, CaptionSide)
        return;
    case CSSPropertyClear:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(clear, Clear)
        return;
    case CSSPropertyEmptyCells:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(emptyCells, EmptyCells)
        return;
    case CSSPropertyFloat:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(floating, Floating)
        return;
    case CSSPropertyPageBreakBefore:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(pageBreakBefore, PageBreakBefore, PageBreak)
        return;
    case CSSPropertyPageBreakAfter:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(pageBreakAfter, PageBreakAfter, PageBreak)
        return;
    case CSSPropertyPageBreakInside:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(pageBreakInside, PageBreakInside, PageBreak)
        return;
    case CSSPropertyPosition:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(position, Position)
        return;
    case CSSPropertyTableLayout:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(tableLayout, TableLayout)
        return;
    case CSSPropertyUnicodeBidi:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(unicodeBidi, UnicodeBidi)
        return;
    case CSSPropertyTextTransform:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(textTransform, TextTransform)
        return;
    case CSSPropertyVisibility:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(visibility, Visibility)
        return;
    case CSSPropertyWhiteSpace:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(whiteSpace, WhiteSpace)
        return;
// uri || inherit
    case CSSPropertyWordBreak:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(wordBreak, WordBreak)
        return;
    case CSSPropertyWordWrap:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(wordWrap, WordWrap)
        return;
    case CSSPropertyWebkitNbspMode:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(nbspMode, NBSPMode)
        return;
    case CSSPropertyWebkitLineBreak:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(khtmlLineBreak, KHTMLLineBreak)
        return;
    case CSSPropertyWebkitMatchNearestMailBlockquoteColor:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(matchNearestMailBlockquoteColor, MatchNearestMailBlockquoteColor)
        return;
    case CSSPropertyWidows:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(widows, Widows)
        return;
    case CSSPropertyOrphans:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(orphans, Orphans)
        return;

// lists
    case CSSPropertyContent:
        // list of string, uri, counter, attr, i
    {
        // FIXME: In CSS3, it will be possible to inherit content.  In CSS2 it is not.  This
        // note is a reminder that eventually "inherit" needs to be supported.

        if (isInitial) {
            m_style->clearContent();
            return;
        }

        if (!value->isValueList())
            return;

        bool didSet = false;
        for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
            CSSValue* item = i.value();
            if (item->isImageGeneratorValue()) {
                m_style->setContent(StyleGeneratedImage::create(static_cast<CSSImageGeneratorValue*>(item)), didSet);
                didSet = true;
            }

            if (!item->isPrimitiveValue())
                continue;

            CSSPrimitiveValue* contentValue = static_cast<CSSPrimitiveValue*>(item);

            if (contentValue->isString()) {
                m_style->setContent(contentValue->getStringValue().impl(), didSet);
                didSet = true;
            } else if (contentValue->isAttr()) {
                // FIXME: Can a namespace be specified for an attr(foo)?
                if (m_style->styleType() == NOPSEUDO)
                    m_style->setUnique();
                else
                    m_parentStyle->setUnique();
                QualifiedName attr(nullAtom, contentValue->getStringValue().impl(), nullAtom);
                const AtomicString& value = m_element->getAttribute(attr);
                m_style->setContent(value.isNull() ? emptyAtom : value.impl(), didSet);
                didSet = true;
                // register the fact that the attribute value affects the style
                m_features.attrsInRules.add(attr.localName().impl());
            } else if (contentValue->isURI()) {
                if (!contentValue->isImageValue())
                    break;
                m_style->setContent(cachedOrPendingFromValue(CSSPropertyContent, static_cast<CSSImageValue*>(contentValue)), didSet);
                didSet = true;
            } else if (contentValue->isCounter()) {
                Counter* counterValue = contentValue->getCounterValue();
                EListStyleType listStyleType = NoneListStyle;
                int listStyleIdent = counterValue->listStyleIdent();
                if (listStyleIdent != CSSValueNone)
                    listStyleType = static_cast<EListStyleType>(listStyleIdent - CSSValueDisc);
                OwnPtr<CounterContent> counter = adoptPtr(new CounterContent(counterValue->identifier(), listStyleType, counterValue->separator()));
                m_style->setContent(counter.release(), didSet);
                didSet = true;
            } else {
                switch (contentValue->getIdent()) {
                case CSSValueOpenQuote:
                    m_style->setContent(OPEN_QUOTE, didSet);
                    didSet = true;
                    break;
                case CSSValueCloseQuote:
                    m_style->setContent(CLOSE_QUOTE, didSet);
                    didSet = true;
                    break;
                case CSSValueNoOpenQuote:
                    m_style->setContent(NO_OPEN_QUOTE, didSet);
                    didSet = true;
                    break;
                case CSSValueNoCloseQuote:
                    m_style->setContent(NO_CLOSE_QUOTE, didSet);
                    didSet = true;
                    break;
                default:
                    // normal and none do not have any effect.
                    {}
                }
            }
        }
        if (!didSet)
            m_style->clearContent();
        return;
    }
    case CSSPropertyQuotes:
        if (isInherit) {
            if (m_parentStyle)
                m_style->setQuotes(m_parentStyle->quotes());
            return;
        }
        if (isInitial) {
            m_style->setQuotes(0);
            return;
        }
        if (value->isValueList()) {
            CSSValueList* list = static_cast<CSSValueList*>(value);
            QuotesData* data = QuotesData::create(list->length());
            if (!data)
                return; // Out of memory
            String* quotes = data->data();
            for (CSSValueListIterator i = list; i.hasMore(); i.advance()) {
                CSSValue* item = i.value();
                ASSERT(item->isPrimitiveValue());
                primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                ASSERT(primitiveValue->isString());
                quotes[i.index()] = primitiveValue->getStringValue();
            }
            m_style->setQuotes(adoptRef(data));
        } else if (primitiveValue) {
            ASSERT(primitiveValue->isIdent());
            if (primitiveValue->getIdent() == CSSValueNone)
                m_style->setQuotes(adoptRef(QuotesData::create(0)));
        }
        return;
    case CSSPropertyFontFamily: {
        // list of strings and ids
        if (isInherit) {
            FontDescription parentFontDescription = m_parentStyle->fontDescription();
            FontDescription fontDescription = m_style->fontDescription();
            fontDescription.setGenericFamily(parentFontDescription.genericFamily());
            fontDescription.setFamily(parentFontDescription.firstFamily());
            fontDescription.setIsSpecifiedFont(parentFontDescription.isSpecifiedFont());
            setFontDescription(fontDescription);
            return;
        } else if (isInitial) {
            FontDescription initialDesc = FontDescription();
            FontDescription fontDescription = m_style->fontDescription();
            // We need to adjust the size to account for the generic family change from monospace
            // to non-monospace.
            if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize())
                setFontSize(fontDescription, fontSizeForKeyword(m_checker.document(), CSSValueXxSmall + fontDescription.keywordSize() - 1, false));
            fontDescription.setGenericFamily(initialDesc.genericFamily());
            if (!initialDesc.firstFamily().familyIsEmpty())
                fontDescription.setFamily(initialDesc.firstFamily());
            setFontDescription(fontDescription);
            return;
        }

        if (!value->isValueList())
            return;
        FontDescription fontDescription = m_style->fontDescription();
        FontFamily& firstFamily = fontDescription.firstFamily();
        FontFamily* currFamily = 0;

        // Before mapping in a new font-family property, we should reset the generic family.
        bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();
        fontDescription.setGenericFamily(FontDescription::NoFamily);

        for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
            CSSValue* item = i.value();
            if (!item->isPrimitiveValue())
                continue;
            CSSPrimitiveValue* contentValue = static_cast<CSSPrimitiveValue*>(item);
            AtomicString face;
            Settings* settings = m_checker.document()->settings();
            if (contentValue->isString()) {
                if (contentValue->isFontFamilyValue())
                    face = static_cast<FontFamilyValue*>(contentValue)->familyName();
            } else if (settings) {
                switch (contentValue->getIdent()) {
                    case CSSValueWebkitBody:
                        face = settings->standardFontFamily();
                        break;
                    case CSSValueSerif:
                        face = serifFamily;
                        fontDescription.setGenericFamily(FontDescription::SerifFamily);
                        break;
                    case CSSValueSansSerif:
                        face = sansSerifFamily;
                        fontDescription.setGenericFamily(FontDescription::SansSerifFamily);
                        break;
                    case CSSValueCursive:
                        face = cursiveFamily;
                        fontDescription.setGenericFamily(FontDescription::CursiveFamily);
                        break;
                    case CSSValueFantasy:
                        face = fantasyFamily;
                        fontDescription.setGenericFamily(FontDescription::FantasyFamily);
                        break;
                    case CSSValueMonospace:
                        face = monospaceFamily;
                        fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
                        break;
                    case CSSValueWebkitPictograph:
                        face = pictographFamily;
                        fontDescription.setGenericFamily(FontDescription::PictographFamily);
                        break;
                }
            }

            if (!face.isEmpty()) {
                if (!currFamily) {
                    // Filling in the first family.
                    firstFamily.setFamily(face);
                    firstFamily.appendFamily(0); // Remove any inherited family-fallback list.
                    currFamily = &firstFamily;
                    fontDescription.setIsSpecifiedFont(fontDescription.genericFamily() == FontDescription::NoFamily);
                } else {
                    RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
                    newFamily->setFamily(face);
                    currFamily->appendFamily(newFamily);
                    currFamily = newFamily.get();
                }
            }
        }

        // We can't call useFixedDefaultSize() until all new font families have been added
        // If currFamily is non-zero then we set at least one family on this description.
        if (currFamily) {
            if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize)
                setFontSize(fontDescription, fontSizeForKeyword(m_checker.document(), CSSValueXxSmall + fontDescription.keywordSize() - 1, !oldFamilyUsedFixedDefaultSize));

            setFontDescription(fontDescription);
        }
        return;
    }
// shorthand properties
    case CSSPropertyBackground:
        if (isInitial) {
            m_style->clearBackgroundLayers();
            m_style->setBackgroundColor(Color());
        }
        else if (isInherit) {
            m_style->inheritBackgroundLayers(*m_parentStyle->backgroundLayers());
            m_style->setBackgroundColor(m_parentStyle->backgroundColor());
        }
        return;
    case CSSPropertyWebkitMask:
        if (isInitial)
            m_style->clearMaskLayers();
        else if (isInherit)
            m_style->inheritMaskLayers(*m_parentStyle->maskLayers());
        return;
    case CSSPropertyFont:
        if (isInherit) {
            FontDescription fontDescription = m_parentStyle->fontDescription();
            m_style->setLineHeight(m_parentStyle->lineHeight());
            m_lineHeightValue = 0;
            setFontDescription(fontDescription);
        } else if (isInitial) {
            Settings* settings = m_checker.document()->settings();
            ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
            if (!settings)
                return;
            FontDescription fontDescription;
            fontDescription.setGenericFamily(FontDescription::StandardFamily);
            fontDescription.setRenderingMode(settings->fontRenderingMode());
            fontDescription.setUsePrinterFont(m_checker.document()->printing());
            const AtomicString& standardFontFamily = m_checker.document()->settings()->standardFontFamily();
            if (!standardFontFamily.isEmpty()) {
                fontDescription.firstFamily().setFamily(standardFontFamily);
                fontDescription.firstFamily().appendFamily(0);
            }
            fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
            setFontSize(fontDescription, fontSizeForKeyword(m_checker.document(), CSSValueMedium, false));
            m_style->setLineHeight(RenderStyle::initialLineHeight());
            m_lineHeightValue = 0;
            setFontDescription(fontDescription);
        } else if (primitiveValue) {
            m_style->setLineHeight(RenderStyle::initialLineHeight());
            m_lineHeightValue = 0;

            FontDescription fontDescription;
            RenderTheme::defaultTheme()->systemFont(primitiveValue->getIdent(), fontDescription);

            // Double-check and see if the theme did anything.  If not, don't bother updating the font.
            if (fontDescription.isAbsoluteSize()) {
                // Make sure the rendering mode and printer font settings are updated.
                Settings* settings = m_checker.document()->settings();
                ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
                if (!settings)
                    return;
                fontDescription.setRenderingMode(settings->fontRenderingMode());
                fontDescription.setUsePrinterFont(m_checker.document()->printing());

                // Handle the zoom factor.
                fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(m_checker.document(), m_style.get(), fontDescription.isAbsoluteSize(), fontDescription.specifiedSize(), useSVGZoomRules()));
                setFontDescription(fontDescription);
            }
        } else if (value->isFontValue()) {
            FontValue *font = static_cast<FontValue*>(value);
            if (!font->style || !font->variant || !font->weight ||
                 !font->size || !font->lineHeight || !font->family)
                return;
            applyProperty(CSSPropertyFontStyle, font->style.get());
            applyProperty(CSSPropertyFontVariant, font->variant.get());
            applyProperty(CSSPropertyFontWeight, font->weight.get());
            // The previous properties can dirty our font but they don't try to read the font's
            // properties back, which is safe. However if font-size is using the 'ex' unit, it will
            // need query the dirtied font's x-height to get the computed size. To be safe in this
            // case, let's just update the font now.
            updateFont();
            applyProperty(CSSPropertyFontSize, font->size.get());

            m_lineHeightValue = font->lineHeight.get();

            applyProperty(CSSPropertyFontFamily, font->family.get());
        }
        return;

    // CSS3 Properties
    case CSSPropertyWebkitAppearance:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(appearance, Appearance)
        return;
    case CSSPropertyImageRendering:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(imageRendering, ImageRendering);
        return;
    case CSSPropertyTextShadow:
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow: {
        if (isInherit) {
            if (id == CSSPropertyTextShadow)
                return m_style->setTextShadow(m_parentStyle->textShadow() ? adoptPtr(new ShadowData(*m_parentStyle->textShadow())) : nullptr);
            return m_style->setBoxShadow(m_parentStyle->boxShadow() ? adoptPtr(new ShadowData(*m_parentStyle->boxShadow())) : nullptr);
        }
        if (isInitial || primitiveValue) // initial | none
            return id == CSSPropertyTextShadow ? m_style->setTextShadow(nullptr) : m_style->setBoxShadow(nullptr);

        if (!value->isValueList())
            return;

        for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
            CSSValue* currValue = i.value();
            if (!currValue->isShadowValue())
                continue;
            ShadowValue* item = static_cast<ShadowValue*>(currValue);
            int x = item->x->computeLength<int>(style(), m_rootElementStyle, zoomFactor);
            int y = item->y->computeLength<int>(style(), m_rootElementStyle, zoomFactor);
            int blur = item->blur ? item->blur->computeLength<int>(style(), m_rootElementStyle, zoomFactor) : 0;
            int spread = item->spread ? item->spread->computeLength<int>(style(), m_rootElementStyle, zoomFactor) : 0;
            ShadowStyle shadowStyle = item->style && item->style->getIdent() == CSSValueInset ? Inset : Normal;
            Color color;
            if (item->color)
                color = colorFromPrimitiveValue(item->color.get());
            OwnPtr<ShadowData> shadowData = adoptPtr(new ShadowData(x, y, blur, spread, shadowStyle, id == CSSPropertyWebkitBoxShadow, color.isValid() ? color : Color::transparent));
            if (id == CSSPropertyTextShadow)
                m_style->setTextShadow(shadowData.release(), i.index()); // add to the list if this is not the first entry
            else
                m_style->setBoxShadow(shadowData.release(), i.index()); // add to the list if this is not the first entry
        }
        return;
    }
    case CSSPropertyWebkitBoxReflect: {
        HANDLE_INHERIT_AND_INITIAL(boxReflect, BoxReflect)
        if (primitiveValue) {
            m_style->setBoxReflect(RenderStyle::initialBoxReflect());
            return;
        }

        if (!value->isReflectValue())
            return;

        CSSReflectValue* reflectValue = static_cast<CSSReflectValue*>(value);
        RefPtr<StyleReflection> reflection = StyleReflection::create();
        reflection->setDirection(reflectValue->direction());
        if (reflectValue->offset()) {
            if (reflectValue->offset()->isPercentage())
                reflection->setOffset(Length(reflectValue->offset()->getDoubleValue(), Percent));
            else
                reflection->setOffset(reflectValue->offset()->computeLength<Length>(style(), m_rootElementStyle, zoomFactor));
        }
        NinePieceImage mask;
        mask.setMaskDefaults();
        mapNinePieceImage(property, reflectValue->mask(), mask);
        reflection->setMask(mask);

        m_style->setBoxReflect(reflection.release());
        return;
    }
    case CSSPropertyOpacity:
        HANDLE_INHERIT_AND_INITIAL(opacity, Opacity)
        if (!primitiveValue || !primitiveValue->isNumber())
            return; // Error case.
        // Clamp opacity to the range 0-1
        m_style->setOpacity(clampTo<float>(primitiveValue->getDoubleValue(), 0, 1));
        return;
    case CSSPropertyWebkitBoxAlign:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxAlign, BoxAlign)
        return;
    case CSSPropertySrc: // Only used in @font-face rules.
        return;
    case CSSPropertyUnicodeRange: // Only used in @font-face rules.
        return;
    case CSSPropertyWebkitBackfaceVisibility:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(backfaceVisibility, BackfaceVisibility)
        return;
    case CSSPropertyWebkitBoxDirection:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxDirection, BoxDirection)
        return;
    case CSSPropertyWebkitBoxLines:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxLines, BoxLines)
        return;
    case CSSPropertyWebkitBoxOrient:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxOrient, BoxOrient)
        return;
    case CSSPropertyWebkitBoxPack:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxPack, BoxPack)
        return;
    case CSSPropertyWebkitBoxFlex:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxFlex, BoxFlex)
        return;
    case CSSPropertyWebkitBoxFlexGroup:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxFlexGroup, BoxFlexGroup)
        return;
    case CSSPropertyWebkitBoxOrdinalGroup:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxOrdinalGroup, BoxOrdinalGroup)
        return;
    case CSSPropertyBoxSizing:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(boxSizing, BoxSizing);
        return;
    case CSSPropertyWebkitColumnSpan:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(columnSpan, ColumnSpan)
        return;
    case CSSPropertyWebkitColumnRuleStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(columnRuleStyle, ColumnRuleStyle, BorderStyle)
        return;
    case CSSPropertyWebkitColumnBreakBefore:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(columnBreakBefore, ColumnBreakBefore, PageBreak)
        return;
    case CSSPropertyWebkitColumnBreakAfter:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(columnBreakAfter, ColumnBreakAfter, PageBreak)
        return;
    case CSSPropertyWebkitColumnBreakInside:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(columnBreakInside, ColumnBreakInside, PageBreak)
        return;
    case CSSPropertyWebkitColumnRule:
        if (isInherit) {
            m_style->setColumnRuleColor(m_parentStyle->columnRuleColor().isValid() ? m_parentStyle->columnRuleColor() : m_parentStyle->color());
            m_style->setColumnRuleStyle(m_parentStyle->columnRuleStyle());
            m_style->setColumnRuleWidth(m_parentStyle->columnRuleWidth());
        }
        else if (isInitial)
            m_style->resetColumnRule();
        return;
    case CSSPropertyWebkitRegionBreakBefore:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(regionBreakBefore, RegionBreakBefore, PageBreak)
        return;
    case CSSPropertyWebkitRegionBreakAfter:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(regionBreakAfter, RegionBreakAfter, PageBreak)
        return;
    case CSSPropertyWebkitRegionBreakInside:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(regionBreakInside, RegionBreakInside, PageBreak)
        return;
    case CSSPropertyWebkitMarquee:
        if (!m_parentNode || !value->isInheritedValue())
            return;
        m_style->setMarqueeDirection(m_parentStyle->marqueeDirection());
        m_style->setMarqueeIncrement(m_parentStyle->marqueeIncrement());
        m_style->setMarqueeSpeed(m_parentStyle->marqueeSpeed());
        m_style->setMarqueeLoopCount(m_parentStyle->marqueeLoopCount());
        m_style->setMarqueeBehavior(m_parentStyle->marqueeBehavior());
        return;
    case CSSPropertyWebkitMarqueeRepetition: {
        HANDLE_INHERIT_AND_INITIAL(marqueeLoopCount, MarqueeLoopCount)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSSValueInfinite)
            m_style->setMarqueeLoopCount(-1); // -1 means repeat forever.
        else if (primitiveValue->isNumber())
            m_style->setMarqueeLoopCount(primitiveValue->getIntValue());
        return;
    }
    case CSSPropertyWebkitMarqueeSpeed: {
        HANDLE_INHERIT_AND_INITIAL(marqueeSpeed, MarqueeSpeed)
        if (!primitiveValue)
            return;
        if (int ident = primitiveValue->getIdent()) {
            switch (ident) {
                case CSSValueSlow:
                    m_style->setMarqueeSpeed(500); // 500 msec.
                    break;
                case CSSValueNormal:
                    m_style->setMarqueeSpeed(85); // 85msec. The WinIE default.
                    break;
                case CSSValueFast:
                    m_style->setMarqueeSpeed(10); // 10msec. Super fast.
                    break;
            }
        } else if (primitiveValue->isTime())
            m_style->setMarqueeSpeed(primitiveValue->computeTime<int, CSSPrimitiveValue::Milliseconds>());
        else if (primitiveValue->isNumber()) // For scrollamount support.
            m_style->setMarqueeSpeed(primitiveValue->getIntValue());
        return;
    }
    case CSSPropertyWebkitMarqueeIncrement: {
        HANDLE_INHERIT_AND_INITIAL(marqueeIncrement, MarqueeIncrement)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent()) {
                case CSSValueSmall:
                    m_style->setMarqueeIncrement(Length(1, Fixed)); // 1px.
                    break;
                case CSSValueNormal:
                    m_style->setMarqueeIncrement(Length(6, Fixed)); // 6px. The WinIE default.
                    break;
                case CSSValueLarge:
                    m_style->setMarqueeIncrement(Length(36, Fixed)); // 36px.
                    break;
            }
        } else {
            bool ok = true;
            Length marqueeLength = convertToIntLength(primitiveValue, style(), m_rootElementStyle, 1, &ok);
            if (ok)
                m_style->setMarqueeIncrement(marqueeLength);
        }
        return;
    }
    case CSSPropertyWebkitMarqueeStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(marqueeBehavior, MarqueeBehavior)
        return;
    case CSSPropertyWebkitRegionOverflow:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(regionOverflow, RegionOverflow);
        return;
    case CSSPropertyWebkitMarqueeDirection:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(marqueeDirection, MarqueeDirection)
        return;
    case CSSPropertyWebkitUserDrag:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(userDrag, UserDrag)
        return;
    case CSSPropertyWebkitUserModify:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(userModify, UserModify)
        return;
    case CSSPropertyWebkitUserSelect:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(userSelect, UserSelect)
        return;

    case CSSPropertyTextOverflow: {
        // This property is supported by WinIE, and so we leave off the "-webkit-" in order to
        // work with WinIE-specific pages that use the property.
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(textOverflow, TextOverflow)
        return;
    }
    case CSSPropertyWebkitLineClamp: {
        HANDLE_INHERIT_AND_INITIAL(lineClamp, LineClamp)
        if (!primitiveValue)
            return;
        if (primitiveValue->isNumber())
            m_style->setLineClamp(LineClampValue(primitiveValue->getIntValue(CSSPrimitiveValue::CSS_NUMBER), LineClampLineCount));
        else if (primitiveValue->isPercentage())
            m_style->setLineClamp(LineClampValue(primitiveValue->getIntValue(CSSPrimitiveValue::CSS_PERCENTAGE), LineClampPercentage));
        return;
    }
    case CSSPropertyWebkitHyphens:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(hyphens, Hyphens);
        return;
    case CSSPropertyWebkitLocale: {
        HANDLE_INHERIT_AND_INITIAL(locale, Locale);
        if (primitiveValue->getIdent() == CSSValueAuto)
            m_style->setLocale(nullAtom);
        else
            m_style->setLocale(primitiveValue->getStringValue());
        FontDescription fontDescription = m_style->fontDescription();
        fontDescription.setScript(localeToScriptCodeForFontSelection(m_style->locale()));
        setFontDescription(fontDescription);
        return;
    }
    case CSSPropertyWebkitBorderFit:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(borderFit, BorderFit);
        return;
    case CSSPropertyWebkitTextSizeAdjust: {
        HANDLE_INHERIT_AND_INITIAL(textSizeAdjust, TextSizeAdjust)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        setTextSizeAdjust(primitiveValue->getIdent() == CSSValueAuto);
        return;
    }
    case CSSPropertyWebkitTextSecurity:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(textSecurity, TextSecurity)
        return;

#if ENABLE(DASHBOARD_SUPPORT)
    case CSSPropertyWebkitDashboardRegion: {
        HANDLE_INHERIT_AND_INITIAL(dashboardRegions, DashboardRegions)
        if (!primitiveValue)
            return;

        if (primitiveValue->getIdent() == CSSValueNone) {
            m_style->setDashboardRegions(RenderStyle::noneDashboardRegions());
            return;
        }

        DashboardRegion *region = primitiveValue->getDashboardRegionValue();
        if (!region)
            return;

        DashboardRegion *first = region;
        while (region) {
            Length top = convertToIntLength(region->top(), style(), m_rootElementStyle);
            Length right = convertToIntLength(region->right(), style(), m_rootElementStyle);
            Length bottom = convertToIntLength(region->bottom(), style(), m_rootElementStyle);
            Length left = convertToIntLength(region->left(), style(), m_rootElementStyle);
            if (region->m_isCircle)
                m_style->setDashboardRegion(StyleDashboardRegion::Circle, region->m_label, top, right, bottom, left, region == first ? false : true);
            else if (region->m_isRectangle)
                m_style->setDashboardRegion(StyleDashboardRegion::Rectangle, region->m_label, top, right, bottom, left, region == first ? false : true);
            region = region->m_next.get();
        }

        m_element->document()->setHasDashboardRegions(true);

        return;
    }
#endif
    case CSSPropertyWebkitRtlOrdering:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(rtlOrdering, RTLOrdering)
        return;
    case CSSPropertyWebkitTextStrokeWidth: {
        HANDLE_INHERIT_AND_INITIAL(textStrokeWidth, TextStrokeWidth)
        float width = 0;
        switch (primitiveValue->getIdent()) {
            case CSSValueThin:
            case CSSValueMedium:
            case CSSValueThick: {
                double result = 1.0 / 48;
                if (primitiveValue->getIdent() == CSSValueMedium)
                    result *= 3;
                else if (primitiveValue->getIdent() == CSSValueThick)
                    result *= 5;
                width = CSSPrimitiveValue::create(result, CSSPrimitiveValue::CSS_EMS)->computeLength<float>(style(), m_rootElementStyle, zoomFactor);
                break;
            }
            default:
                width = primitiveValue->computeLength<float>(style(), m_rootElementStyle, zoomFactor);
                break;
        }
        m_style->setTextStrokeWidth(width);
        return;
    }
    case CSSPropertyWebkitTransform: {
        HANDLE_INHERIT_AND_INITIAL(transform, Transform);
        TransformOperations operations;
        createTransformOperations(value, style(), m_rootElementStyle, operations);
        m_style->setTransform(operations);
        return;
    }
    case CSSPropertyWebkitTransformStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(transformStyle3D, TransformStyle3D)
        return;
    case CSSPropertyWebkitPrintColorAdjust:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(printColorAdjust, PrintColorAdjust);
        return;
    case CSSPropertyWebkitPerspective: {
        HANDLE_INHERIT_AND_INITIAL(perspective, Perspective)
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone) {
            m_style->setPerspective(0);
            return;
        }

        float perspectiveValue;
        if (primitiveValue->isLength())
            perspectiveValue = primitiveValue->computeLength<float>(style(), m_rootElementStyle, zoomFactor);
        else if (primitiveValue->isNumber()) {
            // For backward compatibility, treat valueless numbers as px.
            perspectiveValue = CSSPrimitiveValue::create(primitiveValue->getDoubleValue(), CSSPrimitiveValue::CSS_PX)->computeLength<float>(style(), m_rootElementStyle, zoomFactor);
        } else
            return;

        if (perspectiveValue >= 0.0f)
            m_style->setPerspective(perspectiveValue);
        return;
    }
    case CSSPropertyWebkitAnimation:
        if (isInitial)
            m_style->clearAnimations();
        else if (isInherit)
            m_style->inheritAnimations(m_parentStyle->animations());
        return;
    case CSSPropertyWebkitTransition:
        if (isInitial)
            m_style->clearTransitions();
        else if (isInherit)
            m_style->inheritTransitions(m_parentStyle->transitions());
        return;
    case CSSPropertyPointerEvents:
    {
#if ENABLE(DASHBOARD_SUPPORT)
        // <rdar://problem/6561077> Work around the Stocks widget's misuse of the
        // pointer-events property by not applying it in Dashboard.
        Settings* settings = m_checker.document()->settings();
        if (settings && settings->usesDashboardBackwardCompatibilityMode())
            return;
#endif
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(pointerEvents, PointerEvents)
        return;
    }
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyWebkitTapHighlightColor: {
        HANDLE_INHERIT_AND_INITIAL(tapHighlightColor, TapHighlightColor);
        if (!primitiveValue)
            break;

        Color col = colorFromPrimitiveValue(primitiveValue);
        m_style->setTapHighlightColor(col);
        return;
    }
#endif
    case CSSPropertyWebkitColorCorrection:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(colorSpace, ColorSpace);
        return;
    case CSSPropertySpeak:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(speak, Speak);
        return;
    case CSSPropertyInvalid:
        return;
    // Directional properties are resolved by resolveDirectionAwareProperty() before the switch.
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
    {
        int newId = CSSProperty::resolveDirectionAwareProperty(id, m_style->direction(), m_style->writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, value);
    }
    case CSSPropertyFontStretch:
    case CSSPropertyPage:
    case CSSPropertyTextLineThrough:
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverline:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderline:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTextUnderlineWidth:
    case CSSPropertyWebkitFontSizeDelta:
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTextEmphasis:
        return;

    // CSS Text Layout Module Level 3: Vertical writing support
    case CSSPropertyWebkitWritingMode: {
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(writingMode, WritingMode)
        if (!isInherit && !isInitial && m_element && m_element == m_element->document()->documentElement())
            m_element->document()->setWritingModeSetOnDocumentElement(true);
        FontDescription fontDescription = m_style->fontDescription();
        fontDescription.setOrientation(m_style->isHorizontalWritingMode() ? Horizontal : Vertical);
        setFontDescription(fontDescription);
        return;
    }

    case CSSPropertyWebkitLineBoxContain: {
        HANDLE_INHERIT_AND_INITIAL(lineBoxContain, LineBoxContain)
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone) {
            m_style->setLineBoxContain(LineBoxContainNone);
            return;
        }

        if (!value->isCSSLineBoxContainValue())
            return;

        CSSLineBoxContainValue* lineBoxContainValue = static_cast<CSSLineBoxContainValue*>(value);
        m_style->setLineBoxContain(lineBoxContainValue->value());
        return;
    }

    case CSSPropertyWebkitColumnAxis:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(columnAxis, ColumnAxis);
        return;

    case CSSPropertyWebkitWrapShapeInside:
        HANDLE_INHERIT_AND_INITIAL(wrapShapeInside, WrapShapeInside);
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSSValueAuto)
            m_style->setWrapShapeInside(0);
        else if (primitiveValue->isShape())
            m_style->setWrapShapeInside(primitiveValue->getShapeValue());
        return;

    case CSSPropertyWebkitWrapShapeOutside:
        HANDLE_INHERIT_AND_INITIAL(wrapShapeOutside, WrapShapeOutside);
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSSValueAuto)
            m_style->setWrapShapeOutside(0);
        else if (primitiveValue->isShape())
            m_style->setWrapShapeOutside(primitiveValue->getShapeValue());
        return;

    // CSS Fonts Module Level 3
    case CSSPropertyWebkitFontFeatureSettings: {
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNormal) {
            setFontDescription(m_style->fontDescription().makeNormalFeatureSettings());
            return;
        }

        if (!value->isValueList())
            return;

        FontDescription fontDescription = m_style->fontDescription();
        CSSValueList* list = static_cast<CSSValueList*>(value);
        RefPtr<FontFeatureSettings> settings = FontFeatureSettings::create();
        int len = list->length();
        for (int i = 0; i < len; ++i) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (!item->isFontFeatureValue())
                continue;
            FontFeatureValue* feature = static_cast<FontFeatureValue*>(item);
            settings->append(FontFeature(feature->tag(), feature->value()));
        }
        fontDescription.setFeatureSettings(settings.release());
        setFontDescription(fontDescription);
        return;
    }

#if ENABLE(CSS_FILTERS)
    case CSSPropertyWebkitFilter: {
        HANDLE_INHERIT_AND_INITIAL(filter, Filter);
        FilterOperations operations;
        if (createFilterOperations(value, style(), m_rootElementStyle, operations))
            m_style->setFilter(operations);
        return;
    }
#endif
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridColumns: {
        Vector<Length> lengths;
        if (!createGridTrackList(value, lengths, this))
            return;
        m_style->setGridColumns(lengths);
        return;
    }
    case CSSPropertyWebkitGridRows: {
        Vector<Length> lengths;
        if (!createGridTrackList(value, lengths, this))
            return;
        m_style->setGridRows(lengths);
        return;
    }
#endif

    // These properties are implemented in the CSSStyleApplyProperty lookup table.
    case CSSPropertyColor:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitAspectRatio:
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitMaskAttachment:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorder:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderWidth:
    case CSSPropertyBorderColor:
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyWebkitMaskBoxImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyWebkitMaskBoxImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyWebkitMaskBoxImageSlice:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitMaskBoxImageWidth:
    case CSSPropertyBorderImageSource:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderSpacing:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyClip:
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
    case CSSPropertyLetterSpacing:
    case CSSPropertyWordSpacing:
    case CSSPropertyWebkitFlexOrder:
    case CSSPropertyWebkitFlexPack:
    case CSSPropertyWebkitFlexAlign:
    case CSSPropertyWebkitFlexItemAlign:
    case CSSPropertyWebkitFlexDirection:
    case CSSPropertyWebkitFlexFlow:
    case CSSPropertyWebkitFlexWrap:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyTextRendering:
    case CSSPropertyWebkitTextOrientation:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyFontWeight:
    case CSSPropertyOutline:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyOutlineOffset:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyOutlineColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyWebkitMaskRepeat:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyOverflow:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyMaxWidth:
    case CSSPropertyTop:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyBottom:
    case CSSPropertyWidth:
    case CSSPropertyMinWidth:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyle:
    case CSSPropertyListStyleImage:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyMarginTop:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMargin:
    case CSSPropertyPaddingTop:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPadding:
    case CSSPropertyResize:
    case CSSPropertySize:
    case CSSPropertyTextAlign:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextIndent:
    case CSSPropertyMaxHeight:
    case CSSPropertyHeight:
    case CSSPropertyMinHeight:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWebkitTransformOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionProperty:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyCursor:
    case CSSPropertyWebkitColumns:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyWebkitFlowInto:
    case CSSPropertyWebkitFlowFrom:
    case CSSPropertyWebkitFontKerning:
    case CSSPropertyWebkitFontVariantLigatures:
    case CSSPropertyWebkitHighlight:
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitHyphenateLimitAfter:
    case CSSPropertyWebkitHyphenateLimitBefore:
    case CSSPropertyWebkitHyphenateLimitLines:
    case CSSPropertyWebkitLineGrid:
    case CSSPropertyWebkitLineGridSnap:
    case CSSPropertyWebkitTextCombine:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextEmphasisStyle:
    case CSSPropertyWebkitWrapMargin:
    case CSSPropertyWebkitWrapPadding:
    case CSSPropertyWebkitWrapFlow:
    case CSSPropertyWebkitWrapThrough:
    case CSSPropertyWebkitWrap:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
        ASSERT_NOT_REACHED();
        return;
#if ENABLE(SVG)
    default:
        // Try the SVG properties
        applySVGProperty(id, value);
        return;
#endif
    }
}

void CSSStyleSelector::mapFillAttachment(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setAttachment(FillLayer::initialFillAttachment(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    switch (primitiveValue->getIdent()) {
        case CSSValueFixed:
            layer->setAttachment(FixedBackgroundAttachment);
            break;
        case CSSValueScroll:
            layer->setAttachment(ScrollBackgroundAttachment);
            break;
        case CSSValueLocal:
            layer->setAttachment(LocalBackgroundAttachment);
            break;
        default:
            return;
    }
}

void CSSStyleSelector::mapFillClip(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setClip(FillLayer::initialFillClip(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setClip(*primitiveValue);
}

void CSSStyleSelector::mapFillComposite(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setComposite(FillLayer::initialFillComposite(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setComposite(*primitiveValue);
}

void CSSStyleSelector::mapFillOrigin(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setOrigin(FillLayer::initialFillOrigin(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setOrigin(*primitiveValue);
}

PassRefPtr<StyleImage> CSSStyleSelector::styleImage(CSSPropertyID property, CSSValue* value)
{
    if (value->isImageValue())
        return cachedOrPendingFromValue(property, static_cast<CSSImageValue*>(value));

    if (value->isImageGeneratorValue())
        return generatedOrPendingFromValue(property, static_cast<CSSImageGeneratorValue*>(value));

    return 0;
}

PassRefPtr<StyleImage> CSSStyleSelector::cachedOrPendingFromValue(CSSPropertyID property, CSSImageValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImage();
    if (image && image->isPendingImage())
        m_pendingImageProperties.add(property);
    return image.release();
}

PassRefPtr<StyleImage> CSSStyleSelector::generatedOrPendingFromValue(CSSPropertyID property, CSSImageGeneratorValue* value)
{
    if (value->isPending()) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    return StyleGeneratedImage::create(value);
}

void CSSStyleSelector::mapFillImage(CSSPropertyID property, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setImage(FillLayer::initialFillImage(layer->type()));
        return;
    }

    layer->setImage(styleImage(property, value));
}

void CSSStyleSelector::mapFillRepeatX(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setRepeatX(FillLayer::initialFillRepeatX(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setRepeatX(*primitiveValue);
}

void CSSStyleSelector::mapFillRepeatY(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setRepeatY(FillLayer::initialFillRepeatY(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setRepeatY(*primitiveValue);
}

void CSSStyleSelector::mapFillSize(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (!value->isPrimitiveValue()) {
        layer->setSizeType(SizeNone);
        return;
    }

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->getIdent() == CSSValueContain)
        layer->setSizeType(Contain);
    else if (primitiveValue->getIdent() == CSSValueCover)
        layer->setSizeType(Cover);
    else
        layer->setSizeType(SizeLength);

    LengthSize b = FillLayer::initialFillSizeLength(layer->type());

    if (value->isInitialValue() || primitiveValue->getIdent() == CSSValueContain || primitiveValue->getIdent() == CSSValueCover) {
        layer->setSizeLength(b);
        return;
    }

    Pair* pair = primitiveValue->getPairValue();

    CSSPrimitiveValue* first = pair ? static_cast<CSSPrimitiveValue*>(pair->first()) : primitiveValue;
    CSSPrimitiveValue* second = pair ? static_cast<CSSPrimitiveValue*>(pair->second()) : 0;

    Length firstLength, secondLength;

    float zoomFactor = m_style->effectiveZoom();

    if (first->getIdent() == CSSValueAuto)
        firstLength = Length();
    else if (first->isLength())
        firstLength = first->computeLength<Length>(style(), m_rootElementStyle, zoomFactor);
    else if (first->isPercentage())
        firstLength = Length(first->getDoubleValue(), Percent);
    else
        return;

    if (!second || second->getIdent() == CSSValueAuto)
        secondLength = Length();
    else if (second->isLength())
        secondLength = second->computeLength<Length>(style(), m_rootElementStyle, zoomFactor);
    else if (second->isPercentage())
        secondLength = Length(second->getDoubleValue(), Percent);
    else
        return;

    b.setWidth(firstLength);
    b.setHeight(secondLength);
    layer->setSizeLength(b);
}

void CSSStyleSelector::mapFillXPosition(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setXPosition(FillLayer::initialFillXPosition(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    float zoomFactor = m_style->effectiveZoom();

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Length l;
    if (primitiveValue->isLength())
        l = primitiveValue->computeLength<Length>(style(), m_rootElementStyle, zoomFactor);
    else if (primitiveValue->isPercentage())
        l = Length(primitiveValue->getDoubleValue(), Percent);
    else
        return;
    layer->setXPosition(l);
}

void CSSStyleSelector::mapFillYPosition(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setYPosition(FillLayer::initialFillYPosition(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    float zoomFactor = m_style->effectiveZoom();

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Length l;
    if (primitiveValue->isLength())
        l = primitiveValue->computeLength<Length>(style(), m_rootElementStyle, zoomFactor);
    else if (primitiveValue->isPercentage())
        l = Length(primitiveValue->getDoubleValue(), Percent);
    else
        return;
    layer->setYPosition(l);
}

void CSSStyleSelector::mapAnimationDelay(Animation* animation, CSSValue* value)
{
    if (value->isInitialValue()) {
        animation->setDelay(Animation::initialAnimationDelay());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    animation->setDelay(primitiveValue->computeTime<float, CSSPrimitiveValue::Seconds>());
}

void CSSStyleSelector::mapAnimationDirection(Animation* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setDirection(Animation::initialAnimationDirection());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setDirection(primitiveValue->getIdent() == CSSValueAlternate ? Animation::AnimationDirectionAlternate : Animation::AnimationDirectionNormal);
}

void CSSStyleSelector::mapAnimationDuration(Animation* animation, CSSValue* value)
{
    if (value->isInitialValue()) {
        animation->setDuration(Animation::initialAnimationDuration());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    animation->setDuration(primitiveValue->computeTime<float, CSSPrimitiveValue::Seconds>());
}

void CSSStyleSelector::mapAnimationFillMode(Animation* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setFillMode(Animation::initialAnimationFillMode());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    switch (primitiveValue->getIdent()) {
    case CSSValueNone:
        layer->setFillMode(AnimationFillModeNone);
        break;
    case CSSValueForwards:
        layer->setFillMode(AnimationFillModeForwards);
        break;
    case CSSValueBackwards:
        layer->setFillMode(AnimationFillModeBackwards);
        break;
    case CSSValueBoth:
        layer->setFillMode(AnimationFillModeBoth);
        break;
    }
}

void CSSStyleSelector::mapAnimationIterationCount(Animation* animation, CSSValue* value)
{
    if (value->isInitialValue()) {
        animation->setIterationCount(Animation::initialAnimationIterationCount());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->getIdent() == CSSValueInfinite)
        animation->setIterationCount(-1);
    else
        animation->setIterationCount(int(primitiveValue->getFloatValue()));
}

void CSSStyleSelector::mapAnimationName(Animation* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setName(Animation::initialAnimationName());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->getIdent() == CSSValueNone)
        layer->setIsNoneAnimation(true);
    else
        layer->setName(primitiveValue->getStringValue());
}

void CSSStyleSelector::mapAnimationPlayState(Animation* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setPlayState(Animation::initialAnimationPlayState());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    EAnimPlayState playState = (primitiveValue->getIdent() == CSSValuePaused) ? AnimPlayStatePaused : AnimPlayStatePlaying;
    layer->setPlayState(playState);
}

void CSSStyleSelector::mapAnimationProperty(Animation* animation, CSSValue* value)
{
    if (value->isInitialValue()) {
        animation->setProperty(Animation::initialAnimationProperty());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->getIdent() == CSSValueAll)
        animation->setProperty(cAnimateAll);
    else if (primitiveValue->getIdent() == CSSValueNone)
        animation->setProperty(cAnimateNone);
    else
        animation->setProperty(static_cast<CSSPropertyID>(primitiveValue->getIdent()));
}

void CSSStyleSelector::mapAnimationTimingFunction(Animation* animation, CSSValue* value)
{
    if (value->isInitialValue()) {
        animation->setTimingFunction(Animation::initialAnimationTimingFunction());
        return;
    }

    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        switch (primitiveValue->getIdent()) {
            case CSSValueLinear:
                animation->setTimingFunction(LinearTimingFunction::create());
                break;
            case CSSValueEase:
                animation->setTimingFunction(CubicBezierTimingFunction::create());
                break;
            case CSSValueEaseIn:
                animation->setTimingFunction(CubicBezierTimingFunction::create(0.42, 0.0, 1.0, 1.0));
                break;
            case CSSValueEaseOut:
                animation->setTimingFunction(CubicBezierTimingFunction::create(0.0, 0.0, 0.58, 1.0));
                break;
            case CSSValueEaseInOut:
                animation->setTimingFunction(CubicBezierTimingFunction::create(0.42, 0.0, 0.58, 1.0));
                break;
            case CSSValueStepStart:
                animation->setTimingFunction(StepsTimingFunction::create(1, true));
                break;
            case CSSValueStepEnd:
                animation->setTimingFunction(StepsTimingFunction::create(1, false));
                break;
        }
        return;
    }

    if (value->isTimingFunctionValue()) {
        CSSTimingFunctionValue* timingFunction = static_cast<CSSTimingFunctionValue*>(value);
        if (timingFunction->isCubicBezierTimingFunctionValue()) {
            CSSCubicBezierTimingFunctionValue* cubicTimingFunction = static_cast<CSSCubicBezierTimingFunctionValue*>(value);
            animation->setTimingFunction(CubicBezierTimingFunction::create(cubicTimingFunction->x1(), cubicTimingFunction->y1(), cubicTimingFunction->x2(), cubicTimingFunction->y2()));
        } else if (timingFunction->isStepsTimingFunctionValue()) {
            CSSStepsTimingFunctionValue* stepsTimingFunction = static_cast<CSSStepsTimingFunctionValue*>(value);
            animation->setTimingFunction(StepsTimingFunction::create(stepsTimingFunction->numberOfSteps(), stepsTimingFunction->stepAtStart()));
        } else
            animation->setTimingFunction(LinearTimingFunction::create());
    }
}

void CSSStyleSelector::mapNinePieceImage(CSSPropertyID property, CSSValue* value, NinePieceImage& image)
{
    // If we're not a value list, then we are "none" and don't need to alter the empty image at all.
    if (!value || !value->isValueList())
        return;

    // Retrieve the border image value.
    CSSValueList* borderImage = static_cast<CSSValueList*>(value);

    // Set the image (this kicks off the load).
    CSSPropertyID imageProperty;
    if (property == CSSPropertyWebkitBorderImage)
        imageProperty = CSSPropertyBorderImageSource;
    else if (property == CSSPropertyWebkitMaskBoxImage)
        imageProperty = CSSPropertyWebkitMaskBoxImageSource;
    else
        imageProperty = property;

    for (unsigned i = 0 ; i < borderImage->length() ; ++i) {
        CSSValue* current = borderImage->item(i);

        if (current->isImageValue() || current->isImageGeneratorValue())
            image.setImage(styleImage(imageProperty, current));
        else if (current->isBorderImageSliceValue())
            mapNinePieceImageSlice(current, image);
        else if (current->isValueList()) {
            CSSValueList* slashList = static_cast<CSSValueList*>(current);
            // Map in the image slices.
            if (slashList->item(0) && slashList->item(0)->isBorderImageSliceValue())
                mapNinePieceImageSlice(slashList->item(0), image);

            // Map in the border slices.
            if (slashList->item(1))
                image.setBorderSlices(mapNinePieceImageQuad(slashList->item(1)));

            // Map in the outset.
            if (slashList->item(2))
                image.setOutset(mapNinePieceImageQuad(slashList->item(2)));
        } else if (current->isPrimitiveValue()) {
            // Set the appropriate rules for stretch/round/repeat of the slices.
            mapNinePieceImageRepeat(current, image);
        }
    }

    if (property == CSSPropertyWebkitBorderImage) {
        // We have to preserve the legacy behavior of -webkit-border-image and make the border slices
        // also set the border widths. We don't need to worry about percentages, since we don't even support
        // those on real borders yet.
        if (image.borderSlices().top().isFixed())
            style()->setBorderTopWidth(image.borderSlices().top().value());
        if (image.borderSlices().right().isFixed())
            style()->setBorderRightWidth(image.borderSlices().right().value());
        if (image.borderSlices().bottom().isFixed())
            style()->setBorderBottomWidth(image.borderSlices().bottom().value());
        if (image.borderSlices().left().isFixed())
            style()->setBorderLeftWidth(image.borderSlices().left().value());
    }
}

void CSSStyleSelector::mapNinePieceImageSlice(CSSValue* value, NinePieceImage& image)
{
    if (!value || !value->isBorderImageSliceValue())
        return;

    // Retrieve the border image value.
    CSSBorderImageSliceValue* borderImageSlice = static_cast<CSSBorderImageSliceValue*>(value);

    // Set up a length box to represent our image slices.
    LengthBox box;
    Quad* slices = borderImageSlice->slices();
    if (slices->top()->isPercentage())
        box.m_top = Length(slices->top()->getDoubleValue(), Percent);
    else
        box.m_top = Length(slices->top()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (slices->bottom()->isPercentage())
        box.m_bottom = Length(slices->bottom()->getDoubleValue(), Percent);
    else
        box.m_bottom = Length((int)slices->bottom()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (slices->left()->isPercentage())
        box.m_left = Length(slices->left()->getDoubleValue(), Percent);
    else
        box.m_left = Length(slices->left()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (slices->right()->isPercentage())
        box.m_right = Length(slices->right()->getDoubleValue(), Percent);
    else
        box.m_right = Length(slices->right()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    image.setImageSlices(box);

    // Set our fill mode.
    image.setFill(borderImageSlice->m_fill);
}

LengthBox CSSStyleSelector::mapNinePieceImageQuad(CSSValue* value)
{
    if (!value || !value->isPrimitiveValue())
        return LengthBox();

    // Get our zoom value.
    float zoom = useSVGZoomRules() ? 1.0f : style()->effectiveZoom();

    // Retrieve the primitive value.
    CSSPrimitiveValue* borderWidths = static_cast<CSSPrimitiveValue*>(value);

    // Set up a length box to represent our image slices.
    LengthBox box; // Defaults to 'auto' so we don't have to handle that explicitly below.
    Quad* slices = borderWidths->getQuadValue();
    if (slices->top()->isNumber())
        box.m_top = Length(slices->top()->getIntValue(), Relative);
    else if (slices->top()->isPercentage())
        box.m_top = Length(slices->top()->getDoubleValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
    else if (slices->top()->getIdent() != CSSValueAuto)
        box.m_top = slices->top()->computeLength<Length>(style(), rootElementStyle(), zoom);

    if (slices->right()->isNumber())
        box.m_right = Length(slices->right()->getIntValue(), Relative);
    else if (slices->right()->isPercentage())
        box.m_right = Length(slices->right()->getDoubleValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
    else if (slices->right()->getIdent() != CSSValueAuto)
        box.m_right = slices->right()->computeLength<Length>(style(), rootElementStyle(), zoom);

    if (slices->bottom()->isNumber())
        box.m_bottom = Length(slices->bottom()->getIntValue(), Relative);
    else if (slices->bottom()->isPercentage())
        box.m_bottom = Length(slices->bottom()->getDoubleValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
    else if (slices->bottom()->getIdent() != CSSValueAuto)
        box.m_bottom = slices->bottom()->computeLength<Length>(style(), rootElementStyle(), zoom);

    if (slices->left()->isNumber())
        box.m_left = Length(slices->left()->getIntValue(), Relative);
    else if (slices->left()->isPercentage())
        box.m_left = Length(slices->left()->getDoubleValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
    else if (slices->left()->getIdent() != CSSValueAuto)
        box.m_left = slices->left()->computeLength<Length>(style(), rootElementStyle(), zoom);

    return box;
}

void CSSStyleSelector::mapNinePieceImageRepeat(CSSValue* value, NinePieceImage& image)
{
    if (!value || !value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Pair* pair = primitiveValue->getPairValue();
    if (!pair || !pair->first() || !pair->second())
        return;

    int firstIdentifier = pair->first()->getIdent();
    int secondIdentifier = pair->second()->getIdent();

    ENinePieceImageRule horizontalRule;
    switch (firstIdentifier) {
    case CSSValueStretch:
        horizontalRule = StretchImageRule;
        break;
    case CSSValueRound:
        horizontalRule = RoundImageRule;
        break;
    case CSSValueSpace:
        horizontalRule = SpaceImageRule;
        break;
    default: // CSSValueRepeat
        horizontalRule = RepeatImageRule;
        break;
    }
    image.setHorizontalRule(horizontalRule);

    ENinePieceImageRule verticalRule;
    switch (secondIdentifier) {
    case CSSValueStretch:
        verticalRule = StretchImageRule;
        break;
    case CSSValueRound:
        verticalRule = RoundImageRule;
        break;
    case CSSValueSpace:
        verticalRule = SpaceImageRule;
        break;
    default: // CSSValueRepeat
        verticalRule = RepeatImageRule;
        break;
    }
    image.setVerticalRule(verticalRule);
}

void CSSStyleSelector::checkForTextSizeAdjust()
{
    if (m_style->textSizeAdjust())
        return;

    FontDescription newFontDescription(m_style->fontDescription());
    newFontDescription.setComputedSize(newFontDescription.specifiedSize());
    m_style->setFontDescription(newFontDescription);
}

void CSSStyleSelector::checkForZoomChange(RenderStyle* style, RenderStyle* parentStyle)
{
    if (style->effectiveZoom() == parentStyle->effectiveZoom())
        return;

    const FontDescription& childFont = style->fontDescription();
    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, childFont.specifiedSize());
    style->setFontDescription(newFontDescription);
}

void CSSStyleSelector::checkForGenericFamilyChange(RenderStyle* style, RenderStyle* parentStyle)
{
    const FontDescription& childFont = style->fontDescription();

    if (childFont.isAbsoluteSize() || !parentStyle)
        return;

    const FontDescription& parentFont = parentStyle->fontDescription();
    if (childFont.useFixedDefaultSize() == parentFont.useFixedDefaultSize())
        return;

    // For now, lump all families but monospace together.
    if (childFont.genericFamily() != FontDescription::MonospaceFamily &&
        parentFont.genericFamily() != FontDescription::MonospaceFamily)
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified.  We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size;
    if (childFont.keywordSize())
        size = fontSizeForKeyword(m_checker.document(), CSSValueXxSmall + childFont.keywordSize() - 1, childFont.useFixedDefaultSize());
    else {
        Settings* settings = m_checker.document()->settings();
        float fixedScaleFactor = settings
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = parentFont.useFixedDefaultSize() ?
                childFont.specifiedSize() / fixedScaleFactor :
                childFont.specifiedSize() * fixedScaleFactor;
    }

    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, size);
    style->setFontDescription(newFontDescription);
}

void CSSStyleSelector::setFontSize(FontDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(m_checker.document(), m_style.get(), fontDescription.isAbsoluteSize(), size, useSVGZoomRules()));
}

float CSSStyleSelector::getComputedSizeFromSpecifiedSize(Document* document, RenderStyle* style, bool isAbsoluteSize, float specifiedSize, bool useSVGZoomRules)
{
    float zoomFactor = 1.0f;
    if (!useSVGZoomRules) {
        zoomFactor = style->effectiveZoom();
        if (Frame* frame = document->frame())
            zoomFactor *= frame->textZoomFactor();
    }

    return CSSStyleSelector::getComputedSizeFromSpecifiedSize(document, zoomFactor, isAbsoluteSize, specifiedSize);
}

float CSSStyleSelector::getComputedSizeFromSpecifiedSize(Document* document, float zoomFactor, bool isAbsoluteSize, float specifiedSize, ESmartMinimumForFontSize useSmartMinimumForFontSize)
{
    // Text with a 0px font size should not be visible and therefore needs to be
    // exempt from minimum font size rules. Acid3 relies on this for pixel-perfect
    // rendering. This is also compatible with other browsers that have minimum
    // font size settings (e.g. Firefox).
    if (fabsf(specifiedSize) < std::numeric_limits<float>::epsilon())
        return 0.0f;

    // We support two types of minimum font size.  The first is a hard override that applies to
    // all fonts.  This is "minSize."  The second type of minimum font size is a "smart minimum"
    // that is applied only when the Web page can't know what size it really asked for, e.g.,
    // when it uses logical sizes like "small" or expresses the font-size as a percentage of
    // the user's default font setting.

    // With the smart minimum, we never want to get smaller than the minimum font size to keep fonts readable.
    // However we always allow the page to set an explicit pixel size that is smaller,
    // since sites will mis-render otherwise (e.g., http://www.gamespot.com with a 9px minimum).

    Settings* settings = document->settings();
    if (!settings)
        return 1.0f;

    int minSize = settings->minimumFontSize();
    int minLogicalSize = settings->minimumLogicalFontSize();
    float zoomedSize = specifiedSize * zoomFactor;

    // Apply the hard minimum first.  We only apply the hard minimum if after zooming we're still too small.
    if (zoomedSize < minSize)
        zoomedSize = minSize;

    // Now apply the "smart minimum."  This minimum is also only applied if we're still too small
    // after zooming.  The font size must either be relative to the user default or the original size
    // must have been acceptable.  In other words, we only apply the smart minimum whenever we're positive
    // doing so won't disrupt the layout.
    if (useSmartMinimumForFontSize && zoomedSize < minLogicalSize && (specifiedSize >= minLogicalSize || !isAbsoluteSize))
        zoomedSize = minLogicalSize;

    // Also clamp to a reasonable maximum to prevent insane font sizes from causing crashes on various
    // platforms (I'm looking at you, Windows.)
    return min(1000000.0f, zoomedSize);
}

const int fontSizeTableMax = 16;
const int fontSizeTableMin = 9;
const int totalKeywords = 8;

// WinIE/Nav4 table for font sizes.  Designed to match the legacy font mapping system of HTML.
static const int quirksFontSizeTable[fontSizeTableMax - fontSizeTableMin + 1][totalKeywords] =
{
      { 9,    9,     9,     9,    11,    14,    18,    28 },
      { 9,    9,     9,    10,    12,    15,    20,    31 },
      { 9,    9,     9,    11,    13,    17,    22,    34 },
      { 9,    9,    10,    12,    14,    18,    24,    37 },
      { 9,    9,    10,    13,    16,    20,    26,    40 }, // fixed font default (13)
      { 9,    9,    11,    14,    17,    21,    28,    42 },
      { 9,   10,    12,    15,    17,    23,    30,    45 },
      { 9,   10,    13,    16,    18,    24,    32,    48 }  // proportional font default (16)
};
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref

// Strict mode table matches MacIE and Mozilla's settings exactly.
static const int strictFontSizeTable[fontSizeTableMax - fontSizeTableMin + 1][totalKeywords] =
{
      { 9,    9,     9,     9,    11,    14,    18,    27 },
      { 9,    9,     9,    10,    12,    15,    20,    30 },
      { 9,    9,    10,    11,    13,    17,    22,    33 },
      { 9,    9,    10,    12,    14,    18,    24,    36 },
      { 9,   10,    12,    13,    16,    20,    26,    39 }, // fixed font default (13)
      { 9,   10,    12,    14,    17,    21,    28,    42 },
      { 9,   10,    13,    15,    18,    23,    30,    45 },
      { 9,   10,    13,    16,    18,    24,    32,    48 }  // proportional font default (16)
};
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref

// For values outside the range of the table, we use Todd Fahrner's suggested scale
// factors for each keyword value.
static const float fontSizeFactors[totalKeywords] = { 0.60f, 0.75f, 0.89f, 1.0f, 1.2f, 1.5f, 2.0f, 3.0f };

float CSSStyleSelector::fontSizeForKeyword(Document* document, int keyword, bool shouldUseFixedDefaultSize)
{
    Settings* settings = document->settings();
    if (!settings)
        return 1.0f;

    bool quirksMode = document->inQuirksMode();
    int mediumSize = shouldUseFixedDefaultSize ? settings->defaultFixedFontSize() : settings->defaultFontSize();
    if (mediumSize >= fontSizeTableMin && mediumSize <= fontSizeTableMax) {
        // Look up the entry in the table.
        int row = mediumSize - fontSizeTableMin;
        int col = (keyword - CSSValueXxSmall);
        return quirksMode ? quirksFontSizeTable[row][col] : strictFontSizeTable[row][col];
    }

    // Value is outside the range of the table. Apply the scale factor instead.
    float minLogicalSize = max(settings->minimumLogicalFontSize(), 1);
    return max(fontSizeFactors[keyword - CSSValueXxSmall]*mediumSize, minLogicalSize);
}

template<typename T>
static int findNearestLegacyFontSize(int pixelFontSize, const T* table, int multiplier)
{
    // Ignore table[0] because xx-small does not correspond to any legacy font size.
    for (int i = 1; i < totalKeywords - 1; i++) {
        if (pixelFontSize * 2 < (table[i] + table[i + 1]) * multiplier)
            return i;
    }
    return totalKeywords - 1;
}

int CSSStyleSelector::legacyFontSize(Document* document, int pixelFontSize, bool shouldUseFixedDefaultSize)
{
    Settings* settings = document->settings();
    if (!settings)
        return 1;

    bool quirksMode = document->inQuirksMode();
    int mediumSize = shouldUseFixedDefaultSize ? settings->defaultFixedFontSize() : settings->defaultFontSize();
    if (mediumSize >= fontSizeTableMin && mediumSize <= fontSizeTableMax) {
        int row = mediumSize - fontSizeTableMin;
        return findNearestLegacyFontSize<int>(pixelFontSize, quirksMode ? quirksFontSizeTable[row] : strictFontSizeTable[row], 1);
    }

    return findNearestLegacyFontSize<float>(pixelFontSize, fontSizeFactors, mediumSize);
}

static Color colorForCSSValue(int cssValueId)
{
    struct ColorValue {
        int cssValueId;
        RGBA32 color;
    };

    static const ColorValue colorValues[] = {
        { CSSValueAqua, 0xFF00FFFF },
        { CSSValueBlack, 0xFF000000 },
        { CSSValueBlue, 0xFF0000FF },
        { CSSValueFuchsia, 0xFFFF00FF },
        { CSSValueGray, 0xFF808080 },
        { CSSValueGreen, 0xFF008000  },
        { CSSValueGrey, 0xFF808080 },
        { CSSValueLime, 0xFF00FF00 },
        { CSSValueMaroon, 0xFF800000 },
        { CSSValueNavy, 0xFF000080 },
        { CSSValueOlive, 0xFF808000  },
        { CSSValueOrange, 0xFFFFA500 },
        { CSSValuePurple, 0xFF800080 },
        { CSSValueRed, 0xFFFF0000 },
        { CSSValueSilver, 0xFFC0C0C0 },
        { CSSValueTeal, 0xFF008080  },
        { CSSValueTransparent, 0x00000000 },
        { CSSValueWhite, 0xFFFFFFFF },
        { CSSValueYellow, 0xFFFFFF00 },
        { 0, 0 }
    };

    for (const ColorValue* col = colorValues; col->cssValueId; ++col) {
        if (col->cssValueId == cssValueId)
            return col->color;
    }
    return RenderTheme::defaultTheme()->systemColor(cssValueId);
}

Color CSSStyleSelector::colorFromPrimitiveValue(CSSPrimitiveValue* value, bool forVisitedLink) const
{
    if (value->isRGBColor())
        return Color(value->getRGBA32Value());

    int ident = value->getIdent();
    switch (ident) {
    case 0:
        return Color();
    case CSSValueWebkitText:
        return m_element->document()->textColor();
    case CSSValueWebkitLink:
        return (m_element->isLink() && forVisitedLink) ? m_element->document()->visitedLinkColor() : m_element->document()->linkColor();
    case CSSValueWebkitActivelink:
        return m_element->document()->activeLinkColor();
    case CSSValueWebkitFocusRingColor:
        return RenderTheme::focusRingColor();
    case CSSValueCurrentcolor:
        return m_style->color();
    default:
        return colorForCSSValue(ident);
    }
}

bool CSSStyleSelector::hasSelectorForAttribute(const AtomicString &attrname) const
{
    return m_features.attrsInRules.contains(attrname.impl());
}

void CSSStyleSelector::addViewportDependentMediaQueryResult(const MediaQueryExp* expr, bool result)
{
    m_viewportDependentMediaQueryResults.append(adoptPtr(new MediaQueryResult(*expr, result)));
}

bool CSSStyleSelector::affectedByViewportChange() const
{
    unsigned s = m_viewportDependentMediaQueryResults.size();
    for (unsigned i = 0; i < s; i++) {
        if (m_medium->eval(&m_viewportDependentMediaQueryResults[i]->m_expression) != m_viewportDependentMediaQueryResults[i]->m_result)
            return true;
    }
    return false;
}

static TransformOperation::OperationType getTransformOperationType(WebKitCSSTransformValue::TransformOperationType type)
{
    switch (type) {
        case WebKitCSSTransformValue::ScaleTransformOperation:          return TransformOperation::SCALE;
        case WebKitCSSTransformValue::ScaleXTransformOperation:         return TransformOperation::SCALE_X;
        case WebKitCSSTransformValue::ScaleYTransformOperation:         return TransformOperation::SCALE_Y;
        case WebKitCSSTransformValue::ScaleZTransformOperation:         return TransformOperation::SCALE_Z;
        case WebKitCSSTransformValue::Scale3DTransformOperation:        return TransformOperation::SCALE_3D;
        case WebKitCSSTransformValue::TranslateTransformOperation:      return TransformOperation::TRANSLATE;
        case WebKitCSSTransformValue::TranslateXTransformOperation:     return TransformOperation::TRANSLATE_X;
        case WebKitCSSTransformValue::TranslateYTransformOperation:     return TransformOperation::TRANSLATE_Y;
        case WebKitCSSTransformValue::TranslateZTransformOperation:     return TransformOperation::TRANSLATE_Z;
        case WebKitCSSTransformValue::Translate3DTransformOperation:    return TransformOperation::TRANSLATE_3D;
        case WebKitCSSTransformValue::RotateTransformOperation:         return TransformOperation::ROTATE;
        case WebKitCSSTransformValue::RotateXTransformOperation:        return TransformOperation::ROTATE_X;
        case WebKitCSSTransformValue::RotateYTransformOperation:        return TransformOperation::ROTATE_Y;
        case WebKitCSSTransformValue::RotateZTransformOperation:        return TransformOperation::ROTATE_Z;
        case WebKitCSSTransformValue::Rotate3DTransformOperation:       return TransformOperation::ROTATE_3D;
        case WebKitCSSTransformValue::SkewTransformOperation:           return TransformOperation::SKEW;
        case WebKitCSSTransformValue::SkewXTransformOperation:          return TransformOperation::SKEW_X;
        case WebKitCSSTransformValue::SkewYTransformOperation:          return TransformOperation::SKEW_Y;
        case WebKitCSSTransformValue::MatrixTransformOperation:         return TransformOperation::MATRIX;
        case WebKitCSSTransformValue::Matrix3DTransformOperation:       return TransformOperation::MATRIX_3D;
        case WebKitCSSTransformValue::PerspectiveTransformOperation:    return TransformOperation::PERSPECTIVE;
        case WebKitCSSTransformValue::UnknownTransformOperation:        return TransformOperation::NONE;
    }
    return TransformOperation::NONE;
}

bool CSSStyleSelector::createTransformOperations(CSSValue* inValue, RenderStyle* style, RenderStyle* rootStyle, TransformOperations& outOperations)
{
    if (!inValue || !inValue->isValueList()) {
        outOperations.clear();
        return false;
    }

    float zoomFactor = style ? style->effectiveZoom() : 1;
    TransformOperations operations;
    for (CSSValueListIterator i = inValue; i.hasMore(); i.advance()) {
        CSSValue* currValue = i.value();
        if (!currValue->isWebKitCSSTransformValue())
            continue;

        WebKitCSSTransformValue* transformValue = static_cast<WebKitCSSTransformValue*>(i.value());
        if (!transformValue->length())
            continue;

        bool haveNonPrimitiveValue = false;
        for (unsigned j = 0; j < transformValue->length(); ++j) {
            if (!transformValue->itemWithoutBoundsCheck(j)->isPrimitiveValue()) {
                haveNonPrimitiveValue = true;
                break;
            }
        }
        if (haveNonPrimitiveValue)
            continue;

        CSSPrimitiveValue* firstValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(0));

        switch (transformValue->operationType()) {
            case WebKitCSSTransformValue::ScaleTransformOperation:
            case WebKitCSSTransformValue::ScaleXTransformOperation:
            case WebKitCSSTransformValue::ScaleYTransformOperation: {
                double sx = 1.0;
                double sy = 1.0;
                if (transformValue->operationType() == WebKitCSSTransformValue::ScaleYTransformOperation)
                    sy = firstValue->getDoubleValue();
                else {
                    sx = firstValue->getDoubleValue();
                    if (transformValue->operationType() != WebKitCSSTransformValue::ScaleXTransformOperation) {
                        if (transformValue->length() > 1) {
                            CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(1));
                            sy = secondValue->getDoubleValue();
                        } else
                            sy = sx;
                    }
                }
                operations.operations().append(ScaleTransformOperation::create(sx, sy, 1.0, getTransformOperationType(transformValue->operationType())));
                break;
            }
            case WebKitCSSTransformValue::ScaleZTransformOperation:
            case WebKitCSSTransformValue::Scale3DTransformOperation: {
                double sx = 1.0;
                double sy = 1.0;
                double sz = 1.0;
                if (transformValue->operationType() == WebKitCSSTransformValue::ScaleZTransformOperation)
                    sz = firstValue->getDoubleValue();
                else if (transformValue->operationType() == WebKitCSSTransformValue::ScaleYTransformOperation)
                    sy = firstValue->getDoubleValue();
                else {
                    sx = firstValue->getDoubleValue();
                    if (transformValue->operationType() != WebKitCSSTransformValue::ScaleXTransformOperation) {
                        if (transformValue->length() > 2) {
                            CSSPrimitiveValue* thirdValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(2));
                            sz = thirdValue->getDoubleValue();
                        }
                        if (transformValue->length() > 1) {
                            CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(1));
                            sy = secondValue->getDoubleValue();
                        } else
                            sy = sx;
                    }
                }
                operations.operations().append(ScaleTransformOperation::create(sx, sy, sz, getTransformOperationType(transformValue->operationType())));
                break;
            }
            case WebKitCSSTransformValue::TranslateTransformOperation:
            case WebKitCSSTransformValue::TranslateXTransformOperation:
            case WebKitCSSTransformValue::TranslateYTransformOperation: {
                bool ok = true;
                Length tx = Length(0, Fixed);
                Length ty = Length(0, Fixed);
                if (transformValue->operationType() == WebKitCSSTransformValue::TranslateYTransformOperation)
                    ty = convertToFloatLength(firstValue, style, rootStyle, zoomFactor, &ok);
                else {
                    tx = convertToFloatLength(firstValue, style, rootStyle, zoomFactor, &ok);
                    if (transformValue->operationType() != WebKitCSSTransformValue::TranslateXTransformOperation) {
                        if (transformValue->length() > 1) {
                            CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(1));
                            ty = convertToFloatLength(secondValue, style, rootStyle, zoomFactor, &ok);
                        }
                    }
                }

                if (!ok)
                    return false;

                operations.operations().append(TranslateTransformOperation::create(tx, ty, Length(0, Fixed), getTransformOperationType(transformValue->operationType())));
                break;
            }
            case WebKitCSSTransformValue::TranslateZTransformOperation:
            case WebKitCSSTransformValue::Translate3DTransformOperation: {
                bool ok = true;
                Length tx = Length(0, Fixed);
                Length ty = Length(0, Fixed);
                Length tz = Length(0, Fixed);
                if (transformValue->operationType() == WebKitCSSTransformValue::TranslateZTransformOperation)
                    tz = convertToFloatLength(firstValue, style, rootStyle, zoomFactor, &ok);
                else if (transformValue->operationType() == WebKitCSSTransformValue::TranslateYTransformOperation)
                    ty = convertToFloatLength(firstValue, style, rootStyle, zoomFactor, &ok);
                else {
                    tx = convertToFloatLength(firstValue, style, rootStyle, zoomFactor, &ok);
                    if (transformValue->operationType() != WebKitCSSTransformValue::TranslateXTransformOperation) {
                        if (transformValue->length() > 2) {
                            CSSPrimitiveValue* thirdValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(2));
                            tz = convertToFloatLength(thirdValue, style, rootStyle, zoomFactor, &ok);
                        }
                        if (transformValue->length() > 1) {
                            CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(1));
                            ty = convertToFloatLength(secondValue, style, rootStyle, zoomFactor, &ok);
                        }
                    }
                }

                if (!ok)
                    return false;

                operations.operations().append(TranslateTransformOperation::create(tx, ty, tz, getTransformOperationType(transformValue->operationType())));
                break;
            }
            case WebKitCSSTransformValue::RotateTransformOperation: {
                double angle = firstValue->computeDegrees();
                operations.operations().append(RotateTransformOperation::create(0, 0, 1, angle, getTransformOperationType(transformValue->operationType())));
                break;
            }
            case WebKitCSSTransformValue::RotateXTransformOperation:
            case WebKitCSSTransformValue::RotateYTransformOperation:
            case WebKitCSSTransformValue::RotateZTransformOperation: {
                double x = 0;
                double y = 0;
                double z = 0;
                double angle = firstValue->computeDegrees();

                if (transformValue->operationType() == WebKitCSSTransformValue::RotateXTransformOperation)
                    x = 1;
                else if (transformValue->operationType() == WebKitCSSTransformValue::RotateYTransformOperation)
                    y = 1;
                else
                    z = 1;
                operations.operations().append(RotateTransformOperation::create(x, y, z, angle, getTransformOperationType(transformValue->operationType())));
                break;
            }
            case WebKitCSSTransformValue::Rotate3DTransformOperation: {
                if (transformValue->length() < 4)
                    break;
                CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(1));
                CSSPrimitiveValue* thirdValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(2));
                CSSPrimitiveValue* fourthValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(3));
                double x = firstValue->getDoubleValue();
                double y = secondValue->getDoubleValue();
                double z = thirdValue->getDoubleValue();
                double angle = fourthValue->computeDegrees();
                operations.operations().append(RotateTransformOperation::create(x, y, z, angle, getTransformOperationType(transformValue->operationType())));
                break;
            }
            case WebKitCSSTransformValue::SkewTransformOperation:
            case WebKitCSSTransformValue::SkewXTransformOperation:
            case WebKitCSSTransformValue::SkewYTransformOperation: {
                double angleX = 0;
                double angleY = 0;
                double angle = firstValue->computeDegrees();
                if (transformValue->operationType() == WebKitCSSTransformValue::SkewYTransformOperation)
                    angleY = angle;
                else {
                    angleX = angle;
                    if (transformValue->operationType() == WebKitCSSTransformValue::SkewTransformOperation) {
                        if (transformValue->length() > 1) {
                            CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(1));
                            angleY = secondValue->computeDegrees();
                        }
                    }
                }
                operations.operations().append(SkewTransformOperation::create(angleX, angleY, getTransformOperationType(transformValue->operationType())));
                break;
            }
            case WebKitCSSTransformValue::MatrixTransformOperation: {
                if (transformValue->length() < 6)
                    break;
                double a = firstValue->getDoubleValue();
                double b = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(1))->getDoubleValue();
                double c = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(2))->getDoubleValue();
                double d = static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(3))->getDoubleValue();
                double e = zoomFactor * static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(4))->getDoubleValue();
                double f = zoomFactor * static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(5))->getDoubleValue();
                operations.operations().append(MatrixTransformOperation::create(a, b, c, d, e, f));
                break;
            }
            case WebKitCSSTransformValue::Matrix3DTransformOperation: {
                if (transformValue->length() < 16)
                    break;
                TransformationMatrix matrix(static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(0))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(1))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(2))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(3))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(4))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(5))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(6))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(7))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(8))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(9))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(10))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(11))->getDoubleValue(),
                                   zoomFactor * static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(12))->getDoubleValue(),
                                   zoomFactor * static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(13))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(14))->getDoubleValue(),
                                   static_cast<CSSPrimitiveValue*>(transformValue->itemWithoutBoundsCheck(15))->getDoubleValue());
                operations.operations().append(Matrix3DTransformOperation::create(matrix));
                break;
            }
            case WebKitCSSTransformValue::PerspectiveTransformOperation: {
                bool ok = true;
                Length p = Length(0, Fixed);
                if (firstValue->isLength())
                    p = convertToFloatLength(firstValue, style, rootStyle, zoomFactor, &ok);
                else {
                    // This is a quirk that should go away when 3d transforms are finalized.
                    double val = firstValue->getDoubleValue();
                    ok = val >= 0;
                    p = Length(clampToPositiveInteger(val), Fixed);
                }

                if (!ok)
                    return false;

                operations.operations().append(PerspectiveTransformOperation::create(p));
                break;
            }
            case WebKitCSSTransformValue::UnknownTransformOperation:
                ASSERT_NOT_REACHED();
                break;
        }
    }

    outOperations = operations;
    return true;
}

#if ENABLE(CSS_FILTERS)
static FilterOperation::OperationType filterOperationForType(WebKitCSSFilterValue::FilterOperationType type)
{
    switch (type) {
    case WebKitCSSFilterValue::ReferenceFilterOperation:
        return FilterOperation::REFERENCE;
    case WebKitCSSFilterValue::GrayscaleFilterOperation:
        return FilterOperation::GRAYSCALE;
    case WebKitCSSFilterValue::SepiaFilterOperation:
        return FilterOperation::SEPIA;
    case WebKitCSSFilterValue::SaturateFilterOperation:
        return FilterOperation::SATURATE;
    case WebKitCSSFilterValue::HueRotateFilterOperation:
        return FilterOperation::HUE_ROTATE;
    case WebKitCSSFilterValue::InvertFilterOperation:
        return FilterOperation::INVERT;
    case WebKitCSSFilterValue::OpacityFilterOperation:
        return FilterOperation::OPACITY;
    case WebKitCSSFilterValue::BrightnessFilterOperation:
        return FilterOperation::BRIGHTNESS;
    case WebKitCSSFilterValue::ContrastFilterOperation:
        return FilterOperation::CONTRAST;
    case WebKitCSSFilterValue::BlurFilterOperation:
        return FilterOperation::BLUR;
    case WebKitCSSFilterValue::DropShadowFilterOperation:
        return FilterOperation::DROP_SHADOW;
#if ENABLE(CSS_SHADERS)
    case WebKitCSSFilterValue::CustomFilterOperation:
        return FilterOperation::CUSTOM;
#endif
    case WebKitCSSFilterValue::UnknownFilterOperation:
        return FilterOperation::NONE;
    }
    return FilterOperation::NONE;
}

#if ENABLE(CSS_SHADERS)
StyleShader* CSSStyleSelector::styleShader(CSSValue* value)
{
    if (value->isWebKitCSSShaderValue())
        return cachedOrPendingStyleShaderFromValue(static_cast<WebKitCSSShaderValue*>(value));
    return 0;
}

StyleShader* CSSStyleSelector::cachedOrPendingStyleShaderFromValue(WebKitCSSShaderValue* value)
{
    StyleShader* shader = value->cachedOrPendingShader();
    if (shader && shader->isPendingShader())
        m_hasPendingShaders = true;
    return shader;
}

void CSSStyleSelector::loadPendingShaders()
{
    if (!m_style->hasFilter() || !m_hasPendingShaders)
        return;

    CachedResourceLoader* cachedResourceLoader = m_element->document()->cachedResourceLoader();

    Vector<RefPtr<FilterOperation> >& filterOperations = m_style->filter().operations();
    for (unsigned i = 0; i < filterOperations.size(); ++i) {
        RefPtr<FilterOperation> filterOperation = filterOperations.at(i);
        if (filterOperation->getOperationType() == FilterOperation::CUSTOM) {
            CustomFilterOperation* customFilter = static_cast<CustomFilterOperation*>(filterOperation.get());
            ASSERT(customFilter->program());
            StyleCustomFilterProgram* program = static_cast<StyleCustomFilterProgram*>(customFilter->program());
            if (program->vertexShader() && program->vertexShader()->isPendingShader()) {
                WebKitCSSShaderValue* shaderValue = static_cast<StylePendingShader*>(program->vertexShader())->cssShaderValue();
                program->setVertexShader(shaderValue->cachedShader(cachedResourceLoader));
            }
            if (program->fragmentShader() && program->fragmentShader()->isPendingShader()) {
                WebKitCSSShaderValue* shaderValue = static_cast<StylePendingShader*>(program->fragmentShader())->cssShaderValue();
                program->setFragmentShader(shaderValue->cachedShader(cachedResourceLoader));
            }
        }
    }
    m_hasPendingShaders = false;
}

static bool sortParametersByNameComparator(const RefPtr<CustomFilterParameter>& a, const RefPtr<CustomFilterParameter>& b)
{
    return codePointCompareLessThan(a->name(), b->name());
}

PassRefPtr<CustomFilterParameter> CSSStyleSelector::parseCustomFilterNumberParamter(const String& name, CSSValueList* values)
{
    RefPtr<CustomFilterNumberParameter> numberParameter = CustomFilterNumberParameter::create(name);
    for (unsigned i = 0; i < values->length(); ++i) {
        CSSValue* value = values->itemWithoutBoundsCheck(i);
        if (!value->isPrimitiveValue())
            return 0;
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return 0;
        numberParameter->addValue(primitiveValue->getDoubleValue());
    }
    return numberParameter.release();
}

bool CSSStyleSelector::parseCustomFilterParameterList(CSSValue* parametersValue, CustomFilterParameterList& parameterList)
{
    HashSet<String> knownParameterNames;
    CSSValueListIterator parameterIterator(parametersValue);
    for (; parameterIterator.hasMore(); parameterIterator.advance()) {
        if (!parameterIterator.value()->isValueList())
            return false;
        CSSValueListIterator iterator(parameterIterator.value());
        if (!iterator.isPrimitiveValue())
            return false;
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(iterator.value());
        if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_STRING)
            return false;
        
        String name = primitiveValue->getStringValue();
        // Do not allow duplicate parameter names.
        if (knownParameterNames.contains(name))
            return false;
        knownParameterNames.add(name);
        
        iterator.advance();
        
        if (!iterator.hasMore())
            return false;
        
        // FIXME: Implement other parameters types parsing.
        // booleans: https://bugs.webkit.org/show_bug.cgi?id=76438
        // textures: https://bugs.webkit.org/show_bug.cgi?id=71442
        // 3d-transforms: https://bugs.webkit.org/show_bug.cgi?id=71443
        // mat2, mat3, mat4: https://bugs.webkit.org/show_bug.cgi?id=71444
        RefPtr<CustomFilterParameter> parameter;
        if (iterator.value()->isValueList()) {
            CSSValueList* values = static_cast<CSSValueList*>(iterator.value());
            iterator.advance();
            
            // We can have only arrays of booleans or numbers, so use the first value to choose between those two.
            // Make sure we have at least one value. We need up to 4 values (all booleans or all numbers).
            if (!values->length() || values->length() > 4)
                return false;
            
            if (!values->itemWithoutBoundsCheck(0)->isPrimitiveValue())
                return false;
            
            CSSPrimitiveValue* firstPrimitiveValue = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(0));
            if (firstPrimitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
                parameter = parseCustomFilterNumberParamter(name, values);
            // FIXME: Implement the boolean array parameter here.
            // https://bugs.webkit.org/show_bug.cgi?id=76438
        }
        
        if (!parameter)
            return false;
        
        parameterList.append(parameter.release());
    }
    
    // Make sure we sort the parameters before passing them down to the CustomFilterOperation.
    std::sort(parameterList.begin(), parameterList.end(), sortParametersByNameComparator);
    
    return true;
}

PassRefPtr<CustomFilterOperation> CSSStyleSelector::createCustomFilterOperation(WebKitCSSFilterValue* filterValue)
{
    ASSERT(filterValue->length());

    CSSValue* shadersValue = filterValue->itemWithoutBoundsCheck(0);
    ASSERT(shadersValue->isValueList());
    CSSValueList* shadersList = static_cast<CSSValueList*>(shadersValue);

    ASSERT(shadersList->length());
    RefPtr<StyleShader> vertexShader = styleShader(shadersList->itemWithoutBoundsCheck(0));
    RefPtr<StyleShader> fragmentShader = (shadersList->length() > 1) ? styleShader(shadersList->itemWithoutBoundsCheck(1)) : 0;
    
    unsigned meshRows = 1;
    unsigned meshColumns = 1;
    CustomFilterOperation::MeshBoxType meshBoxType = CustomFilterOperation::FILTER_BOX;
    CustomFilterOperation::MeshType meshType = CustomFilterOperation::ATTACHED;
    
    CSSValue* parametersValue = 0;
    
    if (filterValue->length() > 1) {
        CSSValueListIterator iterator(filterValue->itemWithoutBoundsCheck(1));
        
        // The second value might be the mesh box or the list of parameters:
        // If it starts with a number or any of the mesh-box identifiers it is 
        // the mesh-box list, if not it means it is the parameters list.

        if (iterator.hasMore() && iterator.isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(iterator.value());
            if (primitiveValue->isNumber()) {
                // If only one integer value is specified, it will set both
                // the rows and the columns.
                meshRows = meshColumns = primitiveValue->getIntValue();
                iterator.advance();
                
                // Try to match another number for the columns.
                if (iterator.hasMore() && iterator.isPrimitiveValue()) {
                    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(iterator.value());
                    if (primitiveValue->isNumber()) {
                        meshColumns = primitiveValue->getIntValue();
                        iterator.advance();
                    }
                }
            }
        }
        
        if (iterator.hasMore() && iterator.isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(iterator.value());
            if (primitiveValue->getIdent() == CSSValueBorderBox
                || primitiveValue->getIdent() == CSSValuePaddingBox
                || primitiveValue->getIdent() == CSSValueContentBox
                || primitiveValue->getIdent() == CSSValueFilterBox) {
                meshBoxType = *primitiveValue;
                iterator.advance();
            }
        }
        
        if (iterator.hasMore() && iterator.isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(iterator.value());
            if (primitiveValue->getIdent() == CSSValueDetached) {
                meshType = CustomFilterOperation::DETACHED;
                iterator.advance();
            }
        }
        
        if (!iterator.index()) {
            // If no value was consumed from the mesh value, then it is just a parameter list, meaning that we end up
            // having just two CSSListValues: list of shaders and list of parameters.
            ASSERT(filterValue->length() == 2);
            parametersValue = filterValue->itemWithoutBoundsCheck(1);
        }
    }
    
    if (filterValue->length() > 2 && !parametersValue)
        parametersValue = filterValue->itemWithoutBoundsCheck(2);
    
    CustomFilterParameterList parameterList;
    if (parametersValue && !parseCustomFilterParameterList(parametersValue, parameterList))
        return 0;
    
    RefPtr<StyleCustomFilterProgram> program = StyleCustomFilterProgram::create(vertexShader.release(), fragmentShader.release());
    return CustomFilterOperation::create(program.release(), parameterList, meshRows, meshColumns, meshBoxType, meshType);
}
#endif

bool CSSStyleSelector::createFilterOperations(CSSValue* inValue, RenderStyle* style, RenderStyle* rootStyle, FilterOperations& outOperations)
{
    if (!inValue || !inValue->isValueList()) {
        outOperations.clear();
        return false;
    }

    float zoomFactor = style ? style->effectiveZoom() : 1;
    FilterOperations operations;
    for (CSSValueListIterator i = inValue; i.hasMore(); i.advance()) {
        CSSValue* currValue = i.value();
        if (!currValue->isWebKitCSSFilterValue())
            continue;

        WebKitCSSFilterValue* filterValue = static_cast<WebKitCSSFilterValue*>(i.value());
        FilterOperation::OperationType operationType = filterOperationForType(filterValue->operationType());

#if ENABLE(CSS_SHADERS)
        if (operationType == FilterOperation::CUSTOM) {
            RefPtr<CustomFilterOperation> operation = createCustomFilterOperation(filterValue);
            if (!operation)
                return false;
            
            operations.operations().append(operation);
            continue;
        }
#endif

        // Check that all parameters are primitive values, with the
        // exception of drop shadow which has a ShadowValue parameter.
        if (operationType != FilterOperation::DROP_SHADOW) {
            bool haveNonPrimitiveValue = false;
            for (unsigned j = 0; j < filterValue->length(); ++j) {
                if (!filterValue->itemWithoutBoundsCheck(j)->isPrimitiveValue()) {
                    haveNonPrimitiveValue = true;
                    break;
                }
            }
            if (haveNonPrimitiveValue)
                continue;
        }

        CSSPrimitiveValue* firstValue = filterValue->length() ? static_cast<CSSPrimitiveValue*>(filterValue->itemWithoutBoundsCheck(0)) : 0;
        switch (filterValue->operationType()) {
        case WebKitCSSFilterValue::ReferenceFilterOperation: {
            if (firstValue)
                operations.operations().append(ReferenceFilterOperation::create(firstValue->getStringValue(), operationType));
            break;
        }
        case WebKitCSSFilterValue::GrayscaleFilterOperation:
        case WebKitCSSFilterValue::SepiaFilterOperation:
        case WebKitCSSFilterValue::SaturateFilterOperation: {
            double amount = 1;
            if (filterValue->length() == 1) {
                amount = firstValue->getDoubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicColorMatrixFilterOperation::create(amount, operationType));
            break;
        }
        case WebKitCSSFilterValue::HueRotateFilterOperation: {
            double angle = 0;
            if (filterValue->length() == 1)
                angle = firstValue->computeDegrees();

            operations.operations().append(BasicColorMatrixFilterOperation::create(angle, operationType));
            break;
        }
        case WebKitCSSFilterValue::InvertFilterOperation:
        case WebKitCSSFilterValue::BrightnessFilterOperation:
        case WebKitCSSFilterValue::ContrastFilterOperation:
        case WebKitCSSFilterValue::OpacityFilterOperation: {
            double amount = (filterValue->operationType() == WebKitCSSFilterValue::BrightnessFilterOperation) ? 0 : 1;
            if (filterValue->length() == 1) {
                amount = firstValue->getDoubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicComponentTransferFilterOperation::create(amount, operationType));
            break;
        }
        case WebKitCSSFilterValue::BlurFilterOperation: {
            bool ok = true;
            Length stdDeviation = Length(0, Fixed);
            if (filterValue->length() >= 1) {
                stdDeviation = convertToFloatLength(firstValue, style, rootStyle, zoomFactor, &ok);
            }
            if (!ok)
                return false;

            operations.operations().append(BlurFilterOperation::create(stdDeviation, operationType));
            break;
        }
        case WebKitCSSFilterValue::DropShadowFilterOperation: {
            if (filterValue->length() != 1)
                return false;

            CSSValue* cssValue = filterValue->itemWithoutBoundsCheck(0);
            if (!cssValue->isShadowValue())
                continue;

            ShadowValue* item = static_cast<ShadowValue*>(cssValue);
            int x = item->x->computeLength<int>(style, rootStyle, zoomFactor);
            int y = item->y->computeLength<int>(style, rootStyle, zoomFactor);
            int blur = item->blur ? item->blur->computeLength<int>(style, rootStyle, zoomFactor) : 0;
            Color color;
            if (item->color)
                color = colorFromPrimitiveValue(item->color.get());

            operations.operations().append(DropShadowFilterOperation::create(x, y, blur, color.isValid() ? color : Color::transparent, operationType));
            break;
        }
        case WebKitCSSFilterValue::UnknownFilterOperation:
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    outOperations = operations;
    return true;
}

#endif

PassRefPtr<StyleImage> CSSStyleSelector::loadPendingImage(StylePendingImage* pendingImage)
{
    CachedResourceLoader* cachedResourceLoader = m_element->document()->cachedResourceLoader();

    if (pendingImage->cssImageValue()) {
        CSSImageValue* imageValue = pendingImage->cssImageValue();
        return imageValue->cachedImage(cachedResourceLoader);
    }

    if (pendingImage->cssImageGeneratorValue()) {
        CSSImageGeneratorValue* imageGeneratorValue = pendingImage->cssImageGeneratorValue();
        imageGeneratorValue->loadSubimages(cachedResourceLoader);
        return StyleGeneratedImage::create(imageGeneratorValue);
    }

    return 0;
}

void CSSStyleSelector::loadPendingImages()
{
    if (m_pendingImageProperties.isEmpty())
        return;

    HashSet<int>::const_iterator end = m_pendingImageProperties.end();
    for (HashSet<int>::const_iterator it = m_pendingImageProperties.begin(); it != end; ++it) {
        CSSPropertyID currentProperty = static_cast<CSSPropertyID>(*it);

        switch (currentProperty) {
            case CSSPropertyBackgroundImage: {
                for (FillLayer* backgroundLayer = m_style->accessBackgroundLayers(); backgroundLayer; backgroundLayer = backgroundLayer->next()) {
                    if (backgroundLayer->image() && backgroundLayer->image()->isPendingImage())
                        backgroundLayer->setImage(loadPendingImage(static_cast<StylePendingImage*>(backgroundLayer->image())));
                }
                break;
            }
            case CSSPropertyContent: {
                for (ContentData* contentData = const_cast<ContentData*>(m_style->contentData()); contentData; contentData = contentData->next()) {
                    if (contentData->isImage()) {
                        StyleImage* image = static_cast<ImageContentData*>(contentData)->image();
                        if (image->isPendingImage()) {
                            RefPtr<StyleImage> loadedImage = loadPendingImage(static_cast<StylePendingImage*>(image));
                            if (loadedImage)
                                static_cast<ImageContentData*>(contentData)->setImage(loadedImage.release());
                        }
                    }
                }
                break;
            }
            case CSSPropertyCursor: {
                if (CursorList* cursorList = m_style->cursors()) {
                    for (size_t i = 0; i < cursorList->size(); ++i) {
                        CursorData& currentCursor = cursorList->at(i);
                        if (StyleImage* image = currentCursor.image()) {
                            if (image->isPendingImage())
                                currentCursor.setImage(loadPendingImage(static_cast<StylePendingImage*>(image)));
                        }
                    }
                }
                break;
            }
            case CSSPropertyListStyleImage: {
                if (m_style->listStyleImage() && m_style->listStyleImage()->isPendingImage())
                    m_style->setListStyleImage(loadPendingImage(static_cast<StylePendingImage*>(m_style->listStyleImage())));
                break;
            }
            case CSSPropertyBorderImageSource: {
                if (m_style->borderImageSource() && m_style->borderImageSource()->isPendingImage())
                    m_style->setBorderImageSource(loadPendingImage(static_cast<StylePendingImage*>(m_style->borderImageSource())));
                break;
            }
            case CSSPropertyWebkitBoxReflect: {
                if (StyleReflection* reflection = m_style->boxReflect()) {
                    const NinePieceImage& maskImage = reflection->mask();
                    if (maskImage.image() && maskImage.image()->isPendingImage()) {
                        RefPtr<StyleImage> loadedImage = loadPendingImage(static_cast<StylePendingImage*>(maskImage.image()));
                        reflection->setMask(NinePieceImage(loadedImage.release(), maskImage.imageSlices(), maskImage.fill(), maskImage.borderSlices(), maskImage.outset(), maskImage.horizontalRule(), maskImage.verticalRule()));
                    }
                }
                break;
            }
            case CSSPropertyWebkitMaskBoxImageSource: {
                if (m_style->maskBoxImageSource() && m_style->maskBoxImageSource()->isPendingImage())
                    m_style->setMaskBoxImageSource(loadPendingImage(static_cast<StylePendingImage*>(m_style->maskBoxImageSource())));
                break;
            }
            case CSSPropertyWebkitMaskImage: {
                for (FillLayer* maskLayer = m_style->accessMaskLayers(); maskLayer; maskLayer = maskLayer->next()) {
                    if (maskLayer->image() && maskLayer->image()->isPendingImage())
                        maskLayer->setImage(loadPendingImage(static_cast<StylePendingImage*>(maskLayer->image())));
                }
                break;
            }
            default:
                ASSERT_NOT_REACHED();
        }
    }

    m_pendingImageProperties.clear();
}

} // namespace WebCore
