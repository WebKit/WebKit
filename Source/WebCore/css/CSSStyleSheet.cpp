/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
#include "CSSStyleSheet.h"

#include "CSSCharsetRule.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSNamespace.h"
#include "CSSParser.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CachedCSSStyleSheet.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"
#include "MediaList.h"
#include "Node.h"
#include "SVGNames.h"
#include "SecurityOrigin.h"
#include "StylePropertySet.h"
#include "StyleRule.h"
#include "TextEncoding.h"
#include <wtf/Deque.h>

namespace WebCore {

class StyleSheetCSSRuleList : public CSSRuleList {
public:
    StyleSheetCSSRuleList(CSSStyleSheet* sheet) : m_styleSheet(sheet) { }
    
private:
    virtual void ref() { m_styleSheet->ref(); }
    virtual void deref() { m_styleSheet->deref(); }
    
    virtual unsigned length() const { return m_styleSheet->length(); }
    virtual CSSRule* item(unsigned index) const { return m_styleSheet->item(index); }
    
    virtual CSSStyleSheet* styleSheet() const { return m_styleSheet; }
    
    CSSStyleSheet* m_styleSheet;
};

#if !ASSERT_DISABLED
static bool isAcceptableCSSStyleSheetParent(Node* parentNode)
{
    // Only these nodes can be parents of StyleSheets, and they need to call clearOwnerNode() when moved out of document.
    return !parentNode
        || parentNode->isDocumentNode()
        || parentNode->hasTagName(HTMLNames::linkTag)
        || parentNode->hasTagName(HTMLNames::styleTag)
#if ENABLE(SVG)
        || parentNode->hasTagName(SVGNames::styleTag)
#endif
        || parentNode->nodeType() == Node::PROCESSING_INSTRUCTION_NODE;
}
#endif

CSSStyleSheet::CSSStyleSheet(Node* parentNode, const String& href, const KURL& baseURL, const String& charset)
    : StyleSheet(parentNode, href, baseURL)
    , m_ownerRule(0)
    , m_charset(charset)
    , m_loadCompleted(false)
    , m_cssParserMode(CSSQuirksMode)
    , m_isUserStyleSheet(false)
    , m_hasSyntacticallyValidCSSHeader(true)
    , m_didLoadErrorOccur(false)
{
    ASSERT(isAcceptableCSSStyleSheetParent(parentNode));
}

CSSStyleSheet::CSSStyleSheet(StyleRuleImport* ownerRule, const String& href, const KURL& baseURL, const String& charset)
    : StyleSheet(href, baseURL)
    , m_ownerRule(ownerRule)
    , m_charset(charset)
    , m_loadCompleted(false)
    , m_cssParserMode((ownerRule && ownerRule->parentStyleSheet()) ? ownerRule->parentStyleSheet()->cssParserMode() : CSSStrictMode)
    , m_hasSyntacticallyValidCSSHeader(true)
    , m_didLoadErrorOccur(false)
{
    CSSStyleSheet* parentSheet = ownerRule ? ownerRule->parentStyleSheet() : 0;
    m_isUserStyleSheet = parentSheet ? parentSheet->isUserStyleSheet() : false;
}

CSSStyleSheet::~CSSStyleSheet()
{
    clearRules();
}

void CSSStyleSheet::parserAppendRule(PassRefPtr<StyleRuleBase> rule)
{
    ASSERT(!rule->isCharsetRule());
    if (rule->isImportRule()) {
        // Parser enforces that @import rules come before anything else except @charset.
        ASSERT(m_childRules.isEmpty());
        m_importRules.append(static_cast<StyleRuleImport*>(rule.get()));
        m_importRules.last()->requestStyleSheet();
        return;
    }
    m_childRules.append(rule);
}

PassRefPtr<CSSRule> CSSStyleSheet::createChildRuleCSSOMWrapper(unsigned index)
{
    ASSERT(index < length());
    
    unsigned childVectorIndex = index;
    if (hasCharsetRule()) {
        if (index == 0)
            return CSSCharsetRule::create(this, m_encodingFromCharsetRule);
        --childVectorIndex;
    }
    if (childVectorIndex < m_importRules.size())
        return m_importRules[childVectorIndex]->createCSSOMWrapper(this);
    
    childVectorIndex -= m_importRules.size();
    return m_childRules[childVectorIndex]->createCSSOMWrapper(this);
}

unsigned CSSStyleSheet::length() const
{
    unsigned result = 0;
    result += hasCharsetRule() ? 1 : 0;
    result += m_importRules.size();
    result += m_childRules.size();
    return result;
}

CSSRule* CSSStyleSheet::item(unsigned index)
{
    unsigned ruleCount = length();
    if (index >= ruleCount)
        return 0;
    
    if (m_childRuleCSSOMWrappers.isEmpty())
        m_childRuleCSSOMWrappers.grow(ruleCount);
    ASSERT(m_childRuleCSSOMWrappers.size() == ruleCount);

    RefPtr<CSSRule>& cssRule = m_childRuleCSSOMWrappers[index];
    if (!cssRule)
        cssRule = createChildRuleCSSOMWrapper(index);
    return cssRule.get();
}

void CSSStyleSheet::clearCharsetRule()
{
    m_encodingFromCharsetRule = String();
}

void CSSStyleSheet::clearRules()
{
    // For style rules outside the document, .parentStyleSheet can become null even if the style rule
    // is still observable from JavaScript. This matches the behavior of .parentNode for nodes, but
    // it's not ideal because it makes the CSSOM's behavior depend on the timing of garbage collection.
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->setParentStyleSheet(0);
    }
    m_childRuleCSSOMWrappers.clear();

    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        ASSERT(m_importRules.at(i)->parentStyleSheet() == this);
        m_importRules[i]->clearParentStyleSheet();
    }
    m_importRules.clear();
    m_childRules.clear();
    clearCharsetRule();
}

void CSSStyleSheet::parserSetEncodingFromCharsetRule(const String& encoding)
{
    // Parser enforces that there is ever only one @charset.
    ASSERT(m_encodingFromCharsetRule.isNull());
    m_encodingFromCharsetRule = encoding; 
}

PassRefPtr<CSSRuleList> CSSStyleSheet::rules()
{
    KURL url = finalURL();
    Document* document = findDocument();
    if (!url.isEmpty() && document && !document->securityOrigin()->canRequest(url))
        return 0;
    // IE behavior.
    RefPtr<StaticCSSRuleList> nonCharsetRules = StaticCSSRuleList::create();
    unsigned ruleCount = length();
    for (unsigned i = 0; i < ruleCount; ++i) {
        CSSRule* rule = item(i);
        if (rule->isCharsetRule())
            continue;
        nonCharsetRules->rules().append(rule);
    }
    return nonCharsetRules.release();
}

unsigned CSSStyleSheet::insertRule(const String& ruleString, unsigned index, ExceptionCode& ec)
{
    ec = 0;
    if (index > length()) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    CSSParser p(cssParserMode());
    RefPtr<StyleRuleBase> rule = p.parseRule(this, ruleString);

    if (!rule) {
        ec = SYNTAX_ERR;
        return 0;
    }
    // Parser::parseRule doesn't currently allow @charset so we don't need to deal with it.
    ASSERT(!rule->isCharsetRule());
    
    unsigned childVectorIndex = index;
    // m_childRules does not contain @charset which is always in index 0 if it exists.
    if (hasCharsetRule()) {
        if (childVectorIndex == 0) {
            // Nothing can be inserted before @charset.
            ec = HIERARCHY_REQUEST_ERR;
            return 0;
        }
        --childVectorIndex;
    }
    
    if (childVectorIndex < m_importRules.size() || (childVectorIndex == m_importRules.size() && rule->isImportRule())) {
        // Inserting non-import rule before @import is not allowed.
        if (!rule->isImportRule()) {
            ec = HIERARCHY_REQUEST_ERR;
            return 0;
        }
        m_importRules.insert(childVectorIndex, static_cast<StyleRuleImport*>(rule.get()));
        m_importRules[childVectorIndex]->requestStyleSheet();
        goto success;
    }
    // Inserting @import rule after a non-import rule is not allowed.
    if (rule->isImportRule()) {
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }
    childVectorIndex -= m_importRules.size();
 
    m_childRules.insert(childVectorIndex, rule.release());
    
success:
    if (!m_childRuleCSSOMWrappers.isEmpty())
        m_childRuleCSSOMWrappers.insert(index, RefPtr<CSSRule>());

    // FIXME: Stylesheet doesn't actually change meaningfully before the imported sheets are loaded.
    styleSheetChanged();
    return index;
}

int CSSStyleSheet::addRule(const String& selector, const String& style, int index, ExceptionCode& ec)
{
    insertRule(selector + " { " + style + " }", index, ec);

    // As per Microsoft documentation, always return -1.
    return -1;
}

int CSSStyleSheet::addRule(const String& selector, const String& style, ExceptionCode& ec)
{
    return addRule(selector, style, length(), ec);
}

PassRefPtr<CSSRuleList> CSSStyleSheet::cssRules()
{
    KURL url = finalURL();
    Document* document = findDocument();
    if (!url.isEmpty() && document && !document->securityOrigin()->canRequest(url))
        return 0;
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = adoptPtr(new StyleSheetCSSRuleList(this));
    return m_ruleListCSSOMWrapper.get();
}

void CSSStyleSheet::deleteRule(unsigned index, ExceptionCode& ec)
{
    if (index >= length()) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    ec = 0;
    unsigned childVectorIndex = index;
    if (hasCharsetRule()) {
        if (childVectorIndex == 0) {
            clearCharsetRule();
            goto success;
        }
        --childVectorIndex;
    }
    if (childVectorIndex < m_importRules.size()) {
        m_importRules[childVectorIndex]->clearParentStyleSheet();
        m_importRules.remove(childVectorIndex);
        goto success;
    }
    childVectorIndex -= m_importRules.size();

    m_childRules.remove(childVectorIndex);

success:
    if (!m_childRuleCSSOMWrappers.isEmpty()) {
        m_childRuleCSSOMWrappers[index]->setParentStyleSheet(0);
        m_childRuleCSSOMWrappers.remove(index);
    }

    styleSheetChanged();
}

void CSSStyleSheet::addNamespace(CSSParser* p, const AtomicString& prefix, const AtomicString& uri)
{
    if (uri.isNull())
        return;

    m_namespaces = adoptPtr(new CSSNamespace(prefix, uri, m_namespaces.release()));

    if (prefix.isEmpty())
        // Set the default namespace on the parser so that selectors that omit namespace info will
        // be able to pick it up easily.
        p->m_defaultNamespace = uri;
}

const AtomicString& CSSStyleSheet::determineNamespace(const AtomicString& prefix)
{
    if (prefix.isNull())
        return nullAtom; // No namespace. If an element/attribute has a namespace, we won't match it.
    if (prefix == starAtom)
        return starAtom; // We'll match any namespace.
    if (m_namespaces) {
        if (CSSNamespace* namespaceForPrefix = m_namespaces->namespaceForPrefix(prefix))
            return namespaceForPrefix->uri;
    }
    return nullAtom; // Assume we won't match any namespaces.
}

bool CSSStyleSheet::parseString(const String &string, CSSParserMode cssParserMode)
{
    return parseStringAtLine(string, cssParserMode, 0);
}

bool CSSStyleSheet::parseStringAtLine(const String& string, CSSParserMode cssParserMode, int startLineNumber)
{
    setCSSParserMode(cssParserMode);
    CSSParser p(cssParserMode);
    p.parseSheet(this, string, startLineNumber);
    return true;
}

bool CSSStyleSheet::isLoading()
{
    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        if (m_importRules[i]->isLoading())
            return true;
    }
    return false;
}

void CSSStyleSheet::checkLoaded()
{
    if (isLoading())
        return;

    // Avoid |this| being deleted by scripts that run via
    // ScriptableDocumentParser::executeScriptsWaitingForStylesheets().
    // See <rdar://problem/6622300>.
    RefPtr<CSSStyleSheet> protector(this);
    if (CSSStyleSheet* styleSheet = parentStyleSheet())
        styleSheet->checkLoaded();

    RefPtr<Node> owner = ownerNode();
    if (!owner)
        m_loadCompleted = true;
    else {
        m_loadCompleted = owner->sheetLoaded();
        if (m_loadCompleted)
            owner->notifyLoadedSheetAndAllCriticalSubresources(m_didLoadErrorOccur);
    }
}

void CSSStyleSheet::notifyLoadedSheet(const CachedCSSStyleSheet* sheet)
{
    ASSERT(sheet);
    m_didLoadErrorOccur |= sheet->errorOccurred();
}

void CSSStyleSheet::startLoadingDynamicSheet()
{
    if (Node* owner = ownerNode())
        owner->startLoadingDynamicSheet();
}

Node* CSSStyleSheet::findStyleSheetOwnerNode() const
{
    for (const CSSStyleSheet* sheet = this; sheet; sheet = sheet->parentStyleSheet()) {
        if (Node* ownerNode = sheet->ownerNode())
            return ownerNode;
    }
    return 0;
}

Document* CSSStyleSheet::findDocument()
{
    Node* ownerNode = findStyleSheetOwnerNode();

    return ownerNode ? ownerNode->document() : 0;
}
    
MediaList* CSSStyleSheet::media() const 
{ 
    if (!m_mediaQueries)
        return 0;
    return m_mediaQueries->ensureMediaList(const_cast<CSSStyleSheet*>(this));
}

void CSSStyleSheet::setMediaQueries(PassRefPtr<MediaQuerySet> mediaQueries)
{
    m_mediaQueries = mediaQueries;
}

void CSSStyleSheet::styleSheetChanged()
{
    CSSStyleSheet* rootSheet = this;
    while (CSSStyleSheet* parent = rootSheet->parentStyleSheet())
        rootSheet = parent;

    /* FIXME: We don't need to do everything updateStyleSelector does,
     * basically we just need to recreate the document's selector with the
     * already existing style sheets.
     */
    if (Document* documentToUpdate = rootSheet->findDocument())
        documentToUpdate->styleSelectorChanged(DeferRecalcStyle);
}

KURL CSSStyleSheet::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    // See also Document::completeURL(const String&)
    if (url.isNull())
        return KURL();
    if (m_charset.isEmpty())
        return KURL(baseURL(), url);
    const TextEncoding encoding = TextEncoding(m_charset);
    return KURL(baseURL(), url, encoding);
}

void CSSStyleSheet::addSubresourceStyleURLs(ListHashSet<KURL>& urls)
{
    Deque<CSSStyleSheet*> styleSheetQueue;
    styleSheetQueue.append(this);

    while (!styleSheetQueue.isEmpty()) {
        CSSStyleSheet* styleSheet = styleSheetQueue.takeFirst();
        
        for (unsigned i = 0; i < styleSheet->m_importRules.size(); ++i) {
            StyleRuleImport* importRule = styleSheet->m_importRules[i].get();
            if (importRule->styleSheet()) {
                styleSheetQueue.append(importRule->styleSheet());
                addSubresourceURL(urls, importRule->styleSheet()->baseURL());
            }
        }
        for (unsigned i = 0; i < styleSheet->m_childRules.size(); ++i) {
            StyleRuleBase* rule = styleSheet->m_childRules[i].get();
            if (rule->isStyleRule())
                static_cast<StyleRule*>(rule)->properties()->addSubresourceStyleURLs(urls, this);
            else if (rule->isFontFaceRule())
                static_cast<StyleRuleFontFace*>(rule)->properties()->addSubresourceStyleURLs(urls, this);
        }
    }
}

CSSImportRule* CSSStyleSheet::ensureCSSOMWrapper(StyleRuleImport* importRule)
{
     for (unsigned i = 0; i < m_importRules.size(); ++i) {
         if (m_importRules[i] == importRule)
             return static_cast<CSSImportRule*>(item(i));
     }
    ASSERT_NOT_REACHED();
    return 0;
}

CSSImportRule* CSSStyleSheet::ownerRule() const 
{  
    return parentStyleSheet() ? parentStyleSheet()->ensureCSSOMWrapper(m_ownerRule) : 0; 
}

CSSStyleSheet* CSSStyleSheet::parentStyleSheet() const
{
    ASSERT(isCSSStyleSheet());
    return m_ownerRule ? m_ownerRule->parentStyleSheet() : 0;
}

}
