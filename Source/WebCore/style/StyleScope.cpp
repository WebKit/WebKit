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

#include "CSSCounterStyleRegistry.h"
#include "CSSFontSelector.h"
#include "CSSStyleSheet.h"
#include "ContainerNodeInlines.h"
#include "DocumentInlines.h"
#include "DocumentView.h"
#include "Element.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "ExtensionStyleSheets.h"
#include "HTMLHeadElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLLinkElement.h"
#include "HTMLSlotElement.h"
#include "HTMLStyleElement.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "MatchResultCache.h"
#include "ProcessingInstruction.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "RuleSet.h"
#include "SVGElementTypeHelpers.h"
#include "SVGStyleElement.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleBuilder.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleInvalidator.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "UserStyleSheet.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Scope);

Scope::Scope(Document& document)
    : m_document(document)
    , m_pendingUpdateTimer(*this, &Scope::pendingUpdateTimerFired)
    , m_customPropertyRegistry(makeUniqueRef<CustomPropertyRegistry>(*this))
    , m_counterStyleRegistry(makeUniqueRef<CSSCounterStyleRegistry>())
{
}

Scope::Scope(ShadowRoot& shadowRoot)
    : m_document(shadowRoot.documentScope())
    , m_shadowRoot(&shadowRoot)
    , m_pendingUpdateTimer(*this, &Scope::pendingUpdateTimerFired)
    , m_customPropertyRegistry(makeUniqueRef<CustomPropertyRegistry>(*this))
    , m_counterStyleRegistry(makeUniqueRef<CSSCounterStyleRegistry>())
{
}

Scope::~Scope()
{
    ASSERT(!hasPendingSheets());
    weakPtrFactory().revokeAll();
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

Ref<Resolver> Scope::protectedResolver()
{
    return resolver();
}

void Scope::createDocumentResolver()
{
    ASSERT(!m_resolver);
    ASSERT(!m_shadowRoot);

    SetForScope isUpdatingStyleResolver { m_isUpdatingStyleResolver, true };

    m_resolver = Resolver::create(m_document, Resolver::ScopeType::Document);

    if (!m_dynamicViewTransitionsStyle)
        m_dynamicViewTransitionsStyle = RuleSet::create();

    m_resolver->ruleSets().setDynamicViewTransitionsStyle(m_dynamicViewTransitionsStyle.get());

    protect(m_document->fontSelector())->buildStarted();

    m_resolver->ruleSets().initializeUserStyle();
    m_resolver->addCurrentSVGFontFaceRules();
    m_resolver->appendAuthorStyleSheets(m_activeStyleSheets);

    protect(m_document->fontSelector())->buildCompleted();
}

void Scope::createOrFindSharedShadowTreeResolver()
{
    ASSERT(!m_resolver);
    ASSERT(m_shadowRoot);

    RELEASE_ASSERT(!m_isUpdatingStyleResolver);

    auto key = makeResolverSharingKey();

    auto result = documentScope().m_sharedShadowTreeResolvers.ensure(WTF::move(key), [&] {
        SetForScope isUpdatingStyleResolver { m_isUpdatingStyleResolver, true };

        m_resolver = Resolver::create(m_document, Resolver::ScopeType::ShadowTree);

        m_resolver->ruleSets().setUsesSharedUserStyle(!isForUserAgentShadowTree());
        m_resolver->appendAuthorStyleSheets(m_activeStyleSheets);

        return Ref { *m_resolver };
    });

    if (!result.isNewEntry) {
        m_resolver = result.iterator->value.ptr();
        m_resolver->setSharedBetweenShadowTrees();
        return;
    }

    static constexpr auto maximimumSharedResolverCount = 256;
    if (documentScope().m_sharedShadowTreeResolvers.size() > maximimumSharedResolverCount)
        documentScope().m_sharedShadowTreeResolvers.remove(documentScope().m_sharedShadowTreeResolvers.random());
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
    RELEASE_ASSERT(!m_isUpdatingStyleResolver);
    RELEASE_ASSERT(!m_document->isResolvingTreeStyle());

    m_resolver = nullptr;
    customPropertyRegistry().clearRegisteredFromStylesheets();
    counterStyleRegistry().clearAuthorCounterStyles();
}

void Scope::clearViewTransitionStyles()
{
    clearResolver();
    m_dynamicViewTransitionsStyle = nullptr;
}

void Scope::releaseMemory()
{
    if (!m_shadowRoot) {
        for (Ref descendantShadowRoot : m_document->inDocumentShadowRoots())
            const_cast<ShadowRoot&>(descendantShadowRoot.get()).styleScope().releaseMemory();
    }

#if ENABLE(CSS_SELECTOR_JIT)
    for (auto& sheet : m_activeStyleSheets) {
        sheet->contents().traverseRules([] (const StyleRuleBase& rule) {
            if (auto* styleRule = dynamicDowncast<StyleRule>(rule))
                styleRule->releaseCompiledSelectors();
            return false;
        });
    }
#endif
    clearResolver();

    m_sharedShadowTreeResolvers.clear();
    m_matchResultCache = { };
}

Scope& Scope::forNode(Node& node)
{
    ASSERT(node.isConnected());
    RefPtr shadowRoot = node.containingShadowRoot();
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
    if (CheckedPtr pseudoElement = dynamicDowncast<PseudoElement>(element))
        return forOrdinal(*pseudoElement->hostElement(), ordinal);

    if (ordinal == ScopeOrdinal::Element)
        return &forNode(element);
    if (ordinal == ScopeOrdinal::Shadow) {
        RefPtr shadowRoot = element.shadowRoot();
        return shadowRoot ? &shadowRoot->styleScope() : nullptr;
    }
    if (ordinal <= ScopeOrdinal::ContainingHost) {
        RefPtr host = hostForScopeOrdinal(element, ordinal);
        return host ? &forNode(*host) : nullptr;
    }
    RefPtr slot = assignedSlotForScopeOrdinal(element, ordinal);
    return slot ? &forNode(*slot) : nullptr;
}

const Scope* Scope::forOrdinal(const Element& element, ScopeOrdinal ordinal)
{
    return forOrdinal(const_cast<Element&>(element), ordinal);
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
    return hasPendingSheetsBeforeBody() || !m_elementsInBodyWithPendingSheets.isEmptyIgnoringNullReferences();
}

bool Scope::hasPendingSheetsBeforeBody() const
{
    return !m_elementsInHeadWithPendingSheets.isEmptyIgnoringNullReferences() || !m_processingInstructionsWithPendingSheets.isEmptyIgnoringNullReferences();
}

bool Scope::hasPendingSheetsInBody() const
{
    return !m_elementsInBodyWithPendingSheets.isEmptyIgnoringNullReferences();
}

void Scope::didRemovePendingStylesheet()
{
    if (hasPendingSheets())
        return;

    didChangeActiveStyleSheetCandidates();

    if (!m_shadowRoot)
        m_document->didRemoveAllPendingStylesheet();
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
    if ((createdByParser && m_document->bodyOrFrameset()) || m_styleSheetCandidateNodes.isEmptyIgnoringNullReferences()) {
        m_styleSheetCandidateNodes.add(node);
        return;
    }

    // Determine an appropriate insertion point.
    auto begin = m_styleSheetCandidateNodes.begin();
    auto end = m_styleSheetCandidateNodes.end();
    auto it = end;
    RefPtr<Node> followingNode;
    do {
        --it;
        Ref<Node> n = *it;
        unsigned short position = n->compareDocumentPosition(node);
        if (position == Node::DOCUMENT_POSITION_FOLLOWING) {
            if (followingNode)
                m_styleSheetCandidateNodes.insertBefore(*followingNode, node);
            else
                m_styleSheetCandidateNodes.appendOrMoveToLast(node);
            return;
        }
        followingNode = WTF::move(n);
    } while (it != begin);

    LOG_WITH_STREAM(StyleSheets, stream << "Scope " << this << " addStyleSheetCandidateNode() " << node);

    m_styleSheetCandidateNodes.insertBefore(*followingNode, node);
}

void Scope::removeStyleSheetCandidateNode(Node& node)
{
    if (m_styleSheetCandidateNodes.remove(node))
        didChangeActiveStyleSheetCandidates();
}

#if ENABLE(XSLT)
// FIXME: <https://webkit.org/b/178830> Remove XSLT relaed code from Style::Scope.
Vector<Ref<ProcessingInstruction>> Scope::collectXSLTransforms()
{
    Vector<Ref<ProcessingInstruction>> processingInstructions;
    for (Ref node : m_styleSheetCandidateNodes) {
        if (RefPtr processingInstruction = dynamicDowncast<ProcessingInstruction>(node); processingInstruction && processingInstruction->isXSL())
            processingInstructions.append(*processingInstruction);
    }
    return processingInstructions;
}
#endif

auto Scope::collectActiveStyleSheets() -> ActiveStyleSheetCollection
{
    if (!m_document->settings().authorAndUserStylesEnabled())
        return { };

    LOG_WITH_STREAM(StyleSheets, stream << "Scope " << this << " collectActiveStyleSheets()");

    Vector<Ref<StyleSheet>> sheets;
    Vector<Ref<StyleSheet>> styleSheetsForStyleSheetsList;

    for (Ref node : m_styleSheetCandidateNodes) {
        RefPtr<StyleSheet> sheet;
        if (RefPtr processingInstruction = dynamicDowncast<ProcessingInstruction>(node)) {
            if (!processingInstruction->isCSS())
                continue;
            // We don't support linking to embedded CSS stylesheets, see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
            sheet = processingInstruction->sheet();
            if (sheet)
                styleSheetsForStyleSheetsList.append(*sheet);
            LOG_WITH_STREAM(StyleSheets, stream << " adding sheet " << sheet << " from ProcessingInstruction node " << node);
        } else if (is<HTMLLinkElement>(node) || is<HTMLStyleElement>(node) || is<SVGStyleElement>(node)) {
            Ref element = uncheckedDowncast<Element>(node);
            AtomString title = element->isInShadowTree() ? nullAtom() : element->attributeWithoutSynchronization(titleAttr);
            bool enabledViaScript = false;
            if (RefPtr linkElement = dynamicDowncast<HTMLLinkElement>(element.get())) {
                // <LINK> element
                if (linkElement->isDisabled())
                    continue;
                enabledViaScript = linkElement->isEnabledViaScript();
                if (linkElement->styleSheetIsLoading()) {
                    // it is loading but we should still decide which style sheet set to use
                    if (!enabledViaScript && !title.isEmpty() && m_preferredStylesheetSetName.isEmpty()) {
                        if (!linkElement->attributeWithoutSynchronization(relAttr).contains("alternate"_s))
                            m_preferredStylesheetSetName = title;
                    }
                    continue;
                }
                if (!linkElement->sheet())
                    title = nullAtom();
            }
            // Get the current preferred styleset. This is the
            // set of sheets that will be enabled.
            if (auto* svgStyleElement = dynamicDowncast<SVGStyleElement>(element.get()))
                sheet = svgStyleElement->sheet();
            else if (auto* htmlLinkElement = dynamicDowncast<HTMLLinkElement>(element.get()))
                sheet = htmlLinkElement->sheet();
            else
                sheet = downcast<HTMLStyleElement>(element.get()).sheet();

            if (sheet)
                styleSheetsForStyleSheetsList.append(*sheet);

            // Check to see if this sheet belongs to a styleset
            // (thus making it PREFERRED or ALTERNATE rather than
            // PERSISTENT).
            auto& rel = element->attributeWithoutSynchronization(relAttr);
            if (!enabledViaScript && sheet && !title.isEmpty()) {
                // Yes, we have a title.
                if (m_preferredStylesheetSetName.isEmpty()) {
                    // No preferred set has been established. If
                    // we are NOT an alternate sheet, then establish
                    // us as the preferred set. Otherwise, just ignore
                    // this sheet.
                    if (is<HTMLStyleElement>(element.get()) || !rel.contains("alternate"_s))
                        m_preferredStylesheetSetName = title;
                }
                if (title != m_preferredStylesheetSetName)
                    sheet = nullptr;
            }

            if (rel.contains("alternate"_s) && title.isEmpty())
                sheet = nullptr;

            if (sheet)
                LOG_WITH_STREAM(StyleSheets, stream << " adding sheet " << sheet << " from " << node);
        }
        if (sheet)
            sheets.append(sheet.releaseNonNull());
    }

    auto canActivateAdoptedStyleSheet = [&](auto& sheet) {
        if (sheet.disabled())
            return false;
        return sheet.title().isEmpty() || sheet.title() == m_preferredStylesheetSetName;
    };

    for (auto& adoptedStyleSheet : treeScope().adoptedStyleSheets()) {
        if (!canActivateAdoptedStyleSheet(adoptedStyleSheet.get()))
            continue;
        styleSheetsForStyleSheetsList.append(adoptedStyleSheet.get());
        sheets.append(adoptedStyleSheet.get());
    }

    return { WTF::move(sheets), WTF::move(styleSheetsForStyleSheetsList) };
}

Scope::StyleSheetChange Scope::analyzeStyleSheetChange(const Vector<Ref<CSSStyleSheet>>& newStylesheets)
{
    unsigned newStylesheetCount = newStylesheets.size();

    RefPtr resolver = resolverIfExists();
    if (!resolver)
        return { ResolverUpdateType::Reconstruct };

    if (resolver->isSharedBetweenShadowTrees())
        return { ResolverUpdateType::Reconstruct };

    // Find out which stylesheets are new.
    unsigned oldStylesheetCount = m_activeStyleSheets.size();
    if (newStylesheetCount < oldStylesheetCount)
        return { ResolverUpdateType::Reconstruct };

    Vector<Ref<StyleSheetContents>> addedSheets;
    unsigned newIndex = 0;
    for (unsigned oldIndex = 0; oldIndex < oldStylesheetCount; ++oldIndex) {
        if (newIndex >= newStylesheetCount)
            return { ResolverUpdateType::Reconstruct };

        while (m_activeStyleSheets[oldIndex] != newStylesheets[newIndex]) {
            addedSheets.append(newStylesheets[newIndex]->contents());
            ++newIndex;
            if (newIndex == newStylesheetCount)
                return { ResolverUpdateType::Reconstruct };
        }
        ++newIndex;
    }
    bool hasInsertions = !addedSheets.isEmpty();
    while (newIndex < newStylesheetCount) {
        addedSheets.append(newStylesheets[newIndex]->contents());
        ++newIndex;
    }

    // If all new sheets were added at the end of the list we can just add them to existing Resolver.
    // If there were insertions we need to re-add all the stylesheets so rules are ordered correctly.
    return { hasInsertions ? ResolverUpdateType::Reset : ResolverUpdateType::Additive, WTF::move(addedSheets) };
}

static void filterEnabledNonemptyCSSStyleSheets(Vector<Ref<CSSStyleSheet>>& result, const Vector<Ref<StyleSheet>>& sheets)
{
    for (auto& sheet : sheets) {
        RefPtr styleSheet = dynamicDowncast<CSSStyleSheet>(sheet.get());
        if (!styleSheet)
            continue;
        if (styleSheet->isLoading())
            continue;
        if (styleSheet->disabled())
            continue;
        if (!styleSheet->length())
            continue;
        result.append(*styleSheet);
    }
}

void Scope::updateActiveStyleSheets(UpdateType updateType)
{
    ASSERT(!m_pendingUpdate);

    if (!m_document->hasLivingRenderTree())
        return;

    if (m_document->inStyleRecalc() || m_document->inRenderTreeUpdate()) {
        // Protect against deleting style resolver in the middle of a style resolution.
        // Crash stacks indicate we can get here when a resource load fails synchronously (for example due to content blocking).
        // FIXME: These kind of cases should be eliminated and this path replaced by an assert.
        m_pendingUpdate = UpdateType::ContentsOrInterpretation;
        m_document->scheduleFullStyleRebuild();
        return;
    }

    auto collection = collectActiveStyleSheets();

    Vector<Ref<CSSStyleSheet>> activeCSSStyleSheets;

    if (!isForUserAgentShadowTree()) {
        activeCSSStyleSheets.appendVector(m_document->extensionStyleSheets().injectedAuthorStyleSheets());
        activeCSSStyleSheets.appendVector(m_document->extensionStyleSheets().authorStyleSheetsForTesting());
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
    bool invalidateAll = !m_document->bodyOrFrameset() || m_document->hasNodesWithMissingStyle();

    if (styleSheetChange.resolverUpdateType == ResolverUpdateType::Reconstruct || invalidateAll) {
        Invalidator::invalidateAllStyle(*this);
        return;
    }

    Invalidator invalidator(styleSheetChange.addedSheets, m_resolver->mediaQueryEvaluator());
    invalidator.invalidateStyle(*this);
}

void Scope::updateResolver(std::span<const Ref<CSSStyleSheet>> activeStyleSheets, ResolverUpdateType updateType)
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
        counterStyleRegistry().clearAuthorCounterStyles();
        m_resolver->ruleSets().resetAuthorStyle();
        m_resolver->appendAuthorStyleSheets(activeStyleSheets);
        return;
    }

    ASSERT(updateType == ResolverUpdateType::Additive);
    ASSERT(activeStyleSheets.size() >= m_activeStyleSheets.size());

    auto firstNewIndex = m_activeStyleSheets.size();
    m_resolver->appendAuthorStyleSheets(activeStyleSheets.subspan(firstNewIndex));
}

const Vector<Ref<CSSStyleSheet>> Scope::activeStyleSheetsForInspector()
{
    Vector<Ref<CSSStyleSheet>> result;

    if (CheckedPtr extensionStyleSheets = m_document->extensionStyleSheetsIfExists()) {
        if (RefPtr pageUserSheet = extensionStyleSheets->pageUserSheet())
            result.append(*pageUserSheet);
        result.appendVector(extensionStyleSheets->documentUserStyleSheets());
        result.appendVector(extensionStyleSheets->injectedUserStyleSheets());
        result.appendVector(extensionStyleSheets->injectedAuthorStyleSheets());
        result.appendVector(extensionStyleSheets->authorStyleSheetsForTesting());
    }

    for (auto& styleSheet : m_styleSheetsForStyleSheetList) {
        RefPtr sheet = dynamicDowncast<CSSStyleSheet>(styleSheet.get());
        if (!sheet)
            continue;

        if (sheet->disabled())
            continue;

        result.append(*sheet);
    }

    return result;
}

bool Scope::activeStyleSheetsContains(const CSSStyleSheet& sheet) const
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
    for (Ref descendantShadowRoot : m_document->inDocumentShadowRoots())
        const_cast<ShadowRoot&>(descendantShadowRoot.get()).styleScope().flushPendingUpdate();
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

        clearResolver();

        m_matchResultCache = { };
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

    ResolverScopes resolverScopes;

    if (RefPtr resolver = resolverIfExists())
        resolverScopes.add(*resolver, Vector<WeakPtr<Scope>> { this });

    for (Ref shadowRoot : m_document->inDocumentShadowRoots()) {
        auto& scope = const_cast<ShadowRoot&>(shadowRoot.get()).styleScope();

        if (RefPtr resolver = scope.resolverIfExists())
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
    RELEASE_ASSERT(!m_isUpdatingStyleResolver);
    RELEASE_ASSERT(!m_document->isResolvingTreeStyle());

    if (!m_shadowRoot) {
        m_sharedShadowTreeResolvers.clear();

        for (Ref descendantShadowRoot : m_document->inDocumentShadowRoots())
            const_cast<ShadowRoot&>(descendantShadowRoot.get()).styleScope().scheduleUpdate(UpdateType::ContentsOrInterpretation);

        m_document->invalidateCachedCSSParserContext();
    }

    scheduleUpdate(UpdateType::ContentsOrInterpretation);
}

void Scope::didChangeExtensionStyleSheets()
{
    ASSERT(!m_shadowRoot);

    // Extension stylesheets may mutate in the middle of a style update when resource loading triggers
    // content extension processing. In this case we schedule an asyncronous full stylesheet update.
    // FIXME: We should defer all resource loading after style resolution completes.
    for (Ref descendantShadowRoot : m_document->inDocumentShadowRoots())
        const_cast<ShadowRoot&>(descendantShadowRoot.get()).styleScope().scheduleUpdate(UpdateType::FullForExtensionStyleSheets);

    scheduleUpdate(UpdateType::FullForExtensionStyleSheets);
}

void Scope::didChangeViewportSize()
{
    Ref<ContainerNode> rootNode = m_document;
    if (m_shadowRoot)
        rootNode = *m_shadowRoot;
    else {
        if (!m_document->hasStyleWithViewportUnits())
            return;

        for (Ref descendantShadowRoot : m_document->inDocumentShadowRoots()) {
            if (descendantShadowRoot->mode() == ShadowRootMode::UserAgent)
                continue;
            const_cast<ShadowRoot&>(descendantShadowRoot.get()).styleScope().didChangeViewportSize();
        }
    }

    RefPtr resolver = resolverIfExists();
    if (!resolver)
        return;
    resolver->clearCachedDeclarationsAffectedByViewportUnits();

    if (customPropertyRegistry().invalidatePropertiesWithViewportUnits(m_document)) {
        if (!m_shadowRoot) {
            if (RefPtr element = m_document->documentElement())
                element->invalidateStyleForSubtree();
        }
        return;
    }

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
        for (Ref descendantShadowRoot : m_document->inDocumentShadowRoots())
            const_cast<ShadowRoot&>(descendantShadowRoot.get()).styleScope().invalidateMatchedDeclarationsCache();
    }

    if (RefPtr resolver = resolverIfExists())
        resolver->invalidateMatchedDeclarationsCache();
}

void Scope::pendingUpdateTimerFired()
{
    RefPtr protectedShadowRoot { m_shadowRoot };
    Ref protectedDocument { m_document.get() };
    flushPendingUpdate();
}

const Vector<Ref<StyleSheet>>& Scope::styleSheetsForStyleSheetList()
{
    // FIXME: StyleSheetList content should be updated separately from style resolver updates.
    flushPendingUpdate();
    return m_styleSheetsForStyleSheetList;
}

Scope& Scope::documentScope()
{
    return m_document->styleScope();
}

bool Scope::isForUserAgentShadowTree() const
{
    return m_shadowRoot && m_shadowRoot->mode() == ShadowRootMode::UserAgent;
}

bool Scope::invalidateForLayoutDependencies(LayoutDependencyUpdateContext& context)
{
    auto didInvalidate = false;
    didInvalidate |= invalidateForContainerDependencies(context);
    didInvalidate |= invalidateForAnchorDependencies(context);
    didInvalidate |= invalidateForPositionTryFallbacks(context);
    return didInvalidate;
}

bool Scope::invalidateForContainerDependencies(LayoutDependencyUpdateContext& context)
{
    ASSERT(!m_shadowRoot);

    if (!m_document->renderView())
        return false;

    auto previousQueryContainerDimensions = WTF::move(m_queryContainerDimensionsOnLastUpdate);
    m_queryContainerDimensionsOnLastUpdate.clear();

    Vector<CheckedPtr<Element>> containersToInvalidate;

    for (auto& containerRenderer : m_document->renderView()->containerQueryBoxes()) {
        CheckedPtr containerElement = containerRenderer.element();

        // Invalidation uses real elements, replace ::before/::after with its host.
        if (auto* pseudoElement = dynamicDowncast<PseudoElement>(containerElement.get()))
            containerElement = pseudoElement->hostElement();

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

        auto it = previousQueryContainerDimensions.find(*containerElement);
        bool changed = it == previousQueryContainerDimensions.end() || sizeChanged(it->value);
        // Protect against unstable layout by invalidating only once per container.
        if (changed && context.invalidatedContainers.add(*containerElement).isNewEntry)
            containersToInvalidate.append(containerElement);
        m_queryContainerDimensionsOnLastUpdate.add(*containerElement, size);
    }

    for (auto& toInvalidate : containersToInvalidate)
        toInvalidate->invalidateForQueryContainerSizeChange();

    return !containersToInvalidate.isEmpty();
}

bool Scope::invalidateForAnchorDependencies(LayoutDependencyUpdateContext& context)
{
    ASSERT(!m_shadowRoot);

    if (!m_document->renderView())
        return false;

    auto previousAnchorPositions = WTF::move(m_anchorPositionsOnLastUpdate);
    m_anchorPositionsOnLastUpdate.clear();

    Vector<CheckedRef<Element>> anchoredElementsToInvalidate;

    if (m_document->renderView()->anchors().isEmptyIgnoringNullReferences())
        return false;

    auto anchorMap = AnchorPositionEvaluator::makeAnchorPositionedForAnchorMap(m_anchorPositionedToAnchorMap);

    auto makeAnchorPosition = [&](const RenderBoxModelObject& anchorRenderer) {
        AnchorPosition result;
        result.absoluteRect = anchorRenderer.absoluteBoundingBoxRectIgnoringTransforms();
        // Include containing block sizes as anchor function insets may be computed against any side and if they change
        // we need to invalidate.
        for (auto* containingBlock = anchorRenderer.containingBlock(); containingBlock; containingBlock = containingBlock->containingBlock()) {
            if (containingBlock->canContainAbsolutelyPositionedObjects())
                result.containingBlockSizes.append(containingBlock->contentBoxSize());
        }
        return result;
    };

    for (auto& anchorRenderer : m_document->renderView()->anchors()) {
        auto anchorPosition = makeAnchorPosition(anchorRenderer);
        m_anchorPositionsOnLastUpdate.add(anchorRenderer, anchorPosition);

        auto it = previousAnchorPositions.find(anchorRenderer);
        bool changed = it == previousAnchorPositions.end() || it->value != anchorPosition;
        if (!changed)
            continue;

        auto anchoredElements = anchorMap.getOptional(anchorRenderer);
        if (!anchoredElements)
            continue;

        for (auto& anchoredElement : *anchoredElements) {
            if (!context.invalidatedAnchorPositioned.add(anchoredElement.get()).isNewEntry)
                continue;
            anchoredElementsToInvalidate.append(anchoredElement);
        }
    }

    for (auto& toInvalidate : anchoredElementsToInvalidate) {
        CheckedPtr renderer = toInvalidate->renderer();
        if (renderer && AnchorPositionEvaluator::isLayoutTimeAnchorPositioned(renderer->style()))
            renderer->setNeedsLayout();
        toInvalidate->invalidateForAnchorRectChange();
    }

    return !anchoredElementsToInvalidate.isEmpty();
}

bool Scope::invalidateForPositionTryFallbacks(LayoutDependencyUpdateContext& context)
{
    ASSERT(!m_shadowRoot);

    if (!m_document->renderView())
        return false;

    bool invalidated = false;

    for (auto& box : m_document->renderView()->positionTryBoxes()) {
        if (!AnchorPositionEvaluator::overflowsInsetModifiedContainingBlock(box))
            continue;

        CheckedPtr element = box.element();
        if (auto* pseudoElement = dynamicDowncast<PseudoElement>(element.get()))
            element = pseudoElement->hostElement();

        if (element) {
            if (!context.invalidatedAnchorPositioned.add(*element).isNewEntry)
                continue;
            element->invalidateForAnchorRectChange();
            invalidated = true;
        }
    }

    return invalidated;
}

MatchResultCache& Scope::matchResultCache()
{
    ASSERT(!m_shadowRoot);

    if (!m_matchResultCache)
        m_matchResultCache = makeUnique<MatchResultCache>();
    return *m_matchResultCache;
}

RefPtr<HTMLSlotElement> assignedSlotForScopeOrdinal(const Element& element, ScopeOrdinal scopeOrdinal)
{
    ASSERT(scopeOrdinal >= ScopeOrdinal::FirstSlot);
    RefPtr slot = element.assignedSlot();
    for (auto scopeDepth = ScopeOrdinal::FirstSlot; slot && scopeDepth != scopeOrdinal; ++scopeDepth)
        slot = slot->assignedSlot();
    return slot;
}

RefPtr<Element> hostForScopeOrdinal(const Element& element, ScopeOrdinal scopeOrdinal)
{
    ASSERT(scopeOrdinal <= ScopeOrdinal::ContainingHost);
    RefPtr host = element.shadowHost();
    for (auto scopeDepth = ScopeOrdinal::ContainingHost; host && scopeDepth != scopeOrdinal; --scopeDepth)
        host = host->shadowHost();
    return host;
}

CheckedPtr<const Scope> Scope::hostScope() const
{
    if (!m_shadowRoot || !m_shadowRoot->host())
        return nullptr;
    return &forNode(*m_shadowRoot->host());
}

void Scope::updateAnchorPositioningStateAfterStyleResolution()
{
    if (CheckedPtr renderView = m_document->renderView())
        AnchorPositionEvaluator::updateScrollAdjustments(*renderView); // Is this necessary? Or will the combination of layout and scroll invalidation handle it sufficiently?

    m_anchorPositionedToAnchorMap.removeIf([](auto& elementAndState) {
        return elementAndState.value.anchors.isEmpty();
    });
}

std::optional<size_t> Scope::lastSuccessfulPositionOptionIndexFor(const Styleable& styleable)
{
    AnchorPositionedKey key { styleable.element, styleable.pseudoElementIdentifier };
    return m_lastSuccessfulPositionOptionIndexes.getOptional(key);
}

void Scope::setLastSuccessfulPositionOptionIndexMap(HashMap<AnchorPositionedKey, size_t>&& map)
{
    m_lastSuccessfulPositionOptionIndexes = WTF::move(map);
}

void Scope::forgetLastSuccessfulPositionOptionIndex(const Styleable& styleable)
{
    AnchorPositionedKey key { styleable.element, styleable.pseudoElementIdentifier };
    m_lastSuccessfulPositionOptionIndexes.remove(key);
}

}
}
