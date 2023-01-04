/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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
#include "JSCSSStyleSheet.h"
#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
#include "MediaList.h"
#include "MediaQueryParser.h"
#include "Node.h"
#include "SVGElementTypeHelpers.h"
#include "SVGStyleElement.h"
#include "SecurityOrigin.h"
#include "ShadowRoot.h"
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

ExceptionOr<Ref<CSSStyleSheet>> CSSStyleSheet::create(Document& document, Init&& init)
{
    URL baseURL;
    if (init.baseURL.isNull())
        baseURL = document.baseURL();
    else {
        baseURL = document.completeURL(init.baseURL);
        if (!baseURL.isValid())
            return Exception { NotAllowedError, "Base URL is invalid"_s };
    }

    CSSParserContext parserContext(document, baseURL);
    parserContext.shouldIgnoreImportRules = true;
    return adoptRef(*new CSSStyleSheet(StyleSheetContents::create(parserContext), document, WTFMove(init)));
}

CSSStyleSheet::CSSStyleSheet(Ref<StyleSheetContents>&& contents, CSSImportRule* ownerRule)
    : m_contents(WTFMove(contents))
    , m_ownerRule(ownerRule)
{
    if (auto* parent = parentStyleSheet())
        m_styleScope = parent->styleScope();

    m_contents->registerClient(this);
}

CSSStyleSheet::CSSStyleSheet(Ref<StyleSheetContents>&& contents, Node& ownerNode, const TextPosition& startPosition, bool isInlineStylesheet, const std::optional<bool>& isOriginClean)
    : m_contents(WTFMove(contents))
    , m_isInlineStylesheet(isInlineStylesheet)
    , m_isOriginClean(isOriginClean)
    , m_styleScope(Style::Scope::forNode(ownerNode))
    , m_ownerNode(ownerNode)
    , m_startPosition(startPosition)
{
    ASSERT(isAcceptableCSSStyleSheetParent(&ownerNode));
    m_contents->registerClient(this);
}

// https://w3c.github.io/csswg-drafts/cssom-1/#dom-cssstylesheet-cssstylesheet
CSSStyleSheet::CSSStyleSheet(Ref<StyleSheetContents>&& contents, Document& document, Init&& options)
    : m_contents(WTFMove(contents))
    , m_isDisabled(options.disabled)
    , m_wasConstructedByJS(true)
    , m_isOriginClean(true)
    , m_constructorDocument(document)
{
    m_contents->registerClient(this);

    WTF::switchOn(WTFMove(options.media), [this](RefPtr<MediaList>&& mediaList) {
        if (auto queries = mediaList->mediaQueries(); !queries.isEmpty())
            setMediaQueries(WTFMove(queries));
    }, [this](String&& mediaString) {
        setMediaQueries(MQ::MediaQueryParser::parse(mediaString, { }));
    });
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
        m_mediaCSSOMWrapper->detachFromParent();

    m_contents->unregisterClient(this);
}

Node* CSSStyleSheet::ownerNode() const
{
    return m_ownerNode.get();
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

// FIXME: counter-style: we might need something similar for counter-style (rdar://103018993).
void CSSStyleSheet::didMutateRules(RuleMutationType mutationType, WhetherContentsWereClonedForMutation contentsWereClonedForMutation, StyleRuleKeyframes* insertedKeyframesRule, const String& modifiedKeyframesRuleName)
{
    ASSERT(m_contents->isMutable());
    ASSERT(m_contents->hasOneClient());

    forEachStyleScope([&](Style::Scope& scope) {
        if ((mutationType == RuleInsertion || mutationType == RuleReplace) && !contentsWereClonedForMutation && !scope.activeStyleSheetsContains(this)) {
            if (insertedKeyframesRule) {
                if (auto* resolver = scope.resolverIfExists())
                    resolver->addKeyframeStyle(*insertedKeyframesRule);
                return;
            }
            scope.didChangeActiveStyleSheetCandidates();
            return;
        }

        if (mutationType == KeyframesRuleMutation) {
            if (auto* ownerDocument = this->ownerDocument())
                ownerDocument->keyframesRuleDidChange(modifiedKeyframesRuleName);
        }

        scope.didChangeStyleSheetContents();

        m_mutatedRules = true;
    });
}

void CSSStyleSheet::didMutate()
{
    forEachStyleScope([](auto& scope) {
        scope.didChangeStyleSheetContents();
    });
}

void CSSStyleSheet::forEachStyleScope(const Function<void(Style::Scope&)>& apply)
{
    if (auto* scope = styleScope()) {
        apply(*scope);
        return;
    }
    for (auto& shadowRoot : m_adoptingShadowRoots)
        apply(shadowRoot.styleScope());
    for (auto& document : m_adoptingDocuments)
        apply(document.styleScope());
}

void CSSStyleSheet::clearOwnerNode()
{
    m_ownerNode = nullptr;
}

CSSImportRule* CSSStyleSheet::ownerRule() const
{
    return m_ownerRule.get();
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

    forEachStyleScope([](auto& scope) {
        scope.didChangeActiveStyleSheetCandidates();
    });
}

void CSSStyleSheet::setMediaQueries(MQ::MediaQueryList&& mediaQueries)
{
    m_mediaQueries = WTFMove(mediaQueries);
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
        cssRule = m_contents->ruleAt(index)->createCSSOMWrapper(*this);
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

    if (m_wasConstructedByJS && rule->isImportRule())
        return Exception { SyntaxError, "Cannot inserted an @import rule in a constructed CSSStyleSheet object"_s };

    RuleMutationScope mutationScope(this, RuleInsertion, dynamicDowncast<StyleRuleKeyframes>(*rule));

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
    if (!m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper = MediaList::create(const_cast<CSSStyleSheet*>(this));
    return m_mediaCSSOMWrapper.get();
}

CSSStyleSheet* CSSStyleSheet::parentStyleSheet() const 
{ 
    RefPtr ownerRule = m_ownerRule.get();
    return ownerRule ? ownerRule->parentStyleSheet() : nullptr;
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
    return m_styleScope.get();
}

void CSSStyleSheet::clearChildRuleCSSOMWrappers()
{
    m_childRuleCSSOMWrappers.clear();
}

String CSSStyleSheet::debugDescription() const
{
    return makeString("CSSStyleSheet "_s, "0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase), ' ', href());
}

// https://w3c.github.io/csswg-drafts/cssom-1/#dom-cssstylesheet-replace
void CSSStyleSheet::replace(String&& text, Ref<DeferredPromise>&& promise)
{
    auto result = replaceSync(WTFMove(text));
    if (result.hasException())
        promise->reject(result.releaseException());
    promise->resolve<IDLInterface<CSSStyleSheet>>(*this);
}

// https://w3c.github.io/csswg-drafts/cssom-1/#dom-cssstylesheet-replacesync
ExceptionOr<void> CSSStyleSheet::replaceSync(String&& text)
{
    if (!m_wasConstructedByJS)
        return Exception { NotAllowedError, "This CSSStyleSheet object was not constructed by JavaScript"_s };

    RuleMutationScope mutationScope(this, RuleReplace);
    m_contents->clearRules();
    for (auto& childRuleWrapper : m_childRuleCSSOMWrappers)
        childRuleWrapper->setParentStyleSheet(nullptr);
    m_childRuleCSSOMWrappers.clear();

    m_contents->parseString(WTFMove(text));
    return { };
}

Document* CSSStyleSheet::constructorDocument() const
{
    return m_constructorDocument.get();
}

void CSSStyleSheet::addAdoptingTreeScope(Document& document)
{
    m_adoptingDocuments.add(document);
    document.styleScope().didChangeActiveStyleSheetCandidates();
}

void CSSStyleSheet::addAdoptingTreeScope(ShadowRoot& shadowRoot)
{
    m_adoptingShadowRoots.add(shadowRoot);
    shadowRoot.styleScope().didChangeActiveStyleSheetCandidates();
}

void CSSStyleSheet::removeAdoptingTreeScope(Document& document, IsTreeScopeBeingDestroyed isTreeScopeBeingDestroyed)
{
    m_adoptingDocuments.remove(document);
    if (isTreeScopeBeingDestroyed == IsTreeScopeBeingDestroyed::No)
        document.styleScope().didChangeStyleSheetContents();
}

void CSSStyleSheet::removeAdoptingTreeScope(ShadowRoot& shadowRoot, IsTreeScopeBeingDestroyed isTreeScopeBeingDestroyed)
{
    m_adoptingShadowRoots.remove(shadowRoot);
    if (isTreeScopeBeingDestroyed == IsTreeScopeBeingDestroyed::No)
        shadowRoot.styleScope().didChangeStyleSheetContents();
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
    , m_mutationType(is<CSSKeyframesRule>(rule) ? KeyframesRuleMutation : OtherMutation)
    , m_contentsWereClonedForMutation(ContentsWereNotClonedForMutation)
    , m_insertedKeyframesRule(nullptr)
    , m_modifiedKeyframesRuleName(is<CSSKeyframesRule>(rule) ? downcast<CSSKeyframesRule>(*rule).name() : emptyAtom())
{
    if (m_styleSheet)
        m_contentsWereClonedForMutation = m_styleSheet->willMutateRules();
}

CSSStyleSheet::RuleMutationScope::~RuleMutationScope()
{
    if (m_styleSheet)
        m_styleSheet->didMutateRules(m_mutationType, m_contentsWereClonedForMutation, m_insertedKeyframesRule.get(), m_modifiedKeyframesRuleName);
}

}
