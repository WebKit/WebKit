/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2012, 2013 Apple Inc. All rights reserved.
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

#include "CSSImportRule.h"
#include "CSSKeyframesRule.h"
#include "CSSParser.h"
#include "CSSRuleList.h"
#include "Document.h"
#include "HTMLLinkElement.h"
#include "HTMLStyleElement.h"
#include "Logging.h"
#include "MediaList.h"
#include "Node.h"
#include "SVGStyleElement.h"
#include "SecurityOrigin.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"

#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class StyleSheetCSSRuleList final : public CSSRuleList {
public:
    StyleSheetCSSRuleList(CSSStyleSheet* sheet) : m_styleSheet(sheet) { }
    
private:
    void ref() final { m_styleSheet->ref(); }
    void deref() final { m_styleSheet->deref(); }

    unsigned length() const final { return m_styleSheet->length(); }
    CSSRule* item(unsigned index) const final { return m_styleSheet->item(index); }

    CSSStyleSheet* styleSheet() const final { return m_styleSheet; }

    CSSStyleSheet* m_styleSheet;
};

#if ASSERT_ENABLED
static bool isAcceptableCSSStyleSheetParent(Node* parentNode)
{
    // Only these nodes can be parents of StyleSheets, and they need to call clearOwnerNode() when moved out of document.
    return !parentNode
        || parentNode->isDocumentNode()
        || is<HTMLLinkElement>(*parentNode)
        || is<HTMLStyleElement>(*parentNode)
        || is<SVGStyleElement>(*parentNode)
        || parentNode->nodeType() == Node::PROCESSING_INSTRUCTION_NODE;
}
#endif // ASSERT_ENABLED

Ref<CSSStyleSheet> CSSStyleSheet::create(Ref<StyleSheetContents>&& sheet, CSSImportRule* ownerRule)
{
    return adoptRef(*new CSSStyleSheet(WTFMove(sheet), ownerRule));
}

Ref<CSSStyleSheet> CSSStyleSheet::create(Ref<StyleSheetContents>&& sheet, Node& ownerNode, const std::optional<bool>& isCleanOrigin)
{
    return adoptRef(*new CSSStyleSheet(WTFMove(sheet), ownerNode, TextPosition(), false, isCleanOrigin));
}

Ref<CSSStyleSheet> CSSStyleSheet::createInline(Ref<StyleSheetContents>&& sheet, Element& owner, const TextPosition& startPosition)
{
    return adoptRef(*new CSSStyleSheet(WTFMove(sheet), owner, startPosition, true, true));
}

CSSStyleSheet::CSSStyleSheet(Ref<StyleSheetContents>&& contents, CSSImportRule* ownerRule)
    : m_contents(WTFMove(contents))
    , m_ownerRule(ownerRule)
{
    m_contents->registerClient(this);
}

CSSStyleSheet::CSSStyleSheet(Ref<StyleSheetContents>&& contents, Node& ownerNode, const TextPosition& startPosition, bool isInlineStylesheet, const std::optional<bool>& isOriginClean)
    : m_contents(WTFMove(contents))
    , m_isInlineStylesheet(isInlineStylesheet)
    , m_isOriginClean(isOriginClean)
    , m_ownerNode(&ownerNode)
    , m_startPosition(startPosition)
{
    ASSERT(isAcceptableCSSStyleSheetParent(&ownerNode));
    m_contents->registerClient(this);
}

CSSStyleSheet::~CSSStyleSheet()
{
    // For style rules outside the document, .parentStyleSheet can become null even if the style rule
    // is still observable from JavaScript. This matches the behavior of .parentNode for nodes, but
    // it's not ideal because it makes the CSSOM's behavior depend on the timing of garbage collection.
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->setParentStyleSheet(0);
    }
    if (m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper->clearParentStyleSheet();

    m_contents->unregisterClient(this);
}

CSSStyleSheet::WhetherContentsWereClonedForMutation CSSStyleSheet::willMutateRules()
{
    // If we are the only client it is safe to mutate.
    if (m_contents->hasOneClient() && !m_contents->isInMemoryCache()) {
        m_contents->setMutable();
        return ContentsWereNotClonedForMutation;
    }
    // Only cacheable stylesheets should have multiple clients.
    ASSERT(m_contents->isCacheable());

    // Copy-on-write.
    m_contents->unregisterClient(this);
    m_contents = m_contents->copy();
    m_contents->registerClient(this);

    m_contents->setMutable();

    // Any existing CSSOM wrappers need to be connected to the copied child rules.
    reattachChildRuleCSSOMWrappers();

    return ContentsWereClonedForMutation;
}

void CSSStyleSheet::didMutateRuleFromCSSStyleDeclaration()
{
    ASSERT(m_contents->isMutable());
    ASSERT(m_contents->hasOneClient());
    didMutate();
}

void CSSStyleSheet::didMutateRules(RuleMutationType mutationType, WhetherContentsWereClonedForMutation contentsWereClonedForMutation, StyleRuleKeyframes* insertedKeyframesRule)
{
    ASSERT(m_contents->isMutable());
    ASSERT(m_contents->hasOneClient());

    auto* scope = styleScope();
    if (!scope)
        return;

    if (mutationType == RuleInsertion && !contentsWereClonedForMutation && !scope->activeStyleSheetsContains(this)) {
        if (insertedKeyframesRule) {
            if (auto* resolver = scope->resolverIfExists())
                resolver->addKeyframeStyle(*insertedKeyframesRule);
            return;
        }
        scope->didChangeActiveStyleSheetCandidates();
        return;
    }

    scope->didChangeStyleSheetContents();

    m_mutatedRules = true;
}

void CSSStyleSheet::didMutate()
{
    auto* scope = styleScope();
    if (!scope)
        return;
    scope->didChangeStyleSheetContents();
}

void CSSStyleSheet::clearOwnerNode()
{
    m_ownerNode = nullptr;
}

void CSSStyleSheet::reattachChildRuleCSSOMWrappers()
{
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (!m_childRuleCSSOMWrappers[i])
            continue;
        m_childRuleCSSOMWrappers[i]->reattach(*m_contents->ruleAt(i));
    }
}

void CSSStyleSheet::setDisabled(bool disabled)
{ 
    if (disabled == m_isDisabled)
        return;
    m_isDisabled = disabled;

    if (auto* scope = styleScope())
        scope->didChangeActiveStyleSheetCandidates();
}

void CSSStyleSheet::setMediaQueries(Ref<MediaQuerySet>&& mediaQueries)
{
    m_mediaQueries = WTFMove(mediaQueries);
    if (m_mediaCSSOMWrapper && m_mediaQueries)
        m_mediaCSSOMWrapper->reattach(m_mediaQueries.get());
    reportMediaQueryWarningIfNeeded(ownerDocument(), m_mediaQueries.get());
}

unsigned CSSStyleSheet::length() const
{
    return m_contents->ruleCount();
}

CSSRule* CSSStyleSheet::item(unsigned index)
{
    unsigned ruleCount = length();
    if (index >= ruleCount)
        return nullptr;

    ASSERT(m_childRuleCSSOMWrappers.isEmpty() || m_childRuleCSSOMWrappers.size() == ruleCount);
    if (m_childRuleCSSOMWrappers.size() < ruleCount)
        m_childRuleCSSOMWrappers.grow(ruleCount);

    RefPtr<CSSRule>& cssRule = m_childRuleCSSOMWrappers[index];
    if (!cssRule)
        cssRule = m_contents->ruleAt(index)->createCSSOMWrapper(this);
    return cssRule.get();
}

bool CSSStyleSheet::canAccessRules() const
{
    if (m_isOriginClean)
        return m_isOriginClean.value();

    URL baseURL = m_contents->baseURL();
    if (baseURL.isEmpty())
        return true;
    Document* document = ownerDocument();
    if (!document)
        return true;
    return document->securityOrigin().canRequest(baseURL);
}

ExceptionOr<unsigned> CSSStyleSheet::insertRule(const String& ruleString, unsigned index)
{
    LOG_WITH_STREAM(StyleSheets, stream << "CSSStyleSheet " << this << " insertRule() " << ruleString << " at " << index);

    ASSERT(m_childRuleCSSOMWrappers.isEmpty() || m_childRuleCSSOMWrappers.size() == m_contents->ruleCount());

    if (index > length())
        return Exception { IndexSizeError };
    RefPtr<StyleRuleBase> rule = CSSParser::parseRule(m_contents.get().parserContext(), m_contents.ptr(), ruleString);

    if (!rule)
        return Exception { SyntaxError };

    RuleMutationScope mutationScope(this, RuleInsertion, is<StyleRuleKeyframes>(*rule) ? downcast<StyleRuleKeyframes>(rule.get()) : nullptr);

    bool success = m_contents.get().wrapperInsertRule(rule.releaseNonNull(), index);
    if (!success)
        return Exception { HierarchyRequestError };
    if (!m_childRuleCSSOMWrappers.isEmpty())
        m_childRuleCSSOMWrappers.insert(index, RefPtr<CSSRule>());

    return index;
}

ExceptionOr<void> CSSStyleSheet::deleteRule(unsigned index)
{
    LOG_WITH_STREAM(StyleSheets, stream << "CSSStyleSheet " << this << " deleteRule(" << index << ")");

    ASSERT(m_childRuleCSSOMWrappers.isEmpty() || m_childRuleCSSOMWrappers.size() == m_contents->ruleCount());

    if (index >= length())
        return Exception { IndexSizeError };
    RuleMutationScope mutationScope(this);

    m_contents->wrapperDeleteRule(index);

    if (!m_childRuleCSSOMWrappers.isEmpty()) {
        if (m_childRuleCSSOMWrappers[index])
            m_childRuleCSSOMWrappers[index]->setParentStyleSheet(nullptr);
        m_childRuleCSSOMWrappers.remove(index);
    }

    return { };
}

ExceptionOr<int> CSSStyleSheet::addRule(const String& selector, const String& style, std::optional<unsigned> index)
{
    LOG_WITH_STREAM(StyleSheets, stream << "CSSStyleSheet " << this << " addRule() selector " << selector << " style " << style << " at " << index);

    auto text = makeString(selector, " { ", style, !style.isEmpty() ? " " : "", '}');
    auto insertRuleResult = insertRule(text, index.value_or(length()));
    if (insertRuleResult.hasException())
        return insertRuleResult.releaseException();
    // As per Microsoft documentation, always return -1.
    return -1;
}

ExceptionOr<Ref<CSSRuleList>> CSSStyleSheet::cssRulesForBindings()
{
    auto cssRules = this->cssRules();
    if (!cssRules)
        return Exception { SecurityError, "Not allowed to access cross-origin stylesheet"_s };
    return cssRules.releaseNonNull();
}

RefPtr<CSSRuleList> CSSStyleSheet::cssRules()
{
    if (!canAccessRules())
        return nullptr;
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = makeUnique<StyleSheetCSSRuleList>(this);
    return m_ruleListCSSOMWrapper.get();
}

String CSSStyleSheet::href() const
{
    return m_contents->originalURL();
}

URL CSSStyleSheet::baseURL() const
{
    return m_contents->baseURL();
}

bool CSSStyleSheet::isLoading() const
{
    return m_contents->isLoading();
}

MediaList* CSSStyleSheet::media() const 
{
    if (!m_mediaQueries)
        return nullptr;
    if (!m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper = MediaList::create(m_mediaQueries.get(), const_cast<CSSStyleSheet*>(this));
    return m_mediaCSSOMWrapper.get();
}

CSSStyleSheet* CSSStyleSheet::parentStyleSheet() const 
{ 
    return m_ownerRule ? m_ownerRule->parentStyleSheet() : nullptr;
}

CSSStyleSheet& CSSStyleSheet::rootStyleSheet()
{
    auto* root = this;
    while (root->parentStyleSheet())
        root = root->parentStyleSheet();
    return *root;
}

const CSSStyleSheet& CSSStyleSheet::rootStyleSheet() const
{
    return const_cast<CSSStyleSheet&>(*this).rootStyleSheet();
}

Document* CSSStyleSheet::ownerDocument() const
{
    auto& root = rootStyleSheet();
    return root.ownerNode() ? &root.ownerNode()->document() : nullptr;
}

Style::Scope* CSSStyleSheet::styleScope()
{
    auto* ownerNode = rootStyleSheet().ownerNode();
    if (!ownerNode)
        return nullptr;
    return &Style::Scope::forNode(*ownerNode);
}

void CSSStyleSheet::clearChildRuleCSSOMWrappers()
{
    m_childRuleCSSOMWrappers.clear();
}

String CSSStyleSheet::debugDescription() const
{
    return makeString("CSSStyleSheet "_s, "0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase), ' ', href());
}

CSSStyleSheet::RuleMutationScope::RuleMutationScope(CSSStyleSheet* sheet, RuleMutationType mutationType, StyleRuleKeyframes* insertedKeyframesRule)
    : m_styleSheet(sheet)
    , m_mutationType(mutationType)
    , m_insertedKeyframesRule(insertedKeyframesRule)
{
    ASSERT(m_styleSheet);
    m_contentsWereClonedForMutation = m_styleSheet->willMutateRules();
}

CSSStyleSheet::RuleMutationScope::RuleMutationScope(CSSRule* rule)
    : m_styleSheet(rule ? rule->parentStyleSheet() : nullptr)
    , m_mutationType(OtherMutation)
    , m_contentsWereClonedForMutation(ContentsWereNotClonedForMutation)
    , m_insertedKeyframesRule(nullptr)
{
    if (m_styleSheet)
        m_contentsWereClonedForMutation = m_styleSheet->willMutateRules();
}

CSSStyleSheet::RuleMutationScope::~RuleMutationScope()
{
    if (m_styleSheet)
        m_styleSheet->didMutateRules(m_mutationType, m_contentsWereClonedForMutation, m_insertedKeyframesRule);
}

}
