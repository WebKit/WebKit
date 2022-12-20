/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2009, 2011-2012, 2015-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
#include "StyleScope.h"

#include "CSSFontSelector.h"
#include "CSSStyleSheet.h"
#include "CustomPropertyRegistry.h"
#include "Element.h"
#include "ElementChildIterator.h"
#include "ElementRareData.h"
#include "ExtensionStyleSheets.h"
#include "HTMLHeadElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLLinkElement.h"
#include "HTMLSlotElement.h"
#include "HTMLStyleElement.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "ProcessingInstruction.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGStyleElement.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleBuilder.h"
#include "StyleInvalidator.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "UserStyleSheet.h"
#include <wtf/SetForScope.h>

namespace WebCore {

using namespace HTMLNames;

namespace Style {

Scope::Scope(Document& document)
    : m_document(document)
    , m_pendingUpdateTimer(*this, &Scope::pendingUpdateTimerFired)
    , m_customPropertyRegistry(makeUniqueRef<CustomPropertyRegistry>(*this))
{
}

Scope::Scope(ShadowRoot& shadowRoot)
    : m_document(shadowRoot.documentScope())
    , m_shadowRoot(&shadowRoot)
    , m_pendingUpdateTimer(*this, &Scope::pendingUpdateTimerFired)
    , m_customPropertyRegistry(makeUniqueRef<CustomPropertyRegistry>(*this))
{
}

Scope::~Scope()
{
    ASSERT(!hasPendingSheets());
}

Resolver& Scope::resolver()
{
    if (!m_resolver) {
        if (m_shadowRoot)
            createOrFindSharedShadowTreeResolver();
        else
            createDocumentResolver();
        
        if (m_resolver->ruleSets().features().usesHasPseudoClass())
            m_usesHasPseudoClass = true;
    }
    return *m_resolver;
}

void Scope::createDocumentResolver()
{
    ASSERT(!m_resolver);
    ASSERT(!m_shadowRoot);

    SetForScope isUpdatingStyleResolver { m_isUpdatingStyleResolver, true };

    m_resolver = Resolver::create(m_document);

    m_document.fontSelector().buildStarted();

    m_resolver->ruleSets().initializeUserStyle();
    m_resolver->addCurrentSVGFontFaceRules();
    m_resolver->appendAuthorStyleSheets(m_activeStyleSheets);

    m_document.fontSelector().buildCompleted();
}

void Scope::createOrFindSharedShadowTreeResolver()
{
    ASSERT(!m_resolver);
    ASSERT(m_shadowRoot);

    auto key = makeResolverSharingKey();

    auto result = documentScope().m_sharedShadowTreeResolvers.ensure(WTFMove(key), [&] {
        SetForScope isUpdatingStyleResolver { m_isUpdatingStyleResolver, true };

        m_resolver = Resolver::create(m_document);

        m_resolver->ruleSets().setUsesSharedUserStyle(!isForUserAgentShadowTree());
        m_resolver->appendAuthorStyleSheets(m_activeStyleSheets);

        return Ref { *m_resolver };
    });

    if (!result.isNewEntry) {
        m_resolver = result.iterator->value.ptr();
        m_resolver->setSharedBetweenShadowTrees();
    }
}

void Scope::unshareShadowTreeResolverBeforeMutation()
{
    ASSERT(m_shadowRoot);

    documentScope().m_sharedShadowTreeResolvers.remove(makeResolverSharingKey());
}

auto Scope::makeResolverSharingKey() -> ResolverSharingKey
{
    constexpr bool isNonEmptyHashTableValue = true;
    return {
        m_activeStyleSheets.map([&](auto& sheet) { return RefPtr { &sheet->contents() }; }),
        isForUserAgentShadowTree(),
        isNonEmptyHashTableValue
    };
}

void Scope::clearResolver()
{
    m_resolver = nullptr;
    customPropertyRegistry().clearRegisteredFromStylesheets();
}

void Scope::releaseMemory()
{
    if (!m_shadowRoot) {
        for (auto* descendantShadowRoot : m_document.inDocumentShadowRoots())
            descendantShadowRoot->styleScope().releaseMemory();
    }

#if ENABLE(CSS_SELECTOR_JIT)
    for (auto& sheet : m_activeStyleSheets) {
        sheet->contents().traverseRules([] (const StyleRuleBase& rule) {
            if (is<StyleRule>(rule))
                downcast<StyleRule>(rule).releaseCompiledSelectors();
            return false;
        });
    }
#endif
    clearResolver();

    m_sharedShadowTreeResolvers.clear();
}

Scope& Scope::forNode(Node& node)
{
    ASSERT(node.isConnected());
    auto* shadowRoot = node.containingShadowRoot();
    if (shadowRoot)
        return shadowRoot->styleScope();
    return node.document().styleScope();
}

const Scope& Scope::forNode(const Node& node)
{
    return forNode(const_cast<Node&>(node));
}

Scope* Scope::forOrdinal(Element& element, ScopeOrdinal ordinal)
{
    if (ordinal == ScopeOrdinal::Element)
        return &forNode(element);
    if (ordinal == ScopeOrdinal::Shadow) {
        auto* shadowRoot = element.shadowRoot();
        return shadowRoot ? &shadowRoot->styleScope() : nullptr;
    }
    if (ordinal <= ScopeOrdinal::ContainingHost) {
        auto* host = hostForScopeOrdinal(element, ordinal);
        return host ? &forNode(*host) : nullptr;
    }
    auto* slot = assignedSlotForScopeOrdinal(element, ordinal);
    return slot ? &forNode(*slot) : nullptr;
}

void Scope::setPreferredStylesheetSetName(const String& name)
{
    if (m_preferredStylesheetSetName == name)
        return;
    m_preferredStylesheetSetName = name;
    didChangeActiveStyleSheetCandidates();
}

void Scope::addPendingSheet(const Element& element)
{
    ASSERT(!hasPendingSheet(element));

    bool isInHead = ancestorsOfType<HTMLHeadElement>(element).first();

    LOG_WITH_STREAM(StyleSheets, stream << "Scope " << this << " addPendingSheet() " << element << " isInHead " << isInHead);

    if (isInHead)
        m_elementsInHeadWithPendingSheets.add(element);
    else
        m_elementsInBodyWithPendingSheets.add(element);
}

// This method is called whenever a top-level stylesheet has finished loading.
void Scope::removePendingSheet(const Element& element)
{
    ASSERT(hasPendingSheet(element));

    if (!m_elementsInHeadWithPendingSheets.remove(element))
        m_elementsInBodyWithPendingSheets.remove(element);

    didRemovePendingStylesheet();
}

void Scope::addPendingSheet(const ProcessingInstruction& processingInstruction)
{
    ASSERT(!m_processingInstructionsWithPendingSheets.contains(processingInstruction));

    m_processingInstructionsWithPendingSheets.add(processingInstruction);
}

void Scope::removePendingSheet(const ProcessingInstruction& processingInstruction)
{
    ASSERT(m_processingInstructionsWithPendingSheets.contains(processingInstruction));

    m_processingInstructionsWithPendingSheets.remove(processingInstruction);

    didRemovePendingStylesheet();
}

bool Scope::hasPendingSheets() const
{
    return hasPendingSheetsBeforeBody() || !m_elementsInBodyWithPendingSheets.computesEmpty();
}

bool Scope::hasPendingSheetsBeforeBody() const
{
    return !m_elementsInHeadWithPendingSheets.computesEmpty() || !m_processingInstructionsWithPendingSheets.computesEmpty();
}

bool Scope::hasPendingSheetsInBody() const
{
    return !m_elementsInBodyWithPendingSheets.computesEmpty();
}

void Scope::didRemovePendingStylesheet()
{
    if (hasPendingSheets())
        return;

    didChangeActiveStyleSheetCandidates();

    if (!m_shadowRoot)
        m_document.didRemoveAllPendingStylesheet();
}

bool Scope::hasPendingSheet(const Element& element) const
{
    return m_elementsInHeadWithPendingSheets.contains(element) || hasPendingSheetInBody(element);
}

bool Scope::hasPendingSheetInBody(const Element& element) const
{
    return m_elementsInBodyWithPendingSheets.contains(element);
}

bool Scope::hasPendingSheet(const ProcessingInstruction& processingInstruction) const
{
    return m_processingInstructionsWithPendingSheets.contains(processingInstruction);
}

void Scope::addStyleSheetCandidateNode(Node& node, bool createdByParser)
{
    if (!node.isConnected())
        return;
    
    // Until the <body> exists, we have no choice but to compare document positions,
    // since styles outside of the body and head continue to be shunted into the head
    // (and thus can shift to end up before dynamically added DOM content that is also
    // outside the body).
    if ((createdByParser && m_document.bodyOrFrameset()) || m_styleSheetCandidateNodes.isEmpty()) {
        m_styleSheetCandidateNodes.add(&node);
        return;
    }

    // Determine an appropriate insertion point.
    auto begin = m_styleSheetCandidateNodes.begin();
    auto end = m_styleSheetCandidateNodes.end();
    auto it = end;
    Node* followingNode = nullptr;
    do {
        --it;
        Node* n = *it;
        unsigned short position = n->compareDocumentPosition(node);
        if (position == Node::DOCUMENT_POSITION_FOLLOWING) {
            m_styleSheetCandidateNodes.insertBefore(followingNode, &node);
            return;
        }
        followingNode = n;
    } while (it != begin);

    LOG_WITH_STREAM(StyleSheets, stream << "Scope " << this << " addStyleSheetCandidateNode() " << node);

    m_styleSheetCandidateNodes.insertBefore(followingNode, &node);
}

void Scope::removeStyleSheetCandidateNode(Node& node)
{
    if (m_styleSheetCandidateNodes.remove(&node))
        didChangeActiveStyleSheetCandidates();
}

#if ENABLE(XSLT)
// FIXME: <https://webkit.org/b/178830> Remove XSLT relaed code from Style::Scope.
Vector<Ref<ProcessingInstruction>> Scope::collectXSLTransforms()
{
    Vector<Ref<ProcessingInstruction>> processingInstructions;
    for (auto& node : m_styleSheetCandidateNodes) {
        if (is<ProcessingInstruction>(*node) && downcast<ProcessingInstruction>(*node).isXSL())
            processingInstructions.append(downcast<ProcessingInstruction>(*node));
    }
    return processingInstructions;
}
#endif

auto Scope::collectActiveStyleSheets() -> ActiveStyleSheetCollection
{
    if (!m_document.settings().authorAndUserStylesEnabled())
        return { };

    LOG_WITH_STREAM(StyleSheets, stream << "Scope " << this << " collectActiveStyleSheets()");

    Vector<RefPtr<StyleSheet>> sheets;
    Vector<RefPtr<StyleSheet>> styleSheetsForStyleSheetsList;

    for (auto& node : m_styleSheetCandidateNodes) {
        RefPtr<StyleSheet> sheet;
        if (is<ProcessingInstruction>(*node)) {
            if (!downcast<ProcessingInstruction>(*node).isCSS())
                continue;
            // We don't support linking to embedded CSS stylesheets, see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
            sheet = downcast<ProcessingInstruction>(*node).sheet();
            if (sheet)
                styleSheetsForStyleSheetsList.append(sheet);
            LOG_WITH_STREAM(StyleSheets, stream << " adding sheet " << sheet << " from ProcessingInstruction node " << *node);
        } else if (is<HTMLLinkElement>(*node) || is<HTMLStyleElement>(*node) || is<SVGStyleElement>(*node)) {
            Element& element = downcast<Element>(*node);
            AtomString title = element.isInShadowTree() ? nullAtom() : element.attributeWithoutSynchronization(titleAttr);
            bool enabledViaScript = false;
            if (is<HTMLLinkElement>(element)) {
                // <LINK> element
                HTMLLinkElement& linkElement = downcast<HTMLLinkElement>(element);
                if (linkElement.isDisabled())
                    continue;
                enabledViaScript = linkElement.isEnabledViaScript();
                if (linkElement.styleSheetIsLoading()) {
                    // it is loading but we should still decide which style sheet set to use
                    if (!enabledViaScript && !title.isEmpty() && m_preferredStylesheetSetName.isEmpty()) {
                        if (!linkElement.attributeWithoutSynchronization(relAttr).contains("alternate"_s))
                            m_preferredStylesheetSetName = title;
                    }
                    continue;
                }
                if (!linkElement.sheet())
                    title = nullAtom();
            }
            // Get the current preferred styleset. This is the
            // set of sheets that will be enabled.
            if (is<SVGStyleElement>(element))
                sheet = downcast<SVGStyleElement>(element).sheet();
            else if (is<HTMLLinkElement>(element))
                sheet = downcast<HTMLLinkElement>(element).sheet();
            else
                sheet = downcast<HTMLStyleElement>(element).sheet();

            if (sheet)
                styleSheetsForStyleSheetsList.append(sheet);

            // Check to see if this sheet belongs to a styleset
            // (thus making it PREFERRED or ALTERNATE rather than
            // PERSISTENT).
            auto& rel = element.attributeWithoutSynchronization(relAttr);
            if (!enabledViaScript && sheet && !title.isEmpty()) {
                // Yes, we have a title.
                if (m_preferredStylesheetSetName.isEmpty()) {
                    // No preferred set has been established. If
                    // we are NOT an alternate sheet, then establish
                    // us as the preferred set. Otherwise, just ignore
                    // this sheet.
                    if (is<HTMLStyleElement>(element) || !rel.contains("alternate"_s))
                        m_preferredStylesheetSetName = title;
                }
                if (title != m_preferredStylesheetSetName)
                    sheet = nullptr;
            }

            if (rel.contains("alternate"_s) && title.isEmpty())
                sheet = nullptr;

            if (sheet)
                LOG_WITH_STREAM(StyleSheets, stream << " adding sheet " << sheet << " from " << *node);
        }
        if (sheet)
            sheets.append(WTFMove(sheet));
    }

    auto canActivateAdoptedStyleSheet = [&](auto& sheet) {
        if (sheet.disabled())
            return false;
        return sheet.title().isEmpty() || sheet.title() == m_preferredStylesheetSetName;
    };

    for (auto& adoptedStyleSheet : treeScope().adoptedStyleSheets()) {
        if (!canActivateAdoptedStyleSheet(*adoptedStyleSheet))
            continue;
        styleSheetsForStyleSheetsList.append(adoptedStyleSheet);
        sheets.append(adoptedStyleSheet);
    }

    return { WTFMove(sheets), WTFMove(styleSheetsForStyleSheetsList) };
}

Scope::StyleSheetChange Scope::analyzeStyleSheetChange(const Vector<RefPtr<CSSStyleSheet>>& newStylesheets)
{
    unsigned newStylesheetCount = newStylesheets.size();

    auto* resolver = resolverIfExists();
    if (!resolver)
        return { ResolverUpdateType::Reconstruct };

    if (resolver->isSharedBetweenShadowTrees())
        return { ResolverUpdateType::Reconstruct };

    // Find out which stylesheets are new.
    unsigned oldStylesheetCount = m_activeStyleSheets.size();
    if (newStylesheetCount < oldStylesheetCount)
        return { ResolverUpdateType::Reconstruct };

    Vector<StyleSheetContents*> addedSheets;
    unsigned newIndex = 0;
    for (unsigned oldIndex = 0; oldIndex < oldStylesheetCount; ++oldIndex) {
        if (newIndex >= newStylesheetCount)
            return { ResolverUpdateType::Reconstruct };

        while (m_activeStyleSheets[oldIndex] != newStylesheets[newIndex]) {
            addedSheets.append(&newStylesheets[newIndex]->contents());
            ++newIndex;
            if (newIndex == newStylesheetCount)
                return { ResolverUpdateType::Reconstruct };
        }
        ++newIndex;
    }
    bool hasInsertions = !addedSheets.isEmpty();
    while (newIndex < newStylesheetCount) {
        addedSheets.append(&newStylesheets[newIndex]->contents());
        ++newIndex;
    }

    // If all new sheets were added at the end of the list we can just add them to existing Resolver.
    // If there were insertions we need to re-add all the stylesheets so rules are ordered correctly.
    return { hasInsertions ? ResolverUpdateType::Reset : ResolverUpdateType::Additive, WTFMove(addedSheets) };
}

static void filterEnabledNonemptyCSSStyleSheets(Vector<RefPtr<CSSStyleSheet>>& result, const Vector<RefPtr<StyleSheet>>& sheets)
{
    for (auto& sheet : sheets) {
        if (!is<CSSStyleSheet>(*sheet))
            continue;
        CSSStyleSheet& styleSheet = downcast<CSSStyleSheet>(*sheet);
        if (styleSheet.isLoading())
            continue;
        if (styleSheet.disabled())
            continue;
        if (!styleSheet.length())
            continue;
        result.append(&styleSheet);
    }
}

void Scope::updateActiveStyleSheets(UpdateType updateType)
{
    ASSERT(!m_pendingUpdate);

    if (!m_document.hasLivingRenderTree())
        return;

    if (m_document.inStyleRecalc() || m_document.inRenderTreeUpdate()) {
        // Protect against deleting style resolver in the middle of a style resolution.
        // Crash stacks indicate we can get here when a resource load fails synchronously (for example due to content blocking).
        // FIXME: These kind of cases should be eliminated and this path replaced by an assert.
        m_pendingUpdate = UpdateType::ContentsOrInterpretation;
        m_document.scheduleFullStyleRebuild();
        return;
    }

    auto collection = collectActiveStyleSheets();

    Vector<RefPtr<CSSStyleSheet>> activeCSSStyleSheets;

    if (!isForUserAgentShadowTree()) {
        activeCSSStyleSheets.appendVector(m_document.extensionStyleSheets().injectedAuthorStyleSheets());
        activeCSSStyleSheets.appendVector(m_document.extensionStyleSheets().authorStyleSheetsForTesting());
    }

    filterEnabledNonemptyCSSStyleSheets(activeCSSStyleSheets, collection.activeStyleSheets);

    LOG_WITH_STREAM(StyleSheets, stream << "Scope::updateActiveStyleSheets for document " << m_document << " sheets " << activeCSSStyleSheets);

    auto styleSheetChange = StyleSheetChange { ResolverUpdateType::Reconstruct };
    if (updateType == UpdateType::ActiveSet)
        styleSheetChange = analyzeStyleSheetChange(activeCSSStyleSheets);

    updateResolver(activeCSSStyleSheets, styleSheetChange.resolverUpdateType);

    m_weakCopyOfActiveStyleSheetListForFastLookup.clear();
    m_activeStyleSheets.swap(activeCSSStyleSheets);
    m_styleSheetsForStyleSheetList.swap(collection.styleSheetsForStyleSheetList);

    InspectorInstrumentation::activeStyleSheetsUpdated(m_document);

    for (const auto& sheet : m_activeStyleSheets) {
        if (sheet->contents().usesStyleBasedEditability())
            m_usesStyleBasedEditability = true;
    }

    if (m_resolver && m_resolver->ruleSets().features().usesHasPseudoClass())
        m_usesHasPseudoClass = true;

    invalidateStyleAfterStyleSheetChange(styleSheetChange);
}

void Scope::invalidateStyleAfterStyleSheetChange(const StyleSheetChange& styleSheetChange)
{
    if (m_shadowRoot && !m_shadowRoot->isConnected())
        return;

    // If we are already parsing the body and so may have significant amount of elements, put some effort into trying to avoid style recalcs.
    bool invalidateAll = !m_document.bodyOrFrameset() || m_document.hasNodesWithNonFinalStyle() || m_document.hasNodesWithMissingStyle();

    if (styleSheetChange.resolverUpdateType == ResolverUpdateType::Reconstruct || invalidateAll) {
        Invalidator::invalidateAllStyle(*this);
        return;
    }

    Invalidator invalidator(styleSheetChange.addedSheets, m_resolver->mediaQueryEvaluator());
    invalidator.invalidateStyle(*this);
}

void Scope::updateResolver(Vector<RefPtr<CSSStyleSheet>>& activeStyleSheets, ResolverUpdateType updateType)
{
    if (updateType == ResolverUpdateType::Reconstruct) {
        clearResolver();
        return;
    }

    if (m_shadowRoot)
        unshareShadowTreeResolverBeforeMutation();

    SetForScope isUpdatingStyleResolver { m_isUpdatingStyleResolver, true };

    if (updateType == ResolverUpdateType::Reset) {
        customPropertyRegistry().clearRegisteredFromStylesheets();
        m_resolver->ruleSets().resetAuthorStyle();
        m_resolver->appendAuthorStyleSheets(activeStyleSheets);
        return;
    }

    ASSERT(updateType == ResolverUpdateType::Additive);
    ASSERT(activeStyleSheets.size() >= m_activeStyleSheets.size());

    unsigned firstNewIndex = m_activeStyleSheets.size();
    Vector<RefPtr<CSSStyleSheet>> newStyleSheets;
    newStyleSheets.appendRange(activeStyleSheets.begin() + firstNewIndex, activeStyleSheets.end());
    m_resolver->appendAuthorStyleSheets(newStyleSheets);
}

const Vector<RefPtr<CSSStyleSheet>> Scope::activeStyleSheetsForInspector()
{
    Vector<RefPtr<CSSStyleSheet>> result;

    if (auto* pageUserSheet = m_document.extensionStyleSheets().pageUserSheet())
        result.append(pageUserSheet);
    result.appendVector(m_document.extensionStyleSheets().documentUserStyleSheets());
    result.appendVector(m_document.extensionStyleSheets().injectedUserStyleSheets());
    result.appendVector(m_document.extensionStyleSheets().injectedAuthorStyleSheets());
    result.appendVector(m_document.extensionStyleSheets().authorStyleSheetsForTesting());

    for (auto& styleSheet : m_styleSheetsForStyleSheetList) {
        if (!is<CSSStyleSheet>(*styleSheet))
            continue;

        CSSStyleSheet& sheet = downcast<CSSStyleSheet>(*styleSheet);
        if (sheet.disabled())
            continue;

        result.append(&sheet);
    }

    return result;
}

bool Scope::activeStyleSheetsContains(const CSSStyleSheet* sheet) const
{
    if (m_activeStyleSheets.isEmpty())
        return false;

    if (m_weakCopyOfActiveStyleSheetListForFastLookup.isEmpty()) {
        for (auto& activeStyleSheet : m_activeStyleSheets)
            m_weakCopyOfActiveStyleSheetListForFastLookup.add(activeStyleSheet.get());
    }
    return m_weakCopyOfActiveStyleSheetListForFastLookup.contains(sheet);
}

void Scope::flushPendingSelfUpdate()
{
    ASSERT(m_pendingUpdate);

    auto updateType = *m_pendingUpdate;

    clearPendingUpdate();
    
    updateActiveStyleSheets(updateType);
}

void Scope::flushPendingDescendantUpdates()
{
    ASSERT(m_hasDescendantWithPendingUpdate);
    ASSERT(!m_shadowRoot);
    for (auto* descendantShadowRoot : m_document.inDocumentShadowRoots())
        descendantShadowRoot->styleScope().flushPendingUpdate();
    m_hasDescendantWithPendingUpdate = false;
}

void Scope::clearPendingUpdate()
{
    m_pendingUpdateTimer.stop();
    m_pendingUpdate = { };
}

TreeScope& Scope::treeScope()
{
    if (m_shadowRoot)
        return *m_shadowRoot;
    return m_document;
}

void Scope::scheduleUpdate(UpdateType update)
{
    if (update == UpdateType::ContentsOrInterpretation) {
        // :host and ::slotted rules might go away.
        if (m_shadowRoot) {
            Invalidator::invalidateHostAndSlottedStyleIfNeeded(*m_shadowRoot);
            unshareShadowTreeResolverBeforeMutation();
        }
        // FIXME: Animation code may trigger resource load in middle of style recalc and that can add a rule to a content extension stylesheet.
        //        Fix and remove isResolvingTreeStyle() test below, see https://bugs.webkit.org/show_bug.cgi?id=194335
        // FIXME: The m_isUpdatingStyleResolver test is here because extension stylesheets can get us here from Resolver::appendAuthorStyleSheets.
        if (!m_isUpdatingStyleResolver && !m_document.isResolvingTreeStyle())
            clearResolver();
    }

    if (!m_pendingUpdate || *m_pendingUpdate < update) {
        m_pendingUpdate = update;
        if (m_shadowRoot)
            documentScope().m_hasDescendantWithPendingUpdate = true;
    }

    if (m_pendingUpdateTimer.isActive())
        return;
    m_pendingUpdateTimer.startOneShot(0_s);
}

auto Scope::mediaQueryViewportStateForDocument(const Document& document) -> MediaQueryViewportState
{
    // These things affect evaluation of viewport dependent media queries.
    return { document.view()->layoutSize(), document.frame()->pageZoomFactor(), document.printing() };
}

void Scope::evaluateMediaQueriesForViewportChange()
{
    auto viewportState = mediaQueryViewportStateForDocument(m_document);

    if (m_viewportStateOnPreviousMediaQueryEvaluation && *m_viewportStateOnPreviousMediaQueryEvaluation == viewportState)
        return;
    // This doesn't need to be invalidated as any changes to the rules will compute their media queries to correct values.
    m_viewportStateOnPreviousMediaQueryEvaluation = viewportState;

    evaluateMediaQueries([] (Resolver& resolver) {
        return resolver.evaluateDynamicMediaQueries();
    });
}

void Scope::evaluateMediaQueriesForAccessibilitySettingsChange()
{
    evaluateMediaQueries([] (Resolver& resolver) {
        return resolver.evaluateDynamicMediaQueries();
    });
}

void Scope::evaluateMediaQueriesForAppearanceChange()
{
    evaluateMediaQueries([] (Resolver& resolver) {
        return resolver.evaluateDynamicMediaQueries();
    });
}

auto Scope::collectResolverScopes() -> ResolverScopes
{
    ASSERT(!m_shadowRoot);

    if (!resolverIfExists())
        return { };

    ResolverScopes resolverScopes;

    resolverScopes.add(*resolverIfExists(), Vector<WeakPtr<Scope>> { this });

    for (auto* shadowRoot : m_document.inDocumentShadowRoots()) {
        auto& scope = shadowRoot->styleScope();
        auto* resolver = scope.resolverIfExists();
        if (!resolver)
            continue;
        resolverScopes.add(*resolver, Vector<WeakPtr<Scope>> { }).iterator->value.append(&scope);
    }
    return resolverScopes;
}

template <typename TestFunction>
void Scope::evaluateMediaQueries(TestFunction&& testFunction)
{
    bool hadChanges = false;

    auto resolverScopes = collectResolverScopes();
    for (auto& [resolver, scopes] : resolverScopes) {
        auto evaluationChanges = testFunction(resolver.get());
        if (!evaluationChanges)
            continue;
        hadChanges = true;

        for (auto& scope : scopes) {
            switch (evaluationChanges->type) {
            case DynamicMediaQueryEvaluationChanges::Type::InvalidateStyle: {
                Invalidator invalidator(evaluationChanges->invalidationRuleSets);
                invalidator.invalidateStyle(*scope);
                break;
            }
            case DynamicMediaQueryEvaluationChanges::Type::ResetStyle:
                scope->scheduleUpdate(UpdateType::ContentsOrInterpretation);
                break;
            }
        }
    }

    if (hadChanges)
        InspectorInstrumentation::mediaQueryResultChanged(m_document);
}

void Scope::didChangeActiveStyleSheetCandidates()
{
    scheduleUpdate(UpdateType::ActiveSet);
}

void Scope::didChangeStyleSheetContents()
{
    scheduleUpdate(UpdateType::ContentsOrInterpretation);
}

void Scope::didChangeStyleSheetEnvironment()
{
    if (!m_shadowRoot) {
        m_sharedShadowTreeResolvers.clear();

        for (auto* descendantShadowRoot : m_document.inDocumentShadowRoots()) {
            // Stylesheets is author shadow roots are potentially affected.
            if (descendantShadowRoot->mode() != ShadowRootMode::UserAgent)
                descendantShadowRoot->styleScope().scheduleUpdate(UpdateType::ContentsOrInterpretation);
        }
    }
    scheduleUpdate(UpdateType::ContentsOrInterpretation);
}

void Scope::didChangeViewportSize()
{
    Ref<ContainerNode> rootNode = m_document;
    if (m_shadowRoot)
        rootNode = *m_shadowRoot;
    else {
        if (!m_document.hasStyleWithViewportUnits())
            return;

        for (auto* descendantShadowRoot : m_document.inDocumentShadowRoots()) {
            if (descendantShadowRoot->mode() == ShadowRootMode::UserAgent)
                continue;
            descendantShadowRoot->styleScope().didChangeViewportSize();
        }
    }

    auto* resolver = resolverIfExists();
    if (!resolver)
        return;
    resolver->clearCachedDeclarationsAffectedByViewportUnits();

    // FIXME: Ideally, we should save the list of elements that have viewport units and only iterate over those.
    for (RefPtr element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::nextIncludingPseudo(*element)) {
        auto* renderer = element->renderer();
        if (renderer && renderer->style().usesViewportUnits())
            element->invalidateStyle();
    }
}

void Scope::invalidateMatchedDeclarationsCache()
{
    if (!m_shadowRoot) {
        for (auto* descendantShadowRoot : m_document.inDocumentShadowRoots())
            descendantShadowRoot->styleScope().invalidateMatchedDeclarationsCache();
    }

    if (auto* resolver = resolverIfExists())
        resolver->invalidateMatchedDeclarationsCache();
}

void Scope::pendingUpdateTimerFired()
{
    flushPendingUpdate();
}

const Vector<RefPtr<StyleSheet>>& Scope::styleSheetsForStyleSheetList()
{
    // FIXME: StyleSheetList content should be updated separately from style resolver updates.
    flushPendingUpdate();
    return m_styleSheetsForStyleSheetList;
}

Scope& Scope::documentScope()
{
    return m_document.styleScope();
}

bool Scope::isForUserAgentShadowTree() const
{
    return m_shadowRoot && m_shadowRoot->mode() == ShadowRootMode::UserAgent;
}

bool Scope::updateQueryContainerState(QueryContainerUpdateContext& context)
{
    ASSERT(!m_shadowRoot);

    if (!m_document.renderView())
        return false;

    auto previousStates = WTFMove(m_queryContainerStates);
    m_queryContainerStates.clear();

    Vector<Element*> containersToInvalidate;

    for (auto& containerRenderer : m_document.renderView()->containerQueryBoxes()) {
        auto* containerElement = containerRenderer.element();
        if (!containerElement)
            continue;
        
        auto size = containerRenderer.logicalSize();

        auto sizeChanged = [&](LayoutSize oldSize) {
            switch (containerRenderer.style().containerType()) {
            case ContainerType::InlineSize:
                return size.width() != oldSize.width();
            case ContainerType::Size:
                return size != oldSize;
            case ContainerType::Normal:
                RELEASE_ASSERT_NOT_REACHED();
            }
            RELEASE_ASSERT_NOT_REACHED();
        };

        auto it = previousStates.find(*containerElement);
        bool changed = it == previousStates.end() || sizeChanged(it->value);
        // Protect against unstable layout by invalidating only once per container.
        if (changed && context.invalidatedContainers.add(containerElement).isNewEntry)
            containersToInvalidate.append(containerElement);
        m_queryContainerStates.add(*containerElement, size);
    }

    for (auto* toInvalidate : containersToInvalidate)
        toInvalidate->invalidateForQueryContainerSizeChange();

    return !containersToInvalidate.isEmpty();
}

HTMLSlotElement* assignedSlotForScopeOrdinal(const Element& element, ScopeOrdinal scopeOrdinal)
{
    ASSERT(scopeOrdinal >= ScopeOrdinal::FirstSlot);
    auto* slot = element.assignedSlot();
    for (auto scopeDepth = ScopeOrdinal::FirstSlot; slot && scopeDepth != scopeOrdinal; ++scopeDepth)
        slot = slot->assignedSlot();
    return slot;
}

Element* hostForScopeOrdinal(const Element& element, ScopeOrdinal scopeOrdinal)
{
    ASSERT(scopeOrdinal <= ScopeOrdinal::ContainingHost);
    auto* host = element.shadowHost();
    for (auto scopeDepth = ScopeOrdinal::ContainingHost; host && scopeDepth != scopeOrdinal; --scopeDepth)
        host = host->shadowHost();
    return host;
}

}
}
