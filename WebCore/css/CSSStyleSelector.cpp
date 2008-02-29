/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
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
#include "CSSImageValue.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CachedImage.h"
#include "Counter.h"
#include "DashboardRegion.h"
#include "FontFamilyValue.h"
#include "FontValue.h"
#include "Frame.h"
#include "FrameView.h"
#include "GlobalHistory.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "Pair.h"
#include "Rect.h"
#include "RenderTheme.h"
#include "SelectionController.h"
#include "Settings.h"
#include "ShadowValue.h"
#include "StyleSheetList.h"
#include "Text.h"
#include "UserAgentStyleSheets.h"
#include "XMLNames.h"
#include "loader.h"
#include <wtf/Vector.h>

#if ENABLE(SVG)
#include "XLinkNames.h"
#include "SVGNames.h"
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

#define HANDLE_MULTILAYER_INHERIT_AND_INITIAL(layerType, LayerType, prop, Prop) \
if (isInherit) { \
    LayerType* currChild = m_style->access##LayerType##s(); \
    LayerType* prevChild = 0; \
    const LayerType* currParent = m_parentStyle->layerType##s(); \
    while (currParent && currParent->is##Prop##Set()) { \
        if (!currChild) { \
            /* Need to make a new layer.*/ \
            currChild = new LayerType(); \
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
    return; \
} \
if (isInitial) { \
    LayerType* currChild = m_style->access##LayerType##s(); \
    currChild->set##Prop(RenderStyle::initial##Prop()); \
    for (currChild = currChild->next(); currChild; currChild = currChild->next()) \
        currChild->clear##Prop(); \
    return; \
}

#define HANDLE_MULTILAYER_VALUE(layerType, LayerType, prop, Prop, value) { \
HANDLE_MULTILAYER_INHERIT_AND_INITIAL(layerType, LayerType, prop, Prop) \
LayerType* currChild = m_style->access##LayerType##s(); \
LayerType* prevChild = 0; \
if (value->isValueList()) { \
    /* Walk each value and put it into a layer, creating new layers as needed. */ \
    CSSValueList* valueList = static_cast<CSSValueList*>(value); \
    for (unsigned int i = 0; i < valueList->length(); i++) { \
        if (!currChild) { \
            /* Need to make a new layer to hold this value */ \
            currChild = new LayerType(); \
            prevChild->setNext(currChild); \
        } \
        map##Prop(currChild, valueList->item(i)); \
        prevChild = currChild; \
        currChild = currChild->next(); \
    } \
} else { \
    map##Prop(currChild, value); \
    currChild = currChild->next(); \
} \
while (currChild) { \
    /* Reset all remaining layers to not have the property set. */ \
    currChild->clear##Prop(); \
    currChild = currChild->next(); \
} }

#define HANDLE_BACKGROUND_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_MULTILAYER_INHERIT_AND_INITIAL(backgroundLayer, BackgroundLayer, prop, Prop)

#define HANDLE_BACKGROUND_VALUE(prop, Prop, value) \
HANDLE_MULTILAYER_VALUE(backgroundLayer, BackgroundLayer, prop, Prop, value)

#define HANDLE_TRANSITION_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_MULTILAYER_INHERIT_AND_INITIAL(transition, Transition, prop, Prop)

#define HANDLE_TRANSITION_VALUE(prop, Prop, value) \
HANDLE_MULTILAYER_VALUE(transition, Transition, prop, Prop, value)

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if (id == propID) { \
    m_style->set##Prop(m_parentStyle->prop()); \
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

class CSSRuleSet
{
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

CSSRuleSet* CSSStyleSelector::m_defaultStyle = 0;
CSSRuleSet* CSSStyleSelector::m_defaultQuirksStyle = 0;
CSSRuleSet* CSSStyleSelector::m_defaultPrintStyle = 0;
CSSRuleSet* CSSStyleSelector::m_defaultViewSourceStyle = 0;

CSSStyleSheet* CSSStyleSelector::m_defaultSheet = 0;
RenderStyle* CSSStyleSelector::m_styleNotYetAvailable = 0;
CSSStyleSheet* CSSStyleSelector::m_quirksSheet = 0;
CSSStyleSheet* CSSStyleSelector::m_viewSourceSheet = 0;

#if ENABLE(SVG)
CSSStyleSheet *CSSStyleSelector::m_svgSheet = 0;
#endif

static CSSStyleSelector::EncodedURL* currentEncodedURL = 0;
static PseudoState pseudoState;

static const MediaQueryEvaluator& screenEval()
{
    static const MediaQueryEvaluator staticScreenEval("screen");
    return staticScreenEval;
}

static const MediaQueryEvaluator& printEval()
{
    static const MediaQueryEvaluator staticPrintEval("print");
    return staticPrintEval;
}

CSSStyleSelector::CSSStyleSelector(Document* doc, const String& userStyleSheet, StyleSheetList *styleSheets, CSSStyleSheet* mappedElementSheet, bool _strictParsing, bool matchAuthorAndUserStyles)
{
    init();
    
    m_document = doc;
    m_fontSelector = new CSSFontSelector(doc);

    m_matchAuthorAndUserStyles = matchAuthorAndUserStyles;

    strictParsing = _strictParsing;
    if (!m_defaultStyle)
        loadDefaultStyle();

    m_userStyle = 0;

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = m_document->view();
    if (view)
        m_medium = new MediaQueryEvaluator(view->mediaType());
    else
        m_medium = new MediaQueryEvaluator("all");

    Element* root = doc->documentElement();

    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, false, true); // dont ref, because the RenderStyle is allocated from global heap

    if (m_rootDefaultStyle && view) {
        delete m_medium;
        m_medium = new MediaQueryEvaluator(view->mediaType(), view->frame(), m_rootDefaultStyle);
    }

    // FIXME: This sucks! The user sheet is reparsed every time!
    if (!userStyleSheet.isEmpty()) {
        m_userSheet = new CSSStyleSheet(doc);
        m_userSheet->parseString(userStyleSheet, strictParsing);

        m_userStyle = new CSSRuleSet();
        m_userStyle->addRulesFromSheet(m_userSheet.get(), *m_medium, this);
    }

    // add stylesheets from document
    m_authorStyle = new CSSRuleSet();
    
    // Add rules from elments like SVG's <font-face>
    if (mappedElementSheet)
        m_authorStyle->addRulesFromSheet(mappedElementSheet, *m_medium, this);

    DeprecatedPtrListIterator<StyleSheet> it(styleSheets->styleSheets);
    for (; it.current(); ++it)
        if (it.current()->isCSSStyleSheet() && !it.current()->disabled())
            m_authorStyle->addRulesFromSheet(static_cast<CSSStyleSheet*>(it.current()), *m_medium, this);
}

void CSSStyleSelector::init()
{
    m_element = 0;
    m_matchedDecls.clear();
    m_ruleList = 0;
    m_collectRulesOnly = false;
    m_rootDefaultStyle = 0;
    m_medium = 0;
}

void CSSStyleSelector::setEncodedURL(const KURL& url)
{
    KURL u = url;

    u.setQuery(String());
    u.setRef(String());
    m_encodedURL.file = u.string();
    int pos = m_encodedURL.file.reverseFind('/');
    m_encodedURL.path = m_encodedURL.file;
    if (pos > 0) {
        m_encodedURL.path.truncate(pos);
        m_encodedURL.path.append('/');
    }
    u.setPath(String());
    m_encodedURL.prefix = u.string();
}

CSSStyleSelector::~CSSStyleSelector()
{
    delete m_medium;
    ::delete m_rootDefaultStyle;

    delete m_authorStyle;
    delete m_userStyle;
}

static CSSStyleSheet* parseUASheet(const char* characters, unsigned size)
{
    CSSStyleSheet* const parent = 0;
    CSSStyleSheet* sheet = new CSSStyleSheet(parent);
    sheet->ref(); // leak the sheet on purpose since it will be stored in a global variable
    sheet->parseString(String(characters, size));
    return sheet;
}

template<typename T> CSSStyleSheet* parseUASheet(const T& array)
{
    return parseUASheet(array, sizeof(array));
}

void CSSStyleSelector::loadDefaultStyle()
{
    if (m_defaultStyle)
        return;

    m_defaultStyle = new CSSRuleSet;
    m_defaultPrintStyle = new CSSRuleSet;
    m_defaultQuirksStyle = new CSSRuleSet;
    m_defaultViewSourceStyle = new CSSRuleSet;

    // Strict-mode rules.
    m_defaultSheet = parseUASheet(html4UserAgentStyleSheet);
    m_defaultStyle->addRulesFromSheet(m_defaultSheet, screenEval());
    m_defaultPrintStyle->addRulesFromSheet(m_defaultSheet, printEval());

    // Quirks-mode rules.
    m_quirksSheet = parseUASheet(quirksUserAgentStyleSheet);
    m_defaultQuirksStyle->addRulesFromSheet(m_quirksSheet, screenEval());
    
    // View source rules.
    m_viewSourceSheet = parseUASheet(sourceUserAgentStyleSheet);
    m_defaultViewSourceStyle->addRulesFromSheet(m_viewSourceSheet, screenEval());
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
        const ClassNames& classNames = *m_element->getClassNames();
        size_t classNamesSize = classNames.size();
        for (size_t i = 0; i < classNamesSize; ++i)
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
    if (!m_collectRulesOnly) {
        for (unsigned i = 0; i < m_matchedRules.size(); i++)
            addMatchedDeclaration(m_matchedRules[i]->rule()->declaration());
    } else {
        for (unsigned i = 0; i < m_matchedRules.size(); i++) {
            if (!m_ruleList)
                m_ruleList = new CSSRuleList();
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
            if (dynamicPseudo != RenderStyle::NOPSEUDO && m_pseudoStyle == RenderStyle::NOPSEUDO) {
                if (m_collectRulesOnly)
                    return;
                if (dynamicPseudo < RenderStyle::FIRST_INTERNAL_PSEUDOID)
                    m_style->setHasPseudoStyle(dynamicPseudo);
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

bool operator >(CSSRuleData& r1, CSSRuleData& r2)
{
    int spec1 = r1.selector()->specificity();
    int spec2 = r2.selector()->specificity();
    return (spec1 == spec2) ? r1.position() > r2.position() : spec1 > spec2; 
}
bool operator <=(CSSRuleData& r1, CSSRuleData& r2)
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
    rulesMergeBuffer.reserveCapacity(end - start); 

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
    currentEncodedURL = &m_encodedURL;
    pseudoState = PseudoUnknown;
}

void CSSStyleSelector::initForStyleResolve(Element* e, RenderStyle* defaultParent)
{
    // set some variables we will need
    m_pseudoStyle = RenderStyle::NOPSEUDO;

    m_parentNode = e->parentNode();

#if ENABLE(SVG)
    if (!m_parentNode && e->isSVGElement() && e->isShadowNode())
        m_parentNode = e->shadowParentNode();
#endif

    if (defaultParent)
        m_parentStyle = defaultParent;
    else
        m_parentStyle = m_parentNode ? m_parentNode->renderStyle() : 0;
    m_isXMLDoc = !m_element->document()->isHTMLDocument();

    m_style = 0;
    
    m_matchedDecls.clear();

    m_ruleList = 0;

    m_fontDirty = false;
}

static inline int findSlashDotDotSlash(const UChar* characters, size_t length)
{
    unsigned loopLimit = length < 4 ? 0 : length - 3;
    for (unsigned i = 0; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '.' && characters[i + 2] == '.' && characters[i + 3] == '/')
            return i;
    }
    return -1;
}

static inline int findSlashSlash(const UChar* characters, size_t length, int position)
{
    unsigned loopLimit = length < 2 ? 0 : length - 1;
    for (unsigned i = position; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '/')
            return i;
    }
    return -1;
}

static inline int findSlashDotSlash(const UChar* characters, size_t length)
{
    unsigned loopLimit = length < 3 ? 0 : length - 2;
    for (unsigned i = 0; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '.' && characters[i + 2] == '/')
            return i;
    }
    return -1;
}

static inline bool containsColonSlashSlash(const UChar* characters, unsigned length)
{
    unsigned loopLimit = length < 3 ? 0 : length - 2;
    for (unsigned i = 0; i < loopLimit; ++i)
        if (characters[i] == ':' && characters[i + 1] == '/' && characters[i + 2] == '/')
            return true;
    return false;
}

static void cleanPath(Vector<UChar, 512>& path)
{
    int pos;
    while ((pos = findSlashDotDotSlash(path.data(), path.size())) != -1) {
        int prev = reverseFind(path.data(), path.size(), '/', pos - 1);
        // don't remove the host, i.e. http://foo.org/../foo.html
        if (prev < 0 || (prev > 3 && path[prev - 2] == ':' && path[prev - 1] == '/'))
            path.remove(pos, 3);
        else
            path.remove(prev, pos - prev + 3);
    }

    // Don't remove "//" from an anchor identifier. -rjw
    // Set refPos to -2 to mean "I haven't looked for the anchor yet".
    // We don't want to waste a function call on the search for the the anchor
    // in the vast majority of cases where there is no "//" in the path.
    pos = 0;
    int refPos = -2;
    while ((pos = findSlashSlash(path.data(), path.size(), pos)) != -1) {
        if (refPos == -2)
            refPos = find(path.data(), path.size(), '#');
        if (refPos > 0 && pos >= refPos)
            break;

        if (pos == 0 || path[pos - 1] != ':')
            path.remove(pos);
        else
            pos += 2;
    }

    // FIXME: We don't want to remove "/./" from an anchor identifier either.
    while ((pos = findSlashDotSlash(path.data(), path.size())) != -1)
        path.remove(pos, 2);
}

static void checkPseudoState(Element *e, bool checkVisited = true)
{
    if (!e->isLink()) {
        pseudoState = PseudoNone;
        return;
    }

    const AtomicString* attr;
    if (e->isHTMLElement())
        attr = &e->getAttribute(hrefAttr);
#if ENABLE(SVG)
    else if (e->isSVGElement())
        attr = &e->getAttribute(XLinkNames::hrefAttr);
#endif
    else {
        pseudoState = PseudoNone;
        return;
    }

    if (attr->isNull()) {
        pseudoState = PseudoNone;
        return;
    }

    if (!checkVisited) {
        pseudoState = PseudoAnyLink;
        return;
    }

    const UChar* characters = attr->characters();
    unsigned length = attr->length();

    if (containsColonSlashSlash(characters, length)) {
        // FIXME: Strange to not clean the path just beacause it has "://" in it.
        pseudoState = historyContains(characters, length) ? PseudoVisited : PseudoLink;
        return;
    }

    Vector<UChar, 512> buffer;
    if (length && characters[0] == '/') {
        buffer.append(currentEncodedURL->prefix.characters(), currentEncodedURL->prefix.length());
    } else if (length && characters[0] == '#') {
        buffer.append(currentEncodedURL->file.characters(), currentEncodedURL->file.length());
    } else {
        buffer.append(currentEncodedURL->path.characters(), currentEncodedURL->path.length());
    }
    buffer.append(characters, length);
    cleanPath(buffer);
    pseudoState = historyContains(buffer.data(), buffer.size()) ? PseudoVisited : PseudoLink;
}

// a helper function for parsing nth-arguments
static bool parseNth(const String& nth, int &a, int &b)
{
    if (nth.isEmpty())
        return false;
    a = 0;
    b = 0;
    if (nth == "odd") {
        a = 2;
        b = 1;
    } else if (nth == "even") {
        a = 2;
        b = 0;
    } else {
        int n = nth.find('n');
        if (n != -1) {
            if (nth[0] == '-') {
                if (n == 1)
                    a = -1; // -n == -1n
                else
                    a = nth.substring(0, n).toInt();
            } else if (!n)
                a = 1; // n == 1n
            else
                a = nth.substring(0, n).toInt();

            int p = nth.find('+', n);
            if (p != -1)
                b = nth.substring(p + 1, nth.length() - p - 1).toInt();
            else {
                p = nth.find('-', n);
                b = -nth.substring(p + 1, nth.length() - p - 1).toInt();
            }
        } else
            b = nth.toInt();
    }
    return true;
}

// a helper function for checking nth-arguments
static bool matchNth(int count, int a, int b)
{
    if (!a)
        return count == b;
    else if (a > 0) {
        if (count < b)
            return false;
        return (count - b) % a == 0;
    } else {
        if (count > b)
            return false;
        return (b - count) % (-a) == 0;
    }
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
                r = locateCousinList(static_cast<Element*>(parent->parentNode()), depth + 1);
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
            (s != s->document()->getCSSTarget() && m_element != m_element->document()->getCSSTarget()) &&
            (s->getAttribute(typeAttr) == m_element->getAttribute(typeAttr)) &&
            (s->getAttribute(XMLNames::langAttr) == m_element->getAttribute(XMLNames::langAttr)) &&
            (s->getAttribute(langAttr) == m_element->getAttribute(langAttr)) &&
            (s->getAttribute(readonlyAttr) == m_element->getAttribute(readonlyAttr)) &&
            (s->getAttribute(cellpaddingAttr) == m_element->getAttribute(cellpaddingAttr))) {
            bool isControl = s->isControl();
            if (isControl != m_element->isControl())
                return false;
            if (isControl && (s->isEnabled() != m_element->isEnabled()) ||
                             (s->isIndeterminate() != m_element->isIndeterminate()) ||
                             (s->isChecked() != m_element->isChecked()))
                return false;
            
            if (style->transitions())
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
                        Color linkColor = m_element->document()->linkColor();
                        Color visitedColor = m_element->document()->visitedLinkColor();
                        if (pseudoState == PseudoUnknown)
                            checkPseudoState(m_element, style->pseudoState() != PseudoAnyLink || linkColor != visitedColor);
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
            n = locateCousinList(static_cast<Element*>(m_element->parentNode()));
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
        ? m_defaultPrintStyle : m_defaultStyle;
    matchRules(userAgentStyleSheet, firstUARule, lastUARule);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (!strictParsing)
        matchRules(m_defaultQuirksStyle, firstUARule, lastUARule);
        
    // If we're in view source mode, then we match rules from the view source style sheet.
    if (m_document->frame() && m_document->frame()->inViewSourceMode())
        matchRules(m_defaultViewSourceStyle, firstUARule, lastUARule);
}

// If resolveForRootDefault is true, style based on user agent style sheet only. This is used in media queries, where
// relative units are interpreted according to document root element style, styled only with UA stylesheet

RenderStyle* CSSStyleSelector::styleForElement(Element* e, RenderStyle* defaultParent, bool allowSharing, bool resolveForRootDefault)
{
    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (allowSharing && !e->document()->haveStylesheetsLoaded() && !e->renderer()) {
        if (!m_styleNotYetAvailable) {
            m_styleNotYetAvailable = ::new RenderStyle;
            m_styleNotYetAvailable->ref();
            m_styleNotYetAvailable->setDisplay(NONE);
            m_styleNotYetAvailable->font().update(m_fontSelector);
        }
        m_styleNotYetAvailable->ref();
        e->document()->setHasNodesWithPlaceholderStyle();
        return m_styleNotYetAvailable;
    }
    
    initElementAndPseudoState(e);
    if (allowSharing) {
        m_style = locateSharedStyle();
#ifdef STYLE_SHARING_STATS
        fraction += m_style != 0;
        total++;
        printf("Sharing %d out of %d\n", fraction, total);
#endif
        if (m_style) {
            m_style->ref();
            return m_style;
        }
    }
    initForStyleResolve(e, defaultParent);

    if (resolveForRootDefault) {
        m_style = ::new RenderStyle();
        // don't ref, because we want to delete this, but we cannot unref it
    } else {
        m_style = new (e->document()->renderArena()) RenderStyle();
        m_style->ref();
    }
    if (m_parentStyle)
        m_style->inheritFrom(m_parentStyle);
    else
        m_parentStyle = m_style;

#if ENABLE(SVG)
    if (e->isSVGElement() && !m_svgSheet) {
        // SVG rules.
        m_svgSheet = parseUASheet(svgUserAgentStyleSheet);
        m_defaultStyle->addRulesFromSheet(m_svgSheet, screenEval());
        m_defaultPrintStyle->addRulesFromSheet(m_svgSheet, printEval());
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
                    MappedAttribute* attr = map->attributeItem(i);
                    if (attr->decl()) {
                        lastAuthorRule = m_matchedDecls.size();
                        if (firstAuthorRule == -1)
                            firstAuthorRule = lastAuthorRule;
                        addMatchedDeclaration(attr->decl());
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
        applyProperty(CSS_PROP_LINE_HEIGHT, m_lineHeightValue);

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
    adjustRenderStyle(m_style, e);

    // If we are a link, cache the determined pseudo-state.
    if (e->isLink())
        m_style->setPseudoState(pseudoState);

    // If we have first-letter pseudo style, do not share this style
    if (m_style->hasPseudoStyle(RenderStyle::FIRST_LETTER))
        m_style->setUnique();

    // Now return the style.
    return m_style;
}

RenderStyle* CSSStyleSelector::pseudoStyleForElement(RenderStyle::PseudoId pseudo, Element* e, RenderStyle* parentStyle)
{
    if (!e)
        return 0;

    initElementAndPseudoState(e);
    initForStyleResolve(e, parentStyle);
    m_pseudoStyle = pseudo;
    
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
    
    m_style = new (e->document()->renderArena()) RenderStyle();
    m_style->ref();
    if (parentStyle)
        m_style->inheritFrom(parentStyle);
    else
        parentStyle = m_style;
    m_style->noninherited_flags._styleType = m_pseudoStyle;
    
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
        applyProperty(CSS_PROP_LINE_HEIGHT, m_lineHeightValue);
    
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
    adjustRenderStyle(m_style, 0);

    // Now return the style.
    return m_style;
}

static void addIntrinsicMargins(RenderStyle* style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = 2;
    
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
        if (!strictParsing && e) {
            if (e->hasTagName(tdTag)) {
                style->setDisplay(TABLE_CELL);
                style->setFloating(FNONE);
            }
            else if (e->hasTagName(tableTag))
                style->setDisplay(style->isDisplayInlineType() ? INLINE_TABLE : TABLE);
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
                if (!strictParsing && style->floating() != FNONE)
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

    // Make sure our z-index value is only applied if the object is positioned,
    // relatively positioned, transparent, or has a transform.
    if (style->position() == StaticPosition && style->opacity() == 1.0f && !style->hasTransform())
        style->setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects.  This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them.  Auto z-index also becomes 0 for objects that specify transforms.
    if (style->hasAutoZIndex() && ((e && e->document()->documentElement() == e) || style->opacity() < 1.0f || style->hasTransform()))
        style->setZIndex(0);
    
    // Button, legend, input, select and textarea all consider width values of 'auto' to be 'intrinsic'.
    // This will be important when we use block flows for all form controls.
    if (e && (e->hasTagName(legendTag) || e->hasTagName(buttonTag) || e->hasTagName(inputTag) ||
              e->hasTagName(selectTag) || e->hasTagName(textareaTag))) {
        if (style->width().isAuto())
            style->setWidth(Length(Intrinsic));
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

    // Cull out any useless layers and also repeat patterns into additional layers.
    style->adjustBackgroundLayers();

    // Do the same for transitions.
    style->adjustTransitions();

    // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
    // alter fonts and heights/widths.
    if (e && e->isControl() && style->fontSize() >= 11) {
        // Don't apply intrinsic margins to image buttons.  The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!e->hasTagName(inputTag) || static_cast<HTMLInputElement*>(e)->inputType() != HTMLInputElement::IMAGE)
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style->hasAppearance())
        theme()->adjustStyle(this, style, e, m_hasUAAppearance, m_borderData, m_backgroundData, m_backgroundColor);

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
    checkForGenericFamilyChange(m_style, m_parentStyle);
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

RefPtr<CSSRuleList> CSSStyleSelector::styleRulesForElement(Element* e, bool authorOnly)
{
    if (!e || !e->document()->haveStylesheetsLoaded())
        return 0;

    m_collectRulesOnly = true;
    
    initElementAndPseudoState(e);
    initForStyleResolve(e, 0);
    
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

    m_collectRulesOnly = false;
    
    return m_ruleList;
}

RefPtr<CSSRuleList> CSSStyleSelector::pseudoStyleRulesForElement(Element* e, StringImpl* pseudoStyle, bool authorOnly)
{
    // FIXME: Implement this.
    return 0;
}

bool CSSStyleSelector::checkSelector(CSSSelector* sel)
{
    dynamicPseudo = RenderStyle::NOPSEUDO;

    // Check the selector
    SelectorMatch match = checkSelector(sel, m_element, true, false);
    if (match != SelectorMatches)
        return false;

    if (m_pseudoStyle != RenderStyle::NOPSEUDO && m_pseudoStyle != dynamicPseudo)
        return false;

    return true;
}

// Recursive check of selectors and combinators
// It can return 3 different values:
// * SelectorMatches         - the selector matches the element e
// * SelectorFailsLocally    - the selector fails for the element e
// * SelectorFailsCompletely - the selector fails for e and any sibling or ancestor of e
CSSStyleSelector::SelectorMatch CSSStyleSelector::checkSelector(CSSSelector* sel, Element* e, bool isAncestor, bool isSubSelector)
{
#if ENABLE(SVG)
    // Spec: CSS2 selectors cannot be applied to the (conceptually) cloned DOM tree
    // because its contents are not part of the formal document structure.
    if (e->isSVGElement() && e->isShadowNode())
        return SelectorFailsCompletely;
#endif

    // first selector has to match
    if (!checkOneSelector(sel, e, isAncestor, isSubSelector))
        return SelectorFailsLocally;

    // The rest of the selectors has to match
    CSSSelector::Relation relation = sel->relation();

    // Prepare next sel
    sel = sel->m_tagHistory;
    if (!sel)
        return SelectorMatches;

    if (relation != CSSSelector::SubSelector)
        // Bail-out if this selector is irrelevant for the pseudoStyle
        if (m_pseudoStyle != RenderStyle::NOPSEUDO && m_pseudoStyle != dynamicPseudo)
            return SelectorFailsCompletely;

    switch (relation) {
        case CSSSelector::Descendant:
            while (true) {
                Node* n = e->parentNode();
                if (!n || !n->isElementNode())
                    return SelectorFailsCompletely;
                e = static_cast<Element*>(n);
                SelectorMatch match = checkSelector(sel, e, true, false);
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
            return checkSelector(sel, e, true, false);
        }
        case CSSSelector::DirectAdjacent:
        {
            if (!m_collectRulesOnly && e->parentNode() && e->parentNode()->isElementNode()) {
                RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : e->parentNode()->renderStyle();
                if (parentStyle)
                    parentStyle->setChildrenAffectedByDirectAdjacentRules();
            }
            Node* n = e->previousSibling();
            while (n && !n->isElementNode())
                n = n->previousSibling();
            if (!n)
                return SelectorFailsLocally;
            e = static_cast<Element*>(n);
            return checkSelector(sel, e, false, false); 
        }
        case CSSSelector::IndirectAdjacent:
            if (!m_collectRulesOnly && e->parentNode() && e->parentNode()->isElementNode()) {
                RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : e->parentNode()->renderStyle();
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
                SelectorMatch match = checkSelector(sel, e, false, false);
                if (match != SelectorFailsLocally)
                    return match;
            };
            break;
        case CSSSelector::SubSelector:
            // a selector is invalid if something follows a pseudo-element
            if (e == m_element && dynamicPseudo != RenderStyle::NOPSEUDO)
                return SelectorFailsCompletely;
            return checkSelector(sel, e, isAncestor, true);
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

bool CSSStyleSelector::checkOneSelector(CSSSelector* sel, Element* e, bool isAncestor, bool isSubSelector)
{
    if (!e)
        return false;

    if (sel->hasTag()) {
        const AtomicString& localName = e->localName();
        const AtomicString& ns = e->namespaceURI();
        const AtomicString& selLocalName = sel->m_tag.localName();
        const AtomicString& selNS = sel->m_tag.namespaceURI();
    
        if ((selLocalName != starAtom && localName != selLocalName) ||
            (selNS != starAtom && ns != selNS))
            return false;
    }

    if (sel->hasAttribute()) {
        if (sel->m_match == CSSSelector::Class) {
            if (!e->hasClass())
                return false;
            return e->getClassNames()->contains(sel->m_value);
        } else if (sel->m_match == CSSSelector::Id)
            return e->hasID() && e->getIDAttribute() == sel->m_value;
        else if (m_style && (e != m_element || !m_styledElement || (!m_styledElement->isMappedAttribute(sel->m_attr) && sel->m_attr != typeAttr && sel->m_attr != readonlyAttr))) {
            m_style->setAffectedByAttributeSelectors(); // Special-case the "type" and "readonly" attributes so input form controls can share style.
            m_selectorAttrs.add(sel->m_attr.localName().impl());
        }

        const AtomicString& value = e->getAttribute(sel->m_attr);
        if (value.isNull())
            return false; // attribute is not set

        bool caseSensitive = m_isXMLDoc || !htmlAttributeHasCaseInsensitiveValue(sel->m_attr);

        switch (sel->m_match) {
        case CSSSelector::Exact:
            if (caseSensitive ? sel->m_value != value : !equalIgnoringCase(sel->m_value, value))
                return false;
            break;
        case CSSSelector::List:
        {
            // The selector's value can't contain a space, or it's totally bogus.
            if (sel->m_value.contains(' '))
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
            if (!value.contains(sel->m_value, caseSensitive))
                return false;
            break;
        case CSSSelector::Begin:
            if (!value.startsWith(sel->m_value, caseSensitive))
                return false;
            break;
        case CSSSelector::End:
            if (!value.endsWith(sel->m_value, caseSensitive))
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
        switch (sel->pseudoType()) {
            // Pseudo classes:
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
                    if (m_element == e && m_style)
                        m_style->setEmptyState(result);
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
                        RenderStyle* childStyle = (m_element == e) ? m_style : e->renderStyle();
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : e->parentNode()->renderStyle();
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
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : e->parentNode()->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoLastChild: {
                // last-child matches the last child that is an element
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    bool result = false;
                    if (parentNode->isFinishedParsingChildren()) {
                        Node* n = e->nextSibling();
                        while (n && !n->isElementNode())
                            n = n->nextSibling();
                        if (!n)
                            result = true;
                    }
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = (m_element == e) ? m_style : e->renderStyle();
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : parentNode->renderStyle();
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
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : parentNode->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentNode->isFinishedParsingChildren())
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
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    bool firstChild = false;
                    bool lastChild = false;
                    
                    Node* n = e->previousSibling();
                    while (n && !n->isElementNode())
                        n = n->previousSibling();
                    if (!n)
                        firstChild = true;
                    if (firstChild && parentNode->isFinishedParsingChildren()) {
                        n = e->nextSibling();
                        while (n && !n->isElementNode())
                            n = n->nextSibling();
                        if (!n)
                            lastChild = true;
                    }
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = (m_element == e) ? m_style : e->renderStyle();
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : parentNode->renderStyle();
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
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : parentNode->renderStyle();
                        if (parentStyle) {
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                        }
                    }
                    if (!parentNode->isFinishedParsingChildren())
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
                int a, b;
                // calculate a and b every time we run through checkOneSelector
                // this should probably be saved after we calculate it once, but currently
                // would require increasing the size of CSSSelector
                if (!parseNth(sel->m_argument, a, b))
                    break;
                if (e->parentNode() && e->parentNode()->isElementNode()) {
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
                        RenderStyle* childStyle = (m_element == e) ? m_style : e->renderStyle();
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : e->parentNode()->renderStyle();
                        if (childStyle)
                            childStyle->setChildIndex(count);
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }
                    
                    if (matchNth(count, a, b))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthOfType: {
                // FIXME: This selector is very slow.
                int a, b;
                // calculate a and b every time we run through checkOneSelector (see above)
                if (!parseNth(sel->m_argument, a, b))
                    break;
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    int count = 1;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            count++;
                        n = n->previousSibling();
                    }
                    
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : e->parentNode()->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }

                    if (matchNth(count, a, b))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthLastChild: {
                int a, b;
                // calculate a and b every time we run through checkOneSelector
                // this should probably be saved after we calculate it once, but currently
                // would require increasing the size of CSSSelector
                if (!parseNth(sel->m_argument, a, b))
                    break;
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : parentNode->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentNode->isFinishedParsingChildren())
                        return false;
                    int count = 1;
                    Node* n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode())
                            count++;
                        n = n->nextSibling();
                    }
                    if (matchNth(count, a, b))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthLastOfType: {
                // FIXME: This selector is very slow.
                int a, b;
                // calculate a and b every time we run through checkOneSelector (see above)
                if (!parseNth(sel->m_argument, a, b))
                    break;
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = (m_element == e) ? m_parentStyle : parentNode->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentNode->isFinishedParsingChildren())
                        return false;
                    int count = 1;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            count++;
                        n = n->nextSibling();
                    }
                    if (matchNth(count, a, b))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoTarget:
                if (e == e->document()->getCSSTarget())
                    return true;
                break;
            case CSSSelector::PseudoAnyLink:
                if (pseudoState == PseudoUnknown)
                    checkPseudoState(e, false);
                if (pseudoState == PseudoAnyLink || pseudoState == PseudoLink || pseudoState == PseudoVisited)
                    return true;
                break;
            case CSSSelector::PseudoAutofill:
                if (e && e->hasTagName(inputTag))
                    return static_cast<HTMLInputElement*>(e)->autofilled();
                break;
            case CSSSelector::PseudoLink:
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    checkPseudoState(e);
                if (pseudoState == PseudoLink)
                    return true;
                break;
            case CSSSelector::PseudoVisited:
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    checkPseudoState(e);
                if (pseudoState == PseudoVisited)
                    return true;
                break;
            case CSSSelector::PseudoDrag: {
                if (m_element == e && m_style)
                    m_style->setAffectedByDragRules(true);
                    if (m_element != e && e->renderStyle())
                        e->renderStyle()->setAffectedByDragRules(true);
                    if (e->renderer() && e->renderer()->isDragging())
                        return true;
                break;
            }
            case CSSSelector::PseudoFocus:
                if (e && e->focused() && e->document()->frame()->selectionController()->isFocusedAndActive())
                    return true;
                break;
            case CSSSelector::PseudoHover: {
                // If we're in quirks mode, then hover should never match anchors with no
                // href and *:hover should not match anything.  This is important for sites like wsj.com.
                if (strictParsing || isSubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                    if (m_element == e && m_style)
                        m_style->setAffectedByHoverRules(true);
                    if (m_element != e && e->renderStyle())
                        e->renderStyle()->setAffectedByHoverRules(true);
                    if (e->hovered())
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoActive:
                // If we're in quirks mode, then :active should never match anchors with no
                // href and *:active should not match anything. 
                if (strictParsing || isSubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                    if (m_element == e && m_style)
                        m_style->setAffectedByActiveRules(true);
                    else if (e->renderStyle())
                        e->renderStyle()->setAffectedByActiveRules(true);
                    if (e->active())
                        return true;
                }
                break;
            case CSSSelector::PseudoEnabled:
                if (e && e->isControl() && !e->isInputTypeHidden())
                    // The UI spec states that you can't match :enabled unless you are an object that can
                    // "receive focus and be activated."  We will limit matching of this pseudo-class to elements
                    // that are non-"hidden" controls.
                    return e->isEnabled();                    
                break;
            case CSSSelector::PseudoDisabled:
                if (e && e->isControl() && !e->isInputTypeHidden())
                    // The UI spec states that you can't match :enabled unless you are an object that can
                    // "receive focus and be activated."  We will limit matching of this pseudo-class to elements
                    // that are non-"hidden" controls.
                    return !e->isEnabled();                    
                break;
            case CSSSelector::PseudoChecked:
                // Even though WinIE allows checked and indeterminate to co-exist, the CSS selector spec says that
                // you can't be both checked and indeterminate.  We will behave like WinIE behind the scenes and just
                // obey the CSS spec here in the test for matching the pseudo.
                if (e && e->isChecked() && !e->isIndeterminate())
                    return true;
                break;
            case CSSSelector::PseudoIndeterminate:
                if (e && e->isIndeterminate())
                    return true;
                break;
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
                if (value.isEmpty() || !value.startsWith(sel->m_argument, false))
                    break;
                if (value.length() != sel->m_argument.length() && value[sel->m_argument.length()] != '-')
                    break;
                return true;
            }
            case CSSSelector::PseudoNot: {
                // check the simple selector
                for (CSSSelector* subSel = sel->m_simpleSelector; subSel; subSel = subSel->m_tagHistory) {
                    // :not cannot nest. I don't really know why this is a
                    // restriction in CSS3, but it is, so let's honour it.
                    if (subSel->m_simpleSelector)
                        break;
                    if (!checkOneSelector(subSel, e, isAncestor, true))
                        return true;
                }
                break;
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
        if (e != m_element) return false;

        switch (sel->pseudoType()) {
            // Pseudo-elements:
            case CSSSelector::PseudoFirstLine:
                dynamicPseudo = RenderStyle::FIRST_LINE;
                return true;
            case CSSSelector::PseudoFirstLetter:
                dynamicPseudo = RenderStyle::FIRST_LETTER;
                if (Document* doc = e->document())
                    doc->setUsesFirstLetterRules(true);
                return true;
            case CSSSelector::PseudoSelection:
                dynamicPseudo = RenderStyle::SELECTION;
                return true;
            case CSSSelector::PseudoBefore:
                dynamicPseudo = RenderStyle::BEFORE;
                return true;
            case CSSSelector::PseudoAfter:
                dynamicPseudo = RenderStyle::AFTER;
                return true;
            case CSSSelector::PseudoFileUploadButton:
                dynamicPseudo = RenderStyle::FILE_UPLOAD_BUTTON;
                return true;
            case CSSSelector::PseudoSliderThumb:
                dynamicPseudo = RenderStyle::SLIDER_THUMB;
                return true; 
            case CSSSelector::PseudoSearchCancelButton:
                dynamicPseudo = RenderStyle::SEARCH_CANCEL_BUTTON;
                return true; 
            case CSSSelector::PseudoSearchDecoration:
                dynamicPseudo = RenderStyle::SEARCH_DECORATION;
                return true;
            case CSSSelector::PseudoSearchResultsDecoration:
                dynamicPseudo = RenderStyle::SEARCH_RESULTS_DECORATION;
                return true;
            case CSSSelector::PseudoSearchResultsButton:
                dynamicPseudo = RenderStyle::SEARCH_RESULTS_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsPanel:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_PANEL;
                return true;
            case CSSSelector::PseudoMediaControlsMuteButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_MUTE_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsPlayButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_PLAY_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsTimeDisplay:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_TIME_DISPLAY;
                return true;
            case CSSSelector::PseudoMediaControlsTimeline:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_TIMELINE;
                return true;
            case CSSSelector::PseudoMediaControlsSeekBackButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_SEEK_BACK_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsSeekForwardButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_SEEK_FORWARD_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsFullscreenButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_FULLSCREEN_BUTTON;
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
    if (!sheet || !sheet->isCSSStyleSheet())
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
            for (CSSSelector* s = rule->selector(); s; s = s->next())
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
                        for (CSSSelector* s = rule->selector(); s; s = s->next())
                            addRule(rule, s);
                    } else if (item->isFontFaceRule() && styleSelector) {
                        // Add this font face to our set.
                        const CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(item);
                        styleSelector->fontSelector()->addFontFaceRule(fontFaceRule);
                    }
                }   // for rules
            }   // if rules
        } else if (item->isFontFaceRule() && styleSelector) {
            // Add this font face to our set.
            const CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(item);
            styleSelector->fontSelector()->addFontFaceRule(fontFaceRule);
        }
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

static Length convertToLength(CSSPrimitiveValue *primitiveValue, RenderStyle *style, bool *ok = 0)
{
    Length l;
    if (!primitiveValue) {
        if (ok)
            *ok = false;
    } else {
        int type = primitiveValue->primitiveType();
        if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(style), Fixed);
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
    if (startIndex == -1) return;
    for (int i = startIndex; i <= endIndex; i++) {
        CSSMutableStyleDeclaration* decl = m_matchedDecls[i];
        DeprecatedValueListConstIterator<CSSProperty> end;
        for (DeprecatedValueListConstIterator<CSSProperty> it = decl->valuesIterator(); it != end; ++it) {
            const CSSProperty& current = *it;
            // give special priority to font-xxx, color properties
            if (isImportant == current.isImportant()) {
                bool first;
                switch (current.id()) {
                    case CSS_PROP_LINE_HEIGHT:
                        m_lineHeightValue = current.value();
                        first = !applyFirst; // we apply line-height later
                        break;
                    case CSS_PROP_COLOR:
                    case CSS_PROP_DIRECTION:
                    case CSS_PROP_DISPLAY:
                    case CSS_PROP_FONT:
                    case CSS_PROP_FONT_SIZE:
                    case CSS_PROP_FONT_STYLE:
                    case CSS_PROP_FONT_FAMILY:
                    case CSS_PROP_FONT_WEIGHT:
                    case CSS_PROP__WEBKIT_TEXT_SIZE_ADJUST:
                    case CSS_PROP_FONT_VARIANT:
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
        Pair* pair = static_cast<CSSPrimitiveValue*>(list->item(i))->getPairValue();
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

void CSSStyleSelector::applyProperty(int id, CSSValue *value)
{
    CSSPrimitiveValue* primitiveValue = 0;
    if (value->isPrimitiveValue())
        primitiveValue = static_cast<CSSPrimitiveValue*>(value);

    Length l;
    bool apply = false;

    unsigned short valueType = value->cssValueType();

    bool isInherit = m_parentNode && valueType == CSSValue::CSS_INHERIT;
    bool isInitial = valueType == CSSValue::CSS_INITIAL || (!m_parentNode && valueType == CSSValue::CSS_INHERIT);

    // These properties are used to set the correct margins/padding on RTL lists.
    if (id == CSS_PROP__WEBKIT_MARGIN_START)
        id = m_style->direction() == LTR ? CSS_PROP_MARGIN_LEFT : CSS_PROP_MARGIN_RIGHT;
    else if (id == CSS_PROP__WEBKIT_PADDING_START)
        id = m_style->direction() == LTR ? CSS_PROP_PADDING_LEFT : CSS_PROP_PADDING_RIGHT;

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.  Shorthands (e.g. border, background) occur in this list as well and
    // are only hit when mapping "inherit" or "initial" into front-end values.
    switch (static_cast<CSSPropertyID>(id)) {
// ident only properties
    case CSS_PROP_BACKGROUND_ATTACHMENT:
        HANDLE_BACKGROUND_VALUE(backgroundAttachment, BackgroundAttachment, value)
        return;
    case CSS_PROP__WEBKIT_BACKGROUND_CLIP:
        HANDLE_BACKGROUND_VALUE(backgroundClip, BackgroundClip, value)
        return;
    case CSS_PROP__WEBKIT_BACKGROUND_COMPOSITE:
        HANDLE_BACKGROUND_VALUE(backgroundComposite, BackgroundComposite, value)
        return;
    case CSS_PROP__WEBKIT_BACKGROUND_ORIGIN:
        HANDLE_BACKGROUND_VALUE(backgroundOrigin, BackgroundOrigin, value)
        return;
    case CSS_PROP_BACKGROUND_REPEAT:
        HANDLE_BACKGROUND_VALUE(backgroundRepeat, BackgroundRepeat, value)
        return;
    case CSS_PROP__WEBKIT_BACKGROUND_SIZE:
        HANDLE_BACKGROUND_VALUE(backgroundSize, BackgroundSize, value)
        return;
    case CSS_PROP_BORDER_COLLAPSE:
        HANDLE_INHERIT_AND_INITIAL(borderCollapse, BorderCollapse)
        if (!primitiveValue)
            return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_COLLAPSE:
                m_style->setBorderCollapse(true);
                break;
            case CSS_VAL_SEPARATE:
                m_style->setBorderCollapse(false);
                break;
            default:
                return;
        }
        return;
        
    case CSS_PROP_BORDER_TOP_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderTopStyle, BorderTopStyle, BorderStyle)
        if (primitiveValue)
            m_style->setBorderTopStyle(*primitiveValue);
        return;
    case CSS_PROP_BORDER_RIGHT_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderRightStyle, BorderRightStyle, BorderStyle)
        if (primitiveValue)
            m_style->setBorderRightStyle(*primitiveValue);
        return;
    case CSS_PROP_BORDER_BOTTOM_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderBottomStyle, BorderBottomStyle, BorderStyle)
        if (primitiveValue)
            m_style->setBorderBottomStyle(*primitiveValue);
        return;
    case CSS_PROP_BORDER_LEFT_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderLeftStyle, BorderLeftStyle, BorderStyle)
        if (primitiveValue)
            m_style->setBorderLeftStyle(*primitiveValue);
        return;
    case CSS_PROP_OUTLINE_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(outlineStyle, OutlineStyle, BorderStyle)
        if (primitiveValue) {
            if (primitiveValue->getIdent() == CSS_VAL_AUTO)
                m_style->setOutlineStyle(DOTTED, true);
            else
                m_style->setOutlineStyle(*primitiveValue);
        }
        return;
    case CSS_PROP_CAPTION_SIDE:
    {
        HANDLE_INHERIT_AND_INITIAL(captionSide, CaptionSide)
        if (primitiveValue)
            m_style->setCaptionSide(*primitiveValue);
        return;
    }
    case CSS_PROP_CLEAR:
    {
        HANDLE_INHERIT_AND_INITIAL(clear, Clear)
        if (primitiveValue)
            m_style->setClear(*primitiveValue);
        return;
    }
    case CSS_PROP_DIRECTION:
    {
        HANDLE_INHERIT_AND_INITIAL(direction, Direction)
        if (primitiveValue)
            m_style->setDirection(*primitiveValue);
        return;
    }
    case CSS_PROP_DISPLAY:
    {
        HANDLE_INHERIT_AND_INITIAL(display, Display)
        if (primitiveValue)
            m_style->setDisplay(*primitiveValue);
        return;
    }

    case CSS_PROP_EMPTY_CELLS:
    {
        HANDLE_INHERIT_AND_INITIAL(emptyCells, EmptyCells)
        if (primitiveValue)
            m_style->setEmptyCells(*primitiveValue);
        return;
    }
    case CSS_PROP_FLOAT:
    {
        HANDLE_INHERIT_AND_INITIAL(floating, Floating)
        if (primitiveValue)
            m_style->setFloating(*primitiveValue);
        return;
    }

    case CSS_PROP_FONT_STYLE:
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
                case CSS_VAL_OBLIQUE:
                // FIXME: oblique is the same as italic for the moment...
                case CSS_VAL_ITALIC:
                    fontDescription.setItalic(true);
                    break;
                case CSS_VAL_NORMAL:
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

    case CSS_PROP_FONT_VARIANT:
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
            if (id == CSS_VAL_NORMAL)
                fontDescription.setSmallCaps(false);
            else if (id == CSS_VAL_SMALL_CAPS)
                fontDescription.setSmallCaps(true);
            else
                return;
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;        
    }

    case CSS_PROP_FONT_WEIGHT:
    {
        FontDescription fontDescription = m_style->fontDescription();
        if (isInherit)
            fontDescription.setWeight(m_parentStyle->fontDescription().weight());
        else if (isInitial)
            fontDescription.setWeight(cNormalWeight);
        else {
            if (!primitiveValue)
                return;
            if (primitiveValue->getIdent()) {
                switch (primitiveValue->getIdent()) {
                    // FIXME: We aren't genuinely supporting specific weight values.
                    case CSS_VAL_BOLD:
                    case CSS_VAL_BOLDER:
                    case CSS_VAL_600:
                    case CSS_VAL_700:
                    case CSS_VAL_800:
                    case CSS_VAL_900:
                        fontDescription.setWeight(cBoldWeight);
                        break;
                    case CSS_VAL_NORMAL:
                    case CSS_VAL_LIGHTER:
                    case CSS_VAL_100:
                    case CSS_VAL_200:
                    case CSS_VAL_300:
                    case CSS_VAL_400:
                    case CSS_VAL_500:
                        fontDescription.setWeight(cNormalWeight);
                        break;
                    default:
                        return;
                }
            }
            else
            {
                // ### fix parsing of 100-900 values in parser, apply them here
            }
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }
        
    case CSS_PROP_LIST_STYLE_POSITION:
    {
        HANDLE_INHERIT_AND_INITIAL(listStylePosition, ListStylePosition)
        if (primitiveValue)
            m_style->setListStylePosition(*primitiveValue);
        return;
    }

    case CSS_PROP_LIST_STYLE_TYPE:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleType, ListStyleType)
        if (primitiveValue)
            m_style->setListStyleType(*primitiveValue);
        return;
    }

    case CSS_PROP_OVERFLOW:
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

    case CSS_PROP_OVERFLOW_X:
    {
        HANDLE_INHERIT_AND_INITIAL(overflowX, OverflowX)
        m_style->setOverflowX(*primitiveValue);
        return;
    }

    case CSS_PROP_OVERFLOW_Y:
    {
        HANDLE_INHERIT_AND_INITIAL(overflowY, OverflowY)
        m_style->setOverflowY(*primitiveValue);
        return;
    }

    case CSS_PROP_PAGE_BREAK_BEFORE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakBefore, PageBreakBefore, PageBreak)
        if (primitiveValue)
            m_style->setPageBreakBefore(*primitiveValue);
        return;
    }

    case CSS_PROP_PAGE_BREAK_AFTER:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakAfter, PageBreakAfter, PageBreak)
        if (primitiveValue)
            m_style->setPageBreakAfter(*primitiveValue);
        return;
    }

    case CSS_PROP_PAGE_BREAK_INSIDE: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakInside, PageBreakInside, PageBreak)
        if (!primitiveValue)
            return;
        EPageBreak pageBreak = *primitiveValue;
        if (pageBreak != PBALWAYS)
            m_style->setPageBreakInside(pageBreak);
        return;
    }
        
    case CSS_PROP_POSITION:
    {
        HANDLE_INHERIT_AND_INITIAL(position, Position)
        if (primitiveValue)
            m_style->setPosition(*primitiveValue);
        return;
    }

    case CSS_PROP_TABLE_LAYOUT: {
        HANDLE_INHERIT_AND_INITIAL(tableLayout, TableLayout)

        ETableLayout l = *primitiveValue;
        if (l == TAUTO)
            l = RenderStyle::initialTableLayout();

        m_style->setTableLayout(l);
        return;
    }
        
    case CSS_PROP_UNICODE_BIDI: {
        HANDLE_INHERIT_AND_INITIAL(unicodeBidi, UnicodeBidi)
        m_style->setUnicodeBidi(*primitiveValue);
        return;
    }
    case CSS_PROP_TEXT_TRANSFORM: {
        HANDLE_INHERIT_AND_INITIAL(textTransform, TextTransform)
        m_style->setTextTransform(*primitiveValue);
        return;
    }

    case CSS_PROP_VISIBILITY:
    {
        HANDLE_INHERIT_AND_INITIAL(visibility, Visibility)
        m_style->setVisibility(*primitiveValue);
        return;
    }
    case CSS_PROP_WHITE_SPACE:
        HANDLE_INHERIT_AND_INITIAL(whiteSpace, WhiteSpace)
        m_style->setWhiteSpace(*primitiveValue);
        return;

    case CSS_PROP_BACKGROUND_POSITION:
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(backgroundXPosition, BackgroundXPosition);
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(backgroundYPosition, BackgroundYPosition);
        return;
    case CSS_PROP_BACKGROUND_POSITION_X: {
        HANDLE_BACKGROUND_VALUE(backgroundXPosition, BackgroundXPosition, value)
        return;
    }
    case CSS_PROP_BACKGROUND_POSITION_Y: {
        HANDLE_BACKGROUND_VALUE(backgroundYPosition, BackgroundYPosition, value)
        return;
    }
    case CSS_PROP_BORDER_SPACING: {
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
    case CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING: {
        HANDLE_INHERIT_AND_INITIAL(horizontalBorderSpacing, HorizontalBorderSpacing)
        if (!primitiveValue)
            return;
        short spacing =  primitiveValue->computeLengthShort(m_style);
        m_style->setHorizontalBorderSpacing(spacing);
        return;
    }
    case CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING: {
        HANDLE_INHERIT_AND_INITIAL(verticalBorderSpacing, VerticalBorderSpacing)
        if (!primitiveValue)
            return;
        short spacing =  primitiveValue->computeLengthShort(m_style);
        m_style->setVerticalBorderSpacing(spacing);
        return;
    }
    case CSS_PROP_CURSOR:
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
                CSSValue* item = list->item(i);
                if (!item->isPrimitiveValue())
                    continue;
                primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                int type = primitiveValue->primitiveType();
                if (type == CSSPrimitiveValue::CSS_URI) {
                    CSSCursorImageValue* image = static_cast<CSSCursorImageValue*>(primitiveValue);
                    if (image->updateIfSVGCursorIsUsed(m_element)) // Elements with SVG cursors are not allowed to share style.
                        m_style->setUnique();
                    m_style->addCursor(image->image(m_element->document()->docLoader()), image->hotspot());
                } else if (type == CSSPrimitiveValue::CSS_IDENT)
                    m_style->setCursor(*primitiveValue);
            }
        } else if (primitiveValue) {
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_IDENT)
                m_style->setCursor(*primitiveValue);
        }
        return;
// colors || inherit
    case CSS_PROP_BACKGROUND_COLOR:
    case CSS_PROP_BORDER_TOP_COLOR:
    case CSS_PROP_BORDER_RIGHT_COLOR:
    case CSS_PROP_BORDER_BOTTOM_COLOR:
    case CSS_PROP_BORDER_LEFT_COLOR:
    case CSS_PROP_COLOR:
    case CSS_PROP_OUTLINE_COLOR:
    case CSS_PROP__WEBKIT_COLUMN_RULE_COLOR:
    case CSS_PROP__WEBKIT_TEXT_STROKE_COLOR:
    case CSS_PROP__WEBKIT_TEXT_FILL_COLOR: {
        Color col;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_BACKGROUND_COLOR, backgroundColor, BackgroundColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_TOP_COLOR, borderTopColor, BorderTopColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_BOTTOM_COLOR, borderBottomColor, BorderBottomColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_RIGHT_COLOR, borderRightColor, BorderRightColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_LEFT_COLOR, borderLeftColor, BorderLeftColor)
            HANDLE_INHERIT_COND(CSS_PROP_COLOR, color, Color)
            HANDLE_INHERIT_COND(CSS_PROP_OUTLINE_COLOR, outlineColor, OutlineColor)
            HANDLE_INHERIT_COND(CSS_PROP__WEBKIT_COLUMN_RULE_COLOR, columnRuleColor, ColumnRuleColor)
            HANDLE_INHERIT_COND(CSS_PROP__WEBKIT_TEXT_STROKE_COLOR, textStrokeColor, TextStrokeColor)
            HANDLE_INHERIT_COND(CSS_PROP__WEBKIT_TEXT_FILL_COLOR, textFillColor, TextFillColor)
            return;
        }
        if (isInitial) {
            // The border/outline colors will just map to the invalid color |col| above.  This will have the
            // effect of forcing the use of the currentColor when it comes time to draw the borders (and of
            // not painting the background since the color won't be valid).
            if (id == CSS_PROP_COLOR)
                col = RenderStyle::initialColor();
        } else {
            if (!primitiveValue)
                return;
            col = getColorFromPrimitiveValue(primitiveValue);
        }

        switch (id) {
        case CSS_PROP_BACKGROUND_COLOR:
            m_style->setBackgroundColor(col);
            break;
        case CSS_PROP_BORDER_TOP_COLOR:
            m_style->setBorderTopColor(col);
            break;
        case CSS_PROP_BORDER_RIGHT_COLOR:
            m_style->setBorderRightColor(col);
            break;
        case CSS_PROP_BORDER_BOTTOM_COLOR:
            m_style->setBorderBottomColor(col);
            break;
        case CSS_PROP_BORDER_LEFT_COLOR:
            m_style->setBorderLeftColor(col);
            break;
        case CSS_PROP_COLOR:
            m_style->setColor(col);
            break;
        case CSS_PROP_OUTLINE_COLOR:
            m_style->setOutlineColor(col);
            break;
        case CSS_PROP__WEBKIT_COLUMN_RULE_COLOR:
            m_style->setColumnRuleColor(col);
            break;
        case CSS_PROP__WEBKIT_TEXT_STROKE_COLOR:
            m_style->setTextStrokeColor(col);
            break;
        case CSS_PROP__WEBKIT_TEXT_FILL_COLOR:
            m_style->setTextFillColor(col);
            break;
        }
        
        return;
    }
    
// uri || inherit
    case CSS_PROP_BACKGROUND_IMAGE:
        HANDLE_BACKGROUND_VALUE(backgroundImage, BackgroundImage, value)
        return;
    case CSS_PROP_LIST_STYLE_IMAGE:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleImage, ListStyleImage)
        if (!primitiveValue)
            return;
        m_style->setListStyleImage(static_cast<CSSImageValue*>(primitiveValue)->image(m_element->document()->docLoader()));
        return;
    }

// length
    case CSS_PROP_BORDER_TOP_WIDTH:
    case CSS_PROP_BORDER_RIGHT_WIDTH:
    case CSS_PROP_BORDER_BOTTOM_WIDTH:
    case CSS_PROP_BORDER_LEFT_WIDTH:
    case CSS_PROP_OUTLINE_WIDTH:
    case CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH:
    {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_TOP_WIDTH, borderTopWidth, BorderTopWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_RIGHT_WIDTH, borderRightWidth, BorderRightWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_BOTTOM_WIDTH, borderBottomWidth, BorderBottomWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_LEFT_WIDTH, borderLeftWidth, BorderLeftWidth)
            HANDLE_INHERIT_COND(CSS_PROP_OUTLINE_WIDTH, outlineWidth, OutlineWidth)
            HANDLE_INHERIT_COND(CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH, columnRuleWidth, ColumnRuleWidth)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_TOP_WIDTH, BorderTopWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_RIGHT_WIDTH, BorderRightWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_BOTTOM_WIDTH, BorderBottomWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_LEFT_WIDTH, BorderLeftWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_OUTLINE_WIDTH, OutlineWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH, ColumnRuleWidth, BorderWidth)
            return;
        }

        if (!primitiveValue)
            return;
        short width = 3;
        switch (primitiveValue->getIdent()) {
        case CSS_VAL_THIN:
            width = 1;
            break;
        case CSS_VAL_MEDIUM:
            width = 3;
            break;
        case CSS_VAL_THICK:
            width = 5;
            break;
        case CSS_VAL_INVALID:
            width = primitiveValue->computeLengthShort(m_style);
            break;
        default:
            return;
        }

        if (width < 0) return;
        switch (id) {
        case CSS_PROP_BORDER_TOP_WIDTH:
            m_style->setBorderTopWidth(width);
            break;
        case CSS_PROP_BORDER_RIGHT_WIDTH:
            m_style->setBorderRightWidth(width);
            break;
        case CSS_PROP_BORDER_BOTTOM_WIDTH:
            m_style->setBorderBottomWidth(width);
            break;
        case CSS_PROP_BORDER_LEFT_WIDTH:
            m_style->setBorderLeftWidth(width);
            break;
        case CSS_PROP_OUTLINE_WIDTH:
            m_style->setOutlineWidth(width);
            break;
        case CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH:
            m_style->setColumnRuleWidth(width);
            break;
        default:
            return;
        }
        return;
    }

    case CSS_PROP_LETTER_SPACING:
    case CSS_PROP_WORD_SPACING:
    {
        
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_LETTER_SPACING, letterSpacing, LetterSpacing)
            HANDLE_INHERIT_COND(CSS_PROP_WORD_SPACING, wordSpacing, WordSpacing)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_LETTER_SPACING, LetterSpacing, LetterWordSpacing)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_WORD_SPACING, WordSpacing, LetterWordSpacing)
            return;
        }
        
        int width = 0;
        if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_NORMAL){
            width = 0;
        } else {
            if (!primitiveValue)
                return;
            width = primitiveValue->computeLengthInt(m_style);
        }
        switch (id) {
        case CSS_PROP_LETTER_SPACING:
            m_style->setLetterSpacing(width);
            break;
        case CSS_PROP_WORD_SPACING:
            m_style->setWordSpacing(width);
            break;
            // ### needs the definitions in renderstyle
        default: break;
        }
        return;
    }

    case CSS_PROP_WORD_BREAK: {
        HANDLE_INHERIT_AND_INITIAL(wordBreak, WordBreak)
        m_style->setWordBreak(*primitiveValue);
        return;
    }

    case CSS_PROP_WORD_WRAP: {
        HANDLE_INHERIT_AND_INITIAL(wordWrap, WordWrap)
        m_style->setWordWrap(*primitiveValue);
        return;
    }

    case CSS_PROP__WEBKIT_NBSP_MODE:
    {
        HANDLE_INHERIT_AND_INITIAL(nbspMode, NBSPMode)
        m_style->setNBSPMode(*primitiveValue);
        return;
    }

    case CSS_PROP__WEBKIT_LINE_BREAK:
    {
        HANDLE_INHERIT_AND_INITIAL(khtmlLineBreak, KHTMLLineBreak)
        m_style->setKHTMLLineBreak(*primitiveValue);
        return;
    }

    case CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:
    {
        HANDLE_INHERIT_AND_INITIAL(matchNearestMailBlockquoteColor, MatchNearestMailBlockquoteColor)
        m_style->setMatchNearestMailBlockquoteColor(*primitiveValue);
        return;
    }

    case CSS_PROP_RESIZE:
    {
        HANDLE_INHERIT_AND_INITIAL(resize, Resize)

        if (!primitiveValue->getIdent())
            return;

        EResize r = RESIZE_NONE;
        if (primitiveValue->getIdent() == CSS_VAL_AUTO) {
            if (Settings* settings = m_document->settings())
                r = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
        } else
            r = *primitiveValue;
            
        m_style->setResize(r);
        return;
    }
    
    // length, percent
    case CSS_PROP_MAX_WIDTH:
        // +none +inherit
        if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
            apply = true;
    case CSS_PROP_TOP:
    case CSS_PROP_LEFT:
    case CSS_PROP_RIGHT:
    case CSS_PROP_BOTTOM:
    case CSS_PROP_WIDTH:
    case CSS_PROP_MIN_WIDTH:
    case CSS_PROP_MARGIN_TOP:
    case CSS_PROP_MARGIN_RIGHT:
    case CSS_PROP_MARGIN_BOTTOM:
    case CSS_PROP_MARGIN_LEFT:
        // +inherit +auto
        if (id == CSS_PROP_WIDTH || id == CSS_PROP_MIN_WIDTH || id == CSS_PROP_MAX_WIDTH) {
            if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_INTRINSIC) {
                l = Length(Intrinsic);
                apply = true;
            }
            else if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_MIN_INTRINSIC) {
                l = Length(MinIntrinsic);
                apply = true;
            }
        }
        if (id != CSS_PROP_MAX_WIDTH && primitiveValue && primitiveValue->getIdent() == CSS_VAL_AUTO)
            apply = true;
    case CSS_PROP_PADDING_TOP:
    case CSS_PROP_PADDING_RIGHT:
    case CSS_PROP_PADDING_BOTTOM:
    case CSS_PROP_PADDING_LEFT:
    case CSS_PROP_TEXT_INDENT:
        // +inherit
    {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_MAX_WIDTH, maxWidth, MaxWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BOTTOM, bottom, Bottom)
            HANDLE_INHERIT_COND(CSS_PROP_TOP, top, Top)
            HANDLE_INHERIT_COND(CSS_PROP_LEFT, left, Left)
            HANDLE_INHERIT_COND(CSS_PROP_RIGHT, right, Right)
            HANDLE_INHERIT_COND(CSS_PROP_WIDTH, width, Width)
            HANDLE_INHERIT_COND(CSS_PROP_MIN_WIDTH, minWidth, MinWidth)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_TOP, paddingTop, PaddingTop)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_RIGHT, paddingRight, PaddingRight)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_BOTTOM, paddingBottom, PaddingBottom)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_LEFT, paddingLeft, PaddingLeft)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_TOP, marginTop, MarginTop)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_RIGHT, marginRight, MarginRight)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_BOTTOM, marginBottom, MarginBottom)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_LEFT, marginLeft, MarginLeft)
            HANDLE_INHERIT_COND(CSS_PROP_TEXT_INDENT, textIndent, TextIndent)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MAX_WIDTH, MaxWidth, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BOTTOM, Bottom, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_TOP, Top, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_LEFT, Left, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_RIGHT, Right, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_WIDTH, Width, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MIN_WIDTH, MinWidth, MinSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_TOP, PaddingTop, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_RIGHT, PaddingRight, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_BOTTOM, PaddingBottom, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_LEFT, PaddingLeft, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_TOP, MarginTop, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_RIGHT, MarginRight, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_BOTTOM, MarginBottom, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_LEFT, MarginLeft, Margin)
            HANDLE_INITIAL_COND(CSS_PROP_TEXT_INDENT, TextIndent)
            return;
        } 

        if (primitiveValue && !apply) {
            int type = primitiveValue->primitiveType();
            if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                // Handle our quirky margin units if we have them.
                l = Length(primitiveValue->computeLengthIntForLength(m_style), Fixed, 
                           primitiveValue->isQuirkValue());
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length(primitiveValue->getDoubleValue(), Percent);
            else
                return;
            if (id == CSS_PROP_PADDING_LEFT || id == CSS_PROP_PADDING_RIGHT ||
                id == CSS_PROP_PADDING_TOP || id == CSS_PROP_PADDING_BOTTOM)
                // Padding can't be negative
                apply = !((l.isFixed() || l.isPercent()) && l.calcValue(100) < 0);
            else
                apply = true;
        }
        if (!apply) return;
        switch (id) {
            case CSS_PROP_MAX_WIDTH:
                m_style->setMaxWidth(l);
                break;
            case CSS_PROP_BOTTOM:
                m_style->setBottom(l);
                break;
            case CSS_PROP_TOP:
                m_style->setTop(l);
                break;
            case CSS_PROP_LEFT:
                m_style->setLeft(l);
                break;
            case CSS_PROP_RIGHT:
                m_style->setRight(l);
                break;
            case CSS_PROP_WIDTH:
                m_style->setWidth(l);
                break;
            case CSS_PROP_MIN_WIDTH:
                m_style->setMinWidth(l);
                break;
            case CSS_PROP_PADDING_TOP:
                m_style->setPaddingTop(l);
                break;
            case CSS_PROP_PADDING_RIGHT:
                m_style->setPaddingRight(l);
                break;
            case CSS_PROP_PADDING_BOTTOM:
                m_style->setPaddingBottom(l);
                break;
            case CSS_PROP_PADDING_LEFT:
                m_style->setPaddingLeft(l);
                break;
            case CSS_PROP_MARGIN_TOP:
                m_style->setMarginTop(l);
                break;
            case CSS_PROP_MARGIN_RIGHT:
                m_style->setMarginRight(l);
                break;
            case CSS_PROP_MARGIN_BOTTOM:
                m_style->setMarginBottom(l);
                break;
            case CSS_PROP_MARGIN_LEFT:
                m_style->setMarginLeft(l);
                break;
            case CSS_PROP_TEXT_INDENT:
                m_style->setTextIndent(l);
                break;
            default:
                break;
            }
        return;
    }

    case CSS_PROP_MAX_HEIGHT:
        if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE) {
            l = Length(undefinedLength, Fixed);
            apply = true;
        }
    case CSS_PROP_HEIGHT:
    case CSS_PROP_MIN_HEIGHT:
        if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_INTRINSIC) {
            l = Length(Intrinsic);
            apply = true;
        } else if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_MIN_INTRINSIC) {
            l = Length(MinIntrinsic);
            apply = true;
        } else if (id != CSS_PROP_MAX_HEIGHT && primitiveValue && primitiveValue->getIdent() == CSS_VAL_AUTO)
            apply = true;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_MAX_HEIGHT, maxHeight, MaxHeight)
            HANDLE_INHERIT_COND(CSS_PROP_HEIGHT, height, Height)
            HANDLE_INHERIT_COND(CSS_PROP_MIN_HEIGHT, minHeight, MinHeight)
            return;
        }
        if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MAX_HEIGHT, MaxHeight, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_HEIGHT, Height, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MIN_HEIGHT, MinHeight, MinSize)
            return;
        }

        if (primitiveValue && !apply) {
            unsigned short type = primitiveValue->primitiveType();
            if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                l = Length(primitiveValue->computeLengthIntForLength(m_style), Fixed);
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length(primitiveValue->getDoubleValue(), Percent);
            else
                return;
            apply = true;
        }
        if (apply)
            switch (id) {
                case CSS_PROP_MAX_HEIGHT:
                    m_style->setMaxHeight(l);
                    break;
                case CSS_PROP_HEIGHT:
                    m_style->setHeight(l);
                    break;
                case CSS_PROP_MIN_HEIGHT:
                    m_style->setMinHeight(l);
                    break;
            }
        return;

    case CSS_PROP_VERTICAL_ALIGN:
        HANDLE_INHERIT_AND_INITIAL(verticalAlign, VerticalAlign)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
          EVerticalAlign align;

          switch (primitiveValue->getIdent()) {
                case CSS_VAL_TOP:
                    align = TOP; break;
                case CSS_VAL_BOTTOM:
                    align = BOTTOM; break;
                case CSS_VAL_MIDDLE:
                    align = MIDDLE; break;
                case CSS_VAL_BASELINE:
                    align = BASELINE; break;
                case CSS_VAL_TEXT_BOTTOM:
                    align = TEXT_BOTTOM; break;
                case CSS_VAL_TEXT_TOP:
                    align = TEXT_TOP; break;
                case CSS_VAL_SUB:
                    align = SUB; break;
                case CSS_VAL_SUPER:
                    align = SUPER; break;
                case CSS_VAL__WEBKIT_BASELINE_MIDDLE:
                    align = BASELINE_MIDDLE; break;
                default:
                    return;
            }
          m_style->setVerticalAlign(align);
          return;
        } else {
          int type = primitiveValue->primitiveType();
          Length l;
          if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(m_style), Fixed);
          else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);

          m_style->setVerticalAlign(LENGTH);
          m_style->setVerticalAlignLength(l);
        }
        return;

    case CSS_PROP_FONT_SIZE:
    {
        FontDescription fontDescription = m_style->fontDescription();
        fontDescription.setKeywordSize(0);
        bool familyIsFixed = fontDescription.genericFamily() == FontDescription::MonospaceFamily;
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
            size = fontSizeForKeyword(CSS_VAL_MEDIUM, m_style->htmlHacks(), familyIsFixed);
            fontDescription.setKeywordSize(CSS_VAL_MEDIUM - CSS_VAL_XX_SMALL + 1);
        } else if (primitiveValue->getIdent()) {
            // Keywords are being used.
            switch (primitiveValue->getIdent()) {
                case CSS_VAL_XX_SMALL:
                case CSS_VAL_X_SMALL:
                case CSS_VAL_SMALL:
                case CSS_VAL_MEDIUM:
                case CSS_VAL_LARGE:
                case CSS_VAL_X_LARGE:
                case CSS_VAL_XX_LARGE:
                case CSS_VAL__WEBKIT_XXX_LARGE:
                    size = fontSizeForKeyword(primitiveValue->getIdent(), m_style->htmlHacks(), familyIsFixed);
                    fontDescription.setKeywordSize(primitiveValue->getIdent() - CSS_VAL_XX_SMALL + 1);
                    break;
                case CSS_VAL_LARGER:
                    size = largerFontSize(oldSize, m_style->htmlHacks());
                    break;
                case CSS_VAL_SMALLER:
                    size = smallerFontSize(oldSize, m_style->htmlHacks());
                    break;
                default:
                    return;
            }

            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize && 
                                              (primitiveValue->getIdent() == CSS_VAL_LARGER ||
                                               primitiveValue->getIdent() == CSS_VAL_SMALLER));
        } else {
            int type = primitiveValue->primitiveType();
            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize ||
                                              (type != CSSPrimitiveValue::CSS_PERCENTAGE &&
                                               type != CSSPrimitiveValue::CSS_EMS && 
                                               type != CSSPrimitiveValue::CSS_EXS));
            if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                size = primitiveValue->computeLengthFloat(m_parentStyle, false);
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

    case CSS_PROP_Z_INDEX: {
        if (isInherit) {
            if (m_parentStyle->hasAutoZIndex())
                m_style->setHasAutoZIndex();
            else
                m_style->setZIndex(m_parentStyle->zIndex());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSS_VAL_AUTO) {
            m_style->setHasAutoZIndex();
            return;
        }
        
        // FIXME: Should clamp all sorts of other integer properties too.
        const double minIntAsDouble = INT_MIN;
        const double maxIntAsDouble = INT_MAX;
        m_style->setZIndex(static_cast<int>(max(minIntAsDouble, min(primitiveValue->getDoubleValue(), maxIntAsDouble))));
        return;
    }
    case CSS_PROP_WIDOWS:
    {
        HANDLE_INHERIT_AND_INITIAL(widows, Widows)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        m_style->setWidows(primitiveValue->getIntValue());
        return;
    }
        
    case CSS_PROP_ORPHANS:
    {
        HANDLE_INHERIT_AND_INITIAL(orphans, Orphans)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        m_style->setOrphans(primitiveValue->getIntValue());
        return;
    }        

// length, percent, number
    case CSS_PROP_LINE_HEIGHT:
    {
        HANDLE_INHERIT_AND_INITIAL(lineHeight, LineHeight)
        if (!primitiveValue)
            return;
        Length lineHeight;
        int type = primitiveValue->primitiveType();
        if (primitiveValue->getIdent() == CSS_VAL_NORMAL)
            lineHeight = Length(-100.0, Percent);
        else if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG) {
            double multiplier = 1.0;
            // Scale for the font zoom factor only for types other than "em" and "ex", since those are
            // already based on the font size.
            if (type != CSSPrimitiveValue::CSS_EMS && type != CSSPrimitiveValue::CSS_EXS && m_style->textSizeAdjust() && m_document->frame()) {
                multiplier = m_document->frame()->zoomFactor() / 100.0;
            }
            lineHeight = Length(primitiveValue->computeLengthIntForLength(m_style, multiplier), Fixed);
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
    case CSS_PROP_TEXT_ALIGN:
    {
        HANDLE_INHERIT_AND_INITIAL(textAlign, TextAlign)
        if (!primitiveValue)
            return;
        int id = primitiveValue->getIdent();
        if (id == CSS_VAL_START)
            m_style->setTextAlign(m_style->direction() == LTR ? LEFT : RIGHT);
        else if (id == CSS_VAL_END)
            m_style->setTextAlign(m_style->direction() == LTR ? RIGHT : LEFT);
        else
            m_style->setTextAlign(*primitiveValue);
        return;
    }

// rect
    case CSS_PROP_CLIP:
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
            }
            else {
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
            top = convertToLength(rect->top(), m_style);
            right = convertToLength(rect->right(), m_style);
            bottom = convertToLength(rect->bottom(), m_style);
            left = convertToLength(rect->left(), m_style);

        } else if (primitiveValue->getIdent() != CSS_VAL_AUTO) {
            return;
        }
        m_style->setClip(top, right, bottom, left);
        m_style->setHasClip(hasClip);
    
        // rect, ident
        return;
    }

// lists
    case CSS_PROP_CONTENT:
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
            CSSValue* item = list->item(i);
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
                    if (m_style->styleType() == RenderStyle::NOPSEUDO)
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
                    CSSImageValue *image = static_cast<CSSImageValue*>(val);
                    m_style->setContent(image->image(m_element->document()->docLoader()), didSet);
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

    case CSS_PROP_COUNTER_INCREMENT:
        applyCounterList(m_style, value->isValueList() ? static_cast<CSSValueList*>(value) : 0, false);
        return;
    case CSS_PROP_COUNTER_RESET:
        applyCounterList(m_style, value->isValueList() ? static_cast<CSSValueList*>(value) : 0, true);
        return;

    case CSS_PROP_FONT_FAMILY: {
        // list of strings and ids
        if (isInherit) {
            FontDescription parentFontDescription = m_parentStyle->fontDescription();
            FontDescription fontDescription = m_style->fontDescription();
            fontDescription.setGenericFamily(parentFontDescription.genericFamily());
            fontDescription.setFamily(parentFontDescription.firstFamily());
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
            return;
        }
        else if (isInitial) {
            FontDescription initialDesc = FontDescription();
            FontDescription fontDescription = m_style->fontDescription();
            // We need to adjust the size to account for the generic family change from monospace
            // to non-monospace.
            if (fontDescription.keywordSize() && fontDescription.genericFamily() == FontDescription::MonospaceFamily)
                setFontSize(fontDescription, fontSizeForKeyword(CSS_VAL_XX_SMALL + fontDescription.keywordSize() - 1, m_style->htmlHacks(), false));
            fontDescription.setGenericFamily(initialDesc.genericFamily());
            fontDescription.setFamily(initialDesc.firstFamily());
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
            return;
        }
        
        if (!value->isValueList()) return;
        FontDescription fontDescription = m_style->fontDescription();
        CSSValueList *list = static_cast<CSSValueList*>(value);
        int len = list->length();
        FontFamily& firstFamily = fontDescription.firstFamily();
        FontFamily *currFamily = 0;
        
        // Before mapping in a new font-family property, we should reset the generic family.
        bool oldFamilyIsMonospace = fontDescription.genericFamily() == FontDescription::MonospaceFamily;
        fontDescription.setGenericFamily(FontDescription::NoFamily);

        for (int i = 0; i < len; i++) {
            CSSValue *item = list->item(i);
            if (!item->isPrimitiveValue()) continue;
            CSSPrimitiveValue *val = static_cast<CSSPrimitiveValue*>(item);
            AtomicString face;
            Settings* settings = m_document->settings();
            if (val->primitiveType() == CSSPrimitiveValue::CSS_STRING)
                face = static_cast<FontFamilyValue*>(val)->familyName();
            else if (val->primitiveType() == CSSPrimitiveValue::CSS_IDENT && settings) {
                switch (val->getIdent()) {
                    case CSS_VAL__WEBKIT_BODY:
                        face = settings->standardFontFamily();
                        break;
                    case CSS_VAL_SERIF:
                        face = "-webkit-serif";
                        fontDescription.setGenericFamily(FontDescription::SerifFamily);
                        break;
                    case CSS_VAL_SANS_SERIF:
                        face = "-webkit-sans-serif";
                        fontDescription.setGenericFamily(FontDescription::SansSerifFamily);
                        break;
                    case CSS_VAL_CURSIVE:
                        face = "-webkit-cursive";
                        fontDescription.setGenericFamily(FontDescription::CursiveFamily);
                        break;
                    case CSS_VAL_FANTASY:
                        face = "-webkit-fantasy";
                        fontDescription.setGenericFamily(FontDescription::FantasyFamily);
                        break;
                    case CSS_VAL_MONOSPACE:
                        face = "-webkit-monospace";
                        fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
                        break;
                }
            }
    
            if (!face.isEmpty()) {
                if (!currFamily) {
                    // Filling in the first family.
                    firstFamily.setFamily(face);
                    currFamily = &firstFamily;
                }
                else {
                    FontFamily *newFamily = new FontFamily;
                    newFamily->setFamily(face);
                    currFamily->appendFamily(newFamily);
                    currFamily = newFamily;
                }
    
                if (fontDescription.keywordSize() && (fontDescription.genericFamily() == FontDescription::MonospaceFamily) != oldFamilyIsMonospace)
                    setFontSize(fontDescription, fontSizeForKeyword(CSS_VAL_XX_SMALL + fontDescription.keywordSize() - 1, m_style->htmlHacks(), !oldFamilyIsMonospace));
            
                if (m_style->setFontDescription(fontDescription))
                    m_fontDirty = true;
            }
        }
      return;
    }
    case CSS_PROP_TEXT_DECORATION: {
        // list of ident
        HANDLE_INHERIT_AND_INITIAL(textDecoration, TextDecoration)
        int t = RenderStyle::initialTextDecoration();
        if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE) {
            // do nothing
        } else {
            if (!value->isValueList()) return;
            CSSValueList *list = static_cast<CSSValueList*>(value);
            int len = list->length();
            for (int i = 0; i < len; i++)
            {
                CSSValue *item = list->item(i);
                if (!item->isPrimitiveValue()) continue;
                primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                switch (primitiveValue->getIdent()) {
                    case CSS_VAL_NONE:
                        t = TDNONE; break;
                    case CSS_VAL_UNDERLINE:
                        t |= UNDERLINE; break;
                    case CSS_VAL_OVERLINE:
                        t |= OVERLINE; break;
                    case CSS_VAL_LINE_THROUGH:
                        t |= LINE_THROUGH; break;
                    case CSS_VAL_BLINK:
                        t |= BLINK; break;
                    default:
                        return;
                }
            }
        }

        m_style->setTextDecoration(t);
        return;
    }

// shorthand properties
    case CSS_PROP_BACKGROUND:
        if (isInitial) {
            m_style->clearBackgroundLayers();
            m_style->setBackgroundColor(Color());
            return;
        }
        else if (isInherit) {
            m_style->inheritBackgroundLayers(*m_parentStyle->backgroundLayers());
            m_style->setBackgroundColor(m_parentStyle->backgroundColor());
        }
        return;
    case CSS_PROP_BORDER:
    case CSS_PROP_BORDER_STYLE:
    case CSS_PROP_BORDER_WIDTH:
    case CSS_PROP_BORDER_COLOR:
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_COLOR)
        {
            if (isInherit) {
                m_style->setBorderTopColor(m_parentStyle->borderTopColor());
                m_style->setBorderBottomColor(m_parentStyle->borderBottomColor());
                m_style->setBorderLeftColor(m_parentStyle->borderLeftColor());
                m_style->setBorderRightColor(m_parentStyle->borderRightColor());
            }
            else if (isInitial) {
                m_style->setBorderTopColor(Color()); // Reset to invalid color so currentColor is used instead.
                m_style->setBorderBottomColor(Color());
                m_style->setBorderLeftColor(Color());
                m_style->setBorderRightColor(Color());
            }
        }
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_STYLE)
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
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_WIDTH)
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
    case CSS_PROP_BORDER_TOP:
        if (isInherit) {
            m_style->setBorderTopColor(m_parentStyle->borderTopColor());
            m_style->setBorderTopStyle(m_parentStyle->borderTopStyle());
            m_style->setBorderTopWidth(m_parentStyle->borderTopWidth());
        }
        else if (isInitial)
            m_style->resetBorderTop();
        return;
    case CSS_PROP_BORDER_RIGHT:
        if (isInherit) {
            m_style->setBorderRightColor(m_parentStyle->borderRightColor());
            m_style->setBorderRightStyle(m_parentStyle->borderRightStyle());
            m_style->setBorderRightWidth(m_parentStyle->borderRightWidth());
        }
        else if (isInitial)
            m_style->resetBorderRight();
        return;
    case CSS_PROP_BORDER_BOTTOM:
        if (isInherit) {
            m_style->setBorderBottomColor(m_parentStyle->borderBottomColor());
            m_style->setBorderBottomStyle(m_parentStyle->borderBottomStyle());
            m_style->setBorderBottomWidth(m_parentStyle->borderBottomWidth());
        }
        else if (isInitial)
            m_style->resetBorderBottom();
        return;
    case CSS_PROP_BORDER_LEFT:
        if (isInherit) {
            m_style->setBorderLeftColor(m_parentStyle->borderLeftColor());
            m_style->setBorderLeftStyle(m_parentStyle->borderLeftStyle());
            m_style->setBorderLeftWidth(m_parentStyle->borderLeftWidth());
        }
        else if (isInitial)
            m_style->resetBorderLeft();
        return;
    case CSS_PROP_MARGIN:
        if (isInherit) {
            m_style->setMarginTop(m_parentStyle->marginTop());
            m_style->setMarginBottom(m_parentStyle->marginBottom());
            m_style->setMarginLeft(m_parentStyle->marginLeft());
            m_style->setMarginRight(m_parentStyle->marginRight());
        }
        else if (isInitial)
            m_style->resetMargin();
        return;
    case CSS_PROP_PADDING:
        if (isInherit) {
            m_style->setPaddingTop(m_parentStyle->paddingTop());
            m_style->setPaddingBottom(m_parentStyle->paddingBottom());
            m_style->setPaddingLeft(m_parentStyle->paddingLeft());
            m_style->setPaddingRight(m_parentStyle->paddingRight());
        }
        else if (isInitial)
            m_style->resetPadding();
        return;
    case CSS_PROP_FONT:
        if (isInherit) {
            FontDescription fontDescription = m_parentStyle->fontDescription();
            m_style->setLineHeight(m_parentStyle->lineHeight());
            m_lineHeightValue = 0;
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
        } else if (isInitial) {
            Settings* settings = m_document->settings();
            FontDescription fontDescription;
            fontDescription.setGenericFamily(FontDescription::StandardFamily);
            fontDescription.setRenderingMode(settings->fontRenderingMode());
            fontDescription.setUsePrinterFont(m_document->printing());
            const AtomicString& standardFontFamily = m_document->settings()->standardFontFamily();
            if (!standardFontFamily.isEmpty()) {
                fontDescription.firstFamily().setFamily(standardFontFamily);
                fontDescription.firstFamily().appendFamily(0);
            }
            fontDescription.setKeywordSize(CSS_VAL_MEDIUM - CSS_VAL_XX_SMALL + 1);
            setFontSize(fontDescription, fontSizeForKeyword(CSS_VAL_MEDIUM, m_style->htmlHacks(), false));
            m_style->setLineHeight(RenderStyle::initialLineHeight());
            m_lineHeightValue = 0;
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
        } else if (primitiveValue) {
            m_style->setLineHeight(RenderStyle::initialLineHeight());
            m_lineHeightValue = 0;
            FontDescription fontDescription;
            theme()->systemFont(primitiveValue->getIdent(), fontDescription);
            // Double-check and see if the theme did anything.  If not, don't bother updating the font.
            if (fontDescription.isAbsoluteSize()) {
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
            applyProperty(CSS_PROP_FONT_STYLE, font->style.get());
            applyProperty(CSS_PROP_FONT_VARIANT, font->variant.get());
            applyProperty(CSS_PROP_FONT_WEIGHT, font->weight.get());
            applyProperty(CSS_PROP_FONT_SIZE, font->size.get());

            m_lineHeightValue = font->lineHeight.get();

            applyProperty(CSS_PROP_FONT_FAMILY, font->family.get());
        }
        return;
        
    case CSS_PROP_LIST_STYLE:
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
    case CSS_PROP_OUTLINE:
        if (isInherit) {
            m_style->setOutlineWidth(m_parentStyle->outlineWidth());
            m_style->setOutlineColor(m_parentStyle->outlineColor());
            m_style->setOutlineStyle(m_parentStyle->outlineStyle());
        }
        else if (isInitial)
            m_style->resetOutline();
        return;

    // CSS3 Properties
    case CSS_PROP__WEBKIT_APPEARANCE: {
        HANDLE_INHERIT_AND_INITIAL(appearance, Appearance)
        if (!primitiveValue)
            return;
        m_style->setAppearance(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_BINDING: {
#if ENABLE(XBL)
        if (isInitial || (primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)) {
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
            CSSValue *item = list->item(i);
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

    case CSS_PROP__WEBKIT_BORDER_IMAGE: {
        HANDLE_INHERIT_AND_INITIAL(borderImage, BorderImage)
        BorderImage image;
        if (primitiveValue) {
            if (primitiveValue->getIdent() == CSS_VAL_NONE)
                m_style->setBorderImage(image);
        } else {
            // Retrieve the border image value.
            CSSBorderImageValue* borderImage = static_cast<CSSBorderImageValue*>(value);
            
            // Set the image (this kicks off the load).
            image.m_image = borderImage->m_image->image(m_element->document()->docLoader());
            
            // Set up a length box to represent our image slices.
            LengthBox& l = image.m_slices;
            Rect* r = borderImage->m_imageSliceRect.get();
            if (r->top()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                l.top = Length(r->top()->getDoubleValue(), Percent);
            else
                l.top = Length(r->top()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
            if (r->bottom()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                l.bottom = Length(r->bottom()->getDoubleValue(), Percent);
            else
                l.bottom = Length((int)r->bottom()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
            if (r->left()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                l.left = Length(r->left()->getDoubleValue(), Percent);
            else
                l.left = Length(r->left()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
            if (r->right()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                l.right = Length(r->right()->getDoubleValue(), Percent);
            else
                l.right = Length(r->right()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
            
            // Set the appropriate rules for stretch/round/repeat of the slices
            switch (borderImage->m_horizontalSizeRule) {
                case CSS_VAL_STRETCH:
                    image.m_horizontalRule = BI_STRETCH;
                    break;
                case CSS_VAL_ROUND:
                    image.m_horizontalRule = BI_ROUND;
                    break;
                default: // CSS_VAL_REPEAT
                    image.m_horizontalRule = BI_REPEAT;
                    break;
            }

            switch (borderImage->m_verticalSizeRule) {
                case CSS_VAL_STRETCH:
                    image.m_verticalRule = BI_STRETCH;
                    break;
                case CSS_VAL_ROUND:
                    image.m_verticalRule = BI_ROUND;
                    break;
                default: // CSS_VAL_REPEAT
                    image.m_verticalRule = BI_REPEAT;
                    break;
            }

            m_style->setBorderImage(image);
        }
        return;
    }

    case CSS_PROP__WEBKIT_BORDER_RADIUS:
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
    case CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS:
    case CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS:
    case CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS:
    case CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS: {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS, borderTopLeftRadius, BorderTopLeftRadius)
            HANDLE_INHERIT_COND(CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS, borderTopRightRadius, BorderTopRightRadius)
            HANDLE_INHERIT_COND(CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS, borderBottomLeftRadius, BorderBottomLeftRadius)
            HANDLE_INHERIT_COND(CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS, borderBottomRightRadius, BorderBottomRightRadius)
            return;
        }
        
        if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS, BorderTopLeftRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS, BorderTopRightRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS, BorderBottomLeftRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS, BorderBottomRightRadius, BorderRadius)
            return;
        }

        if (!primitiveValue)
            return;

        Pair* pair = primitiveValue->getPairValue();
        if (!pair)
            return;

        int width = pair->first()->computeLengthInt(m_style);
        int height = pair->second()->computeLengthInt(m_style);
        if (width < 0 || height < 0)
            return;

        if (width == 0)
            height = 0; // Null out the other value.
        else if (height == 0)
            width = 0; // Null out the other value.

        IntSize size(width, height);
        switch (id) {
            case CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS:
                m_style->setBorderTopLeftRadius(size);
                break;
            case CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS:
                m_style->setBorderTopRightRadius(size);
                break;
            case CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS:
                m_style->setBorderBottomLeftRadius(size);
                break;
            case CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS:
                m_style->setBorderBottomRightRadius(size);
                break;
            default:
                m_style->setBorderRadius(size);
                break;
        }
        return;
    }

    case CSS_PROP_OUTLINE_OFFSET:
        HANDLE_INHERIT_AND_INITIAL(outlineOffset, OutlineOffset)
        m_style->setOutlineOffset(primitiveValue->computeLengthInt(m_style));
        return;

    case CSS_PROP_TEXT_SHADOW:
    case CSS_PROP__WEBKIT_BOX_SHADOW: {
        if (isInherit) {
            if (id == CSS_PROP_TEXT_SHADOW)
                return m_style->setTextShadow(m_parentStyle->textShadow() ? new ShadowData(*m_parentStyle->textShadow()) : 0);
            return m_style->setBoxShadow(m_parentStyle->boxShadow() ? new ShadowData(*m_parentStyle->boxShadow()) : 0);
        }
        if (isInitial || primitiveValue) // initial | none
            return id == CSS_PROP_TEXT_SHADOW ? m_style->setTextShadow(0) : m_style->setBoxShadow(0);

        if (!value->isValueList())
            return;

        CSSValueList *list = static_cast<CSSValueList*>(value);
        int len = list->length();
        for (int i = 0; i < len; i++) {
            ShadowValue* item = static_cast<ShadowValue*>(list->item(i));
            int x = item->x->computeLengthInt(m_style);
            int y = item->y->computeLengthInt(m_style);
            int blur = item->blur ? item->blur->computeLengthInt(m_style) : 0;
            Color color;
            if (item->color)
                color = getColorFromPrimitiveValue(item->color.get());
            ShadowData* shadowData = new ShadowData(x, y, blur, color.isValid() ? color : Color::transparent);
            if (id == CSS_PROP_TEXT_SHADOW)
                m_style->setTextShadow(shadowData, i != 0);
            else
                m_style->setBoxShadow(shadowData, i != 0);
        }
        return;
    }
    case CSS_PROP_OPACITY:
        HANDLE_INHERIT_AND_INITIAL(opacity, Opacity)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        // Clamp opacity to the range 0-1
        m_style->setOpacity(min(1.0f, max(0.0f, primitiveValue->getFloatValue())));
        return;
    case CSS_PROP__WEBKIT_BOX_ALIGN:
    {
        HANDLE_INHERIT_AND_INITIAL(boxAlign, BoxAlign)
        if (!primitiveValue)
            return;
        EBoxAlignment boxAlignment = *primitiveValue;
        if (boxAlignment != BJUSTIFY)
            m_style->setBoxAlign(boxAlignment);
        return;
    }
    case CSS_PROP_SRC: // Only used in @font-face rules.
        return;
    case CSS_PROP_UNICODE_RANGE: // Only used in @font-face rules.
        return;
    case CSS_PROP__WEBKIT_BOX_DIRECTION:
        HANDLE_INHERIT_AND_INITIAL(boxDirection, BoxDirection)
        if (primitiveValue)
            m_style->setBoxDirection(*primitiveValue);
        return;        
    case CSS_PROP__WEBKIT_BOX_LINES:
        HANDLE_INHERIT_AND_INITIAL(boxLines, BoxLines)
        if (primitiveValue)
            m_style->setBoxLines(*primitiveValue);
        return;     
    case CSS_PROP__WEBKIT_BOX_ORIENT:
        HANDLE_INHERIT_AND_INITIAL(boxOrient, BoxOrient)
        if (primitiveValue)
            m_style->setBoxOrient(*primitiveValue);
        return;     
    case CSS_PROP__WEBKIT_BOX_PACK:
    {
        HANDLE_INHERIT_AND_INITIAL(boxPack, BoxPack)
        if (!primitiveValue)
            return;
        EBoxAlignment boxPack = *primitiveValue;
        if (boxPack != BSTRETCH && boxPack != BBASELINE)
            m_style->setBoxPack(boxPack);
        return;
    }
    case CSS_PROP__WEBKIT_BOX_FLEX:
        HANDLE_INHERIT_AND_INITIAL(boxFlex, BoxFlex)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxFlex(primitiveValue->getFloatValue());
        return;
    case CSS_PROP__WEBKIT_BOX_FLEX_GROUP:
        HANDLE_INHERIT_AND_INITIAL(boxFlexGroup, BoxFlexGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxFlexGroup((unsigned int)(primitiveValue->getDoubleValue()));
        return;        
    case CSS_PROP__WEBKIT_BOX_ORDINAL_GROUP:
        HANDLE_INHERIT_AND_INITIAL(boxOrdinalGroup, BoxOrdinalGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxOrdinalGroup((unsigned int)(primitiveValue->getDoubleValue()));
        return;
    case CSS_PROP__WEBKIT_BOX_SIZING:
        HANDLE_INHERIT_AND_INITIAL(boxSizing, BoxSizing)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSS_VAL_CONTENT_BOX)
            m_style->setBoxSizing(CONTENT_BOX);
        else
            m_style->setBoxSizing(BORDER_BOX);
        return;
    case CSS_PROP__WEBKIT_COLUMN_COUNT: {
        if (isInherit) {
            if (m_parentStyle->hasAutoColumnCount())
                m_style->setHasAutoColumnCount();
            else
                m_style->setColumnCount(m_parentStyle->columnCount());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSS_VAL_AUTO) {
            m_style->setHasAutoColumnCount();
            return;
        }
        m_style->setColumnCount(static_cast<unsigned short>(primitiveValue->getDoubleValue()));
        return;
    }
    case CSS_PROP__WEBKIT_COLUMN_GAP: {
        if (isInherit) {
            if (m_parentStyle->hasNormalColumnGap())
                m_style->setHasNormalColumnGap();
            else
                m_style->setColumnGap(m_parentStyle->columnGap());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSS_VAL_NORMAL) {
            m_style->setHasNormalColumnGap();
            return;
        }
        m_style->setColumnGap(primitiveValue->computeLengthFloat(m_style));
        return;
    }
    case CSS_PROP__WEBKIT_COLUMN_WIDTH: {
        if (isInherit) {
            if (m_parentStyle->hasAutoColumnWidth())
                m_style->setHasAutoColumnWidth();
            else
                m_style->setColumnWidth(m_parentStyle->columnWidth());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSS_VAL_AUTO) {
            m_style->setHasAutoColumnWidth();
            return;
        }
        m_style->setColumnWidth(primitiveValue->computeLengthFloat(m_style));
        return;
    }
    case CSS_PROP__WEBKIT_COLUMN_RULE_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnRuleStyle, ColumnRuleStyle, BorderStyle)
        m_style->setColumnRuleStyle(*primitiveValue);
        return;
    case CSS_PROP__WEBKIT_COLUMN_BREAK_BEFORE: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnBreakBefore, ColumnBreakBefore, PageBreak)
        m_style->setColumnBreakBefore(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_COLUMN_BREAK_AFTER: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnBreakAfter, ColumnBreakAfter, PageBreak)
        m_style->setColumnBreakAfter(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_COLUMN_BREAK_INSIDE: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnBreakInside, ColumnBreakInside, PageBreak)
        EPageBreak pb = *primitiveValue;
        if (pb != PBALWAYS)
            m_style->setColumnBreakInside(pb);
        return;
    }
     case CSS_PROP__WEBKIT_COLUMN_RULE:
        if (isInherit) {
            m_style->setColumnRuleColor(m_parentStyle->columnRuleColor());
            m_style->setColumnRuleStyle(m_parentStyle->columnRuleStyle());
            m_style->setColumnRuleWidth(m_parentStyle->columnRuleWidth());
        }
        else if (isInitial)
            m_style->resetColumnRule();
        return;
    case CSS_PROP__WEBKIT_COLUMNS:
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
    case CSS_PROP__WEBKIT_MARQUEE:
        if (valueType != CSSValue::CSS_INHERIT || !m_parentNode) return;
        m_style->setMarqueeDirection(m_parentStyle->marqueeDirection());
        m_style->setMarqueeIncrement(m_parentStyle->marqueeIncrement());
        m_style->setMarqueeSpeed(m_parentStyle->marqueeSpeed());
        m_style->setMarqueeLoopCount(m_parentStyle->marqueeLoopCount());
        m_style->setMarqueeBehavior(m_parentStyle->marqueeBehavior());
        return;
    case CSS_PROP__WEBKIT_MARQUEE_REPETITION: {
        HANDLE_INHERIT_AND_INITIAL(marqueeLoopCount, MarqueeLoopCount)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSS_VAL_INFINITE)
            m_style->setMarqueeLoopCount(-1); // -1 means repeat forever.
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
            m_style->setMarqueeLoopCount(primitiveValue->getIntValue());
        return;
    }
    case CSS_PROP__WEBKIT_MARQUEE_SPEED: {
        HANDLE_INHERIT_AND_INITIAL(marqueeSpeed, MarqueeSpeed)      
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent()) {
                case CSS_VAL_SLOW:
                    m_style->setMarqueeSpeed(500); // 500 msec.
                    break;
                case CSS_VAL_NORMAL:
                    m_style->setMarqueeSpeed(85); // 85msec. The WinIE default.
                    break;
                case CSS_VAL_FAST:
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
    case CSS_PROP__WEBKIT_MARQUEE_INCREMENT: {
        HANDLE_INHERIT_AND_INITIAL(marqueeIncrement, MarqueeIncrement)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent()) {
                case CSS_VAL_SMALL:
                    m_style->setMarqueeIncrement(Length(1, Fixed)); // 1px.
                    break;
                case CSS_VAL_NORMAL:
                    m_style->setMarqueeIncrement(Length(6, Fixed)); // 6px. The WinIE default.
                    break;
                case CSS_VAL_LARGE:
                    m_style->setMarqueeIncrement(Length(36, Fixed)); // 36px.
                    break;
            }
        }
        else {
            bool ok = true;
            Length l = convertToLength(primitiveValue, m_style, &ok);
            if (ok)
                m_style->setMarqueeIncrement(l);
        }
        return;
    }
    case CSS_PROP__WEBKIT_MARQUEE_STYLE: {
        HANDLE_INHERIT_AND_INITIAL(marqueeBehavior, MarqueeBehavior)      
        if (primitiveValue)
            m_style->setMarqueeBehavior(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_MARQUEE_DIRECTION: {
        HANDLE_INHERIT_AND_INITIAL(marqueeDirection, MarqueeDirection)
        if (primitiveValue)
            m_style->setMarqueeDirection(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_USER_DRAG: {
        HANDLE_INHERIT_AND_INITIAL(userDrag, UserDrag)      
        if (primitiveValue)
            m_style->setUserDrag(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_USER_MODIFY: {
        HANDLE_INHERIT_AND_INITIAL(userModify, UserModify)      
        if (primitiveValue)
            m_style->setUserModify(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_USER_SELECT: {
        HANDLE_INHERIT_AND_INITIAL(userSelect, UserSelect)      
        if (primitiveValue)
            m_style->setUserSelect(*primitiveValue);
        return;
    }
    case CSS_PROP_TEXT_OVERFLOW: {
        // This property is supported by WinIE, and so we leave off the "-webkit-" in order to
        // work with WinIE-specific pages that use the property.
        HANDLE_INHERIT_AND_INITIAL(textOverflow, TextOverflow)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        m_style->setTextOverflow(primitiveValue->getIdent() == CSS_VAL_ELLIPSIS);
        return;
    }
    case CSS_PROP__WEBKIT_MARGIN_COLLAPSE: {
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
    case CSS_PROP__WEBKIT_MARGIN_TOP_COLLAPSE: {
        HANDLE_INHERIT_AND_INITIAL(marginTopCollapse, MarginTopCollapse)
        if (primitiveValue)
            m_style->setMarginTopCollapse(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_MARGIN_BOTTOM_COLLAPSE: {
        HANDLE_INHERIT_AND_INITIAL(marginBottomCollapse, MarginBottomCollapse)
        if (primitiveValue)
            m_style->setMarginBottomCollapse(*primitiveValue);
        return;
    }

    // Apple-specific changes.  Do not merge these properties into KHTML.
    case CSS_PROP__WEBKIT_LINE_CLAMP: {
        HANDLE_INHERIT_AND_INITIAL(lineClamp, LineClamp)
        if (!primitiveValue)
            return;
        m_style->setLineClamp(primitiveValue->getIntValue(CSSPrimitiveValue::CSS_PERCENTAGE));
        return;
    }
    case CSS_PROP__WEBKIT_HIGHLIGHT: {
        HANDLE_INHERIT_AND_INITIAL(highlight, Highlight);
        if (primitiveValue->getIdent() == CSS_VAL_NONE)
            m_style->setHighlight(nullAtom);
        else
            m_style->setHighlight(primitiveValue->getStringValue());
        return;
    }
    case CSS_PROP__WEBKIT_BORDER_FIT: {
        HANDLE_INHERIT_AND_INITIAL(borderFit, BorderFit);
        if (primitiveValue->getIdent() == CSS_VAL_BORDER)
            m_style->setBorderFit(BorderFitBorder);
        else
            m_style->setBorderFit(BorderFitLines);
        return;
    }
    case CSS_PROP__WEBKIT_TEXT_SIZE_ADJUST: {
        HANDLE_INHERIT_AND_INITIAL(textSizeAdjust, TextSizeAdjust)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        m_style->setTextSizeAdjust(primitiveValue->getIdent() == CSS_VAL_AUTO);
        m_fontDirty = true;
        return;
    }
    case CSS_PROP__WEBKIT_TEXT_SECURITY: {
        HANDLE_INHERIT_AND_INITIAL(textSecurity, TextSecurity)
        if (primitiveValue)
            m_style->setTextSecurity(*primitiveValue);
        return;
    }
    case CSS_PROP__WEBKIT_DASHBOARD_REGION: {
        HANDLE_INHERIT_AND_INITIAL(dashboardRegions, DashboardRegions)
        if (!primitiveValue)
            return;

        if (primitiveValue->getIdent() == CSS_VAL_NONE) {
            m_style->setDashboardRegions(RenderStyle::noneDashboardRegions());
            return;
        }

        DashboardRegion *region = primitiveValue->getDashboardRegionValue();
        if (!region)
            return;
            
        DashboardRegion *first = region;
        while (region) {
            Length top = convertToLength (region->top(), m_style);
            Length right = convertToLength (region->right(), m_style);
            Length bottom = convertToLength (region->bottom(), m_style);
            Length left = convertToLength (region->left(), m_style);
            if (region->m_isCircle)
                m_style->setDashboardRegion(StyleDashboardRegion::Circle, region->m_label, top, right, bottom, left, region == first ? false : true);
            else if (region->m_isRectangle)
                m_style->setDashboardRegion(StyleDashboardRegion::Rectangle, region->m_label, top, right, bottom, left, region == first ? false : true);
            region = region->m_next.get();
        }
        
        m_element->document()->setHasDashboardRegions(true);
        
        return;
    }
    case CSS_PROP__WEBKIT_RTL_ORDERING:
        HANDLE_INHERIT_AND_INITIAL(visuallyOrdered, VisuallyOrdered)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        m_style->setVisuallyOrdered(primitiveValue->getIdent() == CSS_VAL_VISUAL);
        return;
    case CSS_PROP__WEBKIT_TEXT_STROKE_WIDTH: {
        HANDLE_INHERIT_AND_INITIAL(textStrokeWidth, TextStrokeWidth)
        float width = 0;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_THIN:
            case CSS_VAL_MEDIUM:
            case CSS_VAL_THICK: {
                double result = 1.0 / 48;
                if (primitiveValue->getIdent() == CSS_VAL_MEDIUM)
                    result *= 3;
                else if (primitiveValue->getIdent() == CSS_VAL_THICK)
                    result *= 5;
                CSSPrimitiveValue val(result, CSSPrimitiveValue::CSS_EMS);
                width = val.computeLengthFloat(m_style);
                break;
            }
            default:
                width = primitiveValue->computeLengthFloat(m_style);
                break;
        }
        m_style->setTextStrokeWidth(width);
        return;
    }
    case CSS_PROP__WEBKIT_TRANSFORM: {
        HANDLE_INHERIT_AND_INITIAL(transform, Transform);
        TransformOperations operations;
        if (!value->isPrimitiveValue()) {
            CSSValueList* list = static_cast<CSSValueList*>(value);
            unsigned size = list->length();
            for (unsigned i = 0; i < size; i++) {
                CSSTransformValue* val = static_cast<CSSTransformValue*>(list->item(i));
                CSSValueList* values = val->values();
                
                CSSPrimitiveValue* firstValue = static_cast<CSSPrimitiveValue*>(values->item(0));
                 
                switch (val->type()) {
                    case CSSTransformValue::ScaleTransformOperation:
                    case CSSTransformValue::ScaleXTransformOperation:
                    case CSSTransformValue::ScaleYTransformOperation: {
                        double sx = 1.0;
                        double sy = 1.0;
                        if (val->type() == CSSTransformValue::ScaleYTransformOperation)
                            sy = firstValue->getDoubleValue();
                        else { 
                            sx = firstValue->getDoubleValue();
                            if (val->type() == CSSTransformValue::ScaleTransformOperation) {
                                if (values->length() > 1) {
                                    CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(values->item(1));
                                    sy = secondValue->getDoubleValue();
                                } else 
                                    sy = sx;
                            }
                        }
                        
                        ScaleTransformOperation* scale = new ScaleTransformOperation(sx, sy);
                        operations.append(scale);
                        break;
                    }
                    case CSSTransformValue::TranslateTransformOperation:
                    case CSSTransformValue::TranslateXTransformOperation:
                    case CSSTransformValue::TranslateYTransformOperation: {
                        bool ok;
                        Length tx = Length(0, Fixed);
                        Length ty = Length(0, Fixed);
                        if (val->type() == CSSTransformValue::TranslateYTransformOperation)
                            ty = convertToLength(firstValue, m_style, &ok);
                        else { 
                            tx = convertToLength(firstValue, m_style, &ok);
                            if (val->type() == CSSTransformValue::TranslateTransformOperation) {
                                if (values->length() > 1) {
                                    CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(values->item(1));
                                    ty = convertToLength(secondValue, m_style, &ok);
                                } else
                                    ty = tx;
                            }
                        }
                        
                        TranslateTransformOperation* translate = new TranslateTransformOperation(tx, ty);
                        operations.append(translate);
                        break;
                    }
                    case CSSTransformValue::RotateTransformOperation: {
                        double angle = firstValue->getDoubleValue();
                        if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                            angle = rad2deg(angle);
                        else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                            angle = grad2deg(angle);
                        RotateTransformOperation* rotate = new RotateTransformOperation(angle);
                        operations.append(rotate);
                        break;
                    }
                    case CSSTransformValue::SkewTransformOperation:
                    case CSSTransformValue::SkewXTransformOperation:
                    case CSSTransformValue::SkewYTransformOperation: {
                        double angleX = 0;
                        double angleY = 0;
                        double angle = firstValue->getDoubleValue();
                        if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                            angle = rad2deg(angle);
                        else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                            angle = grad2deg(angle);
                        if (val->type() == CSSTransformValue::SkewYTransformOperation)
                            angleY = angle;
                        else {
                            angleX = angle;
                            if (val->type() == CSSTransformValue::SkewTransformOperation) {
                                if (values->length() > 1) {
                                    CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(values->item(1));
                                    angleY = secondValue->getDoubleValue();
                                    if (secondValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                                        angleY = rad2deg(angle);
                                    else if (secondValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                                        angleY = grad2deg(angle);
                                } else
                                    angleY = angleX;
                            }
                        }
                        
                        SkewTransformOperation* skew = new SkewTransformOperation(angleX, angleY);
                        operations.append(skew);
                        break;
                    }
                    case CSSTransformValue::MatrixTransformOperation: {
                        CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(values->item(1));
                        CSSPrimitiveValue* thirdValue = static_cast<CSSPrimitiveValue*>(values->item(2));
                        CSSPrimitiveValue* fourthValue = static_cast<CSSPrimitiveValue*>(values->item(3));
                        CSSPrimitiveValue* fifthValue = static_cast<CSSPrimitiveValue*>(values->item(4));
                        CSSPrimitiveValue* sixthValue = static_cast<CSSPrimitiveValue*>(values->item(5));
                        MatrixTransformOperation* matrix = new MatrixTransformOperation(firstValue->getDoubleValue(),
                                                                                        secondValue->getDoubleValue(),
                                                                                        thirdValue->getDoubleValue(),
                                                                                        fourthValue->getDoubleValue(),
                                                                                        fifthValue->getDoubleValue(),
                                                                                        sixthValue->getDoubleValue());
                        operations.append(matrix);
                        break;
                    }   
                    
                    default:
                        break;
                }
            }
        }
        m_style->setTransform(operations);
        return;
    }
    case CSS_PROP__WEBKIT_TRANSFORM_ORIGIN:
        HANDLE_INHERIT_AND_INITIAL(transformOriginX, TransformOriginX)
        HANDLE_INHERIT_AND_INITIAL(transformOriginY, TransformOriginY)
        return;
    case CSS_PROP__WEBKIT_TRANSFORM_ORIGIN_X: {
        HANDLE_INHERIT_AND_INITIAL(transformOriginX, TransformOriginX)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length l;
        int type = primitiveValue->primitiveType();
        if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(m_style), Fixed);
        else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else
            return;
        m_style->setTransformOriginX(l);
        break;
    }
    case CSS_PROP__WEBKIT_TRANSFORM_ORIGIN_Y: {
        HANDLE_INHERIT_AND_INITIAL(transformOriginY, TransformOriginY)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length l;
        int type = primitiveValue->primitiveType();
        if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(m_style), Fixed);
        else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else
            return;
        m_style->setTransformOriginY(l);
        break;
    }
    case CSS_PROP__WEBKIT_TRANSITION:
        if (isInitial)
            m_style->clearTransitions();
        else if (isInherit)
            m_style->inheritTransitions(m_parentStyle->transitions());
        return;
    case CSS_PROP__WEBKIT_TRANSITION_DURATION:
        HANDLE_TRANSITION_VALUE(transitionDuration, TransitionDuration, value)
        return;
    case CSS_PROP__WEBKIT_TRANSITION_REPEAT_COUNT:
        HANDLE_TRANSITION_VALUE(transitionRepeatCount, TransitionRepeatCount, value)
        return;
    case CSS_PROP__WEBKIT_TRANSITION_TIMING_FUNCTION:
        HANDLE_TRANSITION_VALUE(transitionTimingFunction, TransitionTimingFunction, value)
        return;
    case CSS_PROP__WEBKIT_TRANSITION_PROPERTY:
        HANDLE_TRANSITION_VALUE(transitionProperty, TransitionProperty, value)
        return;
    case CSS_PROP_INVALID:
        return;
    case CSS_PROP_FONT_STRETCH:
    case CSS_PROP_PAGE:
    case CSS_PROP_QUOTES:
    case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:
    case CSS_PROP_SCROLLBAR_ARROW_COLOR:
    case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:
    case CSS_PROP_SCROLLBAR_FACE_COLOR:
    case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:
    case CSS_PROP_SCROLLBAR_SHADOW_COLOR:
    case CSS_PROP_SCROLLBAR_TRACK_COLOR:
    case CSS_PROP_SIZE:
    case CSS_PROP_TEXT_LINE_THROUGH:
    case CSS_PROP_TEXT_LINE_THROUGH_COLOR:
    case CSS_PROP_TEXT_LINE_THROUGH_MODE:
    case CSS_PROP_TEXT_LINE_THROUGH_STYLE:
    case CSS_PROP_TEXT_LINE_THROUGH_WIDTH:
    case CSS_PROP_TEXT_OVERLINE:
    case CSS_PROP_TEXT_OVERLINE_COLOR:
    case CSS_PROP_TEXT_OVERLINE_MODE:
    case CSS_PROP_TEXT_OVERLINE_STYLE:
    case CSS_PROP_TEXT_OVERLINE_WIDTH:
    case CSS_PROP_TEXT_UNDERLINE:
    case CSS_PROP_TEXT_UNDERLINE_COLOR:
    case CSS_PROP_TEXT_UNDERLINE_MODE:
    case CSS_PROP_TEXT_UNDERLINE_STYLE:
    case CSS_PROP_TEXT_UNDERLINE_WIDTH:
    case CSS_PROP__WEBKIT_FONT_SIZE_DELTA:
    case CSS_PROP__WEBKIT_MARGIN_START:
    case CSS_PROP__WEBKIT_PADDING_START:
    case CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT:
    case CSS_PROP__WEBKIT_TEXT_STROKE:
        return;
#if ENABLE(SVG)
    default:
        // Try the SVG properties
        applySVGProperty(id, value);
#endif
    }
}

void CSSStyleSelector::mapBackgroundAttachment(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundAttachment(RenderStyle::initialBackgroundAttachment());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    switch (primitiveValue->getIdent()) {
        case CSS_VAL_FIXED:
            layer->setBackgroundAttachment(false);
            break;
        case CSS_VAL_SCROLL:
            layer->setBackgroundAttachment(true);
            break;
        default:
            return;
    }
}

void CSSStyleSelector::mapBackgroundClip(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundClip(RenderStyle::initialBackgroundClip());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setBackgroundClip(*primitiveValue);
}

void CSSStyleSelector::mapBackgroundComposite(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundComposite(RenderStyle::initialBackgroundComposite());
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setBackgroundComposite(*primitiveValue);
}

void CSSStyleSelector::mapBackgroundOrigin(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundOrigin(RenderStyle::initialBackgroundOrigin());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setBackgroundOrigin(*primitiveValue);
}

void CSSStyleSelector::mapBackgroundImage(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundImage(RenderStyle::initialBackgroundImage());
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setBackgroundImage(static_cast<CSSImageValue*>(primitiveValue)->image(m_element->document()->docLoader()));
}

void CSSStyleSelector::mapBackgroundRepeat(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundRepeat(RenderStyle::initialBackgroundRepeat());
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setBackgroundRepeat(*primitiveValue);
}

void CSSStyleSelector::mapBackgroundSize(BackgroundLayer* layer, CSSValue* value)
{
    LengthSize b = RenderStyle::initialBackgroundSize();
    
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundSize(b);
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;
        
    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
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
    
    if (firstType == CSSPrimitiveValue::CSS_UNKNOWN)
        firstLength = Length(Auto);
    else if (firstType > CSSPrimitiveValue::CSS_PERCENTAGE && firstType < CSSPrimitiveValue::CSS_DEG)
        firstLength = Length(first->computeLengthIntForLength(m_style), Fixed);
    else if (firstType == CSSPrimitiveValue::CSS_PERCENTAGE)
        firstLength = Length(first->getDoubleValue(), Percent);
    else
        return;

    if (secondType == CSSPrimitiveValue::CSS_UNKNOWN)
        secondLength = Length(Auto);
    else if (secondType > CSSPrimitiveValue::CSS_PERCENTAGE && secondType < CSSPrimitiveValue::CSS_DEG)
        secondLength = Length(second->computeLengthIntForLength(m_style), Fixed);
    else if (secondType == CSSPrimitiveValue::CSS_PERCENTAGE)
        secondLength = Length(second->getDoubleValue(), Percent);
    else
        return;
    
    b.width = firstLength;
    b.height = secondLength;
    layer->setBackgroundSize(b);
}

void CSSStyleSelector::mapBackgroundXPosition(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundXPosition(RenderStyle::initialBackgroundXPosition());
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Length l;
    int type = primitiveValue->primitiveType();
    if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
        l = Length(primitiveValue->computeLengthIntForLength(m_style), Fixed);
    else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length(primitiveValue->getDoubleValue(), Percent);
    else
        return;
    layer->setBackgroundXPosition(l);
}

void CSSStyleSelector::mapBackgroundYPosition(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundYPosition(RenderStyle::initialBackgroundYPosition());
        return;
    }
    
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Length l;
    int type = primitiveValue->primitiveType();
    if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
        l = Length(primitiveValue->computeLengthIntForLength(m_style), Fixed);
    else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length(primitiveValue->getDoubleValue(), Percent);
    else
        return;
    layer->setBackgroundYPosition(l);
}

void CSSStyleSelector::mapTransitionDuration(Transition* transition, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        transition->setTransitionDuration(RenderStyle::initialTransitionDuration());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_S)
        transition->setTransitionDuration(int(1000*primitiveValue->getFloatValue()));
    else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_MS)
        transition->setTransitionDuration(int(primitiveValue->getFloatValue()));
}

void CSSStyleSelector::mapTransitionRepeatCount(Transition* transition, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        transition->setTransitionRepeatCount(RenderStyle::initialTransitionRepeatCount());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->getIdent() == CSS_VAL_INFINITE)
        transition->setTransitionRepeatCount(-1);
    else
        transition->setTransitionRepeatCount(int(primitiveValue->getFloatValue()));
}

void CSSStyleSelector::mapTransitionTimingFunction(Transition* transition, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        transition->setTransitionTimingFunction(RenderStyle::initialTransitionTimingFunction());
        return;
    }

    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_LINEAR:
                transition->setTransitionTimingFunction(TimingFunction(LinearTimingFunction));
                break;
            case CSS_VAL_AUTO:
                transition->setTransitionTimingFunction(TimingFunction());
                break;
            case CSS_VAL_EASE_IN:
                transition->setTransitionTimingFunction(TimingFunction(CubicBezierTimingFunction, .42, .0, 1.0, 1.0));
                break;
            case CSS_VAL_EASE_OUT:
                transition->setTransitionTimingFunction(TimingFunction(CubicBezierTimingFunction, .0, .0, .58, 1.0));
                break;
            case CSS_VAL_EASE_IN_OUT:
                transition->setTransitionTimingFunction(TimingFunction(CubicBezierTimingFunction, .42, .0, .58, 1.0));
                break;
        }
        return;
    }

    if (value->isTransitionTimingFunctionValue()) {
        CSSTimingFunctionValue* timingFunction = static_cast<CSSTimingFunctionValue*>(value);
        transition->setTransitionTimingFunction(TimingFunction(CubicBezierTimingFunction, timingFunction->x1(), timingFunction->y1(), timingFunction->x2(), timingFunction->y2()));
    }
}

void CSSStyleSelector::mapTransitionProperty(Transition* transition, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        transition->setTransitionProperty(RenderStyle::initialTransitionProperty());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    transition->setTransitionProperty(primitiveValue->getIdent());
}

void CSSStyleSelector::checkForTextSizeAdjust()
{
    if (m_style->textSizeAdjust())
        return;
 
    FontDescription newFontDescription(m_style->fontDescription());
    newFontDescription.setComputedSize(newFontDescription.specifiedSize());
    m_style->setFontDescription(newFontDescription);
}

void CSSStyleSelector::checkForGenericFamilyChange(RenderStyle* style, RenderStyle* parentStyle)
{
    const FontDescription& childFont = style->fontDescription();
  
    if (childFont.isAbsoluteSize() || !parentStyle)
        return;

    const FontDescription& parentFont = parentStyle->fontDescription();

    if (childFont.genericFamily() == parentFont.genericFamily())
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
    if (childFont.keywordSize()) {
        size = fontSizeForKeyword(CSS_VAL_XX_SMALL + childFont.keywordSize() - 1, style->htmlHacks(),
                                  childFont.genericFamily() == FontDescription::MonospaceFamily);
    } else {
        Settings* settings = m_document->settings();
        float fixedScaleFactor = settings
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = (parentFont.genericFamily() == FontDescription::MonospaceFamily) ? 
                childFont.specifiedSize()/fixedScaleFactor :
                childFont.specifiedSize()*fixedScaleFactor;
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
    
    Settings* settings = m_document->settings();
    if (!settings)
        return 1.0f;

    int minSize = settings->minimumFontSize();
    int minLogicalSize = settings->minimumLogicalFontSize();

    float zoomPercent = m_document->frame() ? m_document->frame()->zoomFactor() / 100.0f : 1.0f;
    float zoomedSize = specifiedSize * zoomPercent;

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
    Settings* settings = m_document->settings();
    if (!settings)
        return 1.0f;

    int mediumSize = fixed ? settings->defaultFixedFontSize() : settings->defaultFontSize();
    if (mediumSize >= fontSizeTableMin && mediumSize <= fontSizeTableMax) {
        // Look up the entry in the table.
        int row = mediumSize - fontSizeTableMin;
        int col = (keyword - CSS_VAL_XX_SMALL);
        return quirksMode ? quirksFontSizeTable[row][col] : strictFontSizeTable[row][col];
    }
    
    // Value is outside the range of the table. Apply the scale factor instead.
    float minLogicalSize = max(settings->minimumLogicalFontSize(), 1);
    return max(fontSizeFactors[keyword - CSS_VAL_XX_SMALL]*mediumSize, minLogicalSize);
}

float CSSStyleSelector::largerFontSize(float size, bool quirksMode) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level.  
    return size * 1.2f;
}

float CSSStyleSelector::smallerFontSize(float size, bool quirksMode) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level. 
    return size / 1.2f;
}

struct ColorValue {
    int cssValueId;
    RGBA32 color;
};

static const ColorValue colorValues[] = {
    { CSS_VAL_AQUA, 0xFF00FFFF },
    { CSS_VAL_BLACK, 0xFF000000 },
    { CSS_VAL_BLUE, 0xFF0000FF },
    { CSS_VAL_FUCHSIA, 0xFFFF00FF },
    { CSS_VAL_GRAY, 0xFF808080 },
    { CSS_VAL_GREEN, 0xFF008000  },
    { CSS_VAL_LIME, 0xFF00FF00 },
    { CSS_VAL_MAROON, 0xFF800000 },
    { CSS_VAL_NAVY, 0xFF000080 },
    { CSS_VAL_OLIVE, 0xFF808000  },
    { CSS_VAL_ORANGE, 0xFFFFA500 },
    { CSS_VAL_PURPLE, 0xFF800080 },
    { CSS_VAL_RED, 0xFFFF0000 },
    { CSS_VAL_SILVER, 0xFFC0C0C0 },
    { CSS_VAL_TEAL, 0xFF008080  },
    { CSS_VAL_WHITE, 0xFFFFFFFF },
    { CSS_VAL_YELLOW, 0xFFFFFF00 },
    { CSS_VAL_TRANSPARENT, 0x00000000 },
    { CSS_VAL_GREY, 0xFF808080 },
    { 0, 0 }
};

static Color colorForCSSValue(int cssValueId)
{
    for (const ColorValue* col = colorValues; col->cssValueId; ++col)
        if (col->cssValueId == cssValueId)
            return col->color;
    return theme()->systemColor(cssValueId);
}

Color CSSStyleSelector::getColorFromPrimitiveValue(CSSPrimitiveValue* primitiveValue)
{
    Color col;
    int ident = primitiveValue->getIdent();
    if (ident) {
        if (ident == CSS_VAL__WEBKIT_TEXT)
            col = m_element->document()->textColor();
        else if (ident == CSS_VAL__WEBKIT_LINK) {
            Color linkColor = m_element->document()->linkColor();
            Color visitedColor = m_element->document()->visitedLinkColor();
            if (linkColor == visitedColor)
                col = linkColor;
            else {
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    checkPseudoState(m_element);
                col = (pseudoState == PseudoLink) ? linkColor : visitedColor;
            }
        } else if (ident == CSS_VAL__WEBKIT_ACTIVELINK)
            col = m_element->document()->activeLinkColor();
        else if (ident == CSS_VAL__WEBKIT_FOCUS_RING_COLOR)
            col = focusRingColor();
        else
            col = colorForCSSValue(ident);
    } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR)
        col.setRGB(primitiveValue->getRGBColorValue());
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

} // namespace WebCore
