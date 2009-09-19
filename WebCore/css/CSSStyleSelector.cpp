/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "CSSBorderImageValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFontFace.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFaceSource.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CSSVariableDependentValue.h"
#include "CSSVariablesDeclaration.h"
#include "CSSVariablesRule.h"
#include "CachedImage.h"
#include "Counter.h"
#include "CounterContent.h"
#include "FocusController.h"
#include "FontFamilyValue.h"
#include "FontValue.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "LinkHash.h"
#include "MappedAttribute.h"
#include "MatrixTransformOperation.h"
#include "Matrix3DTransformOperation.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageGroup.h"
#include "Pair.h"
#include "PerspectiveTransformOperation.h"
#include "Rect.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarTheme.h"
#include "RenderStyleConstants.h"
#include "RenderTheme.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "SelectionController.h"
#include "Settings.h"
#include "ShadowValue.h"
#include "SkewTransformOperation.h"
#include "StyleCachedImage.h"
#include "StyleGeneratedImage.h"
#include "StyleSheetList.h"
#include "Text.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"
#include "UserAgentStyleSheets.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSTransformValue.h"
#include "XMLNames.h"
#include "loader.h"
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#if ENABLE(SVG)
#include "XLinkNames.h"
#include "SVGNames.h"
#endif

#if ENABLE(WML)
#include "WMLNames.h"
#endif

#if PLATFORM(QT)
#include <qwebhistoryinterface.h>
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// #define STYLE_SHARING_STATS 1

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

#define HANDLE_FILL_LAYER_INHERIT_AND_INITIAL(layerType, LayerType, prop, Prop) \
if (isInherit) { \
    FillLayer* currChild = m_style->access##LayerType##Layers(); \
    FillLayer* prevChild = 0; \
    const FillLayer* currParent = m_parentStyle->layerType##Layers(); \
    while (currParent && currParent->is##Prop##Set()) { \
        if (!currChild) { \
            /* Need to make a new layer.*/ \
            currChild = new FillLayer(LayerType##FillLayer); \
            prevChild->setNext(currChild); \
        } \
        currChild->set##Prop(currParent->prop()); \
        prevChild = currChild; \
        currChild = prevChild->next(); \
        currParent = currParent->next(); \
    } \
    \
    while (currChild) { \
        /* Reset any remaining layers to not have the property set. */ \
        currChild->clear##Prop(); \
        currChild = currChild->next(); \
    } \
} else if (isInitial) { \
    FillLayer* currChild = m_style->access##LayerType##Layers(); \
    currChild->set##Prop(FillLayer::initialFill##Prop(LayerType##FillLayer)); \
    for (currChild = currChild->next(); currChild; currChild = currChild->next()) \
        currChild->clear##Prop(); \
}

#define HANDLE_FILL_LAYER_VALUE(layerType, LayerType, prop, Prop, value) { \
HANDLE_FILL_LAYER_INHERIT_AND_INITIAL(layerType, LayerType, prop, Prop) \
if (isInherit || isInitial) \
    return; \
FillLayer* currChild = m_style->access##LayerType##Layers(); \
FillLayer* prevChild = 0; \
if (value->isValueList()) { \
    /* Walk each value and put it into a layer, creating new layers as needed. */ \
    CSSValueList* valueList = static_cast<CSSValueList*>(value); \
    for (unsigned int i = 0; i < valueList->length(); i++) { \
        if (!currChild) { \
            /* Need to make a new layer to hold this value */ \
            currChild = new FillLayer(LayerType##FillLayer); \
            prevChild->setNext(currChild); \
        } \
        mapFill##Prop(currChild, valueList->itemWithoutBoundsCheck(i)); \
        prevChild = currChild; \
        currChild = currChild->next(); \
    } \
} else { \
    mapFill##Prop(currChild, value); \
    currChild = currChild->next(); \
} \
while (currChild) { \
    /* Reset all remaining layers to not have the property set. */ \
    currChild->clear##Prop(); \
    currChild = currChild->next(); \
} }

#define HANDLE_BACKGROUND_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_FILL_LAYER_INHERIT_AND_INITIAL(background, Background, prop, Prop)

#define HANDLE_BACKGROUND_VALUE(prop, Prop, value) \
HANDLE_FILL_LAYER_VALUE(background, Background, prop, Prop, value)

#define HANDLE_MASK_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_FILL_LAYER_INHERIT_AND_INITIAL(mask, Mask, prop, Prop)

#define HANDLE_MASK_VALUE(prop, Prop, value) \
HANDLE_FILL_LAYER_VALUE(mask, Mask, prop, Prop, value)

#define HANDLE_ANIMATION_INHERIT_AND_INITIAL(prop, Prop) \
if (isInherit) { \
    AnimationList* list = m_style->accessAnimations(); \
    const AnimationList* parentList = m_parentStyle->animations(); \
    size_t i = 0, parentSize = parentList ? parentList->size() : 0; \
    for ( ; i < parentSize && parentList->animation(i)->is##Prop##Set(); ++i) { \
        if (list->size() <= i) \
            list->append(Animation::create()); \
        list->animation(i)->set##Prop(parentList->animation(i)->prop()); \
    } \
    \
    /* Reset any remaining animations to not have the property set. */ \
    for ( ; i < list->size(); ++i) \
        list->animation(i)->clear##Prop(); \
} else if (isInitial) { \
    AnimationList* list = m_style->accessAnimations(); \
    if (list->isEmpty()) \
        list->append(Animation::create()); \
    list->animation(0)->set##Prop(Animation::initialAnimation##Prop()); \
    for (size_t i = 1; i < list->size(); ++i) \
        list->animation(0)->clear##Prop(); \
}

#define HANDLE_ANIMATION_VALUE(prop, Prop, value) { \
HANDLE_ANIMATION_INHERIT_AND_INITIAL(prop, Prop) \
if (isInherit || isInitial) \
    return; \
AnimationList* list = m_style->accessAnimations(); \
size_t childIndex = 0; \
if (value->isValueList()) { \
    /* Walk each value and put it into an animation, creating new animations as needed. */ \
    CSSValueList* valueList = static_cast<CSSValueList*>(value); \
    for (unsigned int i = 0; i < valueList->length(); i++) { \
        if (childIndex <= list->size()) \
            list->append(Animation::create()); \
        mapAnimation##Prop(list->animation(childIndex), valueList->itemWithoutBoundsCheck(i)); \
        ++childIndex; \
    } \
} else { \
    if (list->isEmpty()) \
        list->append(Animation::create()); \
    mapAnimation##Prop(list->animation(childIndex), value); \
    childIndex = 1; \
} \
for ( ; childIndex < list->size(); ++childIndex) { \
    /* Reset all remaining animations to not have the property set. */ \
    list->animation(childIndex)->clear##Prop(); \
} \
}

#define HANDLE_TRANSITION_INHERIT_AND_INITIAL(prop, Prop) \
if (isInherit) { \
    AnimationList* list = m_style->accessTransitions(); \
    const AnimationList* parentList = m_parentStyle->transitions(); \
    size_t i = 0, parentSize = parentList ? parentList->size() : 0; \
    for ( ; i < parentSize && parentList->animation(i)->is##Prop##Set(); ++i) { \
        if (list->size() <= i) \
            list->append(Animation::create()); \
        list->animation(i)->set##Prop(parentList->animation(i)->prop()); \
    } \
    \
    /* Reset any remaining transitions to not have the property set. */ \
    for ( ; i < list->size(); ++i) \
        list->animation(i)->clear##Prop(); \
} else if (isInitial) { \
    AnimationList* list = m_style->accessTransitions(); \
    if (list->isEmpty()) \
        list->append(Animation::create()); \
    list->animation(0)->set##Prop(Animation::initialAnimation##Prop()); \
    for (size_t i = 1; i < list->size(); ++i) \
        list->animation(0)->clear##Prop(); \
}

#define HANDLE_TRANSITION_VALUE(prop, Prop, value) { \
HANDLE_TRANSITION_INHERIT_AND_INITIAL(prop, Prop) \
if (isInherit || isInitial) \
    return; \
AnimationList* list = m_style->accessTransitions(); \
size_t childIndex = 0; \
if (value->isValueList()) { \
    /* Walk each value and put it into a transition, creating new animations as needed. */ \
    CSSValueList* valueList = static_cast<CSSValueList*>(value); \
    for (unsigned int i = 0; i < valueList->length(); i++) { \
        if (childIndex <= list->size()) \
            list->append(Animation::create()); \
        mapAnimation##Prop(list->animation(childIndex), valueList->itemWithoutBoundsCheck(i)); \
        ++childIndex; \
    } \
} else { \
    if (list->isEmpty()) \
        list->append(Animation::create()); \
    mapAnimation##Prop(list->animation(childIndex), value); \
    childIndex = 1; \
} \
for ( ; childIndex < list->size(); ++childIndex) { \
    /* Reset all remaining transitions to not have the property set. */ \
    list->animation(childIndex)->clear##Prop(); \
} \
}

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if (id == propID) { \
    m_style->set##Prop(m_parentStyle->prop()); \
    return; \
}
    
#define HANDLE_INHERIT_COND_WITH_BACKUP(propID, prop, propAlt, Prop) \
if (id == propID) { \
    if (m_parentStyle->prop().isValid()) \
        m_style->set##Prop(m_parentStyle->prop()); \
    else \
        m_style->set##Prop(m_parentStyle->propAlt()); \
    return; \
}

#define HANDLE_INITIAL_COND(propID, Prop) \
if (id == propID) { \
    m_style->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

#define HANDLE_INITIAL_COND_WITH_VALUE(propID, Prop, Value) \
if (id == propID) { \
    m_style->set##Prop(RenderStyle::initial##Value()); \
    return; \
}

class CSSRuleSet {
public:
    CSSRuleSet();
    ~CSSRuleSet();
    
    typedef HashMap<AtomicStringImpl*, CSSRuleDataList*> AtomRuleMap;
    
    void addRulesFromSheet(CSSStyleSheet*, const MediaQueryEvaluator&, CSSStyleSelector* = 0);
    
    void addRule(CSSStyleRule* rule, CSSSelector* sel);
    void addToRuleSet(AtomicStringImpl* key, AtomRuleMap& map,
                      CSSStyleRule* rule, CSSSelector* sel);
    
    CSSRuleDataList* getIDRules(AtomicStringImpl* key) { return m_idRules.get(key); }
    CSSRuleDataList* getClassRules(AtomicStringImpl* key) { return m_classRules.get(key); }
    CSSRuleDataList* getTagRules(AtomicStringImpl* key) { return m_tagRules.get(key); }
    CSSRuleDataList* getUniversalRules() { return m_universalRules; }
    
public:
    AtomRuleMap m_idRules;
    AtomRuleMap m_classRules;
    AtomRuleMap m_tagRules;
    CSSRuleDataList* m_universalRules;
    unsigned m_ruleCount;
};

static CSSRuleSet* defaultStyle;
static CSSRuleSet* defaultQuirksStyle;
static CSSRuleSet* defaultPrintStyle;
static CSSRuleSet* defaultViewSourceStyle;
static CSSStyleSheet* simpleDefaultStyleSheet;

RenderStyle* CSSStyleSelector::s_styleNotYetAvailable;

static PseudoState pseudoState;

static void loadFullDefaultStyle();
static void loadSimpleDefaultStyle();
// FIXME: It would be nice to use some mechanism that guarantees this is in sync with the real UA stylesheet.
static const char* simpleUserAgentStyleSheet = "html,body,div{display:block}body{margin:8px}div:focus,span:focus{outline:auto 5px -webkit-focus-ring-color}a:-webkit-any-link{color:-webkit-link;text-decoration:underline}a:-webkit-any-link:active{color:-webkit-activelink}";

static bool elementCanUseSimpleDefaultStyle(Element* e)
{
    return e->hasTagName(htmlTag) || e->hasTagName(bodyTag) || e->hasTagName(divTag) || e->hasTagName(spanTag) || e->hasTagName(brTag) || e->hasTagName(aTag);
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

CSSStyleSelector::CSSStyleSelector(Document* doc, StyleSheetList* styleSheets, CSSStyleSheet* mappedElementSheet,
                                   CSSStyleSheet* pageUserSheet, const Vector<RefPtr<CSSStyleSheet> >* pageGroupUserSheets,
                                   bool strictParsing, bool matchAuthorAndUserStyles)
    : m_backgroundData(BackgroundFillLayer)
    , m_checker(doc, strictParsing)
    , m_fontSelector(CSSFontSelector::create(doc))
{
    init();

    m_matchAuthorAndUserStyles = matchAuthorAndUserStyles;
    
    Element* root = doc->documentElement();

    if (!defaultStyle) {
        if (!root || elementCanUseSimpleDefaultStyle(root))
            loadSimpleDefaultStyle();
        else
            loadFullDefaultStyle();
    }

    m_userStyle = 0;

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = doc->view();
    if (view)
        m_medium = new MediaQueryEvaluator(view->mediaType());
    else
        m_medium = new MediaQueryEvaluator("all");

    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, false, true); // dont ref, because the RenderStyle is allocated from global heap

    if (m_rootDefaultStyle && view) {
        delete m_medium;
        m_medium = new MediaQueryEvaluator(view->mediaType(), view->frame(), m_rootDefaultStyle.get());
    }

    // FIXME: This sucks! The user sheet is reparsed every time!
    if (pageUserSheet || pageGroupUserSheets) {
        m_userStyle = new CSSRuleSet();
        if (pageUserSheet)
            m_userStyle->addRulesFromSheet(pageUserSheet, *m_medium, this);
        if (pageGroupUserSheets) {
            unsigned length = pageGroupUserSheets->size();
            for (unsigned i = 0; i < length; i++)
                m_userStyle->addRulesFromSheet(pageGroupUserSheets->at(i).get(), *m_medium, this);
        }
    }

    // add stylesheets from document
    m_authorStyle = new CSSRuleSet();
    
    // Add rules from elments like SVG's <font-face>
    if (mappedElementSheet)
        m_authorStyle->addRulesFromSheet(mappedElementSheet, *m_medium, this);

    unsigned length = styleSheets->length();
    for (unsigned i = 0; i < length; i++) {
        StyleSheet* sheet = styleSheets->item(i);
        if (sheet->isCSSStyleSheet() && !sheet->disabled())
            m_authorStyle->addRulesFromSheet(static_cast<CSSStyleSheet*>(sheet), *m_medium, this);
    }
}

// This is a simplified style setting function for keyframe styles
void CSSStyleSelector::addKeyframeStyle(PassRefPtr<WebKitCSSKeyframesRule> rule)
{
    AtomicString s(rule->name());
    m_keyframesRuleMap.add(s.impl(), rule);
}

void CSSStyleSelector::init()
{
    m_element = 0;
    m_matchedDecls.clear();
    m_ruleList = 0;
    m_rootDefaultStyle = 0;
    m_medium = 0;
}

CSSStyleSelector::~CSSStyleSelector()
{
    m_fontSelector->clearDocument();
    delete m_medium;
    delete m_authorStyle;
    delete m_userStyle;
    deleteAllValues(m_viewportDependentMediaQueryResults);
    m_keyframesRuleMap.clear();
}

static CSSStyleSheet* parseUASheet(const String& str)
{
    CSSStyleSheet* sheet = CSSStyleSheet::create().releaseRef(); // leak the sheet on purpose
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
        delete defaultStyle;
        delete simpleDefaultStyleSheet;
        defaultStyle = new CSSRuleSet;
        simpleDefaultStyleSheet = 0;
    } else {
        ASSERT(!defaultStyle);
        defaultStyle = new CSSRuleSet;
        defaultPrintStyle = new CSSRuleSet;
        defaultQuirksStyle = new CSSRuleSet;
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
    
    defaultStyle = new CSSRuleSet;
    defaultPrintStyle = new CSSRuleSet;
    defaultQuirksStyle = new CSSRuleSet;

    simpleDefaultStyleSheet = parseUASheet(simpleUserAgentStyleSheet, strlen(simpleUserAgentStyleSheet));
    defaultStyle->addRulesFromSheet(simpleDefaultStyleSheet, screenEval());
    
    // No need to initialize quirks sheet yet as there are no quirk rules for elements allowed in simple default style.
}
    
static void loadViewSourceStyle()
{
    ASSERT(!defaultViewSourceStyle);
    defaultViewSourceStyle = new CSSRuleSet;
    defaultViewSourceStyle->addRulesFromSheet(parseUASheet(sourceUserAgentStyleSheet, sizeof(sourceUserAgentStyleSheet)), screenEval());
}

void CSSStyleSelector::addMatchedDeclaration(CSSMutableStyleDeclaration* decl)
{
    if (!decl->hasVariableDependentValue()) {
        m_matchedDecls.append(decl);
        return;
    }

    // See if we have already resolved the variables in this declaration.
    CSSMutableStyleDeclaration* resolvedDecl = m_resolvedVariablesDeclarations.get(decl).get();
    if (resolvedDecl) {
        m_matchedDecls.append(resolvedDecl);
        return;
    }

    // If this declaration has any variables in it, then we need to make a cloned
    // declaration with as many variables resolved as possible for this style selector's media.
    RefPtr<CSSMutableStyleDeclaration> newDecl = CSSMutableStyleDeclaration::create(decl->parentRule());
    m_matchedDecls.append(newDecl.get());
    m_resolvedVariablesDeclarations.set(decl, newDecl);

    HashSet<String> usedBlockVariables;
    resolveVariablesForDeclaration(decl, newDecl.get(), usedBlockVariables);
}

void CSSStyleSelector::resolveVariablesForDeclaration(CSSMutableStyleDeclaration* decl, CSSMutableStyleDeclaration* newDecl, HashSet<String>& usedBlockVariables)
{
    // Now iterate over the properties in the original declaration.  As we resolve variables we'll end up
    // mutating the new declaration (possibly expanding shorthands).  The new declaration has no m_node
    // though, so it can't mistakenly call setChanged on anything.
    CSSMutableStyleDeclaration::const_iterator end = decl->end();
    for (CSSMutableStyleDeclaration::const_iterator it = decl->begin(); it != end; ++it) {
        const CSSProperty& current = *it;
        if (!current.value()->isVariableDependentValue()) {
            // We can just add the parsed property directly.
            newDecl->addParsedProperty(current);
            continue;
        }
        CSSValueList* valueList = static_cast<CSSVariableDependentValue*>(current.value())->valueList();
        if (!valueList)
            continue;
        CSSParserValueList resolvedValueList;
        unsigned s = valueList->length();
        bool fullyResolved = true;
        for (unsigned i = 0; i < s; ++i) {
            CSSValue* val = valueList->item(i);
            CSSPrimitiveValue* primitiveValue = val->isPrimitiveValue() ? static_cast<CSSPrimitiveValue*>(val) : 0;
            if (primitiveValue && primitiveValue->isVariable()) {
                CSSVariablesRule* rule = m_variablesMap.get(primitiveValue->getStringValue());
                if (!rule || !rule->variables()) {
                    fullyResolved = false;
                    break;
                }
                
                if (current.id() == CSSPropertyWebkitVariableDeclarationBlock && s == 1) {
                    fullyResolved = false;
                    if (!usedBlockVariables.contains(primitiveValue->getStringValue())) {
                        CSSMutableStyleDeclaration* declBlock = rule->variables()->getParsedVariableDeclarationBlock(primitiveValue->getStringValue());
                        if (declBlock) {
                            usedBlockVariables.add(primitiveValue->getStringValue());
                            resolveVariablesForDeclaration(declBlock, newDecl, usedBlockVariables);
                        }
                    }
                }

                CSSValueList* resolvedVariable = rule->variables()->getParsedVariable(primitiveValue->getStringValue());
                if (!resolvedVariable) {
                    fullyResolved = false;
                    break;
                }
                unsigned valueSize = resolvedVariable->length();
                for (unsigned j = 0; j < valueSize; ++j)
                    resolvedValueList.addValue(resolvedVariable->item(j)->parserValue());
            } else
                resolvedValueList.addValue(val->parserValue());
        }
        
        if (!fullyResolved)
            continue;

        // We now have a fully resolved new value list.  We want the parser to use this value list
        // and parse our new declaration.
        CSSParser(m_checker.m_strictParsing).parsePropertyWithResolvedVariables(current.id(), current.isImportant(), newDecl, &resolvedValueList);
    }
}

void CSSStyleSelector::matchRules(CSSRuleSet* rules, int& firstRuleIndex, int& lastRuleIndex)
{
    m_matchedRules.clear();

    if (!rules || !m_element)
        return;
    
    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    if (m_element->hasID())
        matchRulesForList(rules->getIDRules(m_element->getIDAttribute().impl()), firstRuleIndex, lastRuleIndex);
    if (m_element->hasClass()) {
        ASSERT(m_styledElement);
        const ClassNames& classNames = m_styledElement->classNames();
        size_t size = classNames.size();
        for (size_t i = 0; i < size; ++i)
            matchRulesForList(rules->getClassRules(classNames[i].impl()), firstRuleIndex, lastRuleIndex);
    }
    matchRulesForList(rules->getTagRules(m_element->localName().impl()), firstRuleIndex, lastRuleIndex);
    matchRulesForList(rules->getUniversalRules(), firstRuleIndex, lastRuleIndex);
    
    // If we didn't match any rules, we're done.
    if (m_matchedRules.isEmpty())
        return;
    
    // Sort the set of matched rules.
    sortMatchedRules(0, m_matchedRules.size());
    
    // Now transfer the set of matched rules over to our list of decls.
    if (!m_checker.m_collectRulesOnly) {
        for (unsigned i = 0; i < m_matchedRules.size(); i++)
            addMatchedDeclaration(m_matchedRules[i]->rule()->declaration());
    } else {
        for (unsigned i = 0; i < m_matchedRules.size(); i++) {
            if (!m_ruleList)
                m_ruleList = CSSRuleList::create();
            m_ruleList->append(m_matchedRules[i]->rule());
        }
    }
}

void CSSStyleSelector::matchRulesForList(CSSRuleDataList* rules, int& firstRuleIndex, int& lastRuleIndex)
{
    if (!rules)
        return;

    for (CSSRuleData* d = rules->first(); d; d = d->next()) {
        CSSStyleRule* rule = d->rule();
        const AtomicString& localName = m_element->localName();
        const AtomicString& selectorLocalName = d->selector()->m_tag.localName();
        if ((localName == selectorLocalName || selectorLocalName == starAtom) && checkSelector(d->selector())) {
            // If the rule has no properties to apply, then ignore it.
            CSSMutableStyleDeclaration* decl = rule->declaration();
            if (!decl || !decl->length())
                continue;
            
            // If we're matching normal rules, set a pseudo bit if 
            // we really just matched a pseudo-element.
            if (m_dynamicPseudo != NOPSEUDO && m_checker.m_pseudoStyle == NOPSEUDO) {
                if (m_checker.m_collectRulesOnly)
                    return;
                if (m_dynamicPseudo < FIRST_INTERNAL_PSEUDOID)
                    m_style->setHasPseudoStyle(m_dynamicPseudo);
            } else {
                // Update our first/last rule indices in the matched rules array.
                lastRuleIndex = m_matchedDecls.size() + m_matchedRules.size();
                if (firstRuleIndex == -1)
                    firstRuleIndex = lastRuleIndex;

                // Add this rule to our list of matched rules.
                addMatchedRule(d);
            }
        }
    }
}

static bool operator >(CSSRuleData& r1, CSSRuleData& r2)
{
    int spec1 = r1.selector()->specificity();
    int spec2 = r2.selector()->specificity();
    return (spec1 == spec2) ? r1.position() > r2.position() : spec1 > spec2; 
}
    
static bool operator <=(CSSRuleData& r1, CSSRuleData& r2)
{
    return !(r1 > r2);
}

void CSSStyleSelector::sortMatchedRules(unsigned start, unsigned end)
{
    if (start >= end || (end - start == 1))
        return; // Sanity check.

    if (end - start <= 6) {
        // Apply a bubble sort for smaller lists.
        for (unsigned i = end - 1; i > start; i--) {
            bool sorted = true;
            for (unsigned j = start; j < i; j++) {
                CSSRuleData* elt = m_matchedRules[j];
                CSSRuleData* elt2 = m_matchedRules[j + 1];
                if (*elt > *elt2) {
                    sorted = false;
                    m_matchedRules[j] = elt2;
                    m_matchedRules[j + 1] = elt;
                }
            }
            if (sorted)
                return;
        }
        return;
    }

    // Peform a merge sort for larger lists.
    unsigned mid = (start + end) / 2;
    sortMatchedRules(start, mid);
    sortMatchedRules(mid, end);
    
    CSSRuleData* elt = m_matchedRules[mid - 1];
    CSSRuleData* elt2 = m_matchedRules[mid];
    
    // Handle the fast common case (of equal specificity).  The list may already
    // be completely sorted.
    if (*elt <= *elt2)
        return;
    
    // We have to merge sort.  Ensure our merge buffer is big enough to hold
    // all the items.
    Vector<CSSRuleData*> rulesMergeBuffer;
    rulesMergeBuffer.reserveInitialCapacity(end - start); 

    unsigned i1 = start;
    unsigned i2 = mid;
    
    elt = m_matchedRules[i1];
    elt2 = m_matchedRules[i2];
    
    while (i1 < mid || i2 < end) {
        if (i1 < mid && (i2 == end || *elt <= *elt2)) {
            rulesMergeBuffer.append(elt);
            if (++i1 < mid)
                elt = m_matchedRules[i1];
        } else {
            rulesMergeBuffer.append(elt2);
            if (++i2 < end)
                elt2 = m_matchedRules[i2];
        }
    }
    
    for (unsigned i = start; i < end; i++)
        m_matchedRules[i] = rulesMergeBuffer[i - start];
}

void CSSStyleSelector::initElementAndPseudoState(Element* e)
{
    m_element = e;
    if (m_element && m_element->isStyledElement())
        m_styledElement = static_cast<StyledElement*>(m_element);
    else
        m_styledElement = 0;
    pseudoState = PseudoUnknown;
}

void CSSStyleSelector::initForStyleResolve(Element* e, RenderStyle* parentStyle, PseudoId pseudoID)
{
    m_checker.m_pseudoStyle = pseudoID;

    m_parentNode = e ? e->parentNode() : 0;

#if ENABLE(SVG)
    if (!m_parentNode && e && e->isSVGElement() && e->isShadowNode())
        m_parentNode = e->shadowParentNode();
#endif

    if (parentStyle)
        m_parentStyle = parentStyle;
    else
        m_parentStyle = m_parentNode ? m_parentNode->renderStyle() : 0;

    Node* docElement = e ? e->document()->documentElement() : 0;
    RenderStyle* docStyle = m_checker.m_document->renderStyle();
    m_rootElementStyle = docElement && e != docElement ? docElement->renderStyle() : docStyle;

    m_style = 0;

    m_matchedDecls.clear();

    m_ruleList = 0;

    m_fontDirty = false;
}

static inline const AtomicString* linkAttribute(Node* node)
{
    if (!node->isLink())
        return 0;

    ASSERT(node->isElementNode());
    Element* element = static_cast<Element*>(node);
    if (element->isHTMLElement())
        return &element->getAttribute(hrefAttr);

#if ENABLE(WML)
    if (element->isWMLElement()) {
        // <anchor> elements don't have href attributes, but we still want to
        // appear as link, so linkAttribute() has to return a non-null value!
        if (element->hasTagName(WMLNames::anchorTag))
            return &emptyAtom;

        return &element->getAttribute(hrefAttr);
    }
#endif

#if ENABLE(SVG)
    if (element->isSVGElement())
        return &element->getAttribute(XLinkNames::hrefAttr);
#endif

    return 0;
}

CSSStyleSelector::SelectorChecker::SelectorChecker(Document* document, bool strictParsing)
    : m_document(document)
    , m_strictParsing(strictParsing)
    , m_collectRulesOnly(false)
    , m_pseudoStyle(NOPSEUDO)
    , m_documentIsHTML(document->isHTMLDocument())
{
}

PseudoState CSSStyleSelector::SelectorChecker::checkPseudoState(Element* element, bool checkVisited) const
{
    const AtomicString* attr = linkAttribute(element);
    if (!attr || attr->isNull())
        return PseudoNone;

    if (!checkVisited)
        return PseudoAnyLink;

#if PLATFORM(QT)
    Vector<UChar, 512> url;
    visitedURL(m_document->baseURL(), *attr, url);
    if (url.isEmpty())
        return PseudoLink;

    // If the Qt4.4 interface for the history is used, we will have to fallback
    // to the old global history.
    QWebHistoryInterface* iface = QWebHistoryInterface::defaultInterface();
    if (iface)
        return iface->historyContains(QString(reinterpret_cast<QChar*>(url.data()), url.size())) ? PseudoVisited : PseudoLink;

    LinkHash hash = visitedLinkHash(url.data(), url.size());
    if (!hash)
        return PseudoLink;
#else
    LinkHash hash = visitedLinkHash(m_document->baseURL(), *attr);
    if (!hash)
        return PseudoLink;
#endif

    Frame* frame = m_document->frame();
    if (!frame)
        return PseudoLink;

    Page* page = frame->page();
    if (!page)
        return PseudoLink;

    m_linksCheckedForVisitedState.add(hash);
    return page->group().isLinkVisited(hash) ? PseudoVisited : PseudoLink;
}

bool CSSStyleSelector::SelectorChecker::checkSelector(CSSSelector* sel, Element* element) const
{
    pseudoState = PseudoUnknown;
    PseudoId dynamicPseudo = NOPSEUDO;

    return checkSelector(sel, element, 0, dynamicPseudo, true, false) == SelectorMatches;
}

#ifdef STYLE_SHARING_STATS
static int fraction = 0;
static int total = 0;
#endif

static const unsigned cStyleSearchThreshold = 10;

Node* CSSStyleSelector::locateCousinList(Element* parent, unsigned depth)
{
    if (parent && parent->isStyledElement()) {
        StyledElement* p = static_cast<StyledElement*>(parent);
        if (!p->inlineStyleDecl() && !p->hasID()) {
            Node* r = p->previousSibling();
            unsigned subcount = 0;
            RenderStyle* st = p->renderStyle();
            while (r) {
                if (r->renderStyle() == st)
                    return r->lastChild();
                if (subcount++ == cStyleSearchThreshold)
                    return 0;
                r = r->previousSibling();
            }
            if (!r && depth < cStyleSearchThreshold)
                r = locateCousinList(parent->parentElement(), depth + 1);
            while (r) {
                if (r->renderStyle() == st)
                    return r->lastChild();
                if (subcount++ == cStyleSearchThreshold)
                    return 0;
                r = r->previousSibling();
            }
        }
    }
    return 0;
}

bool CSSStyleSelector::canShareStyleWithElement(Node* n)
{
    if (n->isStyledElement()) {
        StyledElement* s = static_cast<StyledElement*>(n);
        RenderStyle* style = s->renderStyle();
        if (style && !style->unique() &&
            (s->tagQName() == m_element->tagQName()) && !s->hasID() &&
            (s->hasClass() == m_element->hasClass()) && !s->inlineStyleDecl() &&
            (s->hasMappedAttributes() == m_styledElement->hasMappedAttributes()) &&
            (s->isLink() == m_element->isLink()) && 
            !style->affectedByAttributeSelectors() &&
            (s->hovered() == m_element->hovered()) &&
            (s->active() == m_element->active()) &&
            (s->focused() == m_element->focused()) &&
            (s != s->document()->cssTarget() && m_element != m_element->document()->cssTarget()) &&
            (s->getAttribute(typeAttr) == m_element->getAttribute(typeAttr)) &&
            (s->getAttribute(XMLNames::langAttr) == m_element->getAttribute(XMLNames::langAttr)) &&
            (s->getAttribute(langAttr) == m_element->getAttribute(langAttr)) &&
            (s->getAttribute(readonlyAttr) == m_element->getAttribute(readonlyAttr)) &&
            (s->getAttribute(cellpaddingAttr) == m_element->getAttribute(cellpaddingAttr))) {
            bool isControl = s->isFormControlElement();
            if (isControl != m_element->isFormControlElement())
                return false;
            if (isControl) {
                InputElement* thisInputElement = toInputElement(s);
                InputElement* otherInputElement = toInputElement(m_element);
                if (thisInputElement && otherInputElement) {
                    if ((thisInputElement->isAutofilled() != otherInputElement->isAutofilled()) ||
                        (thisInputElement->isChecked() != otherInputElement->isChecked()) ||
                        (thisInputElement->isIndeterminate() != otherInputElement->isIndeterminate()))
                    return false;
                } else
                    return false;

                if (s->isEnabledFormControl() != m_element->isEnabledFormControl())
                    return false;

                if (s->isDefaultButtonForForm() != m_element->isDefaultButtonForForm())
                    return false;

                if ((s->willValidate() && s->isValidFormControlElement()) !=
                    (m_element->willValidate() && m_element->isValidFormControlElement()))
                    return false;
            }

            if (style->transitions() || style->animations())
                return false;

            bool classesMatch = true;
            if (s->hasClass()) {
                const AtomicString& class1 = m_element->getAttribute(classAttr);
                const AtomicString& class2 = s->getAttribute(classAttr);
                classesMatch = (class1 == class2);
            }
            
            if (classesMatch) {
                bool mappedAttrsMatch = true;
                if (s->hasMappedAttributes())
                    mappedAttrsMatch = s->mappedAttributes()->mapsEquivalent(m_styledElement->mappedAttributes());
                if (mappedAttrsMatch) {
                    bool linksMatch = true;

                    if (s->isLink()) {
                        // We need to check to see if the visited state matches.
                        if (pseudoState == PseudoUnknown) {
                            const Color& linkColor = m_element->document()->linkColor();
                            const Color& visitedColor = m_element->document()->visitedLinkColor();
                            pseudoState = m_checker.checkPseudoState(m_element, style->pseudoState() != PseudoAnyLink || linkColor != visitedColor);
                        }
                        linksMatch = (pseudoState == style->pseudoState());
                    }
                    
                    if (linksMatch)
                        return true;
                }
            }
        }
    }
    return false;
}

RenderStyle* CSSStyleSelector::locateSharedStyle()
{
    if (m_styledElement && !m_styledElement->inlineStyleDecl() && !m_styledElement->hasID() && !m_styledElement->document()->usesSiblingRules()) {
        // Check previous siblings.
        unsigned count = 0;
        Node* n;
        for (n = m_element->previousSibling(); n && !n->isElementNode(); n = n->previousSibling()) { }
        while (n) {
            if (canShareStyleWithElement(n))
                return n->renderStyle();
            if (count++ == cStyleSearchThreshold)
                return 0;
            for (n = n->previousSibling(); n && !n->isElementNode(); n = n->previousSibling()) { }
        }
        if (!n) 
            n = locateCousinList(m_element->parentElement());
        while (n) {
            if (canShareStyleWithElement(n))
                return n->renderStyle();
            if (count++ == cStyleSearchThreshold)
                return 0;
            for (n = n->previousSibling(); n && !n->isElementNode(); n = n->previousSibling()) { }
        }        
    }
    return 0;
}

void CSSStyleSelector::matchUARules(int& firstUARule, int& lastUARule)
{
    // First we match rules from the user agent sheet.
    CSSRuleSet* userAgentStyleSheet = m_medium->mediaTypeMatchSpecific("print")
        ? defaultPrintStyle : defaultStyle;
    matchRules(userAgentStyleSheet, firstUARule, lastUARule);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (!m_checker.m_strictParsing)
        matchRules(defaultQuirksStyle, firstUARule, lastUARule);
        
    // If we're in view source mode, then we match rules from the view source style sheet.
    if (m_checker.m_document->frame() && m_checker.m_document->frame()->inViewSourceMode()) {
        if (!defaultViewSourceStyle)
            loadViewSourceStyle();
        matchRules(defaultViewSourceStyle, firstUARule, lastUARule);
    }
}

// If resolveForRootDefault is true, style based on user agent style sheet only. This is used in media queries, where
// relative units are interpreted according to document root element style, styled only with UA stylesheet

PassRefPtr<RenderStyle> CSSStyleSelector::styleForElement(Element* e, RenderStyle* defaultParent, bool allowSharing, bool resolveForRootDefault)
{
    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (allowSharing && !e->document()->haveStylesheetsLoaded() && !e->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = ::new RenderStyle;
            s_styleNotYetAvailable->ref();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(m_fontSelector);
        }
        s_styleNotYetAvailable->ref();
        e->document()->setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    initElementAndPseudoState(e);
    if (allowSharing) {
        RenderStyle* sharedStyle = locateSharedStyle();
        if (sharedStyle)
            return sharedStyle;
    }
    initForStyleResolve(e, defaultParent);

    m_style = RenderStyle::create();

    if (m_parentStyle)
        m_style->inheritFrom(m_parentStyle);
    else
        m_parentStyle = style();

    if (simpleDefaultStyleSheet && !elementCanUseSimpleDefaultStyle(e))
        loadFullDefaultStyle();

#if ENABLE(SVG)
    static bool loadedSVGUserAgentSheet;
    if (e->isSVGElement() && !loadedSVGUserAgentSheet) {
        // SVG rules.
        loadedSVGUserAgentSheet = true;
        CSSStyleSheet* svgSheet = parseUASheet(svgUserAgentStyleSheet, sizeof(svgUserAgentStyleSheet));
        defaultStyle->addRulesFromSheet(svgSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(svgSheet, printEval());
    }
#endif

#if ENABLE(MATHML)
    static bool loadedMathMLUserAgentSheet;
    if (e->isMathMLElement() && !loadedMathMLUserAgentSheet) {
        // MathML rules.
        loadedMathMLUserAgentSheet = true;
        CSSStyleSheet* mathMLSheet = parseUASheet(mathmlUserAgentStyleSheet, sizeof(mathmlUserAgentStyleSheet));
        defaultStyle->addRulesFromSheet(mathMLSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(mathMLSheet, printEval());
    }
#endif

#if ENABLE(WML)
    static bool loadedWMLUserAgentSheet;
    if (e->isWMLElement() && !loadedWMLUserAgentSheet) {
        // WML rules.
        loadedWMLUserAgentSheet = true;
        CSSStyleSheet* wmlSheet = parseUASheet(wmlUserAgentStyleSheet, sizeof(wmlUserAgentStyleSheet));
        defaultStyle->addRulesFromSheet(wmlSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(wmlSheet, printEval());
    }
#endif

#if ENABLE(VIDEO)
    static bool loadedMediaStyleSheet;
    if (!loadedMediaStyleSheet && (e->hasTagName(videoTag) || e->hasTagName(audioTag))) {
        loadedMediaStyleSheet = true;
        String mediaRules = String(mediaControlsUserAgentStyleSheet, sizeof(mediaControlsUserAgentStyleSheet)) + RenderTheme::defaultTheme()->extraMediaControlsStyleSheet();
        CSSStyleSheet* mediaControlsSheet = parseUASheet(mediaRules);
        defaultStyle->addRulesFromSheet(mediaControlsSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(mediaControlsSheet, printEval());
    }
#endif

    int firstUARule = -1, lastUARule = -1;
    int firstUserRule = -1, lastUserRule = -1;
    int firstAuthorRule = -1, lastAuthorRule = -1;
    matchUARules(firstUARule, lastUARule);

    if (!resolveForRootDefault) {
        // 4. Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            matchRules(m_userStyle, firstUserRule, lastUserRule);

        // 5. Now check author rules, beginning first with presentational attributes
        // mapped from HTML.
        if (m_styledElement) {
            // Ask if the HTML element has mapped attributes.
            if (m_styledElement->hasMappedAttributes()) {
                // Walk our attribute list and add in each decl.
                const NamedMappedAttrMap* map = m_styledElement->mappedAttributes();
                for (unsigned i = 0; i < map->length(); i++) {
                    Attribute* attr = map->attributeItem(i);
                    if (attr->isMappedAttribute()) {
                        MappedAttribute* mappedAttr = static_cast<MappedAttribute*>(attr);
                        if (mappedAttr->decl()) {
                            lastAuthorRule = m_matchedDecls.size();
                            if (firstAuthorRule == -1)
                                firstAuthorRule = lastAuthorRule;
                            addMatchedDeclaration(mappedAttr->decl());
                        }
                    }
                }
            }

            // Now we check additional mapped declarations.
            // Tables and table cells share an additional mapped rule that must be applied
            // after all attributes, since their mapped style depends on the values of multiple attributes.
            if (m_styledElement->canHaveAdditionalAttributeStyleDecls()) {
                m_additionalAttributeStyleDecls.clear();
                m_styledElement->additionalAttributeStyleDecls(m_additionalAttributeStyleDecls);
                if (!m_additionalAttributeStyleDecls.isEmpty()) {
                    unsigned additionalDeclsSize = m_additionalAttributeStyleDecls.size();
                    if (firstAuthorRule == -1)
                        firstAuthorRule = m_matchedDecls.size();
                    lastAuthorRule = m_matchedDecls.size() + additionalDeclsSize - 1;
                    for (unsigned i = 0; i < additionalDeclsSize; i++)
                        addMatchedDeclaration(m_additionalAttributeStyleDecls[i]);
                }
            }
        }
    
        // 6. Check the rules in author sheets next.
        if (m_matchAuthorAndUserStyles)
            matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);

        // 7. Now check our inline style attribute.
        if (m_matchAuthorAndUserStyles && m_styledElement) {
            CSSMutableStyleDeclaration* inlineDecl = m_styledElement->inlineStyleDecl();
            if (inlineDecl) {
                lastAuthorRule = m_matchedDecls.size();
                if (firstAuthorRule == -1)
                    firstAuthorRule = lastAuthorRule;
                addMatchedDeclaration(inlineDecl);
            }
        }
    }

    // Now we have all of the matched rules in the appropriate order.  Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    m_lineHeightValue = 0;
    applyDeclarations(true, false, 0, m_matchedDecls.size() - 1);
    if (!resolveForRootDefault) {
        applyDeclarations(true, true, firstAuthorRule, lastAuthorRule);
        applyDeclarations(true, true, firstUserRule, lastUserRule);
    }
    applyDeclarations(true, true, firstUARule, lastUARule);
    
    // If our font got dirtied, go ahead and update it now.
    if (m_fontDirty)
        updateFont();

    // Line-height is set when we are sure we decided on the font-size
    if (m_lineHeightValue)
        applyProperty(CSSPropertyLineHeight, m_lineHeightValue);

    // Now do the normal priority UA properties.
    applyDeclarations(false, false, firstUARule, lastUARule);
    
    // Cache our border and background so that we can examine them later.
    cacheBorderAndBackground();
    
    // Now do the author and user normal priority properties and all the !important properties.
    if (!resolveForRootDefault) {
        applyDeclarations(false, false, lastUARule + 1, m_matchedDecls.size() - 1);
        applyDeclarations(false, true, firstAuthorRule, lastAuthorRule);
        applyDeclarations(false, true, firstUserRule, lastUserRule);
    }
    applyDeclarations(false, true, firstUARule, lastUARule);
    
    // If our font got dirtied by one of the non-essential font props, 
    // go ahead and update it a second time.
    if (m_fontDirty)
        updateFont();
    
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style(), e);

    // If we are a link, cache the determined pseudo-state.
    if (e->isLink())
        m_style->setPseudoState(pseudoState);

    // If we have first-letter pseudo style, do not share this style
    if (m_style->hasPseudoStyle(FIRST_LETTER))
        m_style->setUnique();

    // Now return the style.
    return m_style.release();
}

void CSSStyleSelector::keyframeStylesForAnimation(Element* e, const RenderStyle* elementStyle, KeyframeList& list)
{
    list.clear();
    
    // Get the keyframesRule for this name
    if (!e || list.animationName().isEmpty())
        return;
            
    if (!m_keyframesRuleMap.contains(list.animationName().impl()))
        return;
        
    const WebKitCSSKeyframesRule* rule = m_keyframesRuleMap.find(list.animationName().impl()).get()->second.get();
    
    // Construct and populate the style for each keyframe
    for (unsigned i = 0; i < rule->length(); ++i) {
        // Apply the declaration to the style. This is a simplified version of the logic in styleForElement
        initElementAndPseudoState(e);
        initForStyleResolve(e);
        
        const WebKitCSSKeyframeRule* kf = rule->item(i);
        addMatchedDeclaration(kf->style());

        ASSERT(!m_style);

        // Create the style
        m_style = RenderStyle::clone(elementStyle);
        
        m_lineHeightValue = 0;
        
        // We don't need to bother with !important. Since there is only ever one
        // decl, there's nothing to override. So just add the first properties.
        applyDeclarations(true, false, 0, m_matchedDecls.size() - 1);
        
        // If our font got dirtied, go ahead and update it now.
        if (m_fontDirty)
            updateFont();

        // Line-height is set when we are sure we decided on the font-size
        if (m_lineHeightValue)
            applyProperty(CSSPropertyLineHeight, m_lineHeightValue);
        
        // Now do rest of the properties.
        applyDeclarations(false, false, 0, m_matchedDecls.size() - 1);
        
        // If our font got dirtied by one of the non-essential font props, 
        // go ahead and update it a second time.
        if (m_fontDirty)
            updateFont();

        // Add all the animating properties to the list
        CSSMutableStyleDeclaration::const_iterator end = kf->style()->end();
        for (CSSMutableStyleDeclaration::const_iterator it = kf->style()->begin(); it != end; ++it) {
            int property = (*it).id();
            // Timing-function within keyframes is special, because it is not animated; it just
            // describes the timing function between this keyframe and the next.
            if (property != CSSPropertyWebkitAnimationTimingFunction)
                list.addProperty(property);
        }
        
        // Add this keyframe style to all the indicated key times
        Vector<float> keys;
        kf->getKeys(keys);
        for (size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
            float key = keys[keyIndex];
            list.insert(key, m_style);
        }
        m_style = 0;
    }
    
    // Make sure there is a 0% and a 100% keyframe
    float first = -1;
    float last = -1;
    if (list.size() >= 2) {
        first = list.beginKeyframes()->key();
        last = (list.endKeyframes()-1)->key();
    }
    if (first != 0 || last != 1)
        list.clear();
}

PassRefPtr<RenderStyle> CSSStyleSelector::pseudoStyleForElement(PseudoId pseudo, Element* e, RenderStyle* parentStyle)
{
    if (!e)
        return 0;

    initElementAndPseudoState(e);
    initForStyleResolve(e, parentStyle, pseudo);
    m_style = parentStyle;
    
    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.
    
    // Check UA, user and author rules.
    int firstUARule = -1, lastUARule = -1, firstUserRule = -1, lastUserRule = -1, firstAuthorRule = -1, lastAuthorRule = -1;
    matchUARules(firstUARule, lastUARule);

    if (m_matchAuthorAndUserStyles) {
        matchRules(m_userStyle, firstUserRule, lastUserRule);
        matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    }

    if (m_matchedDecls.isEmpty())
        return 0;
    
    m_style = RenderStyle::create();
    if (parentStyle)
        m_style->inheritFrom(parentStyle);

    m_style->noninherited_flags._styleType = pseudo;
    
    m_lineHeightValue = 0;
    // High-priority properties.
    applyDeclarations(true, false, 0, m_matchedDecls.size() - 1);
    applyDeclarations(true, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(true, true, firstUserRule, lastUserRule);
    applyDeclarations(true, true, firstUARule, lastUARule);
    
    // If our font got dirtied, go ahead and update it now.
    if (m_fontDirty)
        updateFont();

    // Line-height is set when we are sure we decided on the font-size
    if (m_lineHeightValue)
        applyProperty(CSSPropertyLineHeight, m_lineHeightValue);
    
    // Now do the normal priority properties.
    applyDeclarations(false, false, firstUARule, lastUARule);
    
    // Cache our border and background so that we can examine them later.
    cacheBorderAndBackground();
    
    applyDeclarations(false, false, lastUARule + 1, m_matchedDecls.size() - 1);
    applyDeclarations(false, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(false, true, firstUserRule, lastUserRule);
    applyDeclarations(false, true, firstUARule, lastUARule);
    
    // If our font got dirtied by one of the non-essential font props, 
    // go ahead and update it a second time.
    if (m_fontDirty)
        updateFont();
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style(), 0);

    // Now return the style.
    return m_style.release();
}

#if ENABLE(DATAGRID)

PassRefPtr<RenderStyle> CSSStyleSelector::pseudoStyleForDataGridColumn(DataGridColumn*, RenderStyle*)
{
    // FIXME: Implement
    return 0;
}

PassRefPtr<RenderStyle> CSSStyleSelector::pseudoStyleForDataGridColumnHeader(DataGridColumn*, RenderStyle*)
{
    // FIXME: Implement
    return 0;
}

#endif

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

void CSSStyleSelector::adjustRenderStyle(RenderStyle* style, Element *e)
{
    // Cache our original display.
    style->setOriginalDisplay(style->display());

    if (style->display() != NONE) {
        // If we have a <td> that specifies a float property, in quirks mode we just drop the float
        // property.
        // Sites also commonly use display:inline/block on <td>s and <table>s.  In quirks mode we force
        // these tags to retain their display types.
        if (!m_checker.m_strictParsing && e) {
            if (e->hasTagName(tdTag)) {
                style->setDisplay(TABLE_CELL);
                style->setFloating(FNONE);
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
            (style->position() == AbsolutePosition || style->position() == FixedPosition || style->floating() != FNONE ||
             (e && e->document()->documentElement() == e))) {
            if (style->display() == INLINE_TABLE)
                style->setDisplay(TABLE);
            else if (style->display() == INLINE_BOX)
                style->setDisplay(BOX);
            else if (style->display() == LIST_ITEM) {
                // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk,
                // but only in quirks mode.
                if (!m_checker.m_strictParsing && style->floating() != FNONE)
                    style->setDisplay(BLOCK);
            }
            else
                style->setDisplay(BLOCK);
        }
        
        // After performing the display mutation, check table rows.  We do not honor position:relative on
        // table rows or cells.  This has been established in CSS2.1 (and caused a crash in containingBlock()
        // on some sites).
        if ((style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_ROW_GROUP ||
             style->display() == TABLE_FOOTER_GROUP || style->display() == TABLE_ROW || style->display() == TABLE_CELL) &&
             style->position() == RelativePosition)
            style->setPosition(StaticPosition);
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style->position() == StaticPosition)
        style->setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects.  This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them.  Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style->hasAutoZIndex() && ((e && e->document()->documentElement() == e) || style->opacity() < 1.0f || 
        style->hasTransformRelatedProperty() || style->hasMask() || style->boxReflect()))
        style->setZIndex(0);
    
    // Button, legend, input, select and textarea all consider width values of 'auto' to be 'intrinsic'.
    // This will be important when we use block flows for all form controls.
    if (e && (e->hasTagName(legendTag) || e->hasTagName(buttonTag) || e->hasTagName(inputTag) ||
              e->hasTagName(selectTag) || e->hasTagName(textareaTag) || e->hasTagName(datagridTag)
#if ENABLE(WML)
              || e->hasTagName(WMLNames::insertedLegendTag)
              || e->hasTagName(WMLNames::inputTag)
#endif
       )) {
        if (style->width().isAuto())
            style->setWidth(Length(Intrinsic));

        // Textarea considers overflow visible as auto.
        if (e && e->hasTagName(textareaTag)) {
            style->setOverflowX(style->overflowX() == OVISIBLE ? OAUTO : style->overflowX());
            style->setOverflowY(style->overflowY() == OVISIBLE ? OAUTO : style->overflowY());
        }
    }

    // Finally update our text decorations in effect, but don't allow text-decoration to percolate through
    // tables, inline blocks, inline tables, or run-ins.
    if (style->display() == TABLE || style->display() == INLINE_TABLE || style->display() == RUN_IN
        || style->display() == INLINE_BLOCK || style->display() == INLINE_BOX)
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
        if (!e->hasTagName(inputTag) || static_cast<HTMLInputElement*>(e)->inputType() != HTMLInputElement::IMAGE)
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style->hasAppearance())
        RenderTheme::defaultTheme()->adjustStyle(this, style, e, m_hasUAAppearance, m_borderData, m_backgroundData, m_backgroundColor);

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

void CSSStyleSelector::updateFont()
{
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

PassRefPtr<CSSRuleList> CSSStyleSelector::styleRulesForElement(Element* e, bool authorOnly)
{
    if (!e || !e->document()->haveStylesheetsLoaded())
        return 0;

    m_checker.m_collectRulesOnly = true;

    initElementAndPseudoState(e);
    initForStyleResolve(e);

    if (!authorOnly) {
        int firstUARule = -1, lastUARule = -1;
        // First we match rules from the user agent sheet.
        matchUARules(firstUARule, lastUARule);

        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles) {
            int firstUserRule = -1, lastUserRule = -1;
            matchRules(m_userStyle, firstUserRule, lastUserRule);
        }
    }

    if (m_matchAuthorAndUserStyles) {
        // Check the rules in author sheets.
        int firstAuthorRule = -1, lastAuthorRule = -1;
        matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    }

    m_checker.m_collectRulesOnly = false;
    
    return m_ruleList.release();
}

PassRefPtr<CSSRuleList> CSSStyleSelector::pseudoStyleRulesForElement(Element*, const String&, bool)
{
    // FIXME: Implement this.
    return 0;
}

bool CSSStyleSelector::checkSelector(CSSSelector* sel)
{
    m_dynamicPseudo = NOPSEUDO;

    // Check the selector
    SelectorMatch match = m_checker.checkSelector(sel, m_element, &m_selectorAttrs, m_dynamicPseudo, true, false, style(), m_parentStyle);
    if (match != SelectorMatches)
        return false;

    if (m_checker.m_pseudoStyle != NOPSEUDO && m_checker.m_pseudoStyle != m_dynamicPseudo)
        return false;

    return true;
}

// Recursive check of selectors and combinators
// It can return 3 different values:
// * SelectorMatches         - the selector matches the element e
// * SelectorFailsLocally    - the selector fails for the element e
// * SelectorFailsCompletely - the selector fails for e and any sibling or ancestor of e
CSSStyleSelector::SelectorMatch CSSStyleSelector::SelectorChecker::checkSelector(CSSSelector* sel, Element* e, HashSet<AtomicStringImpl*>* selectorAttrs, PseudoId& dynamicPseudo, bool isAncestor, bool isSubSelector, RenderStyle* elementStyle, RenderStyle* elementParentStyle) const
{
#if ENABLE(SVG)
    // Spec: CSS2 selectors cannot be applied to the (conceptually) cloned DOM tree
    // because its contents are not part of the formal document structure.
    if (e->isSVGElement() && e->isShadowNode())
        return SelectorFailsCompletely;
#endif

    // first selector has to match
    if (!checkOneSelector(sel, e, selectorAttrs, dynamicPseudo, isAncestor, isSubSelector, elementStyle, elementParentStyle))
        return SelectorFailsLocally;

    // The rest of the selectors has to match
    CSSSelector::Relation relation = sel->relation();

    // Prepare next sel
    sel = sel->tagHistory();
    if (!sel)
        return SelectorMatches;

    if (relation != CSSSelector::SubSelector)
        // Bail-out if this selector is irrelevant for the pseudoStyle
        if (m_pseudoStyle != NOPSEUDO && m_pseudoStyle != dynamicPseudo)
            return SelectorFailsCompletely;

    switch (relation) {
        case CSSSelector::Descendant:
            while (true) {
                Node* n = e->parentNode();
                if (!n || !n->isElementNode())
                    return SelectorFailsCompletely;
                e = static_cast<Element*>(n);
                SelectorMatch match = checkSelector(sel, e, selectorAttrs, dynamicPseudo, true, false);
                if (match != SelectorFailsLocally)
                    return match;
            }
            break;
        case CSSSelector::Child:
        {
            Node* n = e->parentNode();
            if (!n || !n->isElementNode())
                return SelectorFailsCompletely;
            e = static_cast<Element*>(n);
            return checkSelector(sel, e, selectorAttrs, dynamicPseudo, true, false);
        }
        case CSSSelector::DirectAdjacent:
        {
            if (!m_collectRulesOnly && e->parentNode() && e->parentNode()->isElementNode()) {
                RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                if (parentStyle)
                    parentStyle->setChildrenAffectedByDirectAdjacentRules();
            }
            Node* n = e->previousSibling();
            while (n && !n->isElementNode())
                n = n->previousSibling();
            if (!n)
                return SelectorFailsLocally;
            e = static_cast<Element*>(n);
            return checkSelector(sel, e, selectorAttrs, dynamicPseudo, false, false); 
        }
        case CSSSelector::IndirectAdjacent:
            if (!m_collectRulesOnly && e->parentNode() && e->parentNode()->isElementNode()) {
                RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                if (parentStyle)
                    parentStyle->setChildrenAffectedByForwardPositionalRules();
            }
            while (true) {
                Node* n = e->previousSibling();
                while (n && !n->isElementNode())
                    n = n->previousSibling();
                if (!n)
                    return SelectorFailsLocally;
                e = static_cast<Element*>(n);
                SelectorMatch match = checkSelector(sel, e, selectorAttrs, dynamicPseudo, false, false);
                if (match != SelectorFailsLocally)
                    return match;
            };
            break;
        case CSSSelector::SubSelector:
            // a selector is invalid if something follows a pseudo-element
            // We make an exception for scrollbar pseudo elements and allow a set of pseudo classes (but nothing else)
            // to follow the pseudo elements.
            if (elementStyle && dynamicPseudo != NOPSEUDO && dynamicPseudo != SELECTION &&
                !((RenderScrollbar::scrollbarForStyleResolve() || dynamicPseudo == SCROLLBAR_CORNER || dynamicPseudo == RESIZER) && sel->m_match == CSSSelector::PseudoClass))
                return SelectorFailsCompletely;
            return checkSelector(sel, e, selectorAttrs, dynamicPseudo, isAncestor, true, elementStyle, elementParentStyle);
    }

    return SelectorFailsCompletely;
}

static void addLocalNameToSet(HashSet<AtomicStringImpl*>* set, const QualifiedName& qName)
{
    set->add(qName.localName().impl());
}

static HashSet<AtomicStringImpl*>* createHtmlCaseInsensitiveAttributesSet()
{
    // This is the list of attributes in HTML 4.01 with values marked as "[CI]" or case-insensitive
    // Mozilla treats all other values as case-sensitive, thus so do we.
    HashSet<AtomicStringImpl*>* attrSet = new HashSet<AtomicStringImpl*>;

    addLocalNameToSet(attrSet, accept_charsetAttr);
    addLocalNameToSet(attrSet, acceptAttr);
    addLocalNameToSet(attrSet, alignAttr);
    addLocalNameToSet(attrSet, alinkAttr);
    addLocalNameToSet(attrSet, axisAttr);
    addLocalNameToSet(attrSet, bgcolorAttr);
    addLocalNameToSet(attrSet, charsetAttr);
    addLocalNameToSet(attrSet, checkedAttr);
    addLocalNameToSet(attrSet, clearAttr);
    addLocalNameToSet(attrSet, codetypeAttr);
    addLocalNameToSet(attrSet, colorAttr);
    addLocalNameToSet(attrSet, compactAttr);
    addLocalNameToSet(attrSet, declareAttr);
    addLocalNameToSet(attrSet, deferAttr);
    addLocalNameToSet(attrSet, dirAttr);
    addLocalNameToSet(attrSet, disabledAttr);
    addLocalNameToSet(attrSet, enctypeAttr);
    addLocalNameToSet(attrSet, faceAttr);
    addLocalNameToSet(attrSet, frameAttr);
    addLocalNameToSet(attrSet, hreflangAttr);
    addLocalNameToSet(attrSet, http_equivAttr);
    addLocalNameToSet(attrSet, langAttr);
    addLocalNameToSet(attrSet, languageAttr);
    addLocalNameToSet(attrSet, linkAttr);
    addLocalNameToSet(attrSet, mediaAttr);
    addLocalNameToSet(attrSet, methodAttr);
    addLocalNameToSet(attrSet, multipleAttr);
    addLocalNameToSet(attrSet, nohrefAttr);
    addLocalNameToSet(attrSet, noresizeAttr);
    addLocalNameToSet(attrSet, noshadeAttr);
    addLocalNameToSet(attrSet, nowrapAttr);
    addLocalNameToSet(attrSet, readonlyAttr);
    addLocalNameToSet(attrSet, relAttr);
    addLocalNameToSet(attrSet, revAttr);
    addLocalNameToSet(attrSet, rulesAttr);
    addLocalNameToSet(attrSet, scopeAttr);
    addLocalNameToSet(attrSet, scrollingAttr);
    addLocalNameToSet(attrSet, selectedAttr);
    addLocalNameToSet(attrSet, shapeAttr);
    addLocalNameToSet(attrSet, targetAttr);
    addLocalNameToSet(attrSet, textAttr);
    addLocalNameToSet(attrSet, typeAttr);
    addLocalNameToSet(attrSet, valignAttr);
    addLocalNameToSet(attrSet, valuetypeAttr);
    addLocalNameToSet(attrSet, vlinkAttr);

    return attrSet;
}

static bool htmlAttributeHasCaseInsensitiveValue(const QualifiedName& attr)
{
    static HashSet<AtomicStringImpl*>* htmlCaseInsensitiveAttributesSet = createHtmlCaseInsensitiveAttributesSet();
    bool isPossibleHTMLAttr = !attr.hasPrefix() && (attr.namespaceURI() == nullAtom);
    return isPossibleHTMLAttr && htmlCaseInsensitiveAttributesSet->contains(attr.localName().impl());
}

bool CSSStyleSelector::SelectorChecker::checkOneSelector(CSSSelector* sel, Element* e, HashSet<AtomicStringImpl*>* selectorAttrs, PseudoId& dynamicPseudo, bool isAncestor, bool isSubSelector, RenderStyle* elementStyle, RenderStyle* elementParentStyle) const
{
    if (!e)
        return false;

    if (sel->hasTag()) {
        const AtomicString& selLocalName = sel->m_tag.localName();
        if (selLocalName != starAtom && selLocalName != e->localName())
            return false;
        const AtomicString& selNS = sel->m_tag.namespaceURI();
        if (selNS != starAtom && selNS != e->namespaceURI())
            return false;
    }

    if (sel->hasAttribute()) {
        if (sel->m_match == CSSSelector::Class)
            return e->hasClass() && static_cast<StyledElement*>(e)->classNames().contains(sel->m_value);

        if (sel->m_match == CSSSelector::Id)
            return e->hasID() && e->getIDAttribute() == sel->m_value;
        
        const QualifiedName& attr = sel->attribute();

        // FIXME: Handle the case were elementStyle is 0.
        if (elementStyle && (!e->isStyledElement() || (!static_cast<StyledElement*>(e)->isMappedAttribute(attr) && attr != typeAttr && attr != readonlyAttr))) {
            elementStyle->setAffectedByAttributeSelectors(); // Special-case the "type" and "readonly" attributes so input form controls can share style.
            if (selectorAttrs)
                selectorAttrs->add(attr.localName().impl());
        }

        const AtomicString& value = e->getAttribute(attr);
        if (value.isNull())
            return false; // attribute is not set

        bool caseSensitive = !m_documentIsHTML || !htmlAttributeHasCaseInsensitiveValue(attr);

        switch (sel->m_match) {
        case CSSSelector::Exact:
            if (caseSensitive ? sel->m_value != value : !equalIgnoringCase(sel->m_value, value))
                return false;
            break;
        case CSSSelector::List:
        {
            // Ignore empty selectors or selectors containing spaces
            if (sel->m_value.contains(' ') || sel->m_value.isEmpty())
                return false;

            int startSearchAt = 0;
            while (true) {
                int foundPos = value.find(sel->m_value, startSearchAt, caseSensitive);
                if (foundPos == -1)
                    return false;
                if (foundPos == 0 || value[foundPos-1] == ' ') {
                    unsigned endStr = foundPos + sel->m_value.length();
                    if (endStr == value.length() || value[endStr] == ' ')
                        break; // We found a match.
                }
                
                // No match.  Keep looking.
                startSearchAt = foundPos + 1;
            }
            break;
        }
        case CSSSelector::Contain:
            if (!value.contains(sel->m_value, caseSensitive) || sel->m_value.isEmpty())
                return false;
            break;
        case CSSSelector::Begin:
            if (!value.startsWith(sel->m_value, caseSensitive) || sel->m_value.isEmpty())
                return false;
            break;
        case CSSSelector::End:
            if (!value.endsWith(sel->m_value, caseSensitive) || sel->m_value.isEmpty())
                return false;
            break;
        case CSSSelector::Hyphen:
            if (value.length() < sel->m_value.length())
                return false;
            if (!value.startsWith(sel->m_value, caseSensitive))
                return false;
            // It they start the same, check for exact match or following '-':
            if (value.length() != sel->m_value.length() && value[sel->m_value.length()] != '-')
                return false;
            break;
        case CSSSelector::PseudoClass:
        case CSSSelector::PseudoElement:
        default:
            break;
        }
    }
    
    if (sel->m_match == CSSSelector::PseudoClass) {
        // Handle :not up front.
        if (sel->pseudoType() == CSSSelector::PseudoNot) {
            // check the simple selector
            for (CSSSelector* subSel = sel->simpleSelector(); subSel; subSel = subSel->tagHistory()) {
                // :not cannot nest. I don't really know why this is a
                // restriction in CSS3, but it is, so let's honor it.
                // the parser enforces that this never occurs
                ASSERT(!subSel->simpleSelector());

                if (!checkOneSelector(subSel, e, selectorAttrs, dynamicPseudo, isAncestor, true, elementStyle, elementParentStyle))
                    return true;
            }
        } else if (dynamicPseudo != NOPSEUDO && (RenderScrollbar::scrollbarForStyleResolve() || dynamicPseudo == SCROLLBAR_CORNER || dynamicPseudo == RESIZER)) {
            // CSS scrollbars match a specific subset of pseudo classes, and they have specialized rules for each
            // (since there are no elements involved).
            return checkScrollbarPseudoClass(sel, dynamicPseudo);
        } else if (dynamicPseudo == SELECTION) {
            if (sel->pseudoType() == CSSSelector::PseudoWindowInactive)
                return !m_document->page()->focusController()->isActive();
        }
        
        // Normal element pseudo class checking.
        switch (sel->pseudoType()) {
            // Pseudo classes:
            case CSSSelector::PseudoNot:
                break; // Already handled up above.
            case CSSSelector::PseudoEmpty: {
                bool result = true;
                for (Node* n = e->firstChild(); n; n = n->nextSibling()) {
                    if (n->isElementNode()) {
                        result = false;
                        break;
                    } else if (n->isTextNode()) {
                        Text* textNode = static_cast<Text*>(n);
                        if (!textNode->data().isEmpty()) {
                            result = false;
                            break;
                        }
                    }
                }
                if (!m_collectRulesOnly) {
                    if (elementStyle)
                        elementStyle->setEmptyState(result);
                    else if (e->renderStyle() && (e->document()->usesSiblingRules() || e->renderStyle()->unique()))
                        e->renderStyle()->setEmptyState(result);
                }
                return result;
            }
            case CSSSelector::PseudoFirstChild: {
                // first-child matches the first child that is an element
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    bool result = false;
                    Node* n = e->previousSibling();
                    while (n && !n->isElementNode())
                        n = n->previousSibling();
                    if (!n)
                        result = true;
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByFirstChildRules();
                        if (result && childStyle)
                            childStyle->setFirstChildState();
                    }
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoFirstOfType: {
                // first-of-type matches the first element of its type
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    bool result = false;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->previousSibling();
                    }
                    if (!n)
                        result = true;
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoLastChild: {
                // last-child matches the last child that is an element
                if (Element* parentElement = e->parentElement()) {
                    bool result = false;
                    if (parentElement->isFinishedParsingChildren()) {
                        Node* n = e->nextSibling();
                        while (n && !n->isElementNode())
                            n = n->nextSibling();
                        if (!n)
                            result = true;
                    }
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByLastChildRules();
                        if (result && childStyle)
                            childStyle->setLastChildState();
                    }
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoLastOfType: {
                // last-of-type matches the last element of its type
                if (Element* parentElement = e->parentElement()) {
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentElement->isFinishedParsingChildren())
                        return false;
                    bool result = false;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->nextSibling();
                    }
                    if (!n)
                        result = true;
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoOnlyChild: {
                if (Element* parentElement = e->parentElement()) {
                    bool firstChild = false;
                    bool lastChild = false;
                    
                    Node* n = e->previousSibling();
                    while (n && !n->isElementNode())
                        n = n->previousSibling();
                    if (!n)
                        firstChild = true;
                    if (firstChild && parentElement->isFinishedParsingChildren()) {
                        n = e->nextSibling();
                        while (n && !n->isElementNode())
                            n = n->nextSibling();
                        if (!n)
                            lastChild = true;
                    }
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                        if (parentStyle) {
                            parentStyle->setChildrenAffectedByFirstChildRules();
                            parentStyle->setChildrenAffectedByLastChildRules();
                        }
                        if (firstChild && childStyle)
                            childStyle->setFirstChildState();
                        if (lastChild && childStyle)
                            childStyle->setLastChildState();
                    }
                    return firstChild && lastChild;
                }
                break;
            }
            case CSSSelector::PseudoOnlyOfType: {
                // FIXME: This selector is very slow.
                if (Element* parentElement = e->parentElement()) {
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                        if (parentStyle) {
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                        }
                    }
                    if (!parentElement->isFinishedParsingChildren())
                        return false;
                    bool firstChild = false;
                    bool lastChild = false;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->previousSibling();
                    }
                    if (!n)
                        firstChild = true;
                    if (firstChild) {
                        n = e->nextSibling();
                        while (n) {
                            if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                                break;
                            n = n->nextSibling();
                        }
                        if (!n)
                            lastChild = true;
                    }
                    return firstChild && lastChild;
                }
                break;
            }
            case CSSSelector::PseudoNthChild: {
                if (!sel->parseNth())
                    break;
                if (Element* parentElement = e->parentElement()) {
                    int count = 1;
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode()) {
                            RenderStyle* s = n->renderStyle();
                            unsigned index = s ? s->childIndex() : 0;
                            if (index) {
                                count += index;
                                break;
                            }
                            count++;
                        }
                        n = n->previousSibling();
                    }
                    
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                        if (childStyle)
                            childStyle->setChildIndex(count);
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }
                    
                    if (sel->matchNth(count))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthOfType: {
                if (!sel->parseNth())
                    break;
                if (Element* parentElement = e->parentElement()) {
                    int count = 1;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            count++;
                        n = n->previousSibling();
                    }
                    
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }

                    if (sel->matchNth(count))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthLastChild: {
                if (!sel->parseNth())
                    break;
                if (Element* parentElement = e->parentElement()) {
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentElement->isFinishedParsingChildren())
                        return false;
                    int count = 1;
                    Node* n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode())
                            count++;
                        n = n->nextSibling();
                    }
                    if (sel->matchNth(count))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthLastOfType: {
                if (!sel->parseNth())
                    break;
                if (Element* parentElement = e->parentElement()) {
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentElement->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentElement->isFinishedParsingChildren())
                        return false;
                    int count = 1;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            count++;
                        n = n->nextSibling();
                    }
                    if (sel->matchNth(count))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoTarget:
                if (e == e->document()->cssTarget())
                    return true;
                break;
            case CSSSelector::PseudoAnyLink:
                if (pseudoState == PseudoUnknown)
                    pseudoState = checkPseudoState(e, false);
                if (pseudoState == PseudoAnyLink || pseudoState == PseudoLink || pseudoState == PseudoVisited)
                    return true;
                break;
            case CSSSelector::PseudoAutofill: {
                if (!e || !e->isFormControlElement())
                    break;
                if (InputElement* inputElement = toInputElement(e))
                    return inputElement->isAutofilled();
                break;
            }
            case CSSSelector::PseudoLink:
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    pseudoState = checkPseudoState(e);
                if (pseudoState == PseudoLink)
                    return true;
                break;
            case CSSSelector::PseudoVisited:
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    pseudoState = checkPseudoState(e);
                if (pseudoState == PseudoVisited)
                    return true;
                break;
            case CSSSelector::PseudoDrag: {
                if (elementStyle)
                    elementStyle->setAffectedByDragRules(true);
                else if (e->renderStyle())
                    e->renderStyle()->setAffectedByDragRules(true);
                if (e->renderer() && e->renderer()->isDragging())
                    return true;
                break;
            }
            case CSSSelector::PseudoFocus:
                if (e && e->focused() && e->document()->frame()->selection()->isFocusedAndActive())
                    return true;
                break;
            case CSSSelector::PseudoHover: {
                // If we're in quirks mode, then hover should never match anchors with no
                // href and *:hover should not match anything.  This is important for sites like wsj.com.
                if (m_strictParsing || isSubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                    if (elementStyle)
                        elementStyle->setAffectedByHoverRules(true);
                    else if (e->renderStyle())
                        e->renderStyle()->setAffectedByHoverRules(true);
                    if (e->hovered())
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoActive:
                // If we're in quirks mode, then :active should never match anchors with no
                // href and *:active should not match anything. 
                if (m_strictParsing || isSubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                    if (elementStyle)
                        elementStyle->setAffectedByActiveRules(true);
                    else if (e->renderStyle())
                        e->renderStyle()->setAffectedByActiveRules(true);
                    if (e->active())
                        return true;
                }
                break;
            case CSSSelector::PseudoEnabled:
                if (e && e->isFormControlElement()) {
                    InputElement* inputElement = toInputElement(e);
                    if (inputElement && inputElement->isInputTypeHidden())
                        break;
                    // The UI spec states that you can't match :enabled unless you are an object that can
                    // "receive focus and be activated."  We will limit matching of this pseudo-class to elements
                    // that are non-"hidden" controls.
                    return e->isEnabledFormControl();
                }
                break;
            case CSSSelector::PseudoFullPageMedia:
                return e && e->document() && e->document()->isMediaDocument();
                break;
            case CSSSelector::PseudoDefault:
                return e && e->isDefaultButtonForForm();
            case CSSSelector::PseudoDisabled:
                if (e && e->isFormControlElement()) {
                    InputElement* inputElement = toInputElement(e);
                    if (inputElement && inputElement->isInputTypeHidden())
                        break;

                    // The UI spec states that you can't match :enabled unless you are an object that can
                    // "receive focus and be activated."  We will limit matching of this pseudo-class to elements
                    // that are non-"hidden" controls.
                    return !e->isEnabledFormControl();
                }
                break;
            case CSSSelector::PseudoReadOnly: {
                if (!e || !e->isFormControlElement())
                    return false;
                return e->isTextFormControl() && e->isReadOnlyFormControl();
            }
            case CSSSelector::PseudoReadWrite: {
                if (!e || !e->isFormControlElement())
                    return false;
                return e->isTextFormControl() && !e->isReadOnlyFormControl();
            }
            case CSSSelector::PseudoOptional:
                return e && e->isOptionalFormControl();
            case CSSSelector::PseudoRequired:
                return e && e->isRequiredFormControl();
            case CSSSelector::PseudoValid:
                return e && e->willValidate() && e->isValidFormControlElement();
            case CSSSelector::PseudoInvalid:
                return e && e->willValidate() && !e->isValidFormControlElement();
            case CSSSelector::PseudoChecked: {
                if (!e || !e->isFormControlElement())
                    break;
                // Even though WinIE allows checked and indeterminate to co-exist, the CSS selector spec says that
                // you can't be both checked and indeterminate.  We will behave like WinIE behind the scenes and just
                // obey the CSS spec here in the test for matching the pseudo.
                InputElement* inputElement = toInputElement(e);
                if (inputElement && inputElement->isChecked() && !inputElement->isIndeterminate())
                    return true;
                break;
            }
            case CSSSelector::PseudoIndeterminate: {
                if (!e || !e->isFormControlElement())
                    break;
                InputElement* inputElement = toInputElement(e);
                if (inputElement && inputElement->isIndeterminate())
                    return true;
                break;
            }
            case CSSSelector::PseudoRoot:
                if (e == e->document()->documentElement())
                    return true;
                break;
            case CSSSelector::PseudoLang: {
                Node* n = e;
                AtomicString value;
                // The language property is inherited, so we iterate over the parents
                // to find the first language.
                while (n && value.isEmpty()) {
                    if (n->isElementNode()) {
                        // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
                        value = static_cast<Element*>(n)->getAttribute(XMLNames::langAttr);
                        if (value.isEmpty())
                            value = static_cast<Element*>(n)->getAttribute(langAttr);
                    } else if (n->isDocumentNode())
                        // checking the MIME content-language
                        value = static_cast<Document*>(n)->contentLanguage();

                    n = n->parent();
                }
                const AtomicString& argument = sel->argument();
                if (value.isEmpty() || !value.startsWith(argument, false))
                    break;
                if (value.length() != argument.length() && value[argument.length()] != '-')
                    break;
                return true;
            }
            case CSSSelector::PseudoUnknown:
            case CSSSelector::PseudoNotParsed:
            default:
                ASSERT_NOT_REACHED();
                break;
        }
        return false;
    }
    if (sel->m_match == CSSSelector::PseudoElement) {
        if (!elementStyle)
            return false;

        switch (sel->pseudoType()) {
            // Pseudo-elements:
            case CSSSelector::PseudoFirstLine:
                dynamicPseudo = FIRST_LINE;
                return true;
            case CSSSelector::PseudoFirstLetter:
                dynamicPseudo = FIRST_LETTER;
                if (Document* doc = e->document())
                    doc->setUsesFirstLetterRules(true);
                return true;
            case CSSSelector::PseudoSelection:
                dynamicPseudo = SELECTION;
                return true;
            case CSSSelector::PseudoBefore:
                dynamicPseudo = BEFORE;
                return true;
            case CSSSelector::PseudoAfter:
                dynamicPseudo = AFTER;
                return true;
            case CSSSelector::PseudoFileUploadButton:
                dynamicPseudo = FILE_UPLOAD_BUTTON;
                return true;
            case CSSSelector::PseudoInputPlaceholder:
                dynamicPseudo = INPUT_PLACEHOLDER;
                return true;
            case CSSSelector::PseudoSliderThumb:
                dynamicPseudo = SLIDER_THUMB;
                return true; 
            case CSSSelector::PseudoSearchCancelButton:
                dynamicPseudo = SEARCH_CANCEL_BUTTON;
                return true; 
            case CSSSelector::PseudoSearchDecoration:
                dynamicPseudo = SEARCH_DECORATION;
                return true;
            case CSSSelector::PseudoSearchResultsDecoration:
                dynamicPseudo = SEARCH_RESULTS_DECORATION;
                return true;
            case CSSSelector::PseudoSearchResultsButton:
                dynamicPseudo = SEARCH_RESULTS_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsPanel:
                dynamicPseudo = MEDIA_CONTROLS_PANEL;
                return true;
            case CSSSelector::PseudoMediaControlsMuteButton:
                dynamicPseudo = MEDIA_CONTROLS_MUTE_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsPlayButton:
                dynamicPseudo = MEDIA_CONTROLS_PLAY_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsTimelineContainer:
                dynamicPseudo = MEDIA_CONTROLS_TIMELINE_CONTAINER;
                return true;
            case CSSSelector::PseudoMediaControlsVolumeSliderContainer:
                dynamicPseudo = MEDIA_CONTROLS_VOLUME_SLIDER_CONTAINER;
                return true;
            case CSSSelector::PseudoMediaControlsCurrentTimeDisplay:
                dynamicPseudo = MEDIA_CONTROLS_CURRENT_TIME_DISPLAY;
                return true;
            case CSSSelector::PseudoMediaControlsTimeRemainingDisplay:
                dynamicPseudo = MEDIA_CONTROLS_TIME_REMAINING_DISPLAY;
                return true;
            case CSSSelector::PseudoMediaControlsTimeline:
                dynamicPseudo = MEDIA_CONTROLS_TIMELINE;
                return true;
            case CSSSelector::PseudoMediaControlsVolumeSlider:
                dynamicPseudo = MEDIA_CONTROLS_VOLUME_SLIDER;
                return true;
            case CSSSelector::PseudoMediaControlsSeekBackButton:
                dynamicPseudo = MEDIA_CONTROLS_SEEK_BACK_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsSeekForwardButton:
                dynamicPseudo = MEDIA_CONTROLS_SEEK_FORWARD_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsRewindButton:
                dynamicPseudo = MEDIA_CONTROLS_REWIND_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsReturnToRealtimeButton:
                dynamicPseudo = MEDIA_CONTROLS_RETURN_TO_REALTIME_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsStatusDisplay:
                dynamicPseudo = MEDIA_CONTROLS_STATUS_DISPLAY;
                return true;
            case CSSSelector::PseudoMediaControlsFullscreenButton:
                dynamicPseudo = MEDIA_CONTROLS_FULLSCREEN_BUTTON;
                return true;
            case CSSSelector::PseudoScrollbar:
                dynamicPseudo = SCROLLBAR;
                return true;
            case CSSSelector::PseudoScrollbarButton:
                dynamicPseudo = SCROLLBAR_BUTTON;
                return true;
            case CSSSelector::PseudoScrollbarCorner:
                dynamicPseudo = SCROLLBAR_CORNER;
                return true;
            case CSSSelector::PseudoScrollbarThumb:
                dynamicPseudo = SCROLLBAR_THUMB;
                return true;
            case CSSSelector::PseudoScrollbarTrack:
                dynamicPseudo = SCROLLBAR_TRACK;
                return true;
            case CSSSelector::PseudoScrollbarTrackPiece:
                dynamicPseudo = SCROLLBAR_TRACK_PIECE;
                return true;
            case CSSSelector::PseudoResizer:
                dynamicPseudo = RESIZER;
                return true;
            case CSSSelector::PseudoUnknown:
            case CSSSelector::PseudoNotParsed:
            default:
                ASSERT_NOT_REACHED();
                break;
        }
        return false;
    }
    // ### add the rest of the checks...
    return true;
}

bool CSSStyleSelector::SelectorChecker::checkScrollbarPseudoClass(CSSSelector* sel, PseudoId&) const
{
    RenderScrollbar* scrollbar = RenderScrollbar::scrollbarForStyleResolve();
    ScrollbarPart part = RenderScrollbar::partForStyleResolve();

    // FIXME: This is a temporary hack for resizers and scrollbar corners.  Eventually :window-inactive should become a real
    // pseudo class and just apply to everything.
    if (sel->pseudoType() == CSSSelector::PseudoWindowInactive)
        return !m_document->page()->focusController()->isActive();
    
    if (!scrollbar)
        return false;
        
    ASSERT(sel->m_match == CSSSelector::PseudoClass);
    switch (sel->pseudoType()) {
        case CSSSelector::PseudoEnabled:
            return scrollbar->enabled();
        case CSSSelector::PseudoDisabled:
            return !scrollbar->enabled();
        case CSSSelector::PseudoHover: {
            ScrollbarPart hoveredPart = scrollbar->hoveredPart();
            if (part == ScrollbarBGPart)
                return hoveredPart != NoPart;
            if (part == TrackBGPart)
                return hoveredPart == BackTrackPart || hoveredPart == ForwardTrackPart || hoveredPart == ThumbPart;
            return part == hoveredPart;
        }
        case CSSSelector::PseudoActive: {
            ScrollbarPart pressedPart = scrollbar->pressedPart();
            if (part == ScrollbarBGPart)
                return pressedPart != NoPart;
            if (part == TrackBGPart)
                return pressedPart == BackTrackPart || pressedPart == ForwardTrackPart || pressedPart == ThumbPart;
            return part == pressedPart;
        }
        case CSSSelector::PseudoHorizontal:
            return scrollbar->orientation() == HorizontalScrollbar;
        case CSSSelector::PseudoVertical:
            return scrollbar->orientation() == VerticalScrollbar;
        case CSSSelector::PseudoDecrement:
            return part == BackButtonStartPart || part == BackButtonEndPart || part == BackTrackPart;
        case CSSSelector::PseudoIncrement:
            return part == ForwardButtonStartPart || part == ForwardButtonEndPart || part == ForwardTrackPart;
        case CSSSelector::PseudoStart:
            return part == BackButtonStartPart || part == ForwardButtonStartPart || part == BackTrackPart;
        case CSSSelector::PseudoEnd:
            return part == BackButtonEndPart || part == ForwardButtonEndPart || part == ForwardTrackPart;
        case CSSSelector::PseudoDoubleButton: {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackButtonStartPart || part == ForwardButtonStartPart || part == BackTrackPart)
                return buttonsPlacement == ScrollbarButtonsDoubleStart || buttonsPlacement == ScrollbarButtonsDoubleBoth;
            if (part == BackButtonEndPart || part == ForwardButtonEndPart || part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsDoubleEnd || buttonsPlacement == ScrollbarButtonsDoubleBoth;
            return false;
        } 
        case CSSSelector::PseudoSingleButton: {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackButtonStartPart || part == ForwardButtonEndPart || part == BackTrackPart || part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsSingle;
            return false;
        }
        case CSSSelector::PseudoNoButton: {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackTrackPart)
                return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleEnd;
            if (part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleStart;
            return false;
        }
        case CSSSelector::PseudoCornerPresent:
            return scrollbar->client()->scrollbarCornerPresent();
        default:
            return false;
    }
}

void CSSStyleSelector::addVariables(CSSVariablesRule* variables)
{
    CSSVariablesDeclaration* decl = variables->variables();
    if (!decl)
        return;
    unsigned size = decl->length();
    for (unsigned i = 0; i < size; ++i) {
        String name = decl->item(i);
        m_variablesMap.set(name, variables);
    }
}

CSSValue* CSSStyleSelector::resolveVariableDependentValue(CSSVariableDependentValue*)
{
    return 0;
}

// -----------------------------------------------------------------

CSSRuleSet::CSSRuleSet()
{
    m_universalRules = 0;
    m_ruleCount = 0;
}

CSSRuleSet::~CSSRuleSet()
{ 
    deleteAllValues(m_idRules);
    deleteAllValues(m_classRules);
    deleteAllValues(m_tagRules);

    delete m_universalRules; 
}


void CSSRuleSet::addToRuleSet(AtomicStringImpl* key, AtomRuleMap& map,
                              CSSStyleRule* rule, CSSSelector* sel)
{
    if (!key) return;
    CSSRuleDataList* rules = map.get(key);
    if (!rules) {
        rules = new CSSRuleDataList(m_ruleCount++, rule, sel);
        map.set(key, rules);
    } else
        rules->append(m_ruleCount++, rule, sel);
}

void CSSRuleSet::addRule(CSSStyleRule* rule, CSSSelector* sel)
{
    if (sel->m_match == CSSSelector::Id) {
        addToRuleSet(sel->m_value.impl(), m_idRules, rule, sel);
        return;
    }
    if (sel->m_match == CSSSelector::Class) {
        addToRuleSet(sel->m_value.impl(), m_classRules, rule, sel);
        return;
    }
     
    const AtomicString& localName = sel->m_tag.localName();
    if (localName != starAtom) {
        addToRuleSet(localName.impl(), m_tagRules, rule, sel);
        return;
    }
    
    // Just put it in the universal rule set.
    if (!m_universalRules)
        m_universalRules = new CSSRuleDataList(m_ruleCount++, rule, sel);
    else
        m_universalRules->append(m_ruleCount++, rule, sel);
}

void CSSRuleSet::addRulesFromSheet(CSSStyleSheet* sheet, const MediaQueryEvaluator& medium, CSSStyleSelector* styleSelector)
{
    if (!sheet)
        return;

    // No media implies "all", but if a media list exists it must
    // contain our current medium
    if (sheet->media() && !medium.eval(sheet->media(), styleSelector))
        return; // the style sheet doesn't apply

    int len = sheet->length();

    for (int i = 0; i < len; i++) {
        StyleBase* item = sheet->item(i);
        if (item->isStyleRule()) {
            CSSStyleRule* rule = static_cast<CSSStyleRule*>(item);
            for (CSSSelector* s = rule->selectorList().first(); s; s = CSSSelectorList::next(s))
                addRule(rule, s);
        }
        else if (item->isImportRule()) {
            CSSImportRule* import = static_cast<CSSImportRule*>(item);
            if (!import->media() || medium.eval(import->media(), styleSelector))
                addRulesFromSheet(import->styleSheet(), medium, styleSelector);
        }
        else if (item->isMediaRule()) {
            CSSMediaRule* r = static_cast<CSSMediaRule*>(item);
            CSSRuleList* rules = r->cssRules();

            if ((!r->media() || medium.eval(r->media(), styleSelector)) && rules) {
                // Traverse child elements of the @media rule.
                for (unsigned j = 0; j < rules->length(); j++) {
                    CSSRule *childItem = rules->item(j);
                    if (childItem->isStyleRule()) {
                        // It is a StyleRule, so append it to our list
                        CSSStyleRule* rule = static_cast<CSSStyleRule*>(childItem);
                        for (CSSSelector* s = rule->selectorList().first(); s; s = CSSSelectorList::next(s))
                            addRule(rule, s);
                    } else if (childItem->isFontFaceRule() && styleSelector) {
                        // Add this font face to our set.
                        const CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(childItem);
                        styleSelector->fontSelector()->addFontFaceRule(fontFaceRule);
                    } else if (childItem->isKeyframesRule() && styleSelector) {
                        // Add this keyframe rule to our set.
                        styleSelector->addKeyframeStyle(static_cast<WebKitCSSKeyframesRule*>(childItem));
                    }
                }   // for rules
            }   // if rules
        } else if (item->isFontFaceRule() && styleSelector) {
            // Add this font face to our set.
            const CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(item);
            styleSelector->fontSelector()->addFontFaceRule(fontFaceRule);
        } else if (item->isVariablesRule()) {
            // Evaluate the media query and make sure it matches.
            CSSVariablesRule* variables = static_cast<CSSVariablesRule*>(item);
            if (!variables->media() || medium.eval(variables->media(), styleSelector))
                styleSelector->addVariables(variables);
        } else if (item->isKeyframesRule())
            styleSelector->addKeyframeStyle(static_cast<WebKitCSSKeyframesRule*>(item));
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

static Length convertToLength(CSSPrimitiveValue* primitiveValue, RenderStyle* style, RenderStyle* rootStyle, double multiplier = 1, bool *ok = 0)
{
    // This function is tolerant of a null style value. The only place style is used is in
    // length measurements, like 'ems' and 'px'. And in those cases style is only used
    // when the units are EMS or EXS. So we will just fail in those cases.
    Length l;
    if (!primitiveValue) {
        if (ok)
            *ok = false;
    } else {
        int type = primitiveValue->primitiveType();
        
        if (!style && (type == CSSPrimitiveValue::CSS_EMS || type == CSSPrimitiveValue::CSS_EXS || type == CSSPrimitiveValue::CSS_REMS)) {
            if (ok)
                *ok = false;
        } else if (CSSPrimitiveValue::isUnitTypeLength(type))
            l = Length(primitiveValue->computeLengthIntForLength(style, rootStyle, multiplier), Fixed);
        else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else if (type == CSSPrimitiveValue::CSS_NUMBER)
            l = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
        else if (ok)
            *ok = false;
    }
    return l;
}

void CSSStyleSelector::applyDeclarations(bool applyFirst, bool isImportant,
                                         int startIndex, int endIndex)
{
    if (startIndex == -1)
        return;

    for (int i = startIndex; i <= endIndex; i++) {
        CSSMutableStyleDeclaration* decl = m_matchedDecls[i];
        CSSMutableStyleDeclaration::const_iterator end = decl->end();
        for (CSSMutableStyleDeclaration::const_iterator it = decl->begin(); it != end; ++it) {
            const CSSProperty& current = *it;
            // give special priority to font-xxx, color properties
            if (isImportant == current.isImportant()) {
                bool first;
                switch (current.id()) {
                    case CSSPropertyLineHeight:
                        m_lineHeightValue = current.value();
                        first = !applyFirst; // we apply line-height later
                        break;
                    case CSSPropertyColor:
                    case CSSPropertyDirection:
                    case CSSPropertyDisplay:
                    case CSSPropertyFont:
                    case CSSPropertyFontSize:
                    case CSSPropertyFontStyle:
                    case CSSPropertyFontFamily:
                    case CSSPropertyFontWeight:
                    case CSSPropertyWebkitTextSizeAdjust:
                    case CSSPropertyFontVariant:
                    case CSSPropertyZoom:
                        // these have to be applied first, because other properties use the computed
                        // values of these porperties.
                        first = true;
                        break;
                    default:
                        first = false;
                        break;
                }
                if (first == applyFirst)
                    applyProperty(current.id(), current.value());
            }
        }
    }
}

static void applyCounterList(RenderStyle* style, CSSValueList* list, bool isReset)
{
    CounterDirectiveMap& map = style->accessCounterDirectives();
    typedef CounterDirectiveMap::iterator Iterator;

    Iterator end = map.end();
    for (Iterator it = map.begin(); it != end; ++it)
        if (isReset)
            it->second.m_reset = false;
        else
            it->second.m_increment = false;

    int length = list ? list->length() : 0;
    for (int i = 0; i < length; ++i) {
        Pair* pair = static_cast<CSSPrimitiveValue*>(list->itemWithoutBoundsCheck(i))->getPairValue();
        AtomicString identifier = static_cast<CSSPrimitiveValue*>(pair->first())->getStringValue();
        // FIXME: What about overflow?
        int value = static_cast<CSSPrimitiveValue*>(pair->second())->getIntValue();
        CounterDirectives& directives = map.add(identifier.impl(), CounterDirectives()).first->second;
        if (isReset) {
            directives.m_reset = true;
            directives.m_resetValue = value;
        } else {
            if (directives.m_increment)
                directives.m_incrementValue += value;
            else {
                directives.m_increment = true;
                directives.m_incrementValue = value;
            }
        }
    }
}

void CSSStyleSelector::applyPropertyToStyle(int id, CSSValue *value, RenderStyle* style)
{
    initElementAndPseudoState(0);
    initForStyleResolve(0, style);
    m_style = style;
    applyProperty(id, value);
}

void CSSStyleSelector::applyProperty(int id, CSSValue *value)
{
    CSSPrimitiveValue* primitiveValue = 0;
    if (value->isPrimitiveValue())
        primitiveValue = static_cast<CSSPrimitiveValue*>(value);

    float zoomFactor = m_style->effectiveZoom();

    Length l;
    bool apply = false;

    unsigned short valueType = value->cssValueType();

    bool isInherit = m_parentNode && valueType == CSSValue::CSS_INHERIT;
    bool isInitial = valueType == CSSValue::CSS_INITIAL || (!m_parentNode && valueType == CSSValue::CSS_INHERIT);
    
    // These properties are used to set the correct margins/padding on RTL lists.
    if (id == CSSPropertyWebkitMarginStart)
        id = m_style->direction() == LTR ? CSSPropertyMarginLeft : CSSPropertyMarginRight;
    else if (id == CSSPropertyWebkitPaddingStart)
        id = m_style->direction() == LTR ? CSSPropertyPaddingLeft : CSSPropertyPaddingRight;

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.  Shorthands (e.g. border, background) occur in this list as well and
    // are only hit when mapping "inherit" or "initial" into front-end values.
    switch (static_cast<CSSPropertyID>(id)) {
// ident only properties
    case CSSPropertyBackgroundAttachment:
        HANDLE_BACKGROUND_VALUE(attachment, Attachment, value)
        return;
    case CSSPropertyBackgroundClip:
    case CSSPropertyWebkitBackgroundClip:
        HANDLE_BACKGROUND_VALUE(clip, Clip, value)
        return;
    case CSSPropertyWebkitBackgroundComposite:
        HANDLE_BACKGROUND_VALUE(composite, Composite, value)
        return;
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundOrigin:
        HANDLE_BACKGROUND_VALUE(origin, Origin, value)
        return;
    case CSSPropertyBackgroundSize:
        HANDLE_BACKGROUND_VALUE(size, Size, value)
        return;
    case CSSPropertyWebkitMaskAttachment:
        HANDLE_MASK_VALUE(attachment, Attachment, value)
        return;
    case CSSPropertyWebkitMaskClip:
        HANDLE_MASK_VALUE(clip, Clip, value)
        return;
    case CSSPropertyWebkitMaskComposite:
        HANDLE_MASK_VALUE(composite, Composite, value)
        return;
    case CSSPropertyWebkitMaskOrigin:
        HANDLE_MASK_VALUE(origin, Origin, value)
        return;
    case CSSPropertyWebkitMaskSize:
        HANDLE_MASK_VALUE(size, Size, value)
        return;
    case CSSPropertyBorderCollapse:
        HANDLE_INHERIT_AND_INITIAL(borderCollapse, BorderCollapse)
        if (!primitiveValue)
            return;
        switch (primitiveValue->getIdent()) {
            case CSSValueCollapse:
                m_style->setBorderCollapse(true);
                break;
            case CSSValueSeparate:
                m_style->setBorderCollapse(false);
                break;
            default:
                return;
        }
        return;
    case CSSPropertyBorderTopStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(borderTopStyle, BorderTopStyle, BorderStyle)
        return;
    case CSSPropertyBorderRightStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(borderRightStyle, BorderRightStyle, BorderStyle)
        return;
    case CSSPropertyBorderBottomStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(borderBottomStyle, BorderBottomStyle, BorderStyle)
        return;
    case CSSPropertyBorderLeftStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(borderLeftStyle, BorderLeftStyle, BorderStyle)
        return;
    case CSSPropertyOutlineStyle:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(outlineStyle, OutlineStyle, BorderStyle)
        if (primitiveValue) {
            if (primitiveValue->getIdent() == CSSValueAuto)
                m_style->setOutlineStyle(DOTTED, true);
            else
                m_style->setOutlineStyle(*primitiveValue);
        }
        return;
    case CSSPropertyCaptionSide:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(captionSide, CaptionSide)
        return;
    case CSSPropertyClear:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(clear, Clear)
        return;
    case CSSPropertyDirection:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(direction, Direction)
        return;
    case CSSPropertyDisplay:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(display, Display)
#if ENABLE(WCSS)
        if (primitiveValue) {
            if (primitiveValue->getIdent() == CSSValueWapMarquee) {
                // Initialize Wap Marquee style
                m_style->setOverflowX(OMARQUEE);
                m_style->setOverflowY(OMARQUEE);
                m_style->setWhiteSpace(NOWRAP);
                m_style->setMarqueeDirection(MLEFT);
                m_style->setMarqueeSpeed(85); // Normal speed
                m_style->setMarqueeLoopCount(1);
                m_style->setMarqueeBehavior(MSCROLL);

                if (m_parentStyle)
                    m_style->setDisplay(m_parentStyle->display());
                else
                    m_style->setDisplay(*primitiveValue);
            } else
                m_style->setDisplay(*primitiveValue);
        }
#endif
        return;
    case CSSPropertyEmptyCells:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(emptyCells, EmptyCells)
        return;
    case CSSPropertyFloat:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(floating, Floating)
        return;
    case CSSPropertyFontStyle:
    {
        FontDescription fontDescription = m_style->fontDescription();
        if (isInherit)
            fontDescription.setItalic(m_parentStyle->fontDescription().italic());
        else if (isInitial)
            fontDescription.setItalic(false);
        else {
            if (!primitiveValue)
                return;
            switch (primitiveValue->getIdent()) {
                case CSSValueOblique:
                // FIXME: oblique is the same as italic for the moment...
                case CSSValueItalic:
                    fontDescription.setItalic(true);
                    break;
                case CSSValueNormal:
                    fontDescription.setItalic(false);
                    break;
                default:
                    return;
            }
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }

    case CSSPropertyFontVariant:
    {
        FontDescription fontDescription = m_style->fontDescription();
        if (isInherit) 
            fontDescription.setSmallCaps(m_parentStyle->fontDescription().smallCaps());
        else if (isInitial)
            fontDescription.setSmallCaps(false);
        else {
            if (!primitiveValue)
                return;
            int id = primitiveValue->getIdent();
            if (id == CSSValueNormal)
                fontDescription.setSmallCaps(false);
            else if (id == CSSValueSmallCaps)
                fontDescription.setSmallCaps(true);
            else
                return;
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }

    case CSSPropertyFontWeight:
    {
        FontDescription fontDescription = m_style->fontDescription();
        if (isInherit)
            fontDescription.setWeight(m_parentStyle->fontDescription().weight());
        else if (isInitial)
            fontDescription.setWeight(FontWeightNormal);
        else {
            if (!primitiveValue)
                return;
            if (primitiveValue->getIdent()) {
                switch (primitiveValue->getIdent()) {
                    case CSSValueBolder:
                        fontDescription.setWeight(fontDescription.bolderWeight());
                        break;
                    case CSSValueLighter:
                        fontDescription.setWeight(fontDescription.lighterWeight());
                        break;
                    case CSSValueBold:
                    case CSSValue700:
                        fontDescription.setWeight(FontWeightBold);
                        break;
                    case CSSValueNormal:
                    case CSSValue400:
                        fontDescription.setWeight(FontWeightNormal);
                        break;
                    case CSSValue900:
                        fontDescription.setWeight(FontWeight900);
                        break;
                    case CSSValue800:
                        fontDescription.setWeight(FontWeight800);
                        break;
                    case CSSValue600:
                        fontDescription.setWeight(FontWeight600);
                        break;
                    case CSSValue500:
                        fontDescription.setWeight(FontWeight500);
                        break;
                    case CSSValue300:
                        fontDescription.setWeight(FontWeight300);
                        break;
                    case CSSValue200:
                        fontDescription.setWeight(FontWeight200);
                        break;
                    case CSSValue100:
                        fontDescription.setWeight(FontWeight100);
                        break;
                    default:
                        return;
                }
            } else
                ASSERT_NOT_REACHED();
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }
        
    case CSSPropertyListStylePosition:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(listStylePosition, ListStylePosition)
        return;
    case CSSPropertyListStyleType:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(listStyleType, ListStyleType)
        return;
    case CSSPropertyOverflow:
    {
        if (isInherit) {
            m_style->setOverflowX(m_parentStyle->overflowX());
            m_style->setOverflowY(m_parentStyle->overflowY());
            return;
        }
        
        if (isInitial) {
            m_style->setOverflowX(RenderStyle::initialOverflowX());
            m_style->setOverflowY(RenderStyle::initialOverflowY());
            return;
        }
            
        EOverflow o = *primitiveValue;

        m_style->setOverflowX(o);
        m_style->setOverflowY(o);
        return;
    }

    case CSSPropertyOverflowX:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(overflowX, OverflowX)
        return;
    case CSSPropertyOverflowY:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(overflowY, OverflowY)
        return;
    case CSSPropertyPageBreakBefore:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(pageBreakBefore, PageBreakBefore, PageBreak)
        return;
    case CSSPropertyPageBreakAfter:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(pageBreakAfter, PageBreakAfter, PageBreak)
        return;
    case CSSPropertyPageBreakInside: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakInside, PageBreakInside, PageBreak)
        if (!primitiveValue)
            return;
        EPageBreak pageBreak = *primitiveValue;
        if (pageBreak != PBALWAYS)
            m_style->setPageBreakInside(pageBreak);
        return;
    }
        
    case CSSPropertyPosition:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(position, Position)
        return;
    case CSSPropertyTableLayout: {
        HANDLE_INHERIT_AND_INITIAL(tableLayout, TableLayout)

        ETableLayout l = *primitiveValue;
        if (l == TAUTO)
            l = RenderStyle::initialTableLayout();

        m_style->setTableLayout(l);
        return;
    }
        
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

    case CSSPropertyBackgroundPosition:
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(xPosition, XPosition);
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(yPosition, YPosition);
        return;
    case CSSPropertyBackgroundPositionX: {
        HANDLE_BACKGROUND_VALUE(xPosition, XPosition, value)
        return;
    }
    case CSSPropertyBackgroundPositionY: {
        HANDLE_BACKGROUND_VALUE(yPosition, YPosition, value)
        return;
    }
    case CSSPropertyWebkitMaskPosition:
        HANDLE_MASK_INHERIT_AND_INITIAL(xPosition, XPosition);
        HANDLE_MASK_INHERIT_AND_INITIAL(yPosition, YPosition);
        return;
    case CSSPropertyWebkitMaskPositionX: {
        HANDLE_MASK_VALUE(xPosition, XPosition, value)
        return;
    }
    case CSSPropertyWebkitMaskPositionY: {
        HANDLE_MASK_VALUE(yPosition, YPosition, value)
        return;
    }
    case CSSPropertyBackgroundRepeat:
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(repeatX, RepeatX);
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(repeatY, RepeatY);
        return;
    case CSSPropertyBackgroundRepeatX:
        HANDLE_BACKGROUND_VALUE(repeatX, RepeatX, value)
        return;
    case CSSPropertyBackgroundRepeatY:
        HANDLE_BACKGROUND_VALUE(repeatY, RepeatY, value)
        return;
    case CSSPropertyWebkitMaskRepeat:
        HANDLE_MASK_INHERIT_AND_INITIAL(repeatX, RepeatX);
        HANDLE_MASK_INHERIT_AND_INITIAL(repeatY, RepeatY);
        return;
    case CSSPropertyWebkitMaskRepeatX:
        HANDLE_MASK_VALUE(repeatX, RepeatX, value)
        return;
    case CSSPropertyWebkitMaskRepeatY:
        HANDLE_MASK_VALUE(repeatY, RepeatY, value)
        return;
    case CSSPropertyBorderSpacing: {
        if (isInherit) {
            m_style->setHorizontalBorderSpacing(m_parentStyle->horizontalBorderSpacing());
            m_style->setVerticalBorderSpacing(m_parentStyle->verticalBorderSpacing());
        }
        else if (isInitial) {
            m_style->setHorizontalBorderSpacing(0);
            m_style->setVerticalBorderSpacing(0);
        }
        return;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing: {
        HANDLE_INHERIT_AND_INITIAL(horizontalBorderSpacing, HorizontalBorderSpacing)
        if (!primitiveValue)
            return;
        short spacing = primitiveValue->computeLengthShort(style(), m_rootElementStyle, zoomFactor);
        m_style->setHorizontalBorderSpacing(spacing);
        return;
    }
    case CSSPropertyWebkitBorderVerticalSpacing: {
        HANDLE_INHERIT_AND_INITIAL(verticalBorderSpacing, VerticalBorderSpacing)
        if (!primitiveValue)
            return;
        short spacing = primitiveValue->computeLengthShort(style(), m_rootElementStyle, zoomFactor);
        m_style->setVerticalBorderSpacing(spacing);
        return;
    }
    case CSSPropertyCursor:
        if (isInherit) {
            m_style->setCursor(m_parentStyle->cursor());
            m_style->setCursorList(m_parentStyle->cursors());
            return;
        }
        m_style->clearCursorList();
        if (isInitial) {
            m_style->setCursor(RenderStyle::initialCursor());
            return;
        }
        if (value->isValueList()) {
            CSSValueList* list = static_cast<CSSValueList*>(value);
            int len = list->length();
            m_style->setCursor(CURSOR_AUTO);
            for (int i = 0; i < len; i++) {
                CSSValue* item = list->itemWithoutBoundsCheck(i);
                if (!item->isPrimitiveValue())
                    continue;
                primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                int type = primitiveValue->primitiveType();
                if (type == CSSPrimitiveValue::CSS_URI) {
                    CSSCursorImageValue* image = static_cast<CSSCursorImageValue*>(primitiveValue);
                    if (image->updateIfSVGCursorIsUsed(m_element)) // Elements with SVG cursors are not allowed to share style.
                        m_style->setUnique();
                    // FIXME: Temporary clumsiness to pass off a CachedImage to an API that will eventually convert to using
                    // StyleImage.
                    RefPtr<StyleCachedImage> styleCachedImage(image->cachedImage(m_element->document()->docLoader()));
                    if (styleCachedImage)
                        m_style->addCursor(styleCachedImage->cachedImage(), image->hotspot());
                } else if (type == CSSPrimitiveValue::CSS_IDENT)
                    m_style->setCursor(*primitiveValue);
            }
        } else if (primitiveValue) {
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_IDENT && m_style->cursor() != ECursor(*primitiveValue))
                m_style->setCursor(*primitiveValue);
        }
        return;
// colors || inherit
    case CSSPropertyColor:
        // If the 'currentColor' keyword is set on the 'color' property itself,
        // it is treated as 'color:inherit' at parse time
        if (primitiveValue && primitiveValue->getIdent() == CSSValueCurrentcolor)
            isInherit = true;
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTextFillColor: {
        Color col;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyBackgroundColor, backgroundColor, BackgroundColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyBorderTopColor, borderTopColor, color, BorderTopColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyBorderBottomColor, borderBottomColor, color, BorderBottomColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyBorderRightColor, borderRightColor, color, BorderRightColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyBorderLeftColor, borderLeftColor, color, BorderLeftColor)
            HANDLE_INHERIT_COND(CSSPropertyColor, color, Color)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyOutlineColor, outlineColor, color, OutlineColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyWebkitColumnRuleColor, columnRuleColor, color, ColumnRuleColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyWebkitTextStrokeColor, textStrokeColor, color, TextStrokeColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyWebkitTextFillColor, textFillColor, color, TextFillColor)
            return;
        }
        if (isInitial) {
            // The border/outline colors will just map to the invalid color |col| above.  This will have the
            // effect of forcing the use of the currentColor when it comes time to draw the borders (and of
            // not painting the background since the color won't be valid).
            if (id == CSSPropertyColor)
                col = RenderStyle::initialColor();
        } else {
            if (!primitiveValue)
                return;
            col = getColorFromPrimitiveValue(primitiveValue);
        }

        switch (id) {
        case CSSPropertyBackgroundColor:
            m_style->setBackgroundColor(col);
            break;
        case CSSPropertyBorderTopColor:
            m_style->setBorderTopColor(col);
            break;
        case CSSPropertyBorderRightColor:
            m_style->setBorderRightColor(col);
            break;
        case CSSPropertyBorderBottomColor:
            m_style->setBorderBottomColor(col);
            break;
        case CSSPropertyBorderLeftColor:
            m_style->setBorderLeftColor(col);
            break;
        case CSSPropertyColor:
            m_style->setColor(col);
            break;
        case CSSPropertyOutlineColor:
            m_style->setOutlineColor(col);
            break;
        case CSSPropertyWebkitColumnRuleColor:
            m_style->setColumnRuleColor(col);
            break;
        case CSSPropertyWebkitTextStrokeColor:
            m_style->setTextStrokeColor(col);
            break;
        case CSSPropertyWebkitTextFillColor:
            m_style->setTextFillColor(col);
            break;
        }
        
        return;
    }
    
// uri || inherit
    case CSSPropertyBackgroundImage:
        HANDLE_BACKGROUND_VALUE(image, Image, value)
        return;
    case CSSPropertyWebkitMaskImage:
        HANDLE_MASK_VALUE(image, Image, value)
        return;
    case CSSPropertyListStyleImage:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleImage, ListStyleImage)
        m_style->setListStyleImage(styleImage(value));
        return;
    }

// length
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyOutlineWidth:
    case CSSPropertyWebkitColumnRuleWidth:
    {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyBorderTopWidth, borderTopWidth, BorderTopWidth)
            HANDLE_INHERIT_COND(CSSPropertyBorderRightWidth, borderRightWidth, BorderRightWidth)
            HANDLE_INHERIT_COND(CSSPropertyBorderBottomWidth, borderBottomWidth, BorderBottomWidth)
            HANDLE_INHERIT_COND(CSSPropertyBorderLeftWidth, borderLeftWidth, BorderLeftWidth)
            HANDLE_INHERIT_COND(CSSPropertyOutlineWidth, outlineWidth, OutlineWidth)
            HANDLE_INHERIT_COND(CSSPropertyWebkitColumnRuleWidth, columnRuleWidth, ColumnRuleWidth)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderTopWidth, BorderTopWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderRightWidth, BorderRightWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderBottomWidth, BorderBottomWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderLeftWidth, BorderLeftWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyOutlineWidth, OutlineWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitColumnRuleWidth, ColumnRuleWidth, BorderWidth)
            return;
        }

        if (!primitiveValue)
            return;
        short width = 3;
        switch (primitiveValue->getIdent()) {
        case CSSValueThin:
            width = 1;
            break;
        case CSSValueMedium:
            width = 3;
            break;
        case CSSValueThick:
            width = 5;
            break;
        case CSSValueInvalid:
            width = primitiveValue->computeLengthShort(style(), m_rootElementStyle, zoomFactor);
            break;
        default:
            return;
        }

        if (width < 0) return;
        switch (id) {
        case CSSPropertyBorderTopWidth:
            m_style->setBorderTopWidth(width);
            break;
        case CSSPropertyBorderRightWidth:
            m_style->setBorderRightWidth(width);
            break;
        case CSSPropertyBorderBottomWidth:
            m_style->setBorderBottomWidth(width);
            break;
        case CSSPropertyBorderLeftWidth:
            m_style->setBorderLeftWidth(width);
            break;
        case CSSPropertyOutlineWidth:
            m_style->setOutlineWidth(width);
            break;
        case CSSPropertyWebkitColumnRuleWidth:
            m_style->setColumnRuleWidth(width);
            break;
        default:
            return;
        }
        return;
    }

    case CSSPropertyWebkitFontSmoothing: {
        FontDescription fontDescription = m_style->fontDescription();
        if (isInherit) 
            fontDescription.setFontSmoothing(m_parentStyle->fontDescription().fontSmoothing());
        else if (isInitial)
            fontDescription.setFontSmoothing(AutoSmoothing);
        else {
            if (!primitiveValue)
                return;
            int id = primitiveValue->getIdent();
            FontSmoothingMode smoothing;
            switch (id) {
                case CSSValueAuto:
                    smoothing = AutoSmoothing;
                    break;
                case CSSValueNone:
                    smoothing = NoSmoothing;
                    break;
                case CSSValueAntialiased:
                    smoothing = Antialiased;
                    break;
                case CSSValueSubpixelAntialiased:
                    smoothing = SubpixelAntialiased;
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    smoothing = AutoSmoothing;
            }
            fontDescription.setFontSmoothing(smoothing);
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }

    case CSSPropertyLetterSpacing:
    case CSSPropertyWordSpacing:
    {
        
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyLetterSpacing, letterSpacing, LetterSpacing)
            HANDLE_INHERIT_COND(CSSPropertyWordSpacing, wordSpacing, WordSpacing)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyLetterSpacing, LetterSpacing, LetterWordSpacing)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWordSpacing, WordSpacing, LetterWordSpacing)
            return;
        }
        
        int width = 0;
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNormal) {
            width = 0;
        } else {
            if (!primitiveValue)
                return;
            width = primitiveValue->computeLengthInt(style(), m_rootElementStyle, zoomFactor);
        }
        switch (id) {
        case CSSPropertyLetterSpacing:
            m_style->setLetterSpacing(width);
            break;
        case CSSPropertyWordSpacing:
            m_style->setWordSpacing(width);
            break;
            // ### needs the definitions in renderstyle
        default: break;
        }
        return;
    }

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

    case CSSPropertyResize:
    {
        HANDLE_INHERIT_AND_INITIAL(resize, Resize)

        if (!primitiveValue->getIdent())
            return;

        EResize r = RESIZE_NONE;
        if (primitiveValue->getIdent() == CSSValueAuto) {
            if (Settings* settings = m_checker.m_document->settings())
                r = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
        } else
            r = *primitiveValue;
            
        m_style->setResize(r);
        return;
    }
    
    // length, percent
    case CSSPropertyMaxWidth:
        // +none +inherit
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone)
            apply = true;
    case CSSPropertyTop:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyBottom:
    case CSSPropertyWidth:
    case CSSPropertyMinWidth:
    case CSSPropertyMarginTop:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
        // +inherit +auto
        if (id == CSSPropertyWidth || id == CSSPropertyMinWidth || id == CSSPropertyMaxWidth) {
            if (primitiveValue && primitiveValue->getIdent() == CSSValueIntrinsic) {
                l = Length(Intrinsic);
                apply = true;
            }
            else if (primitiveValue && primitiveValue->getIdent() == CSSValueMinIntrinsic) {
                l = Length(MinIntrinsic);
                apply = true;
            }
        }
        if (id != CSSPropertyMaxWidth && primitiveValue && primitiveValue->getIdent() == CSSValueAuto)
            apply = true;
    case CSSPropertyPaddingTop:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyTextIndent:
        // +inherit
    {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyMaxWidth, maxWidth, MaxWidth)
            HANDLE_INHERIT_COND(CSSPropertyBottom, bottom, Bottom)
            HANDLE_INHERIT_COND(CSSPropertyTop, top, Top)
            HANDLE_INHERIT_COND(CSSPropertyLeft, left, Left)
            HANDLE_INHERIT_COND(CSSPropertyRight, right, Right)
            HANDLE_INHERIT_COND(CSSPropertyWidth, width, Width)
            HANDLE_INHERIT_COND(CSSPropertyMinWidth, minWidth, MinWidth)
            HANDLE_INHERIT_COND(CSSPropertyPaddingTop, paddingTop, PaddingTop)
            HANDLE_INHERIT_COND(CSSPropertyPaddingRight, paddingRight, PaddingRight)
            HANDLE_INHERIT_COND(CSSPropertyPaddingBottom, paddingBottom, PaddingBottom)
            HANDLE_INHERIT_COND(CSSPropertyPaddingLeft, paddingLeft, PaddingLeft)
            HANDLE_INHERIT_COND(CSSPropertyMarginTop, marginTop, MarginTop)
            HANDLE_INHERIT_COND(CSSPropertyMarginRight, marginRight, MarginRight)
            HANDLE_INHERIT_COND(CSSPropertyMarginBottom, marginBottom, MarginBottom)
            HANDLE_INHERIT_COND(CSSPropertyMarginLeft, marginLeft, MarginLeft)
            HANDLE_INHERIT_COND(CSSPropertyTextIndent, textIndent, TextIndent)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMaxWidth, MaxWidth, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBottom, Bottom, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyTop, Top, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyLeft, Left, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyRight, Right, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWidth, Width, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMinWidth, MinWidth, MinSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyPaddingTop, PaddingTop, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyPaddingRight, PaddingRight, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyPaddingBottom, PaddingBottom, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyPaddingLeft, PaddingLeft, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMarginTop, MarginTop, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMarginRight, MarginRight, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMarginBottom, MarginBottom, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMarginLeft, MarginLeft, Margin)
            HANDLE_INITIAL_COND(CSSPropertyTextIndent, TextIndent)
            return;
        } 

        if (primitiveValue && !apply) {
            int type = primitiveValue->primitiveType();
            if (CSSPrimitiveValue::isUnitTypeLength(type))
                // Handle our quirky margin units if we have them.
                l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed, 
                           primitiveValue->isQuirkValue());
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length(primitiveValue->getDoubleValue(), Percent);
            else
                return;
            if (id == CSSPropertyPaddingLeft || id == CSSPropertyPaddingRight ||
                id == CSSPropertyPaddingTop || id == CSSPropertyPaddingBottom)
                // Padding can't be negative
                apply = !((l.isFixed() || l.isPercent()) && l.calcValue(100) < 0);
            else
                apply = true;
        }
        if (!apply) return;
        switch (id) {
            case CSSPropertyMaxWidth:
                m_style->setMaxWidth(l);
                break;
            case CSSPropertyBottom:
                m_style->setBottom(l);
                break;
            case CSSPropertyTop:
                m_style->setTop(l);
                break;
            case CSSPropertyLeft:
                m_style->setLeft(l);
                break;
            case CSSPropertyRight:
                m_style->setRight(l);
                break;
            case CSSPropertyWidth:
                m_style->setWidth(l);
                break;
            case CSSPropertyMinWidth:
                m_style->setMinWidth(l);
                break;
            case CSSPropertyPaddingTop:
                m_style->setPaddingTop(l);
                break;
            case CSSPropertyPaddingRight:
                m_style->setPaddingRight(l);
                break;
            case CSSPropertyPaddingBottom:
                m_style->setPaddingBottom(l);
                break;
            case CSSPropertyPaddingLeft:
                m_style->setPaddingLeft(l);
                break;
            case CSSPropertyMarginTop:
                m_style->setMarginTop(l);
                break;
            case CSSPropertyMarginRight:
                m_style->setMarginRight(l);
                break;
            case CSSPropertyMarginBottom:
                m_style->setMarginBottom(l);
                break;
            case CSSPropertyMarginLeft:
                m_style->setMarginLeft(l);
                break;
            case CSSPropertyTextIndent:
                m_style->setTextIndent(l);
                break;
            default:
                break;
            }
        return;
    }

    case CSSPropertyMaxHeight:
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone) {
            l = Length(undefinedLength, Fixed);
            apply = true;
        }
    case CSSPropertyHeight:
    case CSSPropertyMinHeight:
        if (primitiveValue && primitiveValue->getIdent() == CSSValueIntrinsic) {
            l = Length(Intrinsic);
            apply = true;
        } else if (primitiveValue && primitiveValue->getIdent() == CSSValueMinIntrinsic) {
            l = Length(MinIntrinsic);
            apply = true;
        } else if (id != CSSPropertyMaxHeight && primitiveValue && primitiveValue->getIdent() == CSSValueAuto)
            apply = true;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyMaxHeight, maxHeight, MaxHeight)
            HANDLE_INHERIT_COND(CSSPropertyHeight, height, Height)
            HANDLE_INHERIT_COND(CSSPropertyMinHeight, minHeight, MinHeight)
            return;
        }
        if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMaxHeight, MaxHeight, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyHeight, Height, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMinHeight, MinHeight, MinSize)
            return;
        }

        if (primitiveValue && !apply) {
            unsigned short type = primitiveValue->primitiveType();
            if (CSSPrimitiveValue::isUnitTypeLength(type))
                l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length(primitiveValue->getDoubleValue(), Percent);
            else
                return;
            apply = true;
        }
        if (apply)
            switch (id) {
                case CSSPropertyMaxHeight:
                    m_style->setMaxHeight(l);
                    break;
                case CSSPropertyHeight:
                    m_style->setHeight(l);
                    break;
                case CSSPropertyMinHeight:
                    m_style->setMinHeight(l);
                    break;
            }
        return;

    case CSSPropertyVerticalAlign:
        HANDLE_INHERIT_AND_INITIAL(verticalAlign, VerticalAlign)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
          EVerticalAlign align;

          switch (primitiveValue->getIdent()) {
                case CSSValueTop:
                    align = TOP; break;
                case CSSValueBottom:
                    align = BOTTOM; break;
                case CSSValueMiddle:
                    align = MIDDLE; break;
                case CSSValueBaseline:
                    align = BASELINE; break;
                case CSSValueTextBottom:
                    align = TEXT_BOTTOM; break;
                case CSSValueTextTop:
                    align = TEXT_TOP; break;
                case CSSValueSub:
                    align = SUB; break;
                case CSSValueSuper:
                    align = SUPER; break;
                case CSSValueWebkitBaselineMiddle:
                    align = BASELINE_MIDDLE; break;
                default:
                    return;
            }
          m_style->setVerticalAlign(align);
          return;
        } else {
          int type = primitiveValue->primitiveType();
          Length l;
          if (CSSPrimitiveValue::isUnitTypeLength(type))
            l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
          else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);

          m_style->setVerticalAlign(LENGTH);
          m_style->setVerticalAlignLength(l);
        }
        return;

    case CSSPropertyFontSize:
    {
        FontDescription fontDescription = m_style->fontDescription();
        fontDescription.setKeywordSize(0);
        float oldSize = 0;
        float size = 0;
        
        bool parentIsAbsoluteSize = false;
        if (m_parentNode) {
            oldSize = m_parentStyle->fontDescription().specifiedSize();
            parentIsAbsoluteSize = m_parentStyle->fontDescription().isAbsoluteSize();
        }

        if (isInherit) {
            size = oldSize;
            if (m_parentNode)
                fontDescription.setKeywordSize(m_parentStyle->fontDescription().keywordSize());
        } else if (isInitial) {
            size = fontSizeForKeyword(CSSValueMedium, m_style->htmlHacks(), fontDescription.useFixedDefaultSize());
            fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        } else if (primitiveValue->getIdent()) {
            // Keywords are being used.
            switch (primitiveValue->getIdent()) {
                case CSSValueXxSmall:
                case CSSValueXSmall:
                case CSSValueSmall:
                case CSSValueMedium:
                case CSSValueLarge:
                case CSSValueXLarge:
                case CSSValueXxLarge:
                case CSSValueWebkitXxxLarge:
                    size = fontSizeForKeyword(primitiveValue->getIdent(), m_style->htmlHacks(), fontDescription.useFixedDefaultSize());
                    fontDescription.setKeywordSize(primitiveValue->getIdent() - CSSValueXxSmall + 1);
                    break;
                case CSSValueLarger:
                    size = largerFontSize(oldSize, m_style->htmlHacks());
                    break;
                case CSSValueSmaller:
                    size = smallerFontSize(oldSize, m_style->htmlHacks());
                    break;
                default:
                    return;
            }

            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize && 
                                              (primitiveValue->getIdent() == CSSValueLarger ||
                                               primitiveValue->getIdent() == CSSValueSmaller));
        } else {
            int type = primitiveValue->primitiveType();
            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize ||
                                              (type != CSSPrimitiveValue::CSS_PERCENTAGE &&
                                               type != CSSPrimitiveValue::CSS_EMS && 
                                               type != CSSPrimitiveValue::CSS_EXS &&
                                               type != CSSPrimitiveValue::CSS_REMS));
            if (CSSPrimitiveValue::isUnitTypeLength(type))
                size = primitiveValue->computeLengthFloat(m_parentStyle, m_rootElementStyle, true);
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                size = (primitiveValue->getFloatValue() * oldSize) / 100.0f;
            else
                return;
        }

        if (size < 0)
            return;

        setFontSize(fontDescription, size);
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }

    case CSSPropertyZIndex: {
        if (isInherit) {
            if (m_parentStyle->hasAutoZIndex())
                m_style->setHasAutoZIndex();
            else
                m_style->setZIndex(m_parentStyle->zIndex());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSSValueAuto) {
            m_style->setHasAutoZIndex();
            return;
        }
        
        // FIXME: Should clamp all sorts of other integer properties too.
        const double minIntAsDouble = INT_MIN;
        const double maxIntAsDouble = INT_MAX;
        m_style->setZIndex(static_cast<int>(max(minIntAsDouble, min(primitiveValue->getDoubleValue(), maxIntAsDouble))));
        return;
    }
    case CSSPropertyWidows:
    {
        HANDLE_INHERIT_AND_INITIAL(widows, Widows)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        m_style->setWidows(primitiveValue->getIntValue());
        return;
    }
        
    case CSSPropertyOrphans:
    {
        HANDLE_INHERIT_AND_INITIAL(orphans, Orphans)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        m_style->setOrphans(primitiveValue->getIntValue());
        return;
    }        

// length, percent, number
    case CSSPropertyLineHeight:
    {
        HANDLE_INHERIT_AND_INITIAL(lineHeight, LineHeight)
        if (!primitiveValue)
            return;
        Length lineHeight;
        int type = primitiveValue->primitiveType();
        if (primitiveValue->getIdent() == CSSValueNormal)
            lineHeight = Length(-100.0, Percent);
        else if (CSSPrimitiveValue::isUnitTypeLength(type)) {
            double multiplier = m_style->effectiveZoom();
            if (m_style->textSizeAdjust() && m_checker.m_document->frame() && m_checker.m_document->frame()->shouldApplyTextZoom())
                multiplier *= m_checker.m_document->frame()->textZoomFactor();
            lineHeight = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle,  multiplier), Fixed);
        } else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            lineHeight = Length((m_style->fontSize() * primitiveValue->getIntValue()) / 100, Fixed);
        else if (type == CSSPrimitiveValue::CSS_NUMBER)
            lineHeight = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
        else
            return;
        m_style->setLineHeight(lineHeight);
        return;
    }

// string
    case CSSPropertyTextAlign:
    {
        HANDLE_INHERIT_AND_INITIAL(textAlign, TextAlign)
        if (!primitiveValue)
            return;
        int id = primitiveValue->getIdent();
        if (id == CSSValueStart)
            m_style->setTextAlign(m_style->direction() == LTR ? LEFT : RIGHT);
        else if (id == CSSValueEnd)
            m_style->setTextAlign(m_style->direction() == LTR ? RIGHT : LEFT);
        else
            m_style->setTextAlign(*primitiveValue);
        return;
    }

// rect
    case CSSPropertyClip:
    {
        Length top;
        Length right;
        Length bottom;
        Length left;
        bool hasClip = true;
        if (isInherit) {
            if (m_parentStyle->hasClip()) {
                top = m_parentStyle->clipTop();
                right = m_parentStyle->clipRight();
                bottom = m_parentStyle->clipBottom();
                left = m_parentStyle->clipLeft();
            } else {
                hasClip = false;
                top = right = bottom = left = Length();
            }
        } else if (isInitial) {
            hasClip = false;
            top = right = bottom = left = Length();
        } else if (!primitiveValue) {
            return;
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RECT) {
            Rect* rect = primitiveValue->getRectValue();
            if (!rect)
                return;
            top = convertToLength(rect->top(), style(), m_rootElementStyle, zoomFactor);
            right = convertToLength(rect->right(), style(), m_rootElementStyle, zoomFactor);
            bottom = convertToLength(rect->bottom(), style(), m_rootElementStyle, zoomFactor);
            left = convertToLength(rect->left(), style(), m_rootElementStyle, zoomFactor);
        } else if (primitiveValue->getIdent() != CSSValueAuto) {
            return;
        }
        m_style->setClip(top, right, bottom, left);
        m_style->setHasClip(hasClip);
    
        // rect, ident
        return;
    }

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

        CSSValueList* list = static_cast<CSSValueList*>(value);
        int len = list->length();

        bool didSet = false;
        for (int i = 0; i < len; i++) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (item->isImageGeneratorValue()) {
                m_style->setContent(static_cast<CSSImageGeneratorValue*>(item)->generatedImage(), didSet);
                didSet = true;
            }
            
            if (!item->isPrimitiveValue())
                continue;
            
            CSSPrimitiveValue* val = static_cast<CSSPrimitiveValue*>(item);
            switch (val->primitiveType()) {
                case CSSPrimitiveValue::CSS_STRING:
                    m_style->setContent(val->getStringValue().impl(), didSet);
                    didSet = true;
                    break;
                case CSSPrimitiveValue::CSS_ATTR: {
                    // FIXME: Can a namespace be specified for an attr(foo)?
                    if (m_style->styleType() == NOPSEUDO)
                        m_style->setUnique();
                    else
                        m_parentStyle->setUnique();
                    QualifiedName attr(nullAtom, val->getStringValue().impl(), nullAtom);
                    m_style->setContent(m_element->getAttribute(attr).impl(), didSet);
                    didSet = true;
                    // register the fact that the attribute value affects the style
                    m_selectorAttrs.add(attr.localName().impl());
                    break;
                }
                case CSSPrimitiveValue::CSS_URI: {
                    CSSImageValue* image = static_cast<CSSImageValue*>(val);
                    m_style->setContent(image->cachedImage(m_element->document()->docLoader()), didSet);
                    didSet = true;
                    break;
                }
                case CSSPrimitiveValue::CSS_COUNTER: {
                    Counter* counterValue = val->getCounterValue();
                    CounterContent* counter = new CounterContent(counterValue->identifier(),
                        (EListStyleType)counterValue->listStyleNumber(), counterValue->separator());
                    m_style->setContent(counter, didSet);
                    didSet = true;
                }
            }
        }
        if (!didSet)
            m_style->clearContent();
        return;
    }

    case CSSPropertyCounterIncrement:
        applyCounterList(style(), value->isValueList() ? static_cast<CSSValueList*>(value) : 0, false);
        return;
    case CSSPropertyCounterReset:
        applyCounterList(style(), value->isValueList() ? static_cast<CSSValueList*>(value) : 0, true);
        return;

    case CSSPropertyFontFamily: {
        // list of strings and ids
        if (isInherit) {
            FontDescription parentFontDescription = m_parentStyle->fontDescription();
            FontDescription fontDescription = m_style->fontDescription();
            fontDescription.setGenericFamily(parentFontDescription.genericFamily());
            fontDescription.setFamily(parentFontDescription.firstFamily());
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
            return;
        } else if (isInitial) {
            FontDescription initialDesc = FontDescription();
            FontDescription fontDescription = m_style->fontDescription();
            // We need to adjust the size to account for the generic family change from monospace
            // to non-monospace.
            if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize())
                setFontSize(fontDescription, fontSizeForKeyword(CSSValueXxSmall + fontDescription.keywordSize() - 1, m_style->htmlHacks(), false));
            fontDescription.setGenericFamily(initialDesc.genericFamily());
            if (!initialDesc.firstFamily().familyIsEmpty())
                fontDescription.setFamily(initialDesc.firstFamily());
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
            return;
        }
        
        if (!value->isValueList())
            return;
        FontDescription fontDescription = m_style->fontDescription();
        CSSValueList* list = static_cast<CSSValueList*>(value);
        int len = list->length();
        FontFamily& firstFamily = fontDescription.firstFamily();
        FontFamily* currFamily = 0;
        
        // Before mapping in a new font-family property, we should reset the generic family.
        bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();
        fontDescription.setGenericFamily(FontDescription::NoFamily);

        for (int i = 0; i < len; i++) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (!item->isPrimitiveValue())
                continue;
            CSSPrimitiveValue* val = static_cast<CSSPrimitiveValue*>(item);
            AtomicString face;
            Settings* settings = m_checker.m_document->settings();
            if (val->primitiveType() == CSSPrimitiveValue::CSS_STRING)
                face = static_cast<FontFamilyValue*>(val)->familyName();
            else if (val->primitiveType() == CSSPrimitiveValue::CSS_IDENT && settings) {
                switch (val->getIdent()) {
                    case CSSValueWebkitBody:
                        face = settings->standardFontFamily();
                        break;
                    case CSSValueSerif:
                        face = "-webkit-serif";
                        fontDescription.setGenericFamily(FontDescription::SerifFamily);
                        break;
                    case CSSValueSansSerif:
                        face = "-webkit-sans-serif";
                        fontDescription.setGenericFamily(FontDescription::SansSerifFamily);
                        break;
                    case CSSValueCursive:
                        face = "-webkit-cursive";
                        fontDescription.setGenericFamily(FontDescription::CursiveFamily);
                        break;
                    case CSSValueFantasy:
                        face = "-webkit-fantasy";
                        fontDescription.setGenericFamily(FontDescription::FantasyFamily);
                        break;
                    case CSSValueMonospace:
                        face = "-webkit-monospace";
                        fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
                        break;
                }
            }

            if (!face.isEmpty()) {
                if (!currFamily) {
                    // Filling in the first family.
                    firstFamily.setFamily(face);
                    firstFamily.appendFamily(0); // Remove any inherited family-fallback list.
                    currFamily = &firstFamily;
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
                setFontSize(fontDescription, fontSizeForKeyword(CSSValueXxSmall + fontDescription.keywordSize() - 1, m_style->htmlHacks(), !oldFamilyUsedFixedDefaultSize));

            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
        }
        return;
    }
    case CSSPropertyTextDecoration: {
        // list of ident
        HANDLE_INHERIT_AND_INITIAL(textDecoration, TextDecoration)
        int t = RenderStyle::initialTextDecoration();
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone) {
            // do nothing
        } else {
            if (!value->isValueList()) return;
            CSSValueList *list = static_cast<CSSValueList*>(value);
            int len = list->length();
            for (int i = 0; i < len; i++)
            {
                CSSValue *item = list->itemWithoutBoundsCheck(i);
                if (!item->isPrimitiveValue()) continue;
                primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                switch (primitiveValue->getIdent()) {
                    case CSSValueNone:
                        t = TDNONE; break;
                    case CSSValueUnderline:
                        t |= UNDERLINE; break;
                    case CSSValueOverline:
                        t |= OVERLINE; break;
                    case CSSValueLineThrough:
                        t |= LINE_THROUGH; break;
                    case CSSValueBlink:
                        t |= BLINK; break;
                    default:
                        return;
                }
            }
        }

        m_style->setTextDecoration(t);
        return;
    }

    case CSSPropertyZoom:
    {
        // Reset the zoom in effect before we do anything.  This allows the setZoom method to accurately compute a new
        // zoom in effect.
        m_style->setEffectiveZoom(m_parentStyle ? m_parentStyle->effectiveZoom() : RenderStyle::initialZoom());
        
        // Now we can handle inherit and initial.
        HANDLE_INHERIT_AND_INITIAL(zoom, Zoom)
        
        // Handle normal/reset, numbers and percentages.
        int type = primitiveValue->primitiveType();
        if (primitiveValue->getIdent() == CSSValueNormal)
            m_style->setZoom(RenderStyle::initialZoom());
        else if (primitiveValue->getIdent() == CSSValueReset) {
            m_style->setEffectiveZoom(RenderStyle::initialZoom());
            m_style->setZoom(RenderStyle::initialZoom());
        } else if (primitiveValue->getIdent() == CSSValueDocument) {
            float docZoom = m_checker.m_document->renderer()->style()->zoom();
            m_style->setEffectiveZoom(docZoom);
            m_style->setZoom(docZoom);
        } else if (type == CSSPrimitiveValue::CSS_PERCENTAGE) {
            if (primitiveValue->getFloatValue())
                m_style->setZoom(primitiveValue->getFloatValue() / 100.0f);
        } else if (type == CSSPrimitiveValue::CSS_NUMBER) {
            if (primitiveValue->getFloatValue())
                m_style->setZoom(primitiveValue->getFloatValue());
        }
        
        m_fontDirty = true;
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

    case CSSPropertyBorder:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderWidth:
    case CSSPropertyBorderColor:
        if (id == CSSPropertyBorder || id == CSSPropertyBorderColor)
        {
            if (isInherit) {
                m_style->setBorderTopColor(m_parentStyle->borderTopColor().isValid() ? m_parentStyle->borderTopColor() : m_parentStyle->color());
                m_style->setBorderBottomColor(m_parentStyle->borderBottomColor().isValid() ? m_parentStyle->borderBottomColor() : m_parentStyle->color());
                m_style->setBorderLeftColor(m_parentStyle->borderLeftColor().isValid() ? m_parentStyle->borderLeftColor() : m_parentStyle->color());
                m_style->setBorderRightColor(m_parentStyle->borderRightColor().isValid() ? m_parentStyle->borderRightColor(): m_parentStyle->color());
            }
            else if (isInitial) {
                m_style->setBorderTopColor(Color()); // Reset to invalid color so currentColor is used instead.
                m_style->setBorderBottomColor(Color());
                m_style->setBorderLeftColor(Color());
                m_style->setBorderRightColor(Color());
            }
        }
        if (id == CSSPropertyBorder || id == CSSPropertyBorderStyle)
        {
            if (isInherit) {
                m_style->setBorderTopStyle(m_parentStyle->borderTopStyle());
                m_style->setBorderBottomStyle(m_parentStyle->borderBottomStyle());
                m_style->setBorderLeftStyle(m_parentStyle->borderLeftStyle());
                m_style->setBorderRightStyle(m_parentStyle->borderRightStyle());
            }
            else if (isInitial) {
                m_style->setBorderTopStyle(RenderStyle::initialBorderStyle());
                m_style->setBorderBottomStyle(RenderStyle::initialBorderStyle());
                m_style->setBorderLeftStyle(RenderStyle::initialBorderStyle());
                m_style->setBorderRightStyle(RenderStyle::initialBorderStyle());
            }
        }
        if (id == CSSPropertyBorder || id == CSSPropertyBorderWidth)
        {
            if (isInherit) {
                m_style->setBorderTopWidth(m_parentStyle->borderTopWidth());
                m_style->setBorderBottomWidth(m_parentStyle->borderBottomWidth());
                m_style->setBorderLeftWidth(m_parentStyle->borderLeftWidth());
                m_style->setBorderRightWidth(m_parentStyle->borderRightWidth());
            }
            else if (isInitial) {
                m_style->setBorderTopWidth(RenderStyle::initialBorderWidth());
                m_style->setBorderBottomWidth(RenderStyle::initialBorderWidth());
                m_style->setBorderLeftWidth(RenderStyle::initialBorderWidth());
                m_style->setBorderRightWidth(RenderStyle::initialBorderWidth());
            }
        }
        return;
    case CSSPropertyBorderTop:
        if (isInherit) {
            m_style->setBorderTopColor(m_parentStyle->borderTopColor().isValid() ? m_parentStyle->borderTopColor() : m_parentStyle->color());
            m_style->setBorderTopStyle(m_parentStyle->borderTopStyle());
            m_style->setBorderTopWidth(m_parentStyle->borderTopWidth());
        }
        else if (isInitial)
            m_style->resetBorderTop();
        return;
    case CSSPropertyBorderRight:
        if (isInherit) {
            m_style->setBorderRightColor(m_parentStyle->borderRightColor().isValid() ? m_parentStyle->borderRightColor() : m_parentStyle->color());
            m_style->setBorderRightStyle(m_parentStyle->borderRightStyle());
            m_style->setBorderRightWidth(m_parentStyle->borderRightWidth());
        }
        else if (isInitial)
            m_style->resetBorderRight();
        return;
    case CSSPropertyBorderBottom:
        if (isInherit) {
            m_style->setBorderBottomColor(m_parentStyle->borderBottomColor().isValid() ? m_parentStyle->borderBottomColor() : m_parentStyle->color());
            m_style->setBorderBottomStyle(m_parentStyle->borderBottomStyle());
            m_style->setBorderBottomWidth(m_parentStyle->borderBottomWidth());
        }
        else if (isInitial)
            m_style->resetBorderBottom();
        return;
    case CSSPropertyBorderLeft:
        if (isInherit) {
            m_style->setBorderLeftColor(m_parentStyle->borderLeftColor().isValid() ? m_parentStyle->borderLeftColor() : m_parentStyle->color());
            m_style->setBorderLeftStyle(m_parentStyle->borderLeftStyle());
            m_style->setBorderLeftWidth(m_parentStyle->borderLeftWidth());
        }
        else if (isInitial)
            m_style->resetBorderLeft();
        return;
    case CSSPropertyMargin:
        if (isInherit) {
            m_style->setMarginTop(m_parentStyle->marginTop());
            m_style->setMarginBottom(m_parentStyle->marginBottom());
            m_style->setMarginLeft(m_parentStyle->marginLeft());
            m_style->setMarginRight(m_parentStyle->marginRight());
        }
        else if (isInitial)
            m_style->resetMargin();
        return;
    case CSSPropertyPadding:
        if (isInherit) {
            m_style->setPaddingTop(m_parentStyle->paddingTop());
            m_style->setPaddingBottom(m_parentStyle->paddingBottom());
            m_style->setPaddingLeft(m_parentStyle->paddingLeft());
            m_style->setPaddingRight(m_parentStyle->paddingRight());
        }
        else if (isInitial)
            m_style->resetPadding();
        return;
    case CSSPropertyFont:
        if (isInherit) {
            FontDescription fontDescription = m_parentStyle->fontDescription();
            m_style->setLineHeight(m_parentStyle->lineHeight());
            m_lineHeightValue = 0;
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
        } else if (isInitial) {
            Settings* settings = m_checker.m_document->settings();
            ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
            if (!settings)
                return;
            FontDescription fontDescription;
            fontDescription.setGenericFamily(FontDescription::StandardFamily);
            fontDescription.setRenderingMode(settings->fontRenderingMode());
            fontDescription.setUsePrinterFont(m_checker.m_document->printing());
            const AtomicString& standardFontFamily = m_checker.m_document->settings()->standardFontFamily();
            if (!standardFontFamily.isEmpty()) {
                fontDescription.firstFamily().setFamily(standardFontFamily);
                fontDescription.firstFamily().appendFamily(0);
            }
            fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
            setFontSize(fontDescription, fontSizeForKeyword(CSSValueMedium, m_style->htmlHacks(), false));
            m_style->setLineHeight(RenderStyle::initialLineHeight());
            m_lineHeightValue = 0;
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
        } else if (primitiveValue) {
            m_style->setLineHeight(RenderStyle::initialLineHeight());
            m_lineHeightValue = 0;
            
            FontDescription fontDescription;
            RenderTheme::defaultTheme()->systemFont(primitiveValue->getIdent(), fontDescription);
 
            // Double-check and see if the theme did anything.  If not, don't bother updating the font.
            if (fontDescription.isAbsoluteSize()) {
                // Make sure the rendering mode and printer font settings are updated.
                Settings* settings = m_checker.m_document->settings();
                ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
                if (!settings)
                    return;
                fontDescription.setRenderingMode(settings->fontRenderingMode());
                fontDescription.setUsePrinterFont(m_checker.m_document->printing());
           
                // Handle the zoom factor.
                fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(fontDescription.isAbsoluteSize(), fontDescription.specifiedSize()));
                if (m_style->setFontDescription(fontDescription))
                    m_fontDirty = true;
            }
        } else if (value->isFontValue()) {
            FontValue *font = static_cast<FontValue*>(value);
            if (!font->style || !font->variant || !font->weight ||
                 !font->size || !font->lineHeight || !font->family)
                return;
            applyProperty(CSSPropertyFontStyle, font->style.get());
            applyProperty(CSSPropertyFontVariant, font->variant.get());
            applyProperty(CSSPropertyFontWeight, font->weight.get());
            applyProperty(CSSPropertyFontSize, font->size.get());

            m_lineHeightValue = font->lineHeight.get();

            applyProperty(CSSPropertyFontFamily, font->family.get());
        }
        return;
        
    case CSSPropertyListStyle:
        if (isInherit) {
            m_style->setListStyleType(m_parentStyle->listStyleType());
            m_style->setListStyleImage(m_parentStyle->listStyleImage());
            m_style->setListStylePosition(m_parentStyle->listStylePosition());
        }
        else if (isInitial) {
            m_style->setListStyleType(RenderStyle::initialListStyleType());
            m_style->setListStyleImage(RenderStyle::initialListStyleImage());
            m_style->setListStylePosition(RenderStyle::initialListStylePosition());
        }
        return;
    case CSSPropertyOutline:
        if (isInherit) {
            m_style->setOutlineWidth(m_parentStyle->outlineWidth());
            m_style->setOutlineColor(m_parentStyle->outlineColor().isValid() ? m_parentStyle->outlineColor() : m_parentStyle->color());
            m_style->setOutlineStyle(m_parentStyle->outlineStyle());
        }
        else if (isInitial)
            m_style->resetOutline();
        return;

    // CSS3 Properties
    case CSSPropertyWebkitAppearance: {
        HANDLE_INHERIT_AND_INITIAL(appearance, Appearance)
        if (!primitiveValue)
            return;
        m_style->setAppearance(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitBinding: {
#if ENABLE(XBL)
        if (isInitial || (primitiveValue && primitiveValue->getIdent() == CSSValueNone)) {
            m_style->deleteBindingURIs();
            return;
        }
        else if (isInherit) {
            if (m_parentStyle->bindingURIs())
                m_style->inheritBindingURIs(m_parentStyle->bindingURIs());
            else
                m_style->deleteBindingURIs();
            return;
        }

        if (!value->isValueList()) return;
        CSSValueList* list = static_cast<CSSValueList*>(value);
        bool firstBinding = true;
        for (unsigned int i = 0; i < list->length(); i++) {
            CSSValue *item = list->itemWithoutBoundsCheck(i);
            CSSPrimitiveValue *val = static_cast<CSSPrimitiveValue*>(item);
            if (val->primitiveType() == CSSPrimitiveValue::CSS_URI) {
                if (firstBinding) {
                    firstBinding = false;
                    m_style->deleteBindingURIs();
                }
                m_style->addBindingURI(val->getStringValue());
            }
        }
#endif
        return;
    }

    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitMaskBoxImage: {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyWebkitBorderImage, borderImage, BorderImage)
            HANDLE_INHERIT_COND(CSSPropertyWebkitMaskBoxImage, maskBoxImage, MaskBoxImage)
            return;
        } else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitBorderImage, BorderImage, NinePieceImage)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitMaskBoxImage, MaskBoxImage, NinePieceImage)
            return;
        }

        NinePieceImage image;
        mapNinePieceImage(value, image);
        
        if (id == CSSPropertyWebkitBorderImage)
            m_style->setBorderImage(image);
        else
            m_style->setMaskBoxImage(image);
        return;
    }

    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius:
        if (isInherit) {
            m_style->setBorderTopLeftRadius(m_parentStyle->borderTopLeftRadius());
            m_style->setBorderTopRightRadius(m_parentStyle->borderTopRightRadius());
            m_style->setBorderBottomLeftRadius(m_parentStyle->borderBottomLeftRadius());
            m_style->setBorderBottomRightRadius(m_parentStyle->borderBottomRightRadius());
            return;
        }
        if (isInitial) {
            m_style->resetBorderRadius();
            return;
        }
        // Fall through
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius: {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyBorderTopLeftRadius, borderTopLeftRadius, BorderTopLeftRadius)
            HANDLE_INHERIT_COND(CSSPropertyBorderTopRightRadius, borderTopRightRadius, BorderTopRightRadius)
            HANDLE_INHERIT_COND(CSSPropertyBorderBottomLeftRadius, borderBottomLeftRadius, BorderBottomLeftRadius)
            HANDLE_INHERIT_COND(CSSPropertyBorderBottomRightRadius, borderBottomRightRadius, BorderBottomRightRadius)
            return;
        }
        
        if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderTopLeftRadius, BorderTopLeftRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderTopRightRadius, BorderTopRightRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderBottomLeftRadius, BorderBottomLeftRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderBottomRightRadius, BorderBottomRightRadius, BorderRadius)
            return;
        }

        if (!primitiveValue)
            return;

        Pair* pair = primitiveValue->getPairValue();
        if (!pair)
            return;

        int width = pair->first()->computeLengthInt(style(), m_rootElementStyle, zoomFactor);
        int height = pair->second()->computeLengthInt(style(), m_rootElementStyle,  zoomFactor);
        if (width < 0 || height < 0)
            return;

        if (width == 0)
            height = 0; // Null out the other value.
        else if (height == 0)
            width = 0; // Null out the other value.

        IntSize size(width, height);
        switch (id) {
            case CSSPropertyBorderTopLeftRadius:
                m_style->setBorderTopLeftRadius(size);
                break;
            case CSSPropertyBorderTopRightRadius:
                m_style->setBorderTopRightRadius(size);
                break;
            case CSSPropertyBorderBottomLeftRadius:
                m_style->setBorderBottomLeftRadius(size);
                break;
            case CSSPropertyBorderBottomRightRadius:
                m_style->setBorderBottomRightRadius(size);
                break;
            default:
                m_style->setBorderRadius(size);
                break;
        }
        return;
    }

    case CSSPropertyOutlineOffset:
        HANDLE_INHERIT_AND_INITIAL(outlineOffset, OutlineOffset)
        m_style->setOutlineOffset(primitiveValue->computeLengthInt(style(), m_rootElementStyle, zoomFactor));
        return;

    case CSSPropertyTextShadow:
    case CSSPropertyBoxShadow: {
        if (isInherit) {
            if (id == CSSPropertyTextShadow)
                return m_style->setTextShadow(m_parentStyle->textShadow() ? new ShadowData(*m_parentStyle->textShadow()) : 0);
            return m_style->setBoxShadow(m_parentStyle->boxShadow() ? new ShadowData(*m_parentStyle->boxShadow()) : 0);
        }
        if (isInitial || primitiveValue) // initial | none
            return id == CSSPropertyTextShadow ? m_style->setTextShadow(0) : m_style->setBoxShadow(0);

        if (!value->isValueList())
            return;

        CSSValueList *list = static_cast<CSSValueList*>(value);
        int len = list->length();
        for (int i = 0; i < len; i++) {
            ShadowValue* item = static_cast<ShadowValue*>(list->itemWithoutBoundsCheck(i));
            int x = item->x->computeLengthInt(style(), m_rootElementStyle, zoomFactor);
            int y = item->y->computeLengthInt(style(), m_rootElementStyle, zoomFactor);
            int blur = item->blur ? item->blur->computeLengthInt(style(), m_rootElementStyle, zoomFactor) : 0;
            int spread = item->spread ? item->spread->computeLengthInt(style(), m_rootElementStyle, zoomFactor) : 0;
            ShadowStyle shadowStyle = item->style && item->style->getIdent() == CSSValueInset ? Inset : Normal;
            Color color;
            if (item->color)
                color = getColorFromPrimitiveValue(item->color.get());
            ShadowData* shadowData = new ShadowData(x, y, blur, spread, shadowStyle, color.isValid() ? color : Color::transparent);
            if (id == CSSPropertyTextShadow)
                m_style->setTextShadow(shadowData, i != 0);
            else
                m_style->setBoxShadow(shadowData, i != 0);
        }
        return;
    }
    case CSSPropertyWebkitBoxReflect: {
        HANDLE_INHERIT_AND_INITIAL(boxReflect, BoxReflect)
        if (primitiveValue) {
            m_style->setBoxReflect(RenderStyle::initialBoxReflect());
            return;
        }
        CSSReflectValue* reflectValue = static_cast<CSSReflectValue*>(value);
        RefPtr<StyleReflection> reflection = StyleReflection::create();
        reflection->setDirection(reflectValue->direction());
        if (reflectValue->offset()) {
            int type = reflectValue->offset()->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                reflection->setOffset(Length(reflectValue->offset()->getDoubleValue(), Percent));
            else
                reflection->setOffset(Length(reflectValue->offset()->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed));
        }
        NinePieceImage mask;
        mapNinePieceImage(reflectValue->mask(), mask);
        reflection->setMask(mask);
        
        m_style->setBoxReflect(reflection.release());
        return;
    }
    case CSSPropertyOpacity:
        HANDLE_INHERIT_AND_INITIAL(opacity, Opacity)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        // Clamp opacity to the range 0-1
        m_style->setOpacity(min(1.0f, max(0.0f, primitiveValue->getFloatValue())));
        return;
    case CSSPropertyWebkitBoxAlign:
    {
        HANDLE_INHERIT_AND_INITIAL(boxAlign, BoxAlign)
        if (!primitiveValue)
            return;
        EBoxAlignment boxAlignment = *primitiveValue;
        if (boxAlignment != BJUSTIFY)
            m_style->setBoxAlign(boxAlignment);
        return;
    }
    case CSSPropertySrc: // Only used in @font-face rules.
        return;
    case CSSPropertyUnicodeRange: // Only used in @font-face rules.
        return;
    case CSSPropertyWebkitBackfaceVisibility:
        HANDLE_INHERIT_AND_INITIAL(backfaceVisibility, BackfaceVisibility)
        if (primitiveValue)
            m_style->setBackfaceVisibility((primitiveValue->getIdent() == CSSValueVisible) ? BackfaceVisibilityVisible : BackfaceVisibilityHidden);
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
    {
        HANDLE_INHERIT_AND_INITIAL(boxPack, BoxPack)
        if (!primitiveValue)
            return;
        EBoxAlignment boxPack = *primitiveValue;
        if (boxPack != BSTRETCH && boxPack != BBASELINE)
            m_style->setBoxPack(boxPack);
        return;
    }
    case CSSPropertyWebkitBoxFlex:
        HANDLE_INHERIT_AND_INITIAL(boxFlex, BoxFlex)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxFlex(primitiveValue->getFloatValue());
        return;
    case CSSPropertyWebkitBoxFlexGroup:
        HANDLE_INHERIT_AND_INITIAL(boxFlexGroup, BoxFlexGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxFlexGroup((unsigned int)(primitiveValue->getDoubleValue()));
        return;
    case CSSPropertyWebkitBoxOrdinalGroup:
        HANDLE_INHERIT_AND_INITIAL(boxOrdinalGroup, BoxOrdinalGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxOrdinalGroup((unsigned int)(primitiveValue->getDoubleValue()));
        return;
    case CSSPropertyWebkitBoxSizing:
        HANDLE_INHERIT_AND_INITIAL(boxSizing, BoxSizing)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSSValueContentBox)
            m_style->setBoxSizing(CONTENT_BOX);
        else
            m_style->setBoxSizing(BORDER_BOX);
        return;
    case CSSPropertyWebkitColumnCount: {
        if (isInherit) {
            if (m_parentStyle->hasAutoColumnCount())
                m_style->setHasAutoColumnCount();
            else
                m_style->setColumnCount(m_parentStyle->columnCount());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSSValueAuto) {
            m_style->setHasAutoColumnCount();
            return;
        }
        m_style->setColumnCount(static_cast<unsigned short>(primitiveValue->getDoubleValue()));
        return;
    }
    case CSSPropertyWebkitColumnGap: {
        if (isInherit) {
            if (m_parentStyle->hasNormalColumnGap())
                m_style->setHasNormalColumnGap();
            else
                m_style->setColumnGap(m_parentStyle->columnGap());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSSValueNormal) {
            m_style->setHasNormalColumnGap();
            return;
        }
        m_style->setColumnGap(primitiveValue->computeLengthFloat(style(), m_rootElementStyle, zoomFactor));
        return;
    }
    case CSSPropertyWebkitColumnWidth: {
        if (isInherit) {
            if (m_parentStyle->hasAutoColumnWidth())
                m_style->setHasAutoColumnWidth();
            else
                m_style->setColumnWidth(m_parentStyle->columnWidth());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSSValueAuto) {
            m_style->setHasAutoColumnWidth();
            return;
        }
        m_style->setColumnWidth(primitiveValue->computeLengthFloat(style(), m_rootElementStyle, zoomFactor));
        return;
    }
    case CSSPropertyWebkitColumnRuleStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(columnRuleStyle, ColumnRuleStyle, BorderStyle)
        return;
    case CSSPropertyWebkitColumnBreakBefore:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(columnBreakBefore, ColumnBreakBefore, PageBreak)
        return;
    case CSSPropertyWebkitColumnBreakAfter:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE_WITH_VALUE(columnBreakAfter, ColumnBreakAfter, PageBreak)
        return;
    case CSSPropertyWebkitColumnBreakInside: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnBreakInside, ColumnBreakInside, PageBreak)
        EPageBreak pb = *primitiveValue;
        if (pb != PBALWAYS)
            m_style->setColumnBreakInside(pb);
        return;
    }
     case CSSPropertyWebkitColumnRule:
        if (isInherit) {
            m_style->setColumnRuleColor(m_parentStyle->columnRuleColor().isValid() ? m_parentStyle->columnRuleColor() : m_parentStyle->color());
            m_style->setColumnRuleStyle(m_parentStyle->columnRuleStyle());
            m_style->setColumnRuleWidth(m_parentStyle->columnRuleWidth());
        }
        else if (isInitial)
            m_style->resetColumnRule();
        return;
    case CSSPropertyWebkitColumns:
        if (isInherit) {
            if (m_parentStyle->hasAutoColumnWidth())
                m_style->setHasAutoColumnWidth();
            else
                m_style->setColumnWidth(m_parentStyle->columnWidth());
            m_style->setColumnCount(m_parentStyle->columnCount());
        } else if (isInitial) {
            m_style->setHasAutoColumnWidth();
            m_style->setColumnCount(RenderStyle::initialColumnCount());
        }
        return;
    case CSSPropertyWebkitMarquee:
        if (valueType != CSSValue::CSS_INHERIT || !m_parentNode) return;
        m_style->setMarqueeDirection(m_parentStyle->marqueeDirection());
        m_style->setMarqueeIncrement(m_parentStyle->marqueeIncrement());
        m_style->setMarqueeSpeed(m_parentStyle->marqueeSpeed());
        m_style->setMarqueeLoopCount(m_parentStyle->marqueeLoopCount());
        m_style->setMarqueeBehavior(m_parentStyle->marqueeBehavior());
        return;
#if ENABLE(WCSS)
    case CSSPropertyWapMarqueeLoop:
#endif
    case CSSPropertyWebkitMarqueeRepetition: {
        HANDLE_INHERIT_AND_INITIAL(marqueeLoopCount, MarqueeLoopCount)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSSValueInfinite)
            m_style->setMarqueeLoopCount(-1); // -1 means repeat forever.
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
            m_style->setMarqueeLoopCount(primitiveValue->getIntValue());
        return;
    }
#if ENABLE(WCSS)
    case CSSPropertyWapMarqueeSpeed:
#endif
    case CSSPropertyWebkitMarqueeSpeed: {
        HANDLE_INHERIT_AND_INITIAL(marqueeSpeed, MarqueeSpeed)      
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent()) {
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
        }
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_S)
            m_style->setMarqueeSpeed(1000 * primitiveValue->getIntValue());
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_MS)
            m_style->setMarqueeSpeed(primitiveValue->getIntValue());
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) // For scrollamount support.
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
        }
        else {
            bool ok = true;
            Length l = convertToLength(primitiveValue, style(), m_rootElementStyle, 1, &ok);
            if (ok)
                m_style->setMarqueeIncrement(l);
        }
        return;
    }
#if ENABLE(WCSS)
    case CSSPropertyWapMarqueeStyle:
#endif
    case CSSPropertyWebkitMarqueeStyle:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(marqueeBehavior, MarqueeBehavior)      
        return;
#if ENABLE(WCSS)
    case CSSPropertyWapMarqueeDir:
        HANDLE_INHERIT_AND_INITIAL(marqueeDirection, MarqueeDirection)
        if (primitiveValue && primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent()) {
            case CSSValueLtr:
                m_style->setMarqueeDirection(MRIGHT);
                break;
            case CSSValueRtl:
                m_style->setMarqueeDirection(MLEFT);
                break;
            default:
                m_style->setMarqueeDirection(*primitiveValue);
                break;
            }
        }
        return;
#endif
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
        HANDLE_INHERIT_AND_INITIAL(textOverflow, TextOverflow)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        m_style->setTextOverflow(primitiveValue->getIdent() == CSSValueEllipsis);
        return;
    }
    case CSSPropertyWebkitMarginCollapse: {
        if (isInherit) {
            m_style->setMarginTopCollapse(m_parentStyle->marginTopCollapse());
            m_style->setMarginBottomCollapse(m_parentStyle->marginBottomCollapse());
        }
        else if (isInitial) {
            m_style->setMarginTopCollapse(MCOLLAPSE);
            m_style->setMarginBottomCollapse(MCOLLAPSE);
        }
        return;
    }

    case CSSPropertyWebkitMarginTopCollapse:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(marginTopCollapse, MarginTopCollapse)
        return;
    case CSSPropertyWebkitMarginBottomCollapse:
        HANDLE_INHERIT_AND_INITIAL_AND_PRIMITIVE(marginBottomCollapse, MarginBottomCollapse)
        return;

    // Apple-specific changes.  Do not merge these properties into KHTML.
    case CSSPropertyWebkitLineClamp: {
        HANDLE_INHERIT_AND_INITIAL(lineClamp, LineClamp)
        if (!primitiveValue)
            return;
        m_style->setLineClamp(primitiveValue->getIntValue(CSSPrimitiveValue::CSS_PERCENTAGE));
        return;
    }
    case CSSPropertyWebkitHighlight: {
        HANDLE_INHERIT_AND_INITIAL(highlight, Highlight);
        if (primitiveValue->getIdent() == CSSValueNone)
            m_style->setHighlight(nullAtom);
        else
            m_style->setHighlight(primitiveValue->getStringValue());
        return;
    }
    case CSSPropertyWebkitBorderFit: {
        HANDLE_INHERIT_AND_INITIAL(borderFit, BorderFit);
        if (primitiveValue->getIdent() == CSSValueBorder)
            m_style->setBorderFit(BorderFitBorder);
        else
            m_style->setBorderFit(BorderFitLines);
        return;
    }
    case CSSPropertyWebkitTextSizeAdjust: {
        HANDLE_INHERIT_AND_INITIAL(textSizeAdjust, TextSizeAdjust)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        m_style->setTextSizeAdjust(primitiveValue->getIdent() == CSSValueAuto);
        m_fontDirty = true;
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
            Length top = convertToLength(region->top(), style(), m_rootElementStyle);
            Length right = convertToLength(region->right(), style(), m_rootElementStyle);
            Length bottom = convertToLength(region->bottom(), style(), m_rootElementStyle);
            Length left = convertToLength(region->left(), style(), m_rootElementStyle);
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
        HANDLE_INHERIT_AND_INITIAL(visuallyOrdered, VisuallyOrdered)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        m_style->setVisuallyOrdered(primitiveValue->getIdent() == CSSValueVisual);
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
                width = CSSPrimitiveValue::create(result, CSSPrimitiveValue::CSS_EMS)->computeLengthFloat(style(), m_rootElementStyle, zoomFactor);
                break;
            }
            default:
                width = primitiveValue->computeLengthFloat(style(), m_rootElementStyle, zoomFactor);
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
    case CSSPropertyWebkitTransformOrigin:
        HANDLE_INHERIT_AND_INITIAL(transformOriginX, TransformOriginX)
        HANDLE_INHERIT_AND_INITIAL(transformOriginY, TransformOriginY)
        HANDLE_INHERIT_AND_INITIAL(transformOriginZ, TransformOriginZ)
        return;
    case CSSPropertyWebkitTransformOriginX: {
        HANDLE_INHERIT_AND_INITIAL(transformOriginX, TransformOriginX)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length l;
        int type = primitiveValue->primitiveType();
        if (CSSPrimitiveValue::isUnitTypeLength(type))
            l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
        else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else
            return;
        m_style->setTransformOriginX(l);
        break;
    }
    case CSSPropertyWebkitTransformOriginY: {
        HANDLE_INHERIT_AND_INITIAL(transformOriginY, TransformOriginY)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length l;
        int type = primitiveValue->primitiveType();
        if (CSSPrimitiveValue::isUnitTypeLength(type))
            l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
        else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else
            return;
        m_style->setTransformOriginY(l);
        break;
    }
    case CSSPropertyWebkitTransformOriginZ: {
        HANDLE_INHERIT_AND_INITIAL(transformOriginZ, TransformOriginZ)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        float f;
        int type = primitiveValue->primitiveType();
        if (CSSPrimitiveValue::isUnitTypeLength(type))
            f = static_cast<float>(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle));
        else
            return;
        m_style->setTransformOriginZ(f);
        break;
    }
    case CSSPropertyWebkitTransformStyle:
        HANDLE_INHERIT_AND_INITIAL(transformStyle3D, TransformStyle3D)
        if (primitiveValue)
            m_style->setTransformStyle3D((primitiveValue->getIdent() == CSSValuePreserve3d) ? TransformStyle3DPreserve3D : TransformStyle3DFlat);
        return;
    case CSSPropertyWebkitPerspective: {
        HANDLE_INHERIT_AND_INITIAL(perspective, Perspective)
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone) {
            m_style->setPerspective(0);
            return;
        }
        
        float perspectiveValue;
        int type = primitiveValue->primitiveType();
        if (CSSPrimitiveValue::isUnitTypeLength(type))
            perspectiveValue = static_cast<float>(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor));
        else if (type == CSSPrimitiveValue::CSS_NUMBER) {
            // For backward compatibility, treat valueless numbers as px.
            perspectiveValue = CSSPrimitiveValue::create(primitiveValue->getDoubleValue(), CSSPrimitiveValue::CSS_PX)->computeLengthFloat(style(), m_rootElementStyle, zoomFactor);
        } else
            return;

        if (perspectiveValue >= 0.0f)
            m_style->setPerspective(perspectiveValue);
        return;
    }
    case CSSPropertyWebkitPerspectiveOrigin:
        HANDLE_INHERIT_AND_INITIAL(perspectiveOriginX, PerspectiveOriginX)
        HANDLE_INHERIT_AND_INITIAL(perspectiveOriginY, PerspectiveOriginY)
        return;
    case CSSPropertyWebkitPerspectiveOriginX: {
        HANDLE_INHERIT_AND_INITIAL(perspectiveOriginX, PerspectiveOriginX)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length l;
        int type = primitiveValue->primitiveType();
        if (CSSPrimitiveValue::isUnitTypeLength(type))
            l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
        else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else
            return;
        m_style->setPerspectiveOriginX(l);
        return;
    }
    case CSSPropertyWebkitPerspectiveOriginY: {
        HANDLE_INHERIT_AND_INITIAL(perspectiveOriginY, PerspectiveOriginY)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length l;
        int type = primitiveValue->primitiveType();
        if (CSSPrimitiveValue::isUnitTypeLength(type))
            l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
        else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else
            return;
        m_style->setPerspectiveOriginY(l);
        return;
    }
    case CSSPropertyWebkitAnimation:
        if (isInitial)
            m_style->clearAnimations();
        else if (isInherit)
            m_style->inheritAnimations(m_parentStyle->animations());
        return;
    case CSSPropertyWebkitAnimationDelay:
        HANDLE_ANIMATION_VALUE(delay, Delay, value)
        return;
    case CSSPropertyWebkitAnimationDirection:
        HANDLE_ANIMATION_VALUE(direction, Direction, value)
        return;
    case CSSPropertyWebkitAnimationDuration:
        HANDLE_ANIMATION_VALUE(duration, Duration, value)
        return;
    case CSSPropertyWebkitAnimationIterationCount:
        HANDLE_ANIMATION_VALUE(iterationCount, IterationCount, value)
        return;
    case CSSPropertyWebkitAnimationName:
        HANDLE_ANIMATION_VALUE(name, Name, value)
        return;
    case CSSPropertyWebkitAnimationPlayState:
        HANDLE_ANIMATION_VALUE(playState, PlayState, value)
        return;
    case CSSPropertyWebkitAnimationTimingFunction:
        HANDLE_ANIMATION_VALUE(timingFunction, TimingFunction, value)
        return;
    case CSSPropertyWebkitTransition:
        if (isInitial)
            m_style->clearTransitions();
        else if (isInherit)
            m_style->inheritTransitions(m_parentStyle->transitions());
        return;
    case CSSPropertyWebkitTransitionDelay:
        HANDLE_TRANSITION_VALUE(delay, Delay, value)
        return;
    case CSSPropertyWebkitTransitionDuration:
        HANDLE_TRANSITION_VALUE(duration, Duration, value)
        return;
    case CSSPropertyWebkitTransitionProperty:
        HANDLE_TRANSITION_VALUE(property, Property, value)
        return;
    case CSSPropertyWebkitTransitionTimingFunction:
        HANDLE_TRANSITION_VALUE(timingFunction, TimingFunction, value)
        return;
    case CSSPropertyPointerEvents:
    {
#if ENABLE(DASHBOARD_SUPPORT)
        // <rdar://problem/6561077> Work around the Stocks widget's misuse of the
        // pointer-events property by not applying it in Dashboard.
        Settings* settings = m_checker.m_document->settings();
        if (settings && settings->usesDashboardBackwardCompatibilityMode())
            return;
#endif
        HANDLE_INHERIT_AND_INITIAL(pointerEvents, PointerEvents)
        if (!primitiveValue)
            return;
        m_style->setPointerEvents(*primitiveValue);
        return;
    }
    case CSSPropertyInvalid:
        return;
    case CSSPropertyFontStretch:
    case CSSPropertyPage:
    case CSSPropertyQuotes:
    case CSSPropertySize:
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
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitVariableDeclarationBlock:
        return;
#if ENABLE(SVG)
    default:
        // Try the SVG properties
        applySVGProperty(id, value);
#endif
    }
}

void CSSStyleSelector::mapFillAttachment(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
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

void CSSStyleSelector::mapFillClip(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setClip(FillLayer::initialFillClip(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setClip(*primitiveValue);
}

void CSSStyleSelector::mapFillComposite(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setComposite(FillLayer::initialFillComposite(layer->type()));
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setComposite(*primitiveValue);
}

void CSSStyleSelector::mapFillOrigin(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setOrigin(FillLayer::initialFillOrigin(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setOrigin(*primitiveValue);
}

StyleImage* CSSStyleSelector::styleImage(CSSValue* value)
{
    if (value->isImageValue())
        return static_cast<CSSImageValue*>(value)->cachedImage(m_element->document()->docLoader());
    if (value->isImageGeneratorValue())
        return static_cast<CSSImageGeneratorValue*>(value)->generatedImage();
    return 0;
}

void CSSStyleSelector::mapFillImage(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setImage(FillLayer::initialFillImage(layer->type()));
        return;
    }

    layer->setImage(styleImage(value));
}

void CSSStyleSelector::mapFillRepeatX(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setRepeatX(FillLayer::initialFillRepeatX(layer->type()));
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setRepeatX(*primitiveValue);
}

void CSSStyleSelector::mapFillRepeatY(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setRepeatY(FillLayer::initialFillRepeatY(layer->type()));
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setRepeatY(*primitiveValue);
}

void CSSStyleSelector::mapFillSize(FillLayer* layer, CSSValue* value)
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
    
    if (value->cssValueType() == CSSValue::CSS_INITIAL || primitiveValue->getIdent() == CSSValueContain
        || primitiveValue->getIdent() == CSSValueCover) {
        layer->setSizeLength(b);
        return;
    }

    Pair* pair = primitiveValue->getPairValue();
    if (!pair)
        return;
    
    CSSPrimitiveValue* first = static_cast<CSSPrimitiveValue*>(pair->first());
    CSSPrimitiveValue* second = static_cast<CSSPrimitiveValue*>(pair->second());
    
    if (!first || !second)
        return;
        
    Length firstLength, secondLength;
    int firstType = first->primitiveType();
    int secondType = second->primitiveType();
    
    float zoomFactor = m_style->effectiveZoom();

    if (firstType == CSSPrimitiveValue::CSS_UNKNOWN)
        firstLength = Length(Auto);
    else if (CSSPrimitiveValue::isUnitTypeLength(firstType))
        firstLength = Length(first->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
    else if (firstType == CSSPrimitiveValue::CSS_PERCENTAGE)
        firstLength = Length(first->getDoubleValue(), Percent);
    else
        return;

    if (secondType == CSSPrimitiveValue::CSS_UNKNOWN)
        secondLength = Length(Auto);
    else if (CSSPrimitiveValue::isUnitTypeLength(secondType))
        secondLength = Length(second->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
    else if (secondType == CSSPrimitiveValue::CSS_PERCENTAGE)
        secondLength = Length(second->getDoubleValue(), Percent);
    else
        return;
    
    b.setWidth(firstLength);
    b.setHeight(secondLength);
    layer->setSizeLength(b);
}

void CSSStyleSelector::mapFillXPosition(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setXPosition(FillLayer::initialFillXPosition(layer->type()));
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    float zoomFactor = m_style->effectiveZoom();

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Length l;
    int type = primitiveValue->primitiveType();
    if (CSSPrimitiveValue::isUnitTypeLength(type))
        l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
    else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length(primitiveValue->getDoubleValue(), Percent);
    else
        return;
    layer->setXPosition(l);
}

void CSSStyleSelector::mapFillYPosition(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setYPosition(FillLayer::initialFillYPosition(layer->type()));
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    float zoomFactor = m_style->effectiveZoom();
    
    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Length l;
    int type = primitiveValue->primitiveType();
    if (CSSPrimitiveValue::isUnitTypeLength(type))
        l = Length(primitiveValue->computeLengthIntForLength(style(), m_rootElementStyle, zoomFactor), Fixed);
    else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length(primitiveValue->getDoubleValue(), Percent);
    else
        return;
    layer->setYPosition(l);
}

void CSSStyleSelector::mapAnimationDelay(Animation* animation, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        animation->setDelay(Animation::initialAnimationDelay());
        return;
    }

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_S)
        animation->setDelay(primitiveValue->getFloatValue());
    else
        animation->setDelay(primitiveValue->getFloatValue()/1000.0f);
}

void CSSStyleSelector::mapAnimationDirection(Animation* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setDirection(Animation::initialAnimationDirection());
        return;
    }

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setDirection(primitiveValue->getIdent() == CSSValueAlternate ? Animation::AnimationDirectionAlternate : Animation::AnimationDirectionNormal);
}

void CSSStyleSelector::mapAnimationDuration(Animation* animation, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        animation->setDuration(Animation::initialAnimationDuration());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_S)
        animation->setDuration(primitiveValue->getFloatValue());
    else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_MS)
        animation->setDuration(primitiveValue->getFloatValue()/1000.0f);
}

void CSSStyleSelector::mapAnimationIterationCount(Animation* animation, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
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
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setName(Animation::initialAnimationName());
        return;
    }

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    
    if (primitiveValue->getIdent() == CSSValueNone)
        layer->setIsNoneAnimation(true);
    else
        layer->setName(primitiveValue->getStringValue());
}

void CSSStyleSelector::mapAnimationPlayState(Animation* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setPlayState(Animation::initialAnimationPlayState());
        return;
    }

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    EAnimPlayState playState = (primitiveValue->getIdent() == CSSValuePaused) ? AnimPlayStatePaused : AnimPlayStatePlaying;
    layer->setPlayState(playState);
}

void CSSStyleSelector::mapAnimationProperty(Animation* animation, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
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
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        animation->setTimingFunction(Animation::initialAnimationTimingFunction());
        return;
    }
    
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        switch (primitiveValue->getIdent()) {
            case CSSValueLinear:
                animation->setTimingFunction(TimingFunction(LinearTimingFunction, 0.0, 0.0, 1.0, 1.0));
                break;
            case CSSValueEase:
                animation->setTimingFunction(TimingFunction());
                break;
            case CSSValueEaseIn:
                animation->setTimingFunction(TimingFunction(CubicBezierTimingFunction, 0.42, 0.0, 1.0, 1.0));
                break;
            case CSSValueEaseOut:
                animation->setTimingFunction(TimingFunction(CubicBezierTimingFunction, 0.0, 0.0, 0.58, 1.0));
                break;
            case CSSValueEaseInOut:
                animation->setTimingFunction(TimingFunction(CubicBezierTimingFunction, 0.42, 0.0, 0.58, 1.0));
                break;
        }
        return;
    }
    
    if (value->isTimingFunctionValue()) {
        CSSTimingFunctionValue* timingFunction = static_cast<CSSTimingFunctionValue*>(value);
        animation->setTimingFunction(TimingFunction(CubicBezierTimingFunction, timingFunction->x1(), timingFunction->y1(), timingFunction->x2(), timingFunction->y2()));
    }
}

void CSSStyleSelector::mapNinePieceImage(CSSValue* value, NinePieceImage& image)
{
    // If we're a primitive value, then we are "none" and don't need to alter the empty image at all.
    if (!value || value->isPrimitiveValue())
        return;

    // Retrieve the border image value.
    CSSBorderImageValue* borderImage = static_cast<CSSBorderImageValue*>(value);
    
    // Set the image (this kicks off the load).
    image.m_image = styleImage(borderImage->imageValue());

    // Set up a length box to represent our image slices.
    LengthBox& l = image.m_slices;
    Rect* r = borderImage->m_imageSliceRect.get();
    if (r->top()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        l.m_top = Length(r->top()->getDoubleValue(), Percent);
    else
        l.m_top = Length(r->top()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (r->bottom()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        l.m_bottom = Length(r->bottom()->getDoubleValue(), Percent);
    else
        l.m_bottom = Length((int)r->bottom()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (r->left()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        l.m_left = Length(r->left()->getDoubleValue(), Percent);
    else
        l.m_left = Length(r->left()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (r->right()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        l.m_right = Length(r->right()->getDoubleValue(), Percent);
    else
        l.m_right = Length(r->right()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    
    // Set the appropriate rules for stretch/round/repeat of the slices
    switch (borderImage->m_horizontalSizeRule) {
        case CSSValueStretch:
            image.m_horizontalRule = StretchImageRule;
            break;
        case CSSValueRound:
            image.m_horizontalRule = RoundImageRule;
            break;
        default: // CSSValueRepeat
            image.m_horizontalRule = RepeatImageRule;
            break;
    }

    switch (borderImage->m_verticalSizeRule) {
        case CSSValueStretch:
            image.m_verticalRule = StretchImageRule;
            break;
        case CSSValueRound:
            image.m_verticalRule = RoundImageRule;
            break;
        default: // CSSValueRepeat
            image.m_verticalRule = RepeatImageRule;
            break;
    }
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
        size = fontSizeForKeyword(CSSValueXxSmall + childFont.keywordSize() - 1, style->htmlHacks(), childFont.useFixedDefaultSize());
    else {
        Settings* settings = m_checker.m_document->settings();
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
    fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(fontDescription.isAbsoluteSize(), size));
}

float CSSStyleSelector::getComputedSizeFromSpecifiedSize(bool isAbsoluteSize, float specifiedSize)
{
    // We support two types of minimum font size.  The first is a hard override that applies to
    // all fonts.  This is "minSize."  The second type of minimum font size is a "smart minimum"
    // that is applied only when the Web page can't know what size it really asked for, e.g.,
    // when it uses logical sizes like "small" or expresses the font-size as a percentage of
    // the user's default font setting.

    // With the smart minimum, we never want to get smaller than the minimum font size to keep fonts readable.
    // However we always allow the page to set an explicit pixel size that is smaller,
    // since sites will mis-render otherwise (e.g., http://www.gamespot.com with a 9px minimum).
    
    Settings* settings = m_checker.m_document->settings();
    if (!settings)
        return 1.0f;

    int minSize = settings->minimumFontSize();
    int minLogicalSize = settings->minimumLogicalFontSize();

    float zoomFactor = m_style->effectiveZoom();
    if (m_checker.m_document->frame() && m_checker.m_document->frame()->shouldApplyTextZoom())
        zoomFactor *= m_checker.m_document->frame()->textZoomFactor();

    float zoomedSize = specifiedSize * zoomFactor;

    // Apply the hard minimum first.  We only apply the hard minimum if after zooming we're still too small.
    if (zoomedSize < minSize)
        zoomedSize = minSize;

    // Now apply the "smart minimum."  This minimum is also only applied if we're still too small
    // after zooming.  The font size must either be relative to the user default or the original size
    // must have been acceptable.  In other words, we only apply the smart minimum whenever we're positive
    // doing so won't disrupt the layout.
    if (zoomedSize < minLogicalSize && (specifiedSize >= minLogicalSize || !isAbsoluteSize))
        zoomedSize = minLogicalSize;
    
    // Also clamp to a reasonable maximum to prevent insane font sizes from causing crashes on various
    // platforms (I'm looking at you, Windows.)
    return min(1000000.0f, max(zoomedSize, 1.0f));
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

float CSSStyleSelector::fontSizeForKeyword(int keyword, bool quirksMode, bool fixed) const
{
    Settings* settings = m_checker.m_document->settings();
    if (!settings)
        return 1.0f;

    int mediumSize = fixed ? settings->defaultFixedFontSize() : settings->defaultFontSize();
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

float CSSStyleSelector::largerFontSize(float size, bool) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level.  
    return size * 1.2f;
}

float CSSStyleSelector::smallerFontSize(float size, bool) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level. 
    return size / 1.2f;
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

Color CSSStyleSelector::getColorFromPrimitiveValue(CSSPrimitiveValue* primitiveValue)
{
    Color col;
    int ident = primitiveValue->getIdent();
    if (ident) {
        if (ident == CSSValueWebkitText)
            col = m_element->document()->textColor();
        else if (ident == CSSValueWebkitLink) {
            const Color& linkColor = m_element->document()->linkColor();
            const Color& visitedColor = m_element->document()->visitedLinkColor();
            if (linkColor == visitedColor)
                col = linkColor;
            else {
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    pseudoState = m_checker.checkPseudoState(m_element);
                col = (pseudoState == PseudoLink) ? linkColor : visitedColor;
            }
        } else if (ident == CSSValueWebkitActivelink)
            col = m_element->document()->activeLinkColor();
        else if (ident == CSSValueWebkitFocusRingColor)
            col = RenderTheme::focusRingColor();
        else if (ident == CSSValueCurrentcolor)
            col = m_style->color();
        else
            col = colorForCSSValue(ident);
    } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR)
        col.setRGB(primitiveValue->getRGBA32Value());
    return col;
}

bool CSSStyleSelector::hasSelectorForAttribute(const AtomicString &attrname)
{
    return m_selectorAttrs.contains(attrname.impl());
}

void CSSStyleSelector::addViewportDependentMediaQueryResult(const MediaQueryExp* expr, bool result)
{
    m_viewportDependentMediaQueryResults.append(new MediaQueryResult(*expr, result));
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

void CSSStyleSelector::SelectorChecker::allVisitedStateChanged()
{
    if (m_linksCheckedForVisitedState.isEmpty())
        return;
    for (Node* node = m_document; node; node = node->traverseNextNode()) {
        if (node->isLink())
            node->setNeedsStyleRecalc();
    }
}

void CSSStyleSelector::SelectorChecker::visitedStateChanged(LinkHash visitedHash)
{
    if (!m_linksCheckedForVisitedState.contains(visitedHash))
        return;
    for (Node* node = m_document; node; node = node->traverseNextNode()) {
        const AtomicString* attr = linkAttribute(node);
        if (attr && visitedLinkHash(m_document->baseURL(), *attr) == visitedHash)
            node->setNeedsStyleRecalc();
    }
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
    float zoomFactor = style ? style->effectiveZoom() : 1;

    TransformOperations operations;
    if (inValue && !inValue->isPrimitiveValue()) {
        CSSValueList* list = static_cast<CSSValueList*>(inValue);
        unsigned size = list->length();
        for (unsigned i = 0; i < size; i++) {
            WebKitCSSTransformValue* val = static_cast<WebKitCSSTransformValue*>(list->itemWithoutBoundsCheck(i));
            
            CSSPrimitiveValue* firstValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(0));
             
            switch (val->operationType()) {
                case WebKitCSSTransformValue::ScaleTransformOperation:
                case WebKitCSSTransformValue::ScaleXTransformOperation:
                case WebKitCSSTransformValue::ScaleYTransformOperation: {
                    double sx = 1.0;
                    double sy = 1.0;
                    if (val->operationType() == WebKitCSSTransformValue::ScaleYTransformOperation)
                        sy = firstValue->getDoubleValue();
                    else { 
                        sx = firstValue->getDoubleValue();
                        if (val->operationType() != WebKitCSSTransformValue::ScaleXTransformOperation) {
                            if (val->length() > 1) {
                                CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(1));
                                sy = secondValue->getDoubleValue();
                            } else 
                                sy = sx;
                        }
                    }
                    operations.operations().append(ScaleTransformOperation::create(sx, sy, 1.0, getTransformOperationType(val->operationType())));
                    break;
                }
                case WebKitCSSTransformValue::ScaleZTransformOperation:
                case WebKitCSSTransformValue::Scale3DTransformOperation: {
                    double sx = 1.0;
                    double sy = 1.0;
                    double sz = 1.0;
                    if (val->operationType() == WebKitCSSTransformValue::ScaleZTransformOperation)
                        sz = firstValue->getDoubleValue();
                    else if (val->operationType() == WebKitCSSTransformValue::ScaleYTransformOperation)
                        sy = firstValue->getDoubleValue();
                    else { 
                        sx = firstValue->getDoubleValue();
                        if (val->operationType() != WebKitCSSTransformValue::ScaleXTransformOperation) {
                            if (val->length() > 2) {
                                CSSPrimitiveValue* thirdValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(2));
                                sz = thirdValue->getDoubleValue();
                            }
                            if (val->length() > 1) {
                                CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(1));
                                sy = secondValue->getDoubleValue();
                            } else 
                                sy = sx;
                        }
                    }
                    operations.operations().append(ScaleTransformOperation::create(sx, sy, sz, getTransformOperationType(val->operationType())));
                    break;
                }
                case WebKitCSSTransformValue::TranslateTransformOperation:
                case WebKitCSSTransformValue::TranslateXTransformOperation:
                case WebKitCSSTransformValue::TranslateYTransformOperation: {
                    bool ok = true;
                    Length tx = Length(0, Fixed);
                    Length ty = Length(0, Fixed);
                    if (val->operationType() == WebKitCSSTransformValue::TranslateYTransformOperation)
                        ty = convertToLength(firstValue, style, rootStyle, zoomFactor, &ok);
                    else { 
                        tx = convertToLength(firstValue, style, rootStyle, zoomFactor, &ok);
                        if (val->operationType() != WebKitCSSTransformValue::TranslateXTransformOperation) {
                            if (val->length() > 1) {
                                CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(1));
                                ty = convertToLength(secondValue, style, rootStyle, zoomFactor, &ok);
                            }
                        }
                    }

                    if (!ok)
                        return false;

                    operations.operations().append(TranslateTransformOperation::create(tx, ty, Length(0, Fixed), getTransformOperationType(val->operationType())));
                    break;
                }
                case WebKitCSSTransformValue::TranslateZTransformOperation:
                case WebKitCSSTransformValue::Translate3DTransformOperation: {
                    bool ok = true;
                    Length tx = Length(0, Fixed);
                    Length ty = Length(0, Fixed);
                    Length tz = Length(0, Fixed);
                    if (val->operationType() == WebKitCSSTransformValue::TranslateZTransformOperation)
                        tz = convertToLength(firstValue, style, rootStyle, zoomFactor, &ok);
                    else if (val->operationType() == WebKitCSSTransformValue::TranslateYTransformOperation)
                        ty = convertToLength(firstValue, style, rootStyle, zoomFactor, &ok);
                    else { 
                        tx = convertToLength(firstValue, style, rootStyle, zoomFactor, &ok);
                        if (val->operationType() != WebKitCSSTransformValue::TranslateXTransformOperation) {
                            if (val->length() > 2) {
                                CSSPrimitiveValue* thirdValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(2));
                                tz = convertToLength(thirdValue, style, rootStyle, zoomFactor, &ok);
                            }
                            if (val->length() > 1) {
                                CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(1));
                                ty = convertToLength(secondValue, style, rootStyle, zoomFactor, &ok);
                            }
                        }
                    }

                    if (!ok)
                        return false;

                    operations.operations().append(TranslateTransformOperation::create(tx, ty, tz, getTransformOperationType(val->operationType())));
                    break;
                }
                case WebKitCSSTransformValue::RotateTransformOperation: {
                    double angle = firstValue->getDoubleValue();
                    if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                        angle = rad2deg(angle);
                    else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                        angle = grad2deg(angle);
                    else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_TURN)
                        angle = turn2deg(angle);
                    
                    operations.operations().append(RotateTransformOperation::create(0, 0, 1, angle, getTransformOperationType(val->operationType())));
                    break;
                }
                case WebKitCSSTransformValue::RotateXTransformOperation:
                case WebKitCSSTransformValue::RotateYTransformOperation:
                case WebKitCSSTransformValue::RotateZTransformOperation: {
                    double x = 0;
                    double y = 0;
                    double z = 0;
                    double angle = firstValue->getDoubleValue();
                    if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                        angle = rad2deg(angle);
                    else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                        angle = grad2deg(angle);
                    
                    if (val->operationType() == WebKitCSSTransformValue::RotateXTransformOperation)
                        x = 1;
                    else if (val->operationType() == WebKitCSSTransformValue::RotateYTransformOperation)
                        y = 1;
                    else
                        z = 1;
                    operations.operations().append(RotateTransformOperation::create(x, y, z, angle, getTransformOperationType(val->operationType())));
                    break;
                }
                case WebKitCSSTransformValue::Rotate3DTransformOperation: {
                    CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(1));
                    CSSPrimitiveValue* thirdValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(2));
                    CSSPrimitiveValue* fourthValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(3));
                    double x = firstValue->getDoubleValue();
                    double y = secondValue->getDoubleValue();
                    double z = thirdValue->getDoubleValue();
                    double angle = fourthValue->getDoubleValue();
                    if (fourthValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                        angle = rad2deg(angle);
                    else if (fourthValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                        angle = grad2deg(angle);
                    operations.operations().append(RotateTransformOperation::create(x, y, z, angle, getTransformOperationType(val->operationType())));
                    break;
                }
                case WebKitCSSTransformValue::SkewTransformOperation:
                case WebKitCSSTransformValue::SkewXTransformOperation:
                case WebKitCSSTransformValue::SkewYTransformOperation: {
                    double angleX = 0;
                    double angleY = 0;
                    double angle = firstValue->getDoubleValue();
                    if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                        angle = rad2deg(angle);
                    else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                        angle = grad2deg(angle);
                    else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_TURN)
                        angle = turn2deg(angle);
                    if (val->operationType() == WebKitCSSTransformValue::SkewYTransformOperation)
                        angleY = angle;
                    else {
                        angleX = angle;
                        if (val->operationType() == WebKitCSSTransformValue::SkewTransformOperation) {
                            if (val->length() > 1) {
                                CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(1));
                                angleY = secondValue->getDoubleValue();
                                if (secondValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                                    angleY = rad2deg(angleY);
                                else if (secondValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                                    angleY = grad2deg(angleY);
                                else if (secondValue->primitiveType() == CSSPrimitiveValue::CSS_TURN)
                                    angleY = turn2deg(angleY);
                            }
                        }
                    }
                    operations.operations().append(SkewTransformOperation::create(angleX, angleY, getTransformOperationType(val->operationType())));
                    break;
                }
                case WebKitCSSTransformValue::MatrixTransformOperation: {
                    double a = firstValue->getDoubleValue();
                    double b = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(1))->getDoubleValue();
                    double c = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(2))->getDoubleValue();
                    double d = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(3))->getDoubleValue();
                    double e = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(4))->getDoubleValue();
                    double f = static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(5))->getDoubleValue();
                    operations.operations().append(MatrixTransformOperation::create(a, b, c, d, e, f));
                    break;
                }
                case WebKitCSSTransformValue::Matrix3DTransformOperation: {
                    TransformationMatrix matrix(static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(0))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(1))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(2))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(3))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(4))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(5))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(6))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(7))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(8))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(9))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(10))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(11))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(12))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(13))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(14))->getDoubleValue(),
                                       static_cast<CSSPrimitiveValue*>(val->itemWithoutBoundsCheck(15))->getDoubleValue());
                    operations.operations().append(Matrix3DTransformOperation::create(matrix));
                    break;
                }   
                case WebKitCSSTransformValue::PerspectiveTransformOperation: {
                    double p = firstValue->getDoubleValue();
                    if (p < 0.0)
                        return false;
                    operations.operations().append(PerspectiveTransformOperation::create(p));
                    break;
                }
                case WebKitCSSTransformValue::UnknownTransformOperation:
                    ASSERT_NOT_REACHED();
                    break;
            }
        }
    }
    outOperations = operations;
    return true;
}

} // namespace WebCore
