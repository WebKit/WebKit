/**
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
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
 */

#include "config.h"
#include "cssstyleselector.h"

#include "CSSBorderImageValue.h"
#include "CSSImageValue.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CachedImage.h"
#include "DashboardRegion.h"
#include "FontFamilyValue.h"
#include "FontValue.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "History.h"
#include "KWQKHTMLSettings.h"
#include "MediaList.h"
#include "Pair.h"
#include "RectImpl.h"
#include "RenderTheme.h"
#include "StyleSheetList.h"
#include "UserAgentStyleSheets.h"
#include "loader.h"
#include "ShadowValue.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// #define STYLE_SHARING_STATS 1

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) { \
    style->set##Prop(parentStyle->prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    style->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(prop, Prop, Value) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    style->set##Prop(RenderStyle::initial##Value());\
    return;\
}

#define HANDLE_BACKGROUND_INHERIT_AND_INITIAL(prop, Prop) \
if (isInherit) { \
    BackgroundLayer* currChild = style->accessBackgroundLayers(); \
    BackgroundLayer* prevChild = 0; \
    const BackgroundLayer* currParent = parentStyle->backgroundLayers(); \
    while (currParent && currParent->is##Prop##Set()) { \
        if (!currChild) { \
            /* Need to make a new layer.*/ \
            currChild = new BackgroundLayer(); \
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
    BackgroundLayer* currChild = style->accessBackgroundLayers(); \
    currChild->set##Prop(RenderStyle::initial##Prop()); \
    for (currChild = currChild->next(); currChild; currChild = currChild->next()) \
        currChild->clear##Prop(); \
    return; \
}

#define HANDLE_BACKGROUND_VALUE(prop, Prop, value) { \
HANDLE_BACKGROUND_INHERIT_AND_INITIAL(prop, Prop) \
if (!value->isPrimitiveValue() && !value->isValueList()) \
    return; \
BackgroundLayer* currChild = style->accessBackgroundLayers(); \
BackgroundLayer* prevChild = 0; \
if (value->isPrimitiveValue()) { \
    map##Prop(currChild, value); \
    currChild = currChild->next(); \
} \
else { \
    /* Walk each value and put it into a layer, creating new layers as needed. */ \
    CSSValueList* valueList = static_cast<CSSValueList*>(value); \
    for (unsigned int i = 0; i < valueList->length(); i++) { \
        if (!currChild) { \
            /* Need to make a new layer to hold this value */ \
            currChild = new BackgroundLayer(); \
            prevChild->setNext(currChild); \
        } \
        map##Prop(currChild, valueList->item(i)); \
        prevChild = currChild; \
        currChild = currChild->next(); \
    } \
} \
while (currChild) { \
    /* Reset all remaining layers to not have the property set. */ \
    currChild->clear##Prop(); \
    currChild = currChild->next(); \
} }

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if (id == propID) { \
    style->set##Prop(parentStyle->prop()); \
    return; \
}

#define HANDLE_INITIAL_COND(propID, Prop) \
if (id == propID) { \
    style->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

#define HANDLE_INITIAL_COND_WITH_VALUE(propID, Prop, Value) \
if (id == propID) { \
    style->set##Prop(RenderStyle::initial##Value()); \
    return; \
}

class CSSRuleSet
{
public:
    CSSRuleSet();
    ~CSSRuleSet();
    
    typedef HashMap<AtomicStringImpl*, CSSRuleDataList*> AtomRuleMap;
    
    void addRulesFromSheet(CSSStyleSheet* sheet, const String &medium = "screen");
    
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

CSSRuleSet *CSSStyleSelector::defaultStyle = 0;
CSSRuleSet *CSSStyleSelector::defaultQuirksStyle = 0;
CSSRuleSet *CSSStyleSelector::defaultPrintStyle = 0;
CSSStyleSheet *CSSStyleSelector::defaultSheet = 0;
RenderStyle* CSSStyleSelector::styleNotYetAvailable = 0;
CSSStyleSheet *CSSStyleSelector::quirksSheet = 0;
#if SVG_SUPPORT
CSSStyleSheet *CSSStyleSelector::svgSheet = 0;
#endif

static CSSStyleSelector::Encodedurl *currentEncodedURL = 0;
static PseudoState pseudoState;

CSSStyleSelector::CSSStyleSelector(Document* doc, const String& userStyleSheet, StyleSheetList *styleSheets, bool _strictParsing)
{
    init();

    view = doc->view();
    strictParsing = _strictParsing;
    settings = view ? view->frame()->settings() : 0;
    if (!defaultStyle)
        loadDefaultStyle();
    m_mediaType = view ? view->mediaType() : String("all");

    m_userStyle = 0;
    m_userSheet = 0;

    // FIXME: This sucks! The user sheet is reparsed every time!
    if (!userStyleSheet.isEmpty()) {
        m_userSheet = new CSSStyleSheet(doc);
        m_userSheet->parseString(userStyleSheet, strictParsing);

        m_userStyle = new CSSRuleSet();
        m_userStyle->addRulesFromSheet(m_userSheet, m_mediaType);
    }

    // add stylesheets from document
    m_authorStyle = new CSSRuleSet();

    DeprecatedPtrListIterator<StyleSheet> it(styleSheets->styleSheets);
    for (; it.current(); ++it)
        if (it.current()->isCSSStyleSheet() && !it.current()->disabled())
            m_authorStyle->addRulesFromSheet(static_cast<CSSStyleSheet*>(it.current()), m_mediaType);

    m_ruleList = 0;
    m_collectRulesOnly = false;
}

CSSStyleSelector::CSSStyleSelector(CSSStyleSheet *sheet)
{
    init();

    if (!defaultStyle)
        loadDefaultStyle();
    FrameView *view = sheet->doc()->view();
    m_mediaType =  view ? view->mediaType() : String("all");

    m_authorStyle = new CSSRuleSet();
    m_authorStyle->addRulesFromSheet(sheet, m_mediaType);
}

void CSSStyleSelector::init()
{
    element = 0;
    settings = 0;
    m_matchedRuleCount = m_matchedDeclCount = m_tmpRuleCount = 0;
}

void CSSStyleSelector::setEncodedURL(const KURL& url)
{
    KURL u = url;

    u.setQuery(DeprecatedString::null);
    u.setRef(DeprecatedString::null);
    encodedurl.file = u.url();
    int pos = encodedurl.file.findRev('/');
    encodedurl.path = encodedurl.file;
    if (pos > 0) {
        encodedurl.path.truncate(pos);
        encodedurl.path += '/';
    }
    u.setPath(DeprecatedString::null);
    encodedurl.host = u.url();
}

CSSStyleSelector::~CSSStyleSelector()
{
    delete m_authorStyle;
    delete m_userStyle;
    delete m_userSheet;
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
    if (defaultStyle)
        return;

    defaultStyle = new CSSRuleSet;
    defaultPrintStyle = new CSSRuleSet;
    defaultQuirksStyle = new CSSRuleSet;

    // Strict-mode rules.
    defaultSheet = parseUASheet(html4UserAgentStyleSheet);
    defaultStyle->addRulesFromSheet(defaultSheet, "screen");
    defaultPrintStyle->addRulesFromSheet(defaultSheet, "print");

#if SVG_SUPPORT
    // SVG rules.
    svgSheet = parseUASheet(svgUserAgentStyleSheet);
    defaultStyle->addRulesFromSheet(svgSheet, "screen");
    defaultPrintStyle->addRulesFromSheet(svgSheet, "print");
#endif

    // Quirks-mode rules.
    quirksSheet = parseUASheet(quirksUserAgentStyleSheet);
    defaultQuirksStyle->addRulesFromSheet(quirksSheet, "screen");
}

void CSSStyleSelector::addMatchedRule(CSSRuleData* rule)
{
    if (m_matchedRules.size() <= m_matchedRuleCount)
        m_matchedRules.resize(2*m_matchedRules.size()+1);
    m_matchedRules[m_matchedRuleCount++] = rule;
}

void CSSStyleSelector::addMatchedDeclaration(CSSMutableStyleDeclaration* decl)
{
    if (m_matchedDecls.size() <= m_matchedDeclCount)
        m_matchedDecls.resize(2*m_matchedDecls.size()+1);
    m_matchedDecls[m_matchedDeclCount++] = decl;
}

void CSSStyleSelector::matchRules(CSSRuleSet* rules, int& firstRuleIndex, int& lastRuleIndex)
{
    m_matchedRuleCount = 0;
    if (!rules || !element) return;
    
    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    if (element->hasID())
        matchRulesForList(rules->getIDRules(element->getIDAttribute().impl()), firstRuleIndex, lastRuleIndex);
    if (element->hasClass())
        for (const AtomicStringList* singleClass = element->getClassList(); singleClass; singleClass = singleClass->next())
            matchRulesForList(rules->getClassRules(singleClass->string().impl()), firstRuleIndex, lastRuleIndex);
    matchRulesForList(rules->getTagRules(element->localName().impl()),
                      firstRuleIndex, lastRuleIndex);
    matchRulesForList(rules->getUniversalRules(), firstRuleIndex, lastRuleIndex);
    
    // If we didn't match any rules, we're done.
    if (m_matchedRuleCount == 0) return;
    
    // Sort the set of matched rules.
    sortMatchedRules(0, m_matchedRuleCount);
    
    // Now transfer the set of matched rules over to our list of decls.
    if (!m_collectRulesOnly) {
        for (unsigned i = 0; i < m_matchedRuleCount; i++)
            addMatchedDeclaration(m_matchedRules[i]->rule()->declaration());
    } else {
        for (unsigned i = 0; i < m_matchedRuleCount; i++) {
            if (!m_ruleList)
                m_ruleList = new CSSRuleList();
            m_ruleList->append(m_matchedRules[i]->rule());
        }
    }
}

void CSSStyleSelector::matchRulesForList(CSSRuleDataList* rules, int& firstRuleIndex, int& lastRuleIndex)
{
    if (!rules) return;
    for (CSSRuleData* d = rules->first(); d; d = d->next()) {
        CSSStyleRule* rule = d->rule();
        const AtomicString& localName = element->localName();
        const AtomicString& selectorLocalName = d->selector()->tag.localName();
        if ((localName == selectorLocalName || selectorLocalName == starAtom) && checkSelector(d->selector(), element)) {
            // If the rule has no properties to apply, then ignore it.
            CSSMutableStyleDeclaration* decl = rule->declaration();
            if (!decl || !decl->length()) continue;
            
            // If we're matching normal rules, set a pseudo bit if 
            // we really just matched a pseudo-element.
            if (dynamicPseudo != RenderStyle::NOPSEUDO && pseudoStyle == RenderStyle::NOPSEUDO) {
                if (m_collectRulesOnly)
                    return;
                style->setHasPseudoStyle(dynamicPseudo);
            } else {
                // Update our first/last rule indices in the matched rules array.
                lastRuleIndex = m_matchedDeclCount + m_matchedRuleCount;
                if (firstRuleIndex == -1) firstRuleIndex = m_matchedDeclCount + m_matchedRuleCount;

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
    if (start >= end || (end-start == 1))
        return; // Sanity check.
    
    if (end - start <= 6) {
        // Apply a bubble sort for smaller lists.
        for (unsigned i = end-1; i > start; i--) {
            bool sorted = true;
            for (unsigned j = start; j < i; j++) {
                CSSRuleData* elt = m_matchedRules[j];
                CSSRuleData* elt2 = m_matchedRules[j+1];
                if (*elt > *elt2) {
                    sorted = false;
                    m_matchedRules[j] = elt2;
                    m_matchedRules[j+1] = elt;
                }
            }
            if (sorted)
                return;
        }
    }
    else {
        // Peform a merge sort for larger lists.
        unsigned mid = (start+end)/2;
        sortMatchedRules(start, mid);
        sortMatchedRules(mid, end);
        
        CSSRuleData* elt = m_matchedRules[mid-1];
        CSSRuleData* elt2 = m_matchedRules[mid];
        
        // Handle the fast common case (of equal specificity).  The list may already
        // be completely sorted.
        if (*elt <= *elt2)
            return;
        
        // We have to merge sort.  Ensure our merge buffer is big enough to hold
        // all the items.
        m_tmpRules.resize(end - start);
        unsigned i1 = start;
        unsigned i2 = mid;
        
        elt = m_matchedRules[i1];
        elt2 = m_matchedRules[i2];
        
        while (i1 < mid || i2 < end) {
            if (i1 < mid && (i2 == end || *elt <= *elt2)) {
                m_tmpRules[m_tmpRuleCount++] = elt;
                i1++;
                if (i1 < mid)
                    elt = m_matchedRules[i1];
            }
            else {
                m_tmpRules[m_tmpRuleCount++] = elt2;
                i2++;
                if (i2 < end)
                    elt2 = m_matchedRules[i2];
            }
        }
        
        for (unsigned i = start; i < end; i++)
            m_matchedRules[i] = m_tmpRules[i-start];
        
        m_tmpRuleCount = 0;
    }    
}

void CSSStyleSelector::initElementAndPseudoState(Element* e)
{
    element = e;
    if (element && element->isStyledElement())
        styledElement = static_cast<StyledElement*>(element);
    else
        styledElement = 0;
    currentEncodedURL = &encodedurl;
    pseudoState = PseudoUnknown;
}

void CSSStyleSelector::initForStyleResolve(Element* e, RenderStyle* defaultParent)
{
    // set some variables we will need
    pseudoStyle = RenderStyle::NOPSEUDO;

    parentNode = e->parentNode();
    if (defaultParent)
        parentStyle = defaultParent;
    else
        parentStyle = (parentNode && parentNode->renderer()) ? parentNode->renderer()->style() : 0;
    view = element->document()->view();
    isXMLDoc = !element->document()->isHTMLDocument();
    frame = element->document()->frame();
    settings = frame ? frame->settings() : 0;

    style = 0;
    
    m_matchedRuleCount = 0;
    m_matchedDeclCount = 0;
    m_tmpRuleCount = 0;
    m_ruleList = 0;

    fontDirty = false;
}

// modified version of the one in kurl.cpp
static void cleanpath(DeprecatedString &path)
{
    int pos;
    while ((pos = path.find("/../")) != -1) {
        int prev = 0;
        if (pos > 0)
            prev = path.findRev("/", pos -1);
        // don't remove the host, i.e. http://foo.org/../foo.html
        if (prev < 0 || (prev > 3 && path.findRev("://", prev-1) == prev-2))
            path.remove(pos, 3);
        else
            // matching directory found ?
            path.remove(prev, pos- prev + 3);
    }
    pos = 0;
    
    // Don't remove "//" from an anchor identifier. -rjw
    // Set refPos to -2 to mean "I haven't looked for the anchor yet".
    // We don't want to waste a function call on the search for the the anchor
    // in the vast majority of cases where there is no "//" in the path.
    int refPos = -2;
    while ((pos = path.find("//", pos)) != -1) {
        if (refPos == -2)
            refPos = path.find("#");
        if (refPos > 0 && pos >= refPos)
            break;
        
        if (pos == 0 || path[pos-1] != ':')
            path.remove(pos, 1);
        else
            pos += 2;
    }
    while ((pos = path.find("/./")) != -1)
        path.remove(pos, 2);
}

static void checkPseudoState(Element *e, bool checkVisited = true)
{
    if (!e->isLink()) {
        pseudoState = PseudoNone;
        return;
    }
    
    const AtomicString& attr = e->getAttribute(hrefAttr);
    if (attr.isNull()) {
        pseudoState = PseudoNone;
        return;
    }
    
    if (!checkVisited) {
        pseudoState = PseudoAnyLink;
        return;
    }
    
    QConstString cu(reinterpret_cast<const QChar*>(attr.characters()), attr.length());
    DeprecatedString u = cu.string();
    if (!u.contains("://")) {
        if (u[0] == '/')
            u.prepend(currentEncodedURL->host);
        else if (u[0] == '#')
            u.prepend(currentEncodedURL->file);
        else
            u.prepend(currentEncodedURL->path);
        cleanpath(u);
    }
    pseudoState = historyContains(u) ? PseudoVisited : PseudoLink;
}

#ifdef STYLE_SHARING_STATS
static int fraction = 0;
static int total = 0;
#endif

const int siblingThreshold = 10;

Node* CSSStyleSelector::locateCousinList(Element* parent)
{
    if (parent && parent->isStyledElement()) {
        StyledElement* p = static_cast<StyledElement*>(parent);
        if (p->renderer() && !p->inlineStyleDecl() && !p->hasID()) {
            Node* r = p->previousSibling();
            int subcount = 0;
            RenderStyle* st = p->renderer()->style();
            while (r) {
                if (r->renderer() && r->renderer()->style() == st)
                    return r->lastChild();
                if (subcount++ == siblingThreshold)
                    return 0;
                r = r->previousSibling();
            }
            if (!r)
                r = locateCousinList(static_cast<Element*>(parent->parentNode()));
            while (r) {
                if (r->renderer() && r->renderer()->style() == st)
                    return r->lastChild();
                if (subcount++ == siblingThreshold)
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
        if (s->renderer() && (s->tagQName() == element->tagQName()) && !s->hasID() &&
            (s->hasClass() == element->hasClass()) && !s->inlineStyleDecl() &&
            (s->hasMappedAttributes() == styledElement->hasMappedAttributes()) &&
            (s->isLink() == element->isLink()) && 
            !s->renderer()->style()->affectedByAttributeSelectors() &&
            (s->hovered() == element->hovered()) &&
            (s->active() == element->active()) &&
            (s->focused() == element->focused()) &&
            (s->isEnabled() == element->isEnabled()) &&
            (s->isIndeterminate() == element->isIndeterminate()) &&
            (s->isChecked() == element->isChecked()) &&
            (s != s->document()->getCSSTarget() && element != element->document()->getCSSTarget()) &&
            (s->getAttribute(typeAttr) == element->getAttribute(typeAttr))) {
            bool classesMatch = true;
            if (s->hasClass()) {
                const AtomicString& class1 = element->getAttribute(classAttr);
                const AtomicString& class2 = s->getAttribute(classAttr);
                classesMatch = (class1 == class2);
            }
            
            if (classesMatch) {
                bool mappedAttrsMatch = true;
                if (s->hasMappedAttributes())
                    mappedAttrsMatch = s->mappedAttributes()->mapsEquivalent(styledElement->mappedAttributes());
                if (mappedAttrsMatch) {
                    bool linksMatch = true;
                    if (s->isLink()) {
                        // We need to check to see if the visited state matches.
                        Color linkColor = element->document()->linkColor();
                        Color visitedColor = element->document()->visitedLinkColor();
                        if (pseudoState == PseudoUnknown)
                            checkPseudoState(element, s->renderer()->style()->pseudoState() != PseudoAnyLink ||
                                             linkColor != visitedColor);
                        linksMatch = (pseudoState == s->renderer()->style()->pseudoState());
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
    if (styledElement && !styledElement->inlineStyleDecl() && !styledElement->hasID() &&
        !styledElement->document()->usesSiblingRules()) {
        // Check previous siblings.
        int count = 0;
        Node* n;
        for (n = element->previousSibling(); n && !n->isElementNode(); n = n->previousSibling());
        while (n) {
            if (canShareStyleWithElement(n))
                return n->renderer()->style();
            if (count++ == siblingThreshold)
                return 0;
            for (n = n->previousSibling(); n && !n->isElementNode(); n = n->previousSibling());
        }
        if (!n) 
            n = locateCousinList(static_cast<Element*>(element->parentNode()));
        while (n) {
            if (canShareStyleWithElement(n))
                return n->renderer()->style();
            if (count++ == siblingThreshold)
                return 0;
            for (n = n->previousSibling(); n && !n->isElementNode(); n = n->previousSibling());
        }        
    }
    return 0;
}


RenderStyle* CSSStyleSelector::styleForElement(Element* e, RenderStyle* defaultParent, bool allowSharing)
{
    if (allowSharing && !e->document()->haveStylesheetsLoaded()) {
        if (!styleNotYetAvailable) {
            styleNotYetAvailable = ::new RenderStyle();
            styleNotYetAvailable->setDisplay(NONE);
            styleNotYetAvailable->ref();
        }
        styleNotYetAvailable->ref();
        return styleNotYetAvailable;
    }
    
    initElementAndPseudoState(e);
    if (allowSharing) {
        style = locateSharedStyle();
#ifdef STYLE_SHARING_STATS
        fraction += style != 0;
        total++;
        printf("Sharing %d out of %d\n", fraction, total);
#endif
        if (style) {
            style->ref();
            return style;
        }
    }
    initForStyleResolve(e, defaultParent);

    style = new (e->document()->renderArena()) RenderStyle();
    style->ref();
    if (parentStyle)
        style->inheritFrom(parentStyle);
    else
        parentStyle = style;
    
    // 1. First we match rules from the user agent sheet.
    int firstUARule = -1, lastUARule = -1;
    matchRules(defaultStyle, firstUARule, lastUARule);
    
    // 2. In quirks mode, we match rules from the quirks user agent sheet.
    if (!strictParsing)
        matchRules(defaultQuirksStyle, firstUARule, lastUARule);
    
    // 3. If our medium is print, then we match rules from the print sheet.
    if (m_mediaType == "print")
        matchRules(defaultPrintStyle, firstUARule, lastUARule);

    // 4. Now we check user sheet rules.
    int firstUserRule = -1, lastUserRule = -1;
    matchRules(m_userStyle, firstUserRule, lastUserRule);

    // 5. Now check author rules, beginning first with presentational attributes
    // mapped from HTML.
    int firstAuthorRule = -1, lastAuthorRule = -1;
    if (styledElement) {
        // Ask if the HTML element has mapped attributes.
        if (styledElement->hasMappedAttributes()) {
            // Walk our attribute list and add in each decl.
            const NamedMappedAttrMap* map = styledElement->mappedAttributes();
            for (unsigned i = 0; i < map->length(); i++) {
                MappedAttribute* attr = map->attributeItem(i);
                if (attr->decl()) {
                    if (firstAuthorRule == -1) firstAuthorRule = m_matchedDeclCount;
                    lastAuthorRule = m_matchedDeclCount;
                    addMatchedDeclaration(attr->decl());
                }
            }
        }

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        CSSMutableStyleDeclaration* attributeDecl = styledElement->additionalAttributeStyleDecl();
        if (attributeDecl) {
            if (firstAuthorRule == -1) firstAuthorRule = m_matchedDeclCount;
            lastAuthorRule = m_matchedDeclCount;
            addMatchedDeclaration(attributeDecl);
        }
    }
    
    // 6. Check the rules in author sheets next.
    matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    
    // 7. Now check our inline style attribute.
    if (styledElement) {
        CSSMutableStyleDeclaration* inlineDecl = styledElement->inlineStyleDecl();
        if (inlineDecl) {
            if (firstAuthorRule == -1) firstAuthorRule = m_matchedDeclCount;
            lastAuthorRule = m_matchedDeclCount;
            addMatchedDeclaration(inlineDecl);
        }
    }
    
    // Now we have all of the matched rules in the appropriate order.  Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    applyDeclarations(true, false, 0, m_matchedDeclCount-1);
    applyDeclarations(true, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(true, true, firstUserRule, lastUserRule);
    applyDeclarations(true, true, firstUARule, lastUARule);
    
    // If our font got dirtied, go ahead and update it now.
    if (fontDirty) {
        checkForTextSizeAdjust();
        checkForGenericFamilyChange(style, parentStyle);
        style->font().update();
        fontDirty = false;
    }
    
    // Now do the normal priority UA properties.
    applyDeclarations(false, false, firstUARule, lastUARule);
    
    // Cache our border and background so that we can examine them later.
    m_hasUAAppearance = style->hasAppearance();
    if (m_hasUAAppearance) {
        m_borderData = style->border();
        m_backgroundData = *style->backgroundLayers();
        m_backgroundColor = style->backgroundColor();
    }
    
    // Now do the author and user normal priority properties and all the !important properties.
    applyDeclarations(false, false, lastUARule+1, m_matchedDeclCount-1);
    applyDeclarations(false, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(false, true, firstUserRule, lastUserRule);
    applyDeclarations(false, true, firstUARule, lastUARule);
    
    // If our font got dirtied by one of the non-essential font props, 
    // go ahead and update it a second time.
    if (fontDirty) {
        checkForTextSizeAdjust();
        checkForGenericFamilyChange(style, parentStyle);
        style->font().update();
        fontDirty = false;
    }
    
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style, e);

    // If we are a link, cache the determined pseudo-state.
    if (e->isLink())
        style->setPseudoState(pseudoState);

    // Now return the style.
    return style;
}

RenderStyle* CSSStyleSelector::pseudoStyleForElement(RenderStyle::PseudoId pseudo, Element* e, RenderStyle* parentStyle)
{
    if (!e)
        return 0;

    initElementAndPseudoState(e);
    initForStyleResolve(e, parentStyle);
    pseudoStyle = pseudo;
    
    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.
    
    // Check UA, user and author rules.
    int firstUARule = -1, lastUARule = -1, firstUserRule = -1, lastUserRule = -1, firstAuthorRule = -1, lastAuthorRule = -1;
    matchRules(defaultStyle, firstUARule, lastUARule);
    matchRules(m_userStyle, firstUserRule, lastUserRule);
    matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    
    if (m_matchedDeclCount == 0)
        return 0;
    
    style = new (e->document()->renderArena()) RenderStyle();
    style->ref();
    if (parentStyle)
        style->inheritFrom(parentStyle);
    else
        parentStyle = style;
    style->noninherited_flags._styleType = pseudoStyle;
    
    // High-priority properties.
    applyDeclarations(true, false, 0, m_matchedDeclCount-1);
    applyDeclarations(true, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(true, true, firstUserRule, lastUserRule);
    applyDeclarations(true, true, firstUARule, lastUARule);
    
    // If our font got dirtied, go ahead and update it now.
    if (fontDirty) {
        checkForTextSizeAdjust();
        checkForGenericFamilyChange(style, parentStyle);
        style->font().update();
        fontDirty = false;
    }
    
    // Now do the normal priority properties.
    applyDeclarations(false, false, firstUARule, lastUARule);
    
    // Cache our border and background so that we can examine them later.
    m_hasUAAppearance = style->hasAppearance();
    if (m_hasUAAppearance) {
        m_borderData = style->border();
        m_backgroundData = *style->backgroundLayers();
        m_backgroundColor = style->backgroundColor();
    }
    
    applyDeclarations(false, false, lastUARule+1, m_matchedDeclCount-1);
    applyDeclarations(false, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(false, true, firstUserRule, lastUserRule);
    applyDeclarations(false, true, firstUARule, lastUARule);
    
    // If our font got dirtied by one of the non-essential font props, 
    // go ahead and update it a second time.
    if (fontDirty) {
        checkForTextSizeAdjust();
        checkForGenericFamilyChange(style, parentStyle);
        style->font().update();
        fontDirty = false;
    }
    
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style, 0);

    // Now return the style.
    return style;
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
    // relatively positioned, or transparent.
    if (style->position() == StaticPosition && style->opacity() == 1.0f)
        style->setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects.  This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them.
    if (style->hasAutoZIndex() && ((e && e->document()->documentElement() == e) || style->opacity() < 1.0f))
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
    
    // Table rows, sections and the table itself will support overflow:hidden and will ignore scroll/auto.
    // FIXME: Eventually table sections will support auto and scroll.
    if (style->overflow() != OVISIBLE && style->overflow() != OHIDDEN && 
        (style->display() == TABLE || style->display() == INLINE_TABLE ||
         style->display() == TABLE_ROW_GROUP || style->display() == TABLE_ROW))
        style->setOverflow(OVISIBLE);

    // Links should be user selectable when content editable
    if (e && e->isLink() && (style->userModify() == READ_WRITE || style->userModify() == READ_WRITE_PLAINTEXT_ONLY))
        style->setUserSelect(SELECT_AUTO);

    // Cull out any useless layers and also repeat patterns into additional layers.
    style->adjustBackgroundLayers();

    // Let the theme also have a crack at adjusting the style.
    if (style->hasAppearance()) {
        if (m_hasUAAppearance && theme()->isControlStyled(style, m_borderData, m_backgroundData, m_backgroundColor))
            style->setAppearance(NoAppearance);
        else
            theme()->adjustStyle(this, style, e);
    }

    // Only use slow repaints if we actually have a background image.
    // FIXME: We only need to invalidate the fixed regions when scrolling.  It's total overkill to
    // prevent the entire view from blitting on a scroll.
    if (style->hasFixedBackgroundImage() && view)
        view->useSlowRepaints();
}

RefPtr<CSSRuleList> CSSStyleSelector::styleRulesForElement(Element* e, bool authorOnly)
{
    if (!e->document()->haveStylesheetsLoaded())
        return 0;

    m_collectRulesOnly = true;
    
    initElementAndPseudoState(e);
    initForStyleResolve(e, 0);
    
    if (!authorOnly) {
        // First we match rules from the user agent sheet.
        int firstUARule = -1, lastUARule = -1;
        matchRules(defaultStyle, firstUARule, lastUARule);
        
        // In quirks mode, we match rules from the quirks user agent sheet.
        if (!strictParsing)
            matchRules(defaultQuirksStyle, firstUARule, lastUARule);
        
        // If our medium is print, then we match rules from the print sheet.
        if (m_mediaType == "print")
            matchRules(defaultPrintStyle, firstUARule, lastUARule);

        // Now we check user sheet rules.
        int firstUserRule = -1, lastUserRule = -1;
        matchRules(m_userStyle, firstUserRule, lastUserRule);
    }

    // Check the rules in author sheets.
    int firstAuthorRule = -1, lastAuthorRule = -1;
    matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    
    m_collectRulesOnly = false;
    
    return m_ruleList;
}

RefPtr<CSSRuleList> CSSStyleSelector::pseudoStyleRulesForElement(Element* e, StringImpl* pseudoStyle, bool authorOnly)
{
    // FIXME: Implement this.
    return 0;
}

static bool subject;

bool CSSStyleSelector::checkSelector(CSSSelector* sel, Element *e)
{
    dynamicPseudo = RenderStyle::NOPSEUDO;
    
    Node *n = e;

    // we have the subject part of the selector
    subject = true;

    // We track whether or not the rule contains only :hover and :active in a simple selector. If
    // so, we can't allow that to apply to every element on the page.  We assume the author intended
    // to apply the rules only to links.
    bool onlyHoverActive = (!sel->hasTag() &&
                            (sel->match == CSSSelector::PseudoClass &&
                              (sel->pseudoType() == CSSSelector::PseudoHover ||
                               sel->pseudoType() == CSSSelector::PseudoActive)));
    bool affectedByHover = style ? style->affectedByHoverRules() : false;
    bool affectedByActive = style ? style->affectedByActiveRules() : false;
    bool havePseudo = pseudoStyle != RenderStyle::NOPSEUDO;
    
    // first selector has to match
    if (!checkOneSelector(sel, e))
        return false;

    // check the subselectors
    CSSSelector::Relation relation = sel->relation();
    while((sel = sel->tagHistory)) {
        if (!n->isElementNode())
            return false;
        if (relation != CSSSelector::SubSelector) {
            subject = false;
            if (havePseudo && dynamicPseudo != pseudoStyle)
                return false;
        }
        
        switch(relation)
        {
        case CSSSelector::Descendant:
            // FIXME: This match needs to know how to backtrack and be non-deterministic.
            do {
                n = n->parentNode();
                if (!n || !n->isElementNode())
                    return false;
            } while (!checkOneSelector(sel, static_cast<Element*>(n)));
            break;
        case CSSSelector::Child:
        {
            n = n->parentNode();
            if (!strictParsing)
                while (n && n->implicitNode())
                    n = n->parentNode();
            if (!n || !n->isElementNode())
                return false;
            if (!checkOneSelector(sel, static_cast<Element*>(n)))
                return false;
            break;
        }
        case CSSSelector::DirectAdjacent:
        {
            n = n->previousSibling();
            while (n && !n->isElementNode())
                n = n->previousSibling();
            if (!n)
                return false;
            if (!checkOneSelector(sel, static_cast<Element*>(n)))
                return false;
            break;
        }
        case CSSSelector::IndirectAdjacent:
            // FIXME: This match needs to know how to backtrack and be non-deterministic.
            do {
                n = n->previousSibling();
                while (n && !n->isElementNode())
                    n = n->previousSibling();
                if (!n)
                    return false;
            } while (!checkOneSelector(sel, static_cast<Element*>(n)));
            break;
       case CSSSelector::SubSelector:
       {
            if (onlyHoverActive)
                onlyHoverActive = (sel->match == CSSSelector::PseudoClass &&
                                   (sel->pseudoType() == CSSSelector::PseudoHover ||
                                    sel->pseudoType() == CSSSelector::PseudoActive));
            
            Element *elem = static_cast<Element*>(n);
            // a selector is invalid if something follows :first-xxx
            if (elem == element && dynamicPseudo != RenderStyle::NOPSEUDO)
                return false;
            if (!checkOneSelector(sel, elem, true))
                return false;
            break;
        }
        }
        relation = sel->relation();
    }

    if (subject && havePseudo && dynamicPseudo != pseudoStyle)
        return false;
    
    // disallow *:hover, *:active, and *:hover:active except for links
    if (onlyHoverActive && subject) {
        if (pseudoState == PseudoUnknown)
            checkPseudoState(e);

        if (pseudoState == PseudoNone) {
            if (!affectedByHover && style->affectedByHoverRules())
                style->setAffectedByHoverRules(false);
            if (!affectedByActive && style->affectedByActiveRules())
                style->setAffectedByActiveRules(false);
            return false;
        }
    }

    return true;
}

bool CSSStyleSelector::checkOneSelector(CSSSelector* sel, Element* e, bool isSubSelector)
{
    if(!e)
        return false;

    if (sel->hasTag()) {
        const AtomicString& localName = e->localName();
        const AtomicString& ns = e->namespaceURI();
        const AtomicString& selLocalName = sel->tag.localName();
        const AtomicString& selNS = sel->tag.namespaceURI();
    
        if ((selLocalName != starAtom && localName != selLocalName) ||
            (selNS != starAtom && ns != selNS))
            return false;
    }

    if (sel->hasAttribute()) {
        if (sel->match == CSSSelector::Class) {
            if (!e->hasClass())
                return false;
            for (const AtomicStringList* c = e->getClassList(); c; c = c->next())
                if (c->string() == sel->value)
                    return true;
            return false;
        }
        else if (sel->match == CSSSelector::Id)
            return e->hasID() && e->getIDAttribute() == sel->value;
        else if (style && (e != element || !styledElement || (!styledElement->isMappedAttribute(sel->attr) && sel->attr != typeAttr)))
            style->setAffectedByAttributeSelectors(); // Special-case the "type" attribute so input form controls can share style.

        const AtomicString& value = e->getAttribute(sel->attr);
        if (value.isNull())
            return false; // attribute is not set

        switch(sel->match) {
        case CSSSelector::Exact:
            if ((isXMLDoc && sel->value != value) || (!isXMLDoc && !equalIgnoringCase(sel->value, value)))
                return false;
            break;
        case CSSSelector::List:
        {
            // The selector's value can't contain a space, or it's totally bogus.
            if (sel->value.contains(' '))
                return false;

            int startSearchAt = 0;
            while (true) {
                int foundPos = value.find(sel->value, startSearchAt, isXMLDoc);
                if (foundPos == -1)
                    return false;
                if (foundPos == 0 || value[foundPos-1] == ' ') {
                    unsigned endStr = foundPos + sel->value.length();
                    if (endStr == value.length() || value[endStr] == ' ')
                        break; // We found a match.
                }
                
                // No match.  Keep looking.
                startSearchAt = foundPos + 1;
            }
            break;
        }
        case CSSSelector::Contain:
            if (!value.contains(sel->value, isXMLDoc))
                return false;
            break;
        case CSSSelector::Begin:
            if (!value.startsWith(sel->value, isXMLDoc))
                return false;
            break;
        case CSSSelector::End:
            if (!value.endsWith(sel->value, isXMLDoc))
                return false;
            break;
        case CSSSelector::Hyphen:
            if (value.length() < sel->value.length())
                return false;
            if (!value.startsWith(sel->value, isXMLDoc))
                return false;
            // It they start the same, check for exact match or following '-':
            if (value.length() != sel->value.length() && value[sel->value.length()] != '-')
                return false;
            break;
        case CSSSelector::PseudoClass:
        case CSSSelector::PseudoElement:
        default:
            break;
        }
    }
    if(sel->match == CSSSelector::PseudoClass || sel->match == CSSSelector::PseudoElement)
    {
        // Pseudo elements. We need to check first child here. No dynamic pseudo
        // elements for the moment
            switch (sel->pseudoType()) {
                // Pseudo classes:
            case CSSSelector::PseudoEmpty:
                if (!e->firstChild())
                    return true;
                break;
            case CSSSelector::PseudoFirstChild: {
                // first-child matches the first child that is an element!
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Node *n = e->previousSibling();
                    while (n && !n->isElementNode())
                        n = n->previousSibling();
                    if (!n)
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoFirstOfType: {
                // first-of-type matches the first element of its type!
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    const QualifiedName& type = e->tagQName();
                    Node *n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->previousSibling();
                    }
                    if (!n)
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoLastChild: {
                // last-child matches the last child that is an element!
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Node *n = e->nextSibling();
                    while (n && !n->isElementNode())
                        n = n->nextSibling();
                    if (!n)
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoLastOfType: {
                // last-of-type matches the last element of its type!
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    const QualifiedName& type = e->tagQName();
                    Node *n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->nextSibling();
                    }
                    if (!n)
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoOnlyChild: {
                // If both first-child and last-child apply, then only-child applies.
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Node *n = e->previousSibling();
                    while (n && !n->isElementNode())
                        n = n->previousSibling();
                    if (!n) {
                        n = e->nextSibling();
                        while (n && !n->isElementNode())
                            n = n->nextSibling();
                        if (!n)
                            return true;
                    }
                }
                break;
            }
            case CSSSelector::PseudoOnlyOfType: {
                // If both first-of-type and last-of-type apply, then only-of-type applies.
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    const QualifiedName& type = e->tagQName();
                    Node *n = e->previousSibling();
                    while (n && !static_cast<Element*>(n)->hasTagName(type))
                        n = n->previousSibling();
                    if (!n) {
                        n = e->nextSibling();
                        while (n && !static_cast<Element*>(n)->hasTagName(type))
                            n = n->nextSibling();
                        if (!n)
                            return true;
                    }
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
            case CSSSelector::PseudoHover: {
                // If we're in quirks mode, then hover should never match anchors with no
                // href and *:hover should not match anything.  This is important for sites like wsj.com.
                if (strictParsing || isSubSelector || sel->relation() == CSSSelector::SubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                    if (element == e && style)
                        style->setAffectedByHoverRules(true);
                    if (e->renderer()) {
                        if (element != e)
                            e->renderer()->style()->setAffectedByHoverRules(true);
                        if (e->hovered())
                            return true;
                    }
                }
                break;
            }
            case CSSSelector::PseudoDrag: {
                if (element == e && style)
                    style->setAffectedByDragRules(true);
                if (e->renderer()) {
                    if (element != e)
                        e->renderer()->style()->setAffectedByDragRules(true);
                    if (e->renderer()->isDragging())
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoFocus:
                if (e && e->focused() && e->document()->frame()->displaysWithFocusAttributes()) {
                    return true;
                }
                break;
            case CSSSelector::PseudoActive:
                // If we're in quirks mode, then :active should never match anchors with no
                // href and *:active should not match anything. 
                if (strictParsing || isSubSelector || sel->relation() == CSSSelector::SubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                    if (element == e && style)
                        style->setAffectedByActiveRules(true);
                    else if (e->renderer())
                        e->renderer()->style()->setAffectedByActiveRules(true);
                    if (e->active())
                        return true;
                }
                break;
            case CSSSelector::PseudoEnabled:
                if (e && e->isControl())
                    // The UI spec states that you can't match :enabled unless you are an object that can
                    // "receive focus and be activated."  We will limit matching of this pseudo-class to elements
                    // that are controls.
                    return e->isEnabled();                    
                break;
            case CSSSelector::PseudoDisabled:
                if (e && e->isControl())
                    // The UI spec states that you can't match :enabled unless you are an object that can
                    // "receive focus and be activated."  We will limit matching of this pseudo-class to elements
                    // that are controls.
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
            case CSSSelector::PseudoNot: {
                // check the simple selector
                for (CSSSelector* subSel = sel->simpleSelector; subSel;
                     subSel = subSel->tagHistory) {
                    // :not cannot nest.  I don't really know why this is a restriction in CSS3,
                    // but it is, so let's honor it.
                    if (subSel->simpleSelector)
                        break;
                    if (!checkOneSelector(subSel, e))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoLang:
                /* not supported for now */
            case CSSSelector::PseudoOther:
                break;
            
            // Pseudo-elements:
            case CSSSelector::PseudoFirstLine:
                if (subject) {
                    dynamicPseudo=RenderStyle::FIRST_LINE;
                    return true;
                }
                break;
            case CSSSelector::PseudoFirstLetter:
                if (subject) {
                    dynamicPseudo=RenderStyle::FIRST_LETTER;
                    return true;
                }
                break;
            case CSSSelector::PseudoSelection:
                dynamicPseudo = RenderStyle::SELECTION;
                return true;
            case CSSSelector::PseudoBefore:
                dynamicPseudo = RenderStyle::BEFORE;
                return true;
            case CSSSelector::PseudoAfter:
                dynamicPseudo = RenderStyle::AFTER;
                return true;
                
            case CSSSelector::PseudoNotParsed:
                assert(false);
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
    if (sel->match == CSSSelector::Id) {
        addToRuleSet(sel->value.impl(), m_idRules, rule, sel);
        return;
    }
    if (sel->match == CSSSelector::Class) {
        addToRuleSet(sel->value.impl(), m_classRules, rule, sel);
        return;
    }
     
    const AtomicString& localName = sel->tag.localName();
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

void CSSRuleSet::addRulesFromSheet(CSSStyleSheet *sheet, const String &medium)
{
    if (!sheet || !sheet->isCSSStyleSheet())
        return;

    // No media implies "all", but if a media list exists it must
    // contain our current medium
    if (sheet->media() && !sheet->media()->contains(medium))
        return; // the style sheet doesn't apply

    int len = sheet->length();

    for (int i = 0; i < len; i++) {
        StyleBase* item = sheet->item(i);
        if (item->isStyleRule()) {
            CSSStyleRule* rule = static_cast<CSSStyleRule*>(item);
            for (CSSSelector* s = rule->selector(); s; s = s->next())
                addRule(rule, s);
        }
        else if(item->isImportRule()) {
            CSSImportRule* import = static_cast<CSSImportRule*>(item);
            if (!import->media() || import->media()->contains(medium))
                addRulesFromSheet(import->styleSheet(), medium);
        }
        else if(item->isMediaRule()) {
            CSSMediaRule* r = static_cast<CSSMediaRule*>(item);
            CSSRuleList* rules = r->cssRules();

            if ((!r->media() || r->media()->contains(medium)) && rules) {
                // Traverse child elements of the @media rule.
                for (unsigned j = 0; j < rules->length(); j++) {
                    CSSRule *childItem = rules->item(j);
                    if (childItem->isStyleRule()) {
                        // It is a StyleRule, so append it to our list
                        CSSStyleRule* rule = static_cast<CSSStyleRule*>(childItem);
                        for (CSSSelector* s = rule->selector(); s; s = s->next())
                            addRule(rule, s);
                        
                    }
                }   // for rules
            }   // if rules
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
        if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(style), Fixed);
        else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(int(primitiveValue->getFloatValue()), Percent);
        else if(type == CSSPrimitiveValue::CSS_NUMBER)
            l = Length(int(primitiveValue->getFloatValue() * 100), Percent);
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
                switch(current.id()) {
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

void CSSStyleSelector::applyProperty(int id, CSSValue *value)
{
    CSSPrimitiveValue *primitiveValue = 0;
    if(value->isPrimitiveValue()) primitiveValue = static_cast<CSSPrimitiveValue*>(value);

    Length l;
    bool apply = false;

    unsigned short valueType = value->cssValueType();

    bool isInherit = parentNode && valueType == CSSValue::CSS_INHERIT;
    bool isInitial = valueType == CSSValue::CSS_INITIAL || (!parentNode && valueType == CSSValue::CSS_INHERIT);

    // These properties are used to set the correct margins/padding on RTL lists.
    if (id == CSS_PROP__WEBKIT_MARGIN_START)
        id = style->direction() == LTR ? CSS_PROP_MARGIN_LEFT : CSS_PROP_MARGIN_RIGHT;
    else if (id == CSS_PROP__WEBKIT_PADDING_START)
        id = style->direction() == LTR ? CSS_PROP_PADDING_LEFT : CSS_PROP_PADDING_RIGHT;

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.  Shorthands (e.g. border, background) occur in this list as well and
    // are only hit when mapping "inherit" or "initial" into front-end values.
    switch(id)
    {
// ident only properties
    case CSS_PROP_BACKGROUND_ATTACHMENT:
        HANDLE_BACKGROUND_VALUE(backgroundAttachment, BackgroundAttachment, value)
        break;
    case CSS_PROP__WEBKIT_BACKGROUND_CLIP:
        HANDLE_BACKGROUND_VALUE(backgroundClip, BackgroundClip, value)
        break;
    case CSS_PROP__WEBKIT_BACKGROUND_ORIGIN:
        HANDLE_BACKGROUND_VALUE(backgroundOrigin, BackgroundOrigin, value)
        break;
    case CSS_PROP_BACKGROUND_REPEAT:
        HANDLE_BACKGROUND_VALUE(backgroundRepeat, BackgroundRepeat, value)
        break;
    case CSS_PROP__WEBKIT_BACKGROUND_SIZE:
        HANDLE_BACKGROUND_VALUE(backgroundSize, BackgroundSize, value)
        break;
    case CSS_PROP_BORDER_COLLAPSE:
        HANDLE_INHERIT_AND_INITIAL(borderCollapse, BorderCollapse)
        if(!primitiveValue) break;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_COLLAPSE:
            style->setBorderCollapse(true);
            break;
        case CSS_VAL_SEPARATE:
            style->setBorderCollapse(false);
            break;
        default:
            return;
        }
        break;
        
    case CSS_PROP_BORDER_TOP_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderTopStyle, BorderTopStyle, BorderStyle)
        if (!primitiveValue) return;
        style->setBorderTopStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_BORDER_RIGHT_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderRightStyle, BorderRightStyle, BorderStyle)
        if (!primitiveValue) return;
        style->setBorderRightStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_BORDER_BOTTOM_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderBottomStyle, BorderBottomStyle, BorderStyle)
        if (!primitiveValue) return;
        style->setBorderBottomStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_BORDER_LEFT_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderLeftStyle, BorderLeftStyle, BorderStyle)
        if (!primitiveValue) return;
        style->setBorderLeftStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_OUTLINE_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(outlineStyle, OutlineStyle, BorderStyle)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_AUTO)
            style->setOutlineStyle(DOTTED, true);
        else
            style->setOutlineStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_CAPTION_SIDE:
    {
        HANDLE_INHERIT_AND_INITIAL(captionSide, CaptionSide)
        if(!primitiveValue) break;
        ECaptionSide c = RenderStyle::initialCaptionSide();
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_LEFT:
            c = CAPLEFT; break;
        case CSS_VAL_RIGHT:
            c = CAPRIGHT; break;
        case CSS_VAL_TOP:
            c = CAPTOP; break;
        case CSS_VAL_BOTTOM:
            c = CAPBOTTOM; break;
        default:
            return;
        }
        style->setCaptionSide(c);
        return;
    }
    case CSS_PROP_CLEAR:
    {
        HANDLE_INHERIT_AND_INITIAL(clear, Clear)
        if(!primitiveValue) break;
        EClear c;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_NONE:
            c = CNONE; break;
        case CSS_VAL_LEFT:
            c = CLEFT; break;
        case CSS_VAL_RIGHT:
            c = CRIGHT; break;
        case CSS_VAL_BOTH:
            c = CBOTH; break;
        default:
            return;
        }
        style->setClear(c);
        return;
    }
    case CSS_PROP_DIRECTION:
    {
        HANDLE_INHERIT_AND_INITIAL(direction, Direction)
        if(!primitiveValue) break;
        style->setDirection(primitiveValue->getIdent() == CSS_VAL_LTR ? LTR : RTL);
        return;
    }
    case CSS_PROP_DISPLAY:
    {
        HANDLE_INHERIT_AND_INITIAL(display, Display)
        if(!primitiveValue) break;
        int id = primitiveValue->getIdent();
        EDisplay d;
        if (id == CSS_VAL_NONE)
            d = NONE;
        else
            d = EDisplay(primitiveValue->getIdent() - CSS_VAL_INLINE);

        style->setDisplay(d);
        break;
    }

    case CSS_PROP_EMPTY_CELLS:
    {
        HANDLE_INHERIT_AND_INITIAL(emptyCells, EmptyCells)
        if (!primitiveValue) break;
        int id = primitiveValue->getIdent();
        if (id == CSS_VAL_SHOW)
            style->setEmptyCells(SHOW);
        else if (id == CSS_VAL_HIDE)
            style->setEmptyCells(HIDE);
        break;
    }
    case CSS_PROP_FLOAT:
    {
        HANDLE_INHERIT_AND_INITIAL(floating, Floating)
        if(!primitiveValue) return;
        EFloat f;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_LEFT:
            f = FLEFT; break;
        case CSS_VAL_RIGHT:
            f = FRIGHT; break;
        case CSS_VAL_NONE:
        case CSS_VAL_CENTER:  //Non standart CSS-Value
            f = FNONE; break;
        default:
            return;
        }
        
        style->setFloating(f);
        break;
    }

        break;
    case CSS_PROP_FONT_STRETCH:
        break; /* Not supported. */

    case CSS_PROP_FONT_STYLE:
    {
        FontDescription fontDescription = style->fontDescription();
        if (isInherit)
            fontDescription.setItalic(parentStyle->fontDescription().italic());
        else if (isInitial)
            fontDescription.setItalic(false);
        else {
            if (!primitiveValue) return;
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
        if (style->setFontDescription(fontDescription))
            fontDirty = true;
        break;
    }

    case CSS_PROP_FONT_VARIANT:
    {
        FontDescription fontDescription = style->fontDescription();
        if (isInherit) 
            fontDescription.setSmallCaps(parentStyle->fontDescription().smallCaps());
        else if (isInitial)
            fontDescription.setSmallCaps(false);
        else {
            if (!primitiveValue) return;
            int id = primitiveValue->getIdent();
            if (id == CSS_VAL_NORMAL)
                fontDescription.setSmallCaps(false);
            else if (id == CSS_VAL_SMALL_CAPS)
                fontDescription.setSmallCaps(true);
            else
                return;
        }
        if (style->setFontDescription(fontDescription))
            fontDirty = true;
        break;        
    }

    case CSS_PROP_FONT_WEIGHT:
    {
        FontDescription fontDescription = style->fontDescription();
        if (isInherit)
            fontDescription.setWeight(parentStyle->fontDescription().weight());
        else if (isInitial)
            fontDescription.setWeight(cNormalWeight);
        else {
            if (!primitiveValue) return;
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
        if (style->setFontDescription(fontDescription))
            fontDirty = true;
        break;
    }
        
    case CSS_PROP_LIST_STYLE_POSITION:
    {
        HANDLE_INHERIT_AND_INITIAL(listStylePosition, ListStylePosition)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent())
            style->setListStylePosition((EListStylePosition) (primitiveValue->getIdent() - CSS_VAL_OUTSIDE));
        return;
    }

    case CSS_PROP_LIST_STYLE_TYPE:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleType, ListStyleType)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent())
        {
            EListStyleType t;
            int id = primitiveValue->getIdent();
            if (id == CSS_VAL_NONE) { // important!!
              t = LNONE;
            } else {
              t = EListStyleType(id - CSS_VAL_DISC);
            }
            style->setListStyleType(t);
        }
        return;
    }

    case CSS_PROP_OVERFLOW:
    {
        HANDLE_INHERIT_AND_INITIAL(overflow, Overflow)
        if (!primitiveValue) return;
        EOverflow o;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_VISIBLE:
            o = OVISIBLE; break;
        case CSS_VAL_HIDDEN:
            o = OHIDDEN; break;
        case CSS_VAL_SCROLL:
            o = OSCROLL; break;
        case CSS_VAL_AUTO:
            o = OAUTO; break;
        case CSS_VAL__WEBKIT_MARQUEE:
            o = OMARQUEE; break;
        case CSS_VAL__WEBKIT_OVERLAY:
            o = OOVERLAY; break;
        default:
            return;
        }
        style->setOverflow(o);
        return;
    }

    case CSS_PROP_PAGE_BREAK_BEFORE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakBefore, PageBreakBefore, PageBreak)
        if (!primitiveValue) return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_AUTO:
                style->setPageBreakBefore(PBAUTO);
                break;
            case CSS_VAL_LEFT:
            case CSS_VAL_RIGHT:
            case CSS_VAL_ALWAYS:
                style->setPageBreakBefore(PBALWAYS); // CSS2.1: "Conforming user agents may map left/right to always."
                break;
            case CSS_VAL_AVOID:
                style->setPageBreakBefore(PBAVOID);
                break;
        }
        break;
    }

    case CSS_PROP_PAGE_BREAK_AFTER:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakAfter, PageBreakAfter, PageBreak)
        if (!primitiveValue) return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_AUTO:
                style->setPageBreakAfter(PBAUTO);
                break;
            case CSS_VAL_LEFT:
            case CSS_VAL_RIGHT:
            case CSS_VAL_ALWAYS:
                style->setPageBreakAfter(PBALWAYS); // CSS2.1: "Conforming user agents may map left/right to always."
                break;
            case CSS_VAL_AVOID:
                style->setPageBreakAfter(PBAVOID);
                break;
        }
        break;
    }

    case CSS_PROP_PAGE_BREAK_INSIDE: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakInside, PageBreakInside, PageBreak)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_AUTO)
            style->setPageBreakInside(PBAUTO);
        else if (primitiveValue->getIdent() == CSS_VAL_AVOID)
            style->setPageBreakInside(PBAVOID);
        return;
    }
        
    case CSS_PROP_PAGE:
        break; /* FIXME: Not even sure what this is...  -dwh */

    case CSS_PROP_POSITION:
    {
        HANDLE_INHERIT_AND_INITIAL(position, Position)
        if(!primitiveValue) return;
        EPosition p;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_STATIC:
            p = StaticPosition; break;
        case CSS_VAL_RELATIVE:
            p = RelativePosition; break;
        case CSS_VAL_ABSOLUTE:
            p = AbsolutePosition; break;
        case CSS_VAL_FIXED:
            {
                if (view)
                    view->useSlowRepaints();
                p = FixedPosition;
                break;
            }
        default:
            return;
        }
        style->setPosition(p);
        return;
    }

    case CSS_PROP_TABLE_LAYOUT: {
        HANDLE_INHERIT_AND_INITIAL(tableLayout, TableLayout)

        if (!primitiveValue->getIdent())
            return;

        ETableLayout l = RenderStyle::initialTableLayout();
        switch(primitiveValue->getIdent()) {
            case CSS_VAL_FIXED:
                l = TFIXED;
                // fall through
            case CSS_VAL_AUTO:
                style->setTableLayout(l);
            default:
                break;
        }
        break;
    }
        
    case CSS_PROP_UNICODE_BIDI: {
        HANDLE_INHERIT_AND_INITIAL(unicodeBidi, UnicodeBidi)
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_NORMAL:
                style->setUnicodeBidi(UBNormal); 
                break;
            case CSS_VAL_EMBED:
                style->setUnicodeBidi(Embed); 
                break;
            case CSS_VAL_BIDI_OVERRIDE:
                style->setUnicodeBidi(Override);
                break;
            default:
                return;
        }
        break;
    }
    case CSS_PROP_TEXT_TRANSFORM: {
        HANDLE_INHERIT_AND_INITIAL(textTransform, TextTransform)
        
        if(!primitiveValue->getIdent()) return;

        ETextTransform tt;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_CAPITALIZE:  tt = CAPITALIZE;  break;
        case CSS_VAL_UPPERCASE:   tt = UPPERCASE;   break;
        case CSS_VAL_LOWERCASE:   tt = LOWERCASE;   break;
        case CSS_VAL_NONE:
        default:                  tt = TTNONE;      break;
        }
        style->setTextTransform(tt);
        break;
        }

    case CSS_PROP_VISIBILITY:
    {
        HANDLE_INHERIT_AND_INITIAL(visibility, Visibility)

        switch(primitiveValue->getIdent()) {
        case CSS_VAL_HIDDEN:
            style->setVisibility(HIDDEN);
            break;
        case CSS_VAL_VISIBLE:
            style->setVisibility(VISIBLE);
            break;
        case CSS_VAL_COLLAPSE:
            style->setVisibility(COLLAPSE);
        default:
            break;
        }
        break;
    }
    case CSS_PROP_WHITE_SPACE:
        HANDLE_INHERIT_AND_INITIAL(whiteSpace, WhiteSpace)

        if(!primitiveValue->getIdent()) return;

        EWhiteSpace s;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL__WEBKIT_NOWRAP:
            s = KHTML_NOWRAP;
            break;
        case CSS_VAL_NOWRAP:
            s = NOWRAP;
            break;
        case CSS_VAL_PRE:
            s = PRE;
            break;
        case CSS_VAL_PRE_WRAP:
            s = PRE_WRAP;
            break;
        case CSS_VAL_PRE_LINE:
            s = PRE_LINE;
            break;
        case CSS_VAL_NORMAL:
        default:
            s = NORMAL;
            break;
        }
        style->setWhiteSpace(s);
        break;

    case CSS_PROP_BACKGROUND_POSITION:
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(backgroundXPosition, BackgroundXPosition);
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(backgroundYPosition, BackgroundYPosition);
        break;
    case CSS_PROP_BACKGROUND_POSITION_X: {
        HANDLE_BACKGROUND_VALUE(backgroundXPosition, BackgroundXPosition, value)
        break;
    }
    case CSS_PROP_BACKGROUND_POSITION_Y: {
        HANDLE_BACKGROUND_VALUE(backgroundYPosition, BackgroundYPosition, value)
        break;
    }
    case CSS_PROP_BORDER_SPACING: {
        if (isInherit) {
            style->setHorizontalBorderSpacing(parentStyle->horizontalBorderSpacing());
            style->setVerticalBorderSpacing(parentStyle->verticalBorderSpacing());
        }
        else if (isInitial) {
            style->setHorizontalBorderSpacing(0);
            style->setVerticalBorderSpacing(0);
        }
        break;
    }
    case CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING: {
        HANDLE_INHERIT_AND_INITIAL(horizontalBorderSpacing, HorizontalBorderSpacing)
        if (!primitiveValue) break;
        short spacing =  primitiveValue->computeLengthShort(style);
        style->setHorizontalBorderSpacing(spacing);
        break;
    }
    case CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING: {
        HANDLE_INHERIT_AND_INITIAL(verticalBorderSpacing, VerticalBorderSpacing)
        if (!primitiveValue) break;
        short spacing =  primitiveValue->computeLengthShort(style);
        style->setVerticalBorderSpacing(spacing);
        break;
    }
    case CSS_PROP_CURSOR:
        if (isInherit) {
            style->setCursor(parentStyle->cursor());
            style->setCursorImage(parentStyle->cursorImage());
            return;
        }
        if (isInitial) {
            style->setCursor(RenderStyle::initialCursor());
            style->setCursorImage(0);
            return;
        }
        if (primitiveValue) {
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_IDENT) {
                style->setCursor((ECursor)(primitiveValue->getIdent() - CSS_VAL_AUTO));
                style->setCursorImage(0);
            } else if (type == CSSPrimitiveValue::CSS_URI) {
                CSSImageValue* image = static_cast<CSSImageValue*>(primitiveValue);
                style->setCursor(CURSOR_AUTO);
                style->setCursorImage(image->image(element->document()->docLoader()));
            }
        }
        break;
// colors || inherit
    case CSS_PROP_BACKGROUND_COLOR:
    case CSS_PROP_BORDER_TOP_COLOR:
    case CSS_PROP_BORDER_RIGHT_COLOR:
    case CSS_PROP_BORDER_BOTTOM_COLOR:
    case CSS_PROP_BORDER_LEFT_COLOR:
    case CSS_PROP_COLOR:
    case CSS_PROP_OUTLINE_COLOR:
        // this property is an extension used to get HTML4 <font> right.
    {
        Color col;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_BACKGROUND_COLOR, backgroundColor, BackgroundColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_TOP_COLOR, borderTopColor, BorderTopColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_BOTTOM_COLOR, borderBottomColor, BorderBottomColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_RIGHT_COLOR, borderRightColor, BorderRightColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_LEFT_COLOR, borderLeftColor, BorderLeftColor)
            HANDLE_INHERIT_COND(CSS_PROP_COLOR, color, Color)
            HANDLE_INHERIT_COND(CSS_PROP_OUTLINE_COLOR, outlineColor, OutlineColor)
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

        switch(id) {
        case CSS_PROP_BACKGROUND_COLOR:
            style->setBackgroundColor(col); break;
        case CSS_PROP_BORDER_TOP_COLOR:
            style->setBorderTopColor(col); break;
        case CSS_PROP_BORDER_RIGHT_COLOR:
            style->setBorderRightColor(col); break;
        case CSS_PROP_BORDER_BOTTOM_COLOR:
            style->setBorderBottomColor(col); break;
        case CSS_PROP_BORDER_LEFT_COLOR:
            style->setBorderLeftColor(col); break;
        case CSS_PROP_COLOR:
            style->setColor(col); break;
        case CSS_PROP_OUTLINE_COLOR:
            style->setOutlineColor(col); break;
        }
        return;
    }
    break;
    
// uri || inherit
    case CSS_PROP_BACKGROUND_IMAGE:
        HANDLE_BACKGROUND_VALUE(backgroundImage, BackgroundImage, value)
        break;
    case CSS_PROP_LIST_STYLE_IMAGE:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleImage, ListStyleImage)
        if (!primitiveValue)
            return;
        style->setListStyleImage(static_cast<CSSImageValue*>(primitiveValue)->image(element->document()->docLoader()));
        break;
    }

// length
    case CSS_PROP_BORDER_TOP_WIDTH:
    case CSS_PROP_BORDER_RIGHT_WIDTH:
    case CSS_PROP_BORDER_BOTTOM_WIDTH:
    case CSS_PROP_BORDER_LEFT_WIDTH:
    case CSS_PROP_OUTLINE_WIDTH:
    {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_TOP_WIDTH, borderTopWidth, BorderTopWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_RIGHT_WIDTH, borderRightWidth, BorderRightWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_BOTTOM_WIDTH, borderBottomWidth, BorderBottomWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_LEFT_WIDTH, borderLeftWidth, BorderLeftWidth)
            HANDLE_INHERIT_COND(CSS_PROP_OUTLINE_WIDTH, outlineWidth, OutlineWidth)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_TOP_WIDTH, BorderTopWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_RIGHT_WIDTH, BorderRightWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_BOTTOM_WIDTH, BorderBottomWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_LEFT_WIDTH, BorderLeftWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_OUTLINE_WIDTH, OutlineWidth, BorderWidth)
            return;
        }

        if(!primitiveValue) break;
        short width = 3;
        switch(primitiveValue->getIdent())
        {
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
            width = primitiveValue->computeLengthShort(style);
            break;
        default:
            return;
        }

        if(width < 0) return;
        switch(id)
        {
        case CSS_PROP_BORDER_TOP_WIDTH:
            style->setBorderTopWidth(width);
            break;
        case CSS_PROP_BORDER_RIGHT_WIDTH:
            style->setBorderRightWidth(width);
            break;
        case CSS_PROP_BORDER_BOTTOM_WIDTH:
            style->setBorderBottomWidth(width);
            break;
        case CSS_PROP_BORDER_LEFT_WIDTH:
            style->setBorderLeftWidth(width);
            break;
        case CSS_PROP_OUTLINE_WIDTH:
            style->setOutlineWidth(width);
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
            if(!primitiveValue) return;
            width = primitiveValue->computeLengthInt(style);
        }
        switch(id)
        {
        case CSS_PROP_LETTER_SPACING:
            style->setLetterSpacing(width);
            break;
        case CSS_PROP_WORD_SPACING:
            style->setWordSpacing(width);
            break;
            // ### needs the definitions in renderstyle
        default: break;
        }
        return;
    }

    case CSS_PROP_WORD_WRAP:
    {
        HANDLE_INHERIT_AND_INITIAL(wordWrap, WordWrap)

        if(!primitiveValue->getIdent()) return;

        EWordWrap s;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_BREAK_WORD:
            s = BREAK_WORD;
            break;
        case CSS_VAL_NORMAL:
        default:
            s = WBNORMAL;
            break;
        }
        style->setWordWrap(s);
        break;
    }

    case CSS_PROP__WEBKIT_NBSP_MODE:
    {
        HANDLE_INHERIT_AND_INITIAL(nbspMode, NBSPMode)

        if (!primitiveValue->getIdent()) return;

        ENBSPMode m;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_SPACE:
            m = SPACE;
            break;
        case CSS_VAL_NORMAL:
        default:
            m = NBNORMAL;
            break;
        }
        style->setNBSPMode(m);
        break;
    }

    case CSS_PROP__WEBKIT_LINE_BREAK:
    {
        HANDLE_INHERIT_AND_INITIAL(khtmlLineBreak, KHTMLLineBreak)

        if (!primitiveValue->getIdent()) return;

        EKHTMLLineBreak b;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_AFTER_WHITE_SPACE:
            b = AFTER_WHITE_SPACE;
            break;
        case CSS_VAL_NORMAL:
        default:
            b = LBNORMAL;
            break;
        }
        style->setKHTMLLineBreak(b);
        break;
    }

    case CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:
    {
        HANDLE_INHERIT_AND_INITIAL(matchNearestMailBlockquoteColor, MatchNearestMailBlockquoteColor)

        if (!primitiveValue->getIdent()) return;

        EMatchNearestMailBlockquoteColor c;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_NORMAL:
            c = BCNORMAL;
            break;
        case CSS_VAL_MATCH:
        default:
            c = MATCH;
            break;
        }
        style->setMatchNearestMailBlockquoteColor(c);
        break;
    }

        // length, percent
    case CSS_PROP_MAX_WIDTH:
        // +none +inherit
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
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
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                // Handle our quirky margin units if we have them.
                l = Length(primitiveValue->computeLengthIntForLength(style), Fixed, 
                           primitiveValue->isQuirkValue());
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length((int)primitiveValue->getFloatValue(), Percent);
            else
                return;
            if (id == CSS_PROP_PADDING_LEFT || id == CSS_PROP_PADDING_RIGHT ||
                id == CSS_PROP_PADDING_TOP || id == CSS_PROP_PADDING_BOTTOM)
                // Padding can't be negative
                apply = !((l.isFixed() || l.isPercent()) && l.calcValue(100) < 0);
            else
                apply = true;
        }
        if(!apply) return;
        switch(id)
            {
            case CSS_PROP_MAX_WIDTH:
                style->setMaxWidth(l); break;
            case CSS_PROP_BOTTOM:
                style->setBottom(l); break;
            case CSS_PROP_TOP:
                style->setTop(l); break;
            case CSS_PROP_LEFT:
                style->setLeft(l); break;
            case CSS_PROP_RIGHT:
                style->setRight(l); break;
            case CSS_PROP_WIDTH:
                style->setWidth(l); break;
            case CSS_PROP_MIN_WIDTH:
                style->setMinWidth(l); break;
            case CSS_PROP_PADDING_TOP:
                style->setPaddingTop(l); break;
            case CSS_PROP_PADDING_RIGHT:
                style->setPaddingRight(l); break;
            case CSS_PROP_PADDING_BOTTOM:
                style->setPaddingBottom(l); break;
            case CSS_PROP_PADDING_LEFT:
                style->setPaddingLeft(l); break;
            case CSS_PROP_MARGIN_TOP:
                style->setMarginTop(l); break;
            case CSS_PROP_MARGIN_RIGHT:
                style->setMarginRight(l); break;
            case CSS_PROP_MARGIN_BOTTOM:
                style->setMarginBottom(l); break;
            case CSS_PROP_MARGIN_LEFT:
                style->setMarginLeft(l); break;
            case CSS_PROP_TEXT_INDENT:
                style->setTextIndent(l); break;
            default: break;
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
                l = Length(primitiveValue->computeLengthIntForLength(style), Fixed);
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length((int)primitiveValue->getFloatValue(), Percent);
            else
                return;
            apply = true;
        }
        if (apply)
            switch (id) {
                case CSS_PROP_MAX_HEIGHT:
                    style->setMaxHeight(l);
                    break;
                case CSS_PROP_HEIGHT:
                    style->setHeight(l);
                    break;
                case CSS_PROP_MIN_HEIGHT:
                    style->setMinHeight(l);
                    break;
            }
        break;

    case CSS_PROP_VERTICAL_ALIGN:
        HANDLE_INHERIT_AND_INITIAL(verticalAlign, VerticalAlign)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent()) {
          EVerticalAlign align;

          switch(primitiveValue->getIdent())
            {
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
          style->setVerticalAlign(align);
          return;
        } else {
          int type = primitiveValue->primitiveType();
          Length l;
          if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(style), Fixed);
          else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(int(primitiveValue->getFloatValue()), Percent);

          style->setVerticalAlign(LENGTH);
          style->setVerticalAlignLength(l);
        }
        break;

    case CSS_PROP_FONT_SIZE:
    {
        FontDescription fontDescription = style->fontDescription();
        float oldSize = 0;
        float size = 0;
        
        bool parentIsAbsoluteSize = false;
        if (parentNode) {
            oldSize = parentStyle->fontDescription().specifiedSize();
            parentIsAbsoluteSize = parentStyle->fontDescription().isAbsoluteSize();
        }

        if (isInherit)
            size = oldSize;
        else if (isInitial)
            size = fontSizeForKeyword(CSS_VAL_MEDIUM, style->htmlHacks());
        else if (primitiveValue->getIdent()) {
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
                size = fontSizeForKeyword(primitiveValue->getIdent(), style->htmlHacks());
                break;
            case CSS_VAL_LARGER:
                size = largerFontSize(oldSize, style->htmlHacks());
                break;
            case CSS_VAL_SMALLER:
                size = smallerFontSize(oldSize, style->htmlHacks());
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
                size = primitiveValue->computeLengthFloat(parentStyle, false);
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                size = (primitiveValue->getFloatValue() * oldSize) / 100;
            else
                return;
        }

        if (size <= 0)
            return;

        setFontSize(fontDescription, size);
        if (style->setFontDescription(fontDescription))
            fontDirty = true;
        return;
    }
        break;
    case CSS_PROP_Z_INDEX:
    {
        HANDLE_INHERIT(zIndex, ZIndex)
        else if (isInitial) {
            style->setHasAutoZIndex();
            return;
        }
        
        if (!primitiveValue)
            return;

        if (primitiveValue->getIdent() == CSS_VAL_AUTO) {
            style->setHasAutoZIndex();
            return;
        }
        
        if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        
        style->setZIndex((int)primitiveValue->getFloatValue());
        
        return;
    }
        
    case CSS_PROP_WIDOWS:
    {
        HANDLE_INHERIT_AND_INITIAL(widows, Widows)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        style->setWidows((int)primitiveValue->getFloatValue());
        break;
    }
        
    case CSS_PROP_ORPHANS:
    {
        HANDLE_INHERIT_AND_INITIAL(orphans, Orphans)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        style->setOrphans((int)primitiveValue->getFloatValue());
        break;
    }        

// length, percent, number
    case CSS_PROP_LINE_HEIGHT:
    {
        HANDLE_INHERIT_AND_INITIAL(lineHeight, LineHeight)
        if(!primitiveValue) return;
        Length lineHeight;
        int type = primitiveValue->primitiveType();
        if (primitiveValue->getIdent() == CSS_VAL_NORMAL)
            lineHeight = Length(-100, Percent);
        else if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG) {
            double multiplier = 1.0;
            // Scale for the font zoom factor only for types other than "em" and "ex", since those are
            // already based on the font size.
            if (type != CSSPrimitiveValue::CSS_EMS && type != CSSPrimitiveValue::CSS_EXS && view && view->frame()) {
                multiplier = view->frame()->zoomFactor() / 100.0;
            }
            lineHeight = Length(primitiveValue->computeLengthIntForLength(style, multiplier), Fixed);
        } else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            lineHeight = Length((style->fontSize() * int(primitiveValue->getFloatValue())) / 100, Fixed);
        else if (type == CSSPrimitiveValue::CSS_NUMBER)
            lineHeight = Length(int(primitiveValue->getFloatValue() * 100), Percent);
        else
            return;
        style->setLineHeight(lineHeight);
        return;
    }

// string
    case CSS_PROP_TEXT_ALIGN:
    {
        HANDLE_INHERIT_AND_INITIAL(textAlign, TextAlign)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent())
            style->setTextAlign((ETextAlign) (primitiveValue->getIdent() - CSS_VAL__WEBKIT_AUTO));
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
            if (parentStyle->hasClip()) {
                top = parentStyle->clipTop();
                right = parentStyle->clipRight();
                bottom = parentStyle->clipBottom();
                left = parentStyle->clipLeft();
            }
            else {
                hasClip = false;
                top = right = bottom = left = Length();
            }
        } else if (isInitial) {
            hasClip = false;
            top = right = bottom = left = Length();
        } else if (!primitiveValue) {
            break;
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RECT) {
            RectImpl* rect = primitiveValue->getRectValue();
            if (!rect)
                break;
            top = convertToLength(rect->top(), style);
            right = convertToLength(rect->right(), style);
            bottom = convertToLength(rect->bottom(), style);
            left = convertToLength(rect->left(), style);

        } else if (primitiveValue->getIdent() != CSS_VAL_AUTO) {
            break;
        }
        style->setClip(top, right, bottom, left);
        style->setHasClip(hasClip);
    
        // rect, ident
        break;
    }

// lists
    case CSS_PROP_CONTENT:
        // list of string, uri, counter, attr, i
    {
        // FIXME: In CSS3, it will be possible to inherit content.  In CSS2 it is not.  This
        // note is a reminder that eventually "inherit" needs to be supported.
        if (!(style->styleType()==RenderStyle::BEFORE ||
                style->styleType()==RenderStyle::AFTER))
            break;

        if (isInitial) {
            if (style->contentData())
                style->contentData()->clearContent();
            return;
        }
        
        if (!value->isValueList()) return;
        CSSValueList *list = static_cast<CSSValueList*>(value);
        int len = list->length();

        for (int i = 0; i < len; i++) {
            CSSValue *item = list->item(i);
            if (!item->isPrimitiveValue()) continue;
            CSSPrimitiveValue *val = static_cast<CSSPrimitiveValue*>(item);
            if (val->primitiveType()==CSSPrimitiveValue::CSS_STRING)
                style->setContent(val->getStringValue().impl(), i != 0);
            else if (val->primitiveType()==CSSPrimitiveValue::CSS_ATTR) {
                // FIXME: Can a namespace be specified for an attr(foo)?
                QualifiedName attr(nullAtom, val->getStringValue().impl(), nullAtom);
                style->setContent(element->getAttribute(attr).impl(), i != 0);
            }
            else if (val->primitiveType()==CSSPrimitiveValue::CSS_URI) {
                CSSImageValue *image = static_cast<CSSImageValue*>(val);
                style->setContent(image->image(element->document()->docLoader()), i != 0);
            }
        }
        break;
    }

    case CSS_PROP_COUNTER_INCREMENT:
        // list of CSS2CounterIncrement
    case CSS_PROP_COUNTER_RESET:
        // list of CSS2CounterReset
        break;
    case CSS_PROP_FONT_FAMILY: {
        // list of strings and ids
        if (isInherit) {
            FontDescription parentFontDescription = parentStyle->fontDescription();
            FontDescription fontDescription = style->fontDescription();
            fontDescription.setGenericFamily(parentFontDescription.genericFamily());
            fontDescription.setFamily(parentFontDescription.firstFamily());
            if (style->setFontDescription(fontDescription))
                fontDirty = true;
            return;
        }
        else if (isInitial) {
            FontDescription initialDesc = FontDescription();
            FontDescription fontDescription = style->fontDescription();
            fontDescription.setGenericFamily(initialDesc.genericFamily());
            fontDescription.setFamily(initialDesc.firstFamily());
            if (style->setFontDescription(fontDescription))
                fontDirty = true;
            return;
        }
        
        if (!value->isValueList()) return;
        FontDescription fontDescription = style->fontDescription();
        CSSValueList *list = static_cast<CSSValueList*>(value);
        int len = list->length();
        FontFamily& firstFamily = fontDescription.firstFamily();
        FontFamily *currFamily = 0;
        
        for(int i = 0; i < len; i++) {
            CSSValue *item = list->item(i);
            if(!item->isPrimitiveValue()) continue;
            CSSPrimitiveValue *val = static_cast<CSSPrimitiveValue*>(item);
            AtomicString face;
            if(val->primitiveType() == CSSPrimitiveValue::CSS_STRING)
                face = static_cast<FontFamilyValue*>(val)->fontName();
            else if (val->primitiveType() == CSSPrimitiveValue::CSS_IDENT) {
                switch (val->getIdent()) {
                    case CSS_VAL__WEBKIT_BODY:
                        face = settings->stdFontName();
                        break;
                    case CSS_VAL_SERIF:
                        face = settings->serifFontName();
                        fontDescription.setGenericFamily(FontDescription::SerifFamily);
                        break;
                    case CSS_VAL_SANS_SERIF:
                        face = settings->sansSerifFontName();
                        fontDescription.setGenericFamily(FontDescription::SansSerifFamily);
                        break;
                    case CSS_VAL_CURSIVE:
                        face = settings->cursiveFontName();
                        fontDescription.setGenericFamily(FontDescription::CursiveFamily);
                        break;
                    case CSS_VAL_FANTASY:
                        face = settings->fantasyFontName();
                        fontDescription.setGenericFamily(FontDescription::FantasyFamily);
                        break;
                    case CSS_VAL_MONOSPACE:
                        face = settings->fixedFontName();
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
                
                if (style->setFontDescription(fontDescription))
                    fontDirty = true;
            }
        }
      break;
    }
    case CSS_PROP_QUOTES:
        // list of strings or i
    case CSS_PROP_SIZE:
        // ### look up
      break;
    case CSS_PROP_TEXT_DECORATION: {
        // list of ident
        HANDLE_INHERIT_AND_INITIAL(textDecoration, TextDecoration)
        int t = RenderStyle::initialTextDecoration();
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE) {
            // do nothing
        } else {
            if(!value->isValueList()) return;
            CSSValueList *list = static_cast<CSSValueList*>(value);
            int len = list->length();
            for(int i = 0; i < len; i++)
            {
                CSSValue *item = list->item(i);
                if(!item->isPrimitiveValue()) continue;
                primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                switch(primitiveValue->getIdent())
                {
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

        style->setTextDecoration(t);
        break;
    }

// shorthand properties
    case CSS_PROP_BACKGROUND:
        if (isInitial) {
            style->clearBackgroundLayers();
            return;
        }
        else if (isInherit) {
            if (parentStyle)
                style->inheritBackgroundLayers(*parentStyle->backgroundLayers());
            else
                style->clearBackgroundLayers();
            return;
        }
        break;
    case CSS_PROP_BORDER:
    case CSS_PROP_BORDER_STYLE:
    case CSS_PROP_BORDER_WIDTH:
    case CSS_PROP_BORDER_COLOR:
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_COLOR)
        {
            if (isInherit) {
                style->setBorderTopColor(parentStyle->borderTopColor());
                style->setBorderBottomColor(parentStyle->borderBottomColor());
                style->setBorderLeftColor(parentStyle->borderLeftColor());
                style->setBorderRightColor(parentStyle->borderRightColor());
            }
            else if (isInitial) {
                style->setBorderTopColor(Color()); // Reset to invalid color so currentColor is used instead.
                style->setBorderBottomColor(Color());
                style->setBorderLeftColor(Color());
                style->setBorderRightColor(Color());
            }
        }
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_STYLE)
        {
            if (isInherit) {
                style->setBorderTopStyle(parentStyle->borderTopStyle());
                style->setBorderBottomStyle(parentStyle->borderBottomStyle());
                style->setBorderLeftStyle(parentStyle->borderLeftStyle());
                style->setBorderRightStyle(parentStyle->borderRightStyle());
            }
            else if (isInitial) {
                style->setBorderTopStyle(RenderStyle::initialBorderStyle());
                style->setBorderBottomStyle(RenderStyle::initialBorderStyle());
                style->setBorderLeftStyle(RenderStyle::initialBorderStyle());
                style->setBorderRightStyle(RenderStyle::initialBorderStyle());
            }
        }
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_WIDTH)
        {
            if (isInherit) {
                style->setBorderTopWidth(parentStyle->borderTopWidth());
                style->setBorderBottomWidth(parentStyle->borderBottomWidth());
                style->setBorderLeftWidth(parentStyle->borderLeftWidth());
                style->setBorderRightWidth(parentStyle->borderRightWidth());
            }
            else if (isInitial) {
                style->setBorderTopWidth(RenderStyle::initialBorderWidth());
                style->setBorderBottomWidth(RenderStyle::initialBorderWidth());
                style->setBorderLeftWidth(RenderStyle::initialBorderWidth());
                style->setBorderRightWidth(RenderStyle::initialBorderWidth());
            }
        }
        return;
    case CSS_PROP_BORDER_TOP:
        if (isInherit) {
            style->setBorderTopColor(parentStyle->borderTopColor());
            style->setBorderTopStyle(parentStyle->borderTopStyle());
            style->setBorderTopWidth(parentStyle->borderTopWidth());
        }
        else if (isInitial)
            style->resetBorderTop();
        return;
    case CSS_PROP_BORDER_RIGHT:
        if (isInherit) {
            style->setBorderRightColor(parentStyle->borderRightColor());
            style->setBorderRightStyle(parentStyle->borderRightStyle());
            style->setBorderRightWidth(parentStyle->borderRightWidth());
        }
        else if (isInitial)
            style->resetBorderRight();
        return;
    case CSS_PROP_BORDER_BOTTOM:
        if (isInherit) {
            style->setBorderBottomColor(parentStyle->borderBottomColor());
            style->setBorderBottomStyle(parentStyle->borderBottomStyle());
            style->setBorderBottomWidth(parentStyle->borderBottomWidth());
        }
        else if (isInitial)
            style->resetBorderBottom();
        return;
    case CSS_PROP_BORDER_LEFT:
        if (isInherit) {
            style->setBorderLeftColor(parentStyle->borderLeftColor());
            style->setBorderLeftStyle(parentStyle->borderLeftStyle());
            style->setBorderLeftWidth(parentStyle->borderLeftWidth());
        }
        else if (isInitial)
            style->resetBorderLeft();
        return;
    case CSS_PROP_MARGIN:
        if (isInherit) {
            style->setMarginTop(parentStyle->marginTop());
            style->setMarginBottom(parentStyle->marginBottom());
            style->setMarginLeft(parentStyle->marginLeft());
            style->setMarginRight(parentStyle->marginRight());
        }
        else if (isInitial)
            style->resetMargin();
        return;
    case CSS_PROP_PADDING:
        if (isInherit) {
            style->setPaddingTop(parentStyle->paddingTop());
            style->setPaddingBottom(parentStyle->paddingBottom());
            style->setPaddingLeft(parentStyle->paddingLeft());
            style->setPaddingRight(parentStyle->paddingRight());
        }
        else if (isInitial)
            style->resetPadding();
        return;
    case CSS_PROP_FONT:
        if (isInherit) {
            FontDescription fontDescription = parentStyle->fontDescription();
            style->setLineHeight(parentStyle->lineHeight());
            if (style->setFontDescription(fontDescription))
                fontDirty = true;
        } else if (isInitial) {
            FontDescription fontDescription;
            style->setLineHeight(RenderStyle::initialLineHeight());
            if (style->setFontDescription(fontDescription))
                fontDirty = true;
        } else if (value->isFontValue()) {
            FontValue *font = static_cast<FontValue*>(value);
            if (!font->style || !font->variant || !font->weight ||
                 !font->size || !font->lineHeight || !font->family)
                return;
            applyProperty(CSS_PROP_FONT_STYLE, font->style.get());
            applyProperty(CSS_PROP_FONT_VARIANT, font->variant.get());
            applyProperty(CSS_PROP_FONT_WEIGHT, font->weight.get());
            applyProperty(CSS_PROP_FONT_SIZE, font->size.get());

            // Line-height can depend on font().pixelSize(), so we have to update the font
            // before we evaluate line-height, e.g., font: 1em/1em.  FIXME: Still not
            // good enough: style="font:1em/1em; font-size:36px" should have a line-height of 36px.
            if (fontDirty)
                CSSStyleSelector::style->font().update();
            
            applyProperty(CSS_PROP_LINE_HEIGHT, font->lineHeight.get());
            applyProperty(CSS_PROP_FONT_FAMILY, font->family.get());
        }
        return;
        
    case CSS_PROP_LIST_STYLE:
        if (isInherit) {
            style->setListStyleType(parentStyle->listStyleType());
            style->setListStyleImage(parentStyle->listStyleImage());
            style->setListStylePosition(parentStyle->listStylePosition());
        }
        else if (isInitial) {
            style->setListStyleType(RenderStyle::initialListStyleType());
            style->setListStyleImage(RenderStyle::initialListStyleImage());
            style->setListStylePosition(RenderStyle::initialListStylePosition());
        }
        break;
    case CSS_PROP_OUTLINE:
        if (isInherit) {
            style->setOutlineWidth(parentStyle->outlineWidth());
            style->setOutlineColor(parentStyle->outlineColor());
            style->setOutlineStyle(parentStyle->outlineStyle());
        }
        else if (isInitial)
            style->resetOutline();
        break;

    // CSS3 Properties
    case CSS_PROP__WEBKIT_APPEARANCE: {
        HANDLE_INHERIT_AND_INITIAL(appearance, Appearance)
        if (!primitiveValue) break;
        int id = primitiveValue->getIdent();
        EAppearance appearance;
        if (id == CSS_VAL_NONE)
            appearance = NoAppearance;
        else
            appearance = EAppearance(id - CSS_VAL_CHECKBOX + 1);
        style->setAppearance(appearance);
    }
    case CSS_PROP__WEBKIT_BINDING: {
#ifndef KHTML_NO_XBL
        if (isInitial || (primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)) {
            style->deleteBindingURIs();
            return;
        }
        else if (isInherit) {
            if (parentStyle->bindingURIs())
                style->inheritBindingURIs(parentStyle->bindingURIs());
            else
                style->deleteBindingURIs();
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
                    style->deleteBindingURIs();
                }
                style->addBindingURI(val->getStringValue());
            }
        }
#endif
        break;
    }

    case CSS_PROP__WEBKIT_BORDER_IMAGE: {
        HANDLE_INHERIT_AND_INITIAL(borderImage, BorderImage)
        BorderImage image;
        if (primitiveValue) {
            if (primitiveValue->getIdent() == CSS_VAL_NONE)
                style->setBorderImage(image);
        } else {
            // Retrieve the border image value.
            CSSBorderImageValue* borderImage = static_cast<CSSBorderImageValue*>(value);
            
            // Set the image (this kicks off the load).
            image.m_image = borderImage->m_image->image(element->document()->docLoader());
            
            // Set up a length box to represent our image slices.
            LengthBox& l = image.m_slices;
            RectImpl* r = borderImage->m_imageSliceRect.get();
            if (r->top()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                l.top = Length((int)r->top()->getFloatValue(), Percent);
            else
                l.top = Length((int)r->top()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
            if (r->bottom()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                l.bottom = Length((int)r->bottom()->getFloatValue(), Percent);
            else
                l.bottom = Length((int)r->bottom()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
            if (r->left()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                l.left = Length((int)r->left()->getFloatValue(), Percent);
            else
                l.left = Length((int)r->left()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
            if (r->right()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                l.right = Length((int)r->right()->getFloatValue(), Percent);
            else
                l.right = Length((int)r->right()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
            
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

            style->setBorderImage(image);
        }
        break;
    }

    case CSS_PROP__WEBKIT_BORDER_RADIUS:
        if (isInherit) {
            style->setBorderTopLeftRadius(parentStyle->borderTopLeftRadius());
            style->setBorderTopRightRadius(parentStyle->borderTopRightRadius());
            style->setBorderBottomLeftRadius(parentStyle->borderBottomLeftRadius());
            style->setBorderBottomRightRadius(parentStyle->borderBottomRightRadius());
            return;
        }
        if (isInitial) {
            style->resetBorderRadius();
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

        int width = pair->first()->computeLengthInt(style);
        int height = pair->second()->computeLengthInt(style);
        if (width < 0 || height < 0)
            return;

        if (width == 0)
            height = 0; // Null out the other value.
        else if (height == 0)
            width = 0; // Null out the other value.

        IntSize size(width, height);
        switch (id) {
            case CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS:
                style->setBorderTopLeftRadius(size);
                break;
            case CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS:
                style->setBorderTopRightRadius(size);
                break;
            case CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS:
                style->setBorderBottomLeftRadius(size);
                break;
            case CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS:
                style->setBorderBottomRightRadius(size);
                break;
            default:
                style->setBorderRadius(size);
                break;
        }
    }

    case CSS_PROP_OUTLINE_OFFSET:
        HANDLE_INHERIT_AND_INITIAL(outlineOffset, OutlineOffset)
        style->setOutlineOffset(primitiveValue->computeLengthInt(style));
        break;

    case CSS_PROP_TEXT_SHADOW: {
        if (isInherit) {
            style->setTextShadow(parentStyle->textShadow() ? new ShadowData(*parentStyle->textShadow()) : 0);
            return;
        }
        if (isInitial) {
            style->setTextShadow(0);
            return;
        }

        if (primitiveValue) { // none
            style->setTextShadow(0);
            return;
        }

        if (!value->isValueList())
            return;
        CSSValueList *list = static_cast<CSSValueList*>(value);
        int len = list->length();
        for (int i = 0; i < len; i++) {
            ShadowValue* item = static_cast<ShadowValue*>(list->item(i));
            int x = item->x->computeLengthInt(style);
            int y = item->y->computeLengthInt(style);
            int blur = item->blur ? item->blur->computeLengthInt(style) : 0;
            Color color;
            if (item->color)
                color = getColorFromPrimitiveValue(item->color.get());
            ShadowData* shadowData = new ShadowData(x, y, blur, color.isValid() ? color : Color::transparent);
            style->setTextShadow(shadowData, i != 0);
        }
        return;
    }
    case CSS_PROP_OPACITY:
        HANDLE_INHERIT_AND_INITIAL(opacity, Opacity)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        // Clamp opacity to the range 0-1
        style->setOpacity(min(1.0, max(0.0, primitiveValue->getFloatValue())));
        return;
    case CSS_PROP__WEBKIT_BOX_ALIGN:
        HANDLE_INHERIT_AND_INITIAL(boxAlign, BoxAlign)
        if (!primitiveValue) return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_STRETCH:
                style->setBoxAlign(BSTRETCH);
                break;
            case CSS_VAL_START:
                style->setBoxAlign(BSTART);
                break;
            case CSS_VAL_END:
                style->setBoxAlign(BEND);
                break;
            case CSS_VAL_CENTER:
                style->setBoxAlign(BCENTER);
                break;
            case CSS_VAL_BASELINE:
                style->setBoxAlign(BBASELINE);
                break;
            default:
                return;
        }
        return;        
    case CSS_PROP__WEBKIT_BOX_DIRECTION:
        HANDLE_INHERIT_AND_INITIAL(boxDirection, BoxDirection)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_NORMAL)
            style->setBoxDirection(BNORMAL);
        else
            style->setBoxDirection(BREVERSE);
        return;        
    case CSS_PROP__WEBKIT_BOX_LINES:
        HANDLE_INHERIT_AND_INITIAL(boxLines, BoxLines)
        if(!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_SINGLE)
            style->setBoxLines(SINGLE);
        else
            style->setBoxLines(MULTIPLE);
        return;     
    case CSS_PROP__WEBKIT_BOX_ORIENT:
        HANDLE_INHERIT_AND_INITIAL(boxOrient, BoxOrient)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_HORIZONTAL ||
            primitiveValue->getIdent() == CSS_VAL_INLINE_AXIS)
            style->setBoxOrient(HORIZONTAL);
        else
            style->setBoxOrient(VERTICAL);
        return;     
    case CSS_PROP__WEBKIT_BOX_PACK:
        HANDLE_INHERIT_AND_INITIAL(boxPack, BoxPack)
        if (!primitiveValue) return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_START:
                style->setBoxPack(BSTART);
                break;
            case CSS_VAL_END:
                style->setBoxPack(BEND);
                break;
            case CSS_VAL_CENTER:
                style->setBoxPack(BCENTER);
                break;
            case CSS_VAL_JUSTIFY:
                style->setBoxPack(BJUSTIFY);
                break;
            default:
                return;
        }
        return;        
    case CSS_PROP__WEBKIT_BOX_FLEX:
        HANDLE_INHERIT_AND_INITIAL(boxFlex, BoxFlex)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        style->setBoxFlex(primitiveValue->getFloatValue());
        return;
    case CSS_PROP__WEBKIT_BOX_FLEX_GROUP:
        HANDLE_INHERIT_AND_INITIAL(boxFlexGroup, BoxFlexGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        style->setBoxFlexGroup((unsigned int)(primitiveValue->getFloatValue()));
        return;        
    case CSS_PROP__WEBKIT_BOX_ORDINAL_GROUP:
        HANDLE_INHERIT_AND_INITIAL(boxOrdinalGroup, BoxOrdinalGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        style->setBoxOrdinalGroup((unsigned int)(primitiveValue->getFloatValue()));
        return;
    case CSS_PROP_BOX_SIZING:
        HANDLE_INHERIT_AND_INITIAL(boxSizing, BoxSizing)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_CONTENT_BOX)
            style->setBoxSizing(CONTENT_BOX);
        else
            style->setBoxSizing(BORDER_BOX);
        break;
    case CSS_PROP__WEBKIT_MARQUEE:
        if (valueType != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setMarqueeDirection(parentStyle->marqueeDirection());
        style->setMarqueeIncrement(parentStyle->marqueeIncrement());
        style->setMarqueeSpeed(parentStyle->marqueeSpeed());
        style->setMarqueeLoopCount(parentStyle->marqueeLoopCount());
        style->setMarqueeBehavior(parentStyle->marqueeBehavior());
        break;
    case CSS_PROP__WEBKIT_MARQUEE_REPETITION: {
        HANDLE_INHERIT_AND_INITIAL(marqueeLoopCount, MarqueeLoopCount)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_INFINITE)
            style->setMarqueeLoopCount(-1); // -1 means repeat forever.
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
            style->setMarqueeLoopCount((int)(primitiveValue->getFloatValue()));
        break;
    }
    case CSS_PROP__WEBKIT_MARQUEE_SPEED: {
        HANDLE_INHERIT_AND_INITIAL(marqueeSpeed, MarqueeSpeed)      
        if (!primitiveValue) return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent())
            {
                case CSS_VAL_SLOW:
                    style->setMarqueeSpeed(500); // 500 msec.
                    break;
                case CSS_VAL_NORMAL:
                    style->setMarqueeSpeed(85); // 85msec. The WinIE default.
                    break;
                case CSS_VAL_FAST:
                    style->setMarqueeSpeed(10); // 10msec. Super fast.
                    break;
            }
        }
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_S)
            style->setMarqueeSpeed(int(1000*primitiveValue->getFloatValue()));
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_MS)
            style->setMarqueeSpeed(int(primitiveValue->getFloatValue()));
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) // For scrollamount support.
            style->setMarqueeSpeed(int(primitiveValue->getFloatValue()));
        break;
    }
    case CSS_PROP__WEBKIT_MARQUEE_INCREMENT: {
        HANDLE_INHERIT_AND_INITIAL(marqueeIncrement, MarqueeIncrement)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent())
            {
                case CSS_VAL_SMALL:
                    style->setMarqueeIncrement(Length(1, Fixed)); // 1px.
                    break;
                case CSS_VAL_NORMAL:
                    style->setMarqueeIncrement(Length(6, Fixed)); // 6px. The WinIE default.
                    break;
                case CSS_VAL_LARGE:
                    style->setMarqueeIncrement(Length(36, Fixed)); // 36px.
                    break;
            }
        }
        else {
            bool ok = true;
            Length l = convertToLength(primitiveValue, style, &ok);
            if (ok)
                style->setMarqueeIncrement(l);
        }
        break;
    }
    case CSS_PROP__WEBKIT_MARQUEE_STYLE: {
        HANDLE_INHERIT_AND_INITIAL(marqueeBehavior, MarqueeBehavior)      
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        switch (primitiveValue->getIdent())
        {
            case CSS_VAL_NONE:
                style->setMarqueeBehavior(MNONE);
                break;
            case CSS_VAL_SCROLL:
                style->setMarqueeBehavior(MSCROLL);
                break;
            case CSS_VAL_SLIDE:
                style->setMarqueeBehavior(MSLIDE);
                break;
            case CSS_VAL_ALTERNATE:
                style->setMarqueeBehavior(MALTERNATE);
                break;
            case CSS_VAL_UNFURL:
                style->setMarqueeBehavior(MUNFURL);
                break;
        }
        break;
    }
    case CSS_PROP__WEBKIT_MARQUEE_DIRECTION: {
        HANDLE_INHERIT_AND_INITIAL(marqueeDirection, MarqueeDirection)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        switch (primitiveValue->getIdent())
        {
            case CSS_VAL_FORWARDS:
                style->setMarqueeDirection(MFORWARD);
                break;
            case CSS_VAL_BACKWARDS:
                style->setMarqueeDirection(MBACKWARD);
                break;
            case CSS_VAL_AUTO:
                style->setMarqueeDirection(MAUTO);
                break;
            case CSS_VAL_AHEAD:
            case CSS_VAL_UP: // We don't support vertical languages, so AHEAD just maps to UP.
                style->setMarqueeDirection(MUP);
                break;
            case CSS_VAL_REVERSE:
            case CSS_VAL_DOWN: // REVERSE just maps to DOWN, since we don't do vertical text.
                style->setMarqueeDirection(MDOWN);
                break;
            case CSS_VAL_LEFT:
                style->setMarqueeDirection(MLEFT);
                break;
            case CSS_VAL_RIGHT:
                style->setMarqueeDirection(MRIGHT);
                break;
        }
        break;
    }
    case CSS_PROP__WEBKIT_USER_DRAG: {
        HANDLE_INHERIT_AND_INITIAL(userDrag, UserDrag)      
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_AUTO:
                style->setUserDrag(DRAG_AUTO);
                break;
            case CSS_VAL_NONE:
                style->setUserDrag(DRAG_NONE);
                break;
            case CSS_VAL_ELEMENT:
                style->setUserDrag(DRAG_ELEMENT);
            default:
                return;
        }
        break;
    }
    case CSS_PROP__WEBKIT_USER_MODIFY: {
        HANDLE_INHERIT_AND_INITIAL(userModify, UserModify)      
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        EUserModify userModify = EUserModify(primitiveValue->getIdent() - CSS_VAL_READ_ONLY);
        style->setUserModify(userModify);
        break;
    }
    case CSS_PROP__WEBKIT_USER_SELECT: {
        HANDLE_INHERIT_AND_INITIAL(userSelect, UserSelect)      
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_AUTO:
                style->setUserSelect(SELECT_AUTO);
                break;
            case CSS_VAL_NONE:
                style->setUserSelect(SELECT_NONE);
                break;
            case CSS_VAL_TEXT:
                style->setUserSelect(SELECT_TEXT);
                break;
            case CSS_VAL_IGNORE:
                style->setUserSelect(SELECT_IGNORE);
            default:
                return;
        }
        break;
    }
    case CSS_PROP_TEXT_OVERFLOW: {
        // This property is supported by WinIE, and so we leave off the "-webkit-" in order to
        // work with WinIE-specific pages that use the property.
        HANDLE_INHERIT_AND_INITIAL(textOverflow, TextOverflow)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        style->setTextOverflow(primitiveValue->getIdent() == CSS_VAL_ELLIPSIS);
        break;
    }
    case CSS_PROP__WEBKIT_MARGIN_COLLAPSE: {
        if (isInherit) {
            style->setMarginTopCollapse(parentStyle->marginTopCollapse());
            style->setMarginBottomCollapse(parentStyle->marginBottomCollapse());
        }
        else if (isInitial) {
            style->setMarginTopCollapse(MCOLLAPSE);
            style->setMarginBottomCollapse(MCOLLAPSE);
        }
        break;
    }
    case CSS_PROP__WEBKIT_MARGIN_TOP_COLLAPSE: {
        HANDLE_INHERIT_AND_INITIAL(marginTopCollapse, MarginTopCollapse)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        EMarginCollapse val;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_SEPARATE:
                val = MSEPARATE;
                break;
            case CSS_VAL_DISCARD:
                val = MDISCARD;
                break;
            default:
                val = MCOLLAPSE;
        }
        style->setMarginTopCollapse(val);
        break;
    }
    case CSS_PROP__WEBKIT_MARGIN_BOTTOM_COLLAPSE: {
        HANDLE_INHERIT_AND_INITIAL(marginBottomCollapse, MarginBottomCollapse)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        EMarginCollapse val;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_SEPARATE:
                val = MSEPARATE;
                break;
            case CSS_VAL_DISCARD:
                val = MDISCARD;
                break;
            default:
                val = MCOLLAPSE;
        }
        style->setMarginBottomCollapse(val);
        break;
    }

    // Apple-specific changes.  Do not merge these properties into KHTML.
    case CSS_PROP__WEBKIT_LINE_CLAMP: {
        HANDLE_INHERIT_AND_INITIAL(lineClamp, LineClamp)
        if (!primitiveValue) return;
        style->setLineClamp((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE));
        break;
    }
    case CSS_PROP__WEBKIT_TEXT_SIZE_ADJUST: {
        HANDLE_INHERIT_AND_INITIAL(textSizeAdjust, TextSizeAdjust)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        style->setTextSizeAdjust(primitiveValue->getIdent() == CSS_VAL_AUTO);
        fontDirty = true;
        break;
    }
#if __APPLE__
    case CSS_PROP__WEBKIT_DASHBOARD_REGION: {
        HANDLE_INHERIT_AND_INITIAL(dashboardRegions, DashboardRegions)
        if (!primitiveValue)
            return;

        if(primitiveValue->getIdent() == CSS_VAL_NONE) {
            style->setDashboardRegions(RenderStyle::noneDashboardRegions());
            return;
        }

        DashboardRegion *region = primitiveValue->getDashboardRegionValue();
        if (!region)
            return;
            
        DashboardRegion *first = region;
        while (region) {
            Length top = convertToLength (region->top(), style);
            Length right = convertToLength (region->right(), style);
            Length bottom = convertToLength (region->bottom(), style);
            Length left = convertToLength (region->left(), style);
            if (region->m_isCircle)
                style->setDashboardRegion(StyleDashboardRegion::Circle, region->m_label, top, right, bottom, left, region == first ? false : true);
            else if (region->m_isRectangle)
                style->setDashboardRegion(StyleDashboardRegion::Rectangle, region->m_label, top, right, bottom, left, region == first ? false : true);
            region = region->m_next.get();
        }
        
        element->document()->setHasDashboardRegions(true);
        
        break;
    }
#endif
    case CSS_PROP__WEBKIT_RTL_ORDERING:
        HANDLE_INHERIT_AND_INITIAL(visuallyOrdered, VisuallyOrdered)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        style->setVisuallyOrdered(primitiveValue->getIdent() == CSS_VAL_VISUAL);
        break;

    default:
#if SVG_SUPPORT
        // Try the SVG properties
        applySVGProperty(id, value);
#endif
        break;
    }
}

void CSSStyleSelector::mapBackgroundAttachment(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundAttachment(RenderStyle::initialBackgroundAttachment());
        return;
    }

    if (!value->isPrimitiveValue()) return;
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

    if (!value->isPrimitiveValue()) return;
    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    switch (primitiveValue->getIdent()) {
        case CSS_VAL_BORDER:
            layer->setBackgroundClip(BGBORDER);
            break;
        case CSS_VAL_PADDING:
            layer->setBackgroundClip(BGPADDING);
            break;
        default: // CSS_VAL_CONTENT
            layer->setBackgroundClip(BGCONTENT);
            break;
    }
}

void CSSStyleSelector::mapBackgroundOrigin(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundOrigin(RenderStyle::initialBackgroundOrigin());
        return;
    }

    if (!value->isPrimitiveValue()) return;
    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    switch (primitiveValue->getIdent()) {
        case CSS_VAL_BORDER:
            layer->setBackgroundOrigin(BGBORDER);
            break;
        case CSS_VAL_PADDING:
            layer->setBackgroundOrigin(BGPADDING);
            break;
        default: // CSS_VAL_CONTENT
            layer->setBackgroundOrigin(BGCONTENT);
            break;
    }
}

void CSSStyleSelector::mapBackgroundImage(BackgroundLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundImage(RenderStyle::initialBackgroundImage());
        return;
    }
    
    if (!value->isPrimitiveValue()) return;
    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setBackgroundImage(static_cast<CSSImageValue*>(primitiveValue)->image(element->document()->docLoader()));
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
    switch(primitiveValue->getIdent()) {
        case CSS_VAL_REPEAT:
            layer->setBackgroundRepeat(REPEAT);
            break;
        case CSS_VAL_REPEAT_X:
            layer->setBackgroundRepeat(REPEAT_X);
            break;
        case CSS_VAL_REPEAT_Y:
            layer->setBackgroundRepeat(REPEAT_Y);
            break;
        case CSS_VAL_NO_REPEAT:
            layer->setBackgroundRepeat(NO_REPEAT);
            break;
        default:
            return;
    }
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
        firstLength = Length(first->computeLengthIntForLength(style), Fixed);
    else if (firstType == CSSPrimitiveValue::CSS_PERCENTAGE)
        firstLength = Length((int)first->getFloatValue(), Percent);
    else
        return;

    if (secondType == CSSPrimitiveValue::CSS_UNKNOWN)
        secondLength = Length(Auto);
    else if (secondType > CSSPrimitiveValue::CSS_PERCENTAGE && secondType < CSSPrimitiveValue::CSS_DEG)
        secondLength = Length(second->computeLengthIntForLength(style), Fixed);
    else if (secondType == CSSPrimitiveValue::CSS_PERCENTAGE)
        secondLength = Length((int)second->getFloatValue(), Percent);
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
    if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
        l = Length(primitiveValue->computeLengthIntForLength(style), Fixed);
    else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length((int)primitiveValue->getFloatValue(), Percent);
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
    if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
        l = Length(primitiveValue->computeLengthIntForLength(style), Fixed);
    else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length((int)primitiveValue->getFloatValue(), Percent);
    else
        return;
    layer->setBackgroundYPosition(l);
}

void CSSStyleSelector::checkForTextSizeAdjust()
{
    if (style->textSizeAdjust())
        return;
 
    FontDescription newFontDescription(style->fontDescription());
    newFontDescription.setComputedSize(newFontDescription.specifiedSize());
    style->setFontDescription(newFontDescription);
}

void CSSStyleSelector::checkForGenericFamilyChange(RenderStyle* aStyle, RenderStyle* aParentStyle)
{
    const FontDescription& childFont = aStyle->fontDescription();
  
    if (childFont.isAbsoluteSize() || !aParentStyle)
        return;

    const FontDescription& parentFont = aParentStyle->fontDescription();

    if (childFont.genericFamily() == parentFont.genericFamily())
        return;

    // For now, lump all families but monospace together.
    if (childFont.genericFamily() != FontDescription::MonospaceFamily &&
        parentFont.genericFamily() != FontDescription::MonospaceFamily)
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified.  We want to scale our font size as appropriate.
    float fixedScaleFactor = ((float)settings->mediumFixedFontSize())/settings->mediumFontSize();
    float size = (parentFont.genericFamily() == FontDescription::MonospaceFamily) ? 
        childFont.specifiedSize()/fixedScaleFactor :
        childFont.specifiedSize()*fixedScaleFactor;
  
    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, size);
    aStyle->setFontDescription(newFontDescription);
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
    int minSize = settings->minFontSize();
    int minLogicalSize = settings->minLogicalFontSize();

    float zoomPercent = view ? view->frame()->zoomFactor()/100.0f : 1.0f;
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
    
    return max(zoomedSize, 1.0f);
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
static const float fontSizeFactors[totalKeywords] = { 0.60, 0.75, 0.89, 1.0, 1.2, 1.5, 2.0, 3.0 };

float CSSStyleSelector::fontSizeForKeyword(int keyword, bool quirksMode) const
{
    // FIXME: One issue here is that we set up the fixed font size as a scale factor applied
    // to the proportional pref.  This means that we won't get pixel-perfect size matching for
    // the 13px default, since we actually use the 16px row in the table and apply the fixed size
    // scale later.
    int mediumSize = settings->mediumFontSize();
    if (mediumSize >= fontSizeTableMin && mediumSize <= fontSizeTableMax) {
        // Look up the entry in the table.
        int row = mediumSize - fontSizeTableMin;
        int col = (keyword - CSS_VAL_XX_SMALL);
        return quirksMode ? quirksFontSizeTable[row][col] : strictFontSizeTable[row][col];
    }
    
    // Value is outside the range of the table. Apply the scale factor instead.
    float minLogicalSize = max(settings->minLogicalFontSize(), 1);
    return max(fontSizeFactors[keyword - CSS_VAL_XX_SMALL]*mediumSize, minLogicalSize);
}

float CSSStyleSelector::largerFontSize(float size, bool quirksMode) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level.  
    return size*1.2;
}

float CSSStyleSelector::smallerFontSize(float size, bool quirksMode) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level. 
    return size/1.2;
}

// color mapping code
struct ColorValue {
    int css_value;
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
    { CSS_VAL_ACTIVEBORDER, 0xFFE0E0E0 },
    { CSS_VAL_ACTIVECAPTION, 0xFF000000 },
    { CSS_VAL_APPWORKSPACE, 0xFF000000 },
    { CSS_VAL_BUTTONFACE, 0xFFC0C0C0 },
    { CSS_VAL_BUTTONHIGHLIGHT, 0xFFE0E0E0 },
    { CSS_VAL_BUTTONSHADOW, 0xFFFFFFFF },
    { CSS_VAL_BUTTONTEXT, 0xFF000000 },
    { CSS_VAL_CAPTIONTEXT, 0xFF000000 },
    { CSS_VAL_GRAYTEXT, 0xFF808080 },
    { CSS_VAL_HIGHLIGHT, 0xFFFFFFFF },
    { CSS_VAL_HIGHLIGHTTEXT, 0xFFFFFFFF },
    { CSS_VAL_INACTIVEBORDER, 0xFFFFFFFF },
    { CSS_VAL_INACTIVECAPTION, 0xFFFFFFFF },
    { CSS_VAL_INACTIVECAPTIONTEXT, 0xFF000000 },
    { CSS_VAL_INFOBACKGROUND, 0xFF000000 },
    { CSS_VAL_INFOTEXT, 0xFF000000 },
    { CSS_VAL_MENU, 0xFFFFFFFF },
    { CSS_VAL_MENUTEXT, 0xFFFFFFFF },
    { CSS_VAL_SCROLLBAR, 0xFFFFFFFF },
    { CSS_VAL_TEXT, 0xFF000000 },
    { CSS_VAL_THREEDDARKSHADOW, 0xFF404040 },
    { CSS_VAL_THREEDFACE, 0xFFC0C0C0 },
    { CSS_VAL_THREEDHIGHLIGHT, 0xFFE0E0E0 },
    { CSS_VAL_THREEDLIGHTSHADOW, 0xFFC0C0C0 },
    { CSS_VAL_THREEDSHADOW, 0xFFFFFFFF },
    { CSS_VAL_WINDOW, 0xFFFFFFFF },
    { CSS_VAL_WINDOWFRAME, 0xFFFFFFFF },
    { CSS_VAL_WINDOWTEXT, 0xFF000000 },
    { 0, 0 }
};


static Color colorForCSSValue(int css_value)
{
    for (const ColorValue* col = colorValues; col->css_value; ++col)
        if (col->css_value == css_value)
            return col->color;
    return Color();
}

Color CSSStyleSelector::getColorFromPrimitiveValue(CSSPrimitiveValue* primitiveValue)
{
    Color col;
    int ident = primitiveValue->getIdent();
    if (ident) {
        if (ident == CSS_VAL__WEBKIT_TEXT)
            col = element->document()->textColor();
        else if (ident == CSS_VAL__WEBKIT_LINK) {
            Color linkColor = element->document()->linkColor();
            Color visitedColor = element->document()->visitedLinkColor();
            if (linkColor == visitedColor)
                col = linkColor;
            else {
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    checkPseudoState(element);
                col = (pseudoState == PseudoLink) ? linkColor : visitedColor;
            }
        } else if (ident == CSS_VAL__WEBKIT_ACTIVELINK)
            col = element->document()->activeLinkColor();
        else if (ident == CSS_VAL__WEBKIT_FOCUS_RING_COLOR)
            col = focusRingColor();
        else
            col = colorForCSSValue(ident);
    } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR)
        col.setRgb(primitiveValue->getRGBColorValue());
    return col;
}

} // namespace WebCore
