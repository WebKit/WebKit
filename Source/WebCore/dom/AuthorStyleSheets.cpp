/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2009, 2011-2012, 2015 Apple Inc. All rights reserved.
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
#include "AuthorStyleSheets.h"

#include "CSSStyleSheet.h"
#include "Element.h"
#include "ExtensionStyleSheets.h"
#include "HTMLIFrameElement.h"
#include "HTMLLinkElement.h"
#include "HTMLStyleElement.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "PageGroup.h"
#include "ProcessingInstruction.h"
#include "SVGNames.h"
#include "SVGStyleElement.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleInvalidationAnalysis.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "UserStyleSheet.h"

namespace WebCore {

using namespace ContentExtensions;
using namespace HTMLNames;

AuthorStyleSheets::AuthorStyleSheets(Document& document)
    : m_document(document)
{
}

AuthorStyleSheets::AuthorStyleSheets(ShadowRoot& shadowRoot)
    : m_document(shadowRoot.documentScope())
    , m_shadowRoot(&shadowRoot)
{
}

// This method is called whenever a top-level stylesheet has finished loading.
void AuthorStyleSheets::removePendingSheet(RemovePendingSheetNotificationType notification)
{
    // Make sure we knew this sheet was pending, and that our count isn't out of sync.
    ASSERT(m_pendingStyleSheetCount > 0);

    m_pendingStyleSheetCount--;
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Stylesheet loaded at time %d. %d stylesheets still remain.\n", elapsedTime(), m_pendingStylesheets);
#endif

    if (m_pendingStyleSheetCount)
        return;

    if (notification == RemovePendingSheetNotifyLater) {
        m_document.setNeedsNotifyRemoveAllPendingStylesheet();
        return;
    }

    if (m_shadowRoot) {
        m_shadowRoot->updateStyle();
        return;
    }

    m_document.didRemoveAllPendingStylesheet();
}

void AuthorStyleSheets::addStyleSheetCandidateNode(Node& node, bool createdByParser)
{
    if (!node.inDocument())
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
    
    m_styleSheetCandidateNodes.insertBefore(followingNode, &node);
}

void AuthorStyleSheets::removeStyleSheetCandidateNode(Node& node)
{
    m_styleSheetCandidateNodes.remove(&node);
}

void AuthorStyleSheets::collectActiveStyleSheets(Vector<RefPtr<StyleSheet>>& sheets)
{
    if (m_document.settings() && !m_document.settings()->authorAndUserStylesEnabled())
        return;

    for (auto& node : m_styleSheetCandidateNodes) {
        StyleSheet* sheet = nullptr;
        if (is<ProcessingInstruction>(*node)) {
            // Processing instruction (XML documents only).
            // We don't support linking to embedded CSS stylesheets, see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
            ProcessingInstruction& pi = downcast<ProcessingInstruction>(*node);
            sheet = pi.sheet();
#if ENABLE(XSLT)
            // Don't apply XSL transforms to already transformed documents -- <rdar://problem/4132806>
            if (pi.isXSL() && !m_document.transformSourceDocument()) {
                // Don't apply XSL transforms until loading is finished.
                if (!m_document.parsing())
                    m_document.applyXSLTransform(&pi);
                return;
            }
#endif
        } else if (is<HTMLLinkElement>(*node) || is<HTMLStyleElement>(*node) || is<SVGStyleElement>(*node)) {
            Element& element = downcast<Element>(*node);
            AtomicString title = element.attributeWithoutSynchronization(titleAttr);
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
                        if (!linkElement.attributeWithoutSynchronization(relAttr).contains("alternate")) {
                            m_preferredStylesheetSetName = title;
                            m_selectedStylesheetSetName = title;
                        }
                    }
                    continue;
                }
                if (!linkElement.sheet())
                    title = nullAtom;
            }
            // Get the current preferred styleset. This is the
            // set of sheets that will be enabled.
            if (is<SVGStyleElement>(element))
                sheet = downcast<SVGStyleElement>(element).sheet();
            else if (is<HTMLLinkElement>(element))
                sheet = downcast<HTMLLinkElement>(element).sheet();
            else
                sheet = downcast<HTMLStyleElement>(element).sheet();
            // Check to see if this sheet belongs to a styleset
            // (thus making it PREFERRED or ALTERNATE rather than
            // PERSISTENT).
            auto& rel = element.attributeWithoutSynchronization(relAttr);
            if (!enabledViaScript && !title.isEmpty()) {
                // Yes, we have a title.
                if (m_preferredStylesheetSetName.isEmpty()) {
                    // No preferred set has been established. If
                    // we are NOT an alternate sheet, then establish
                    // us as the preferred set. Otherwise, just ignore
                    // this sheet.
                    if (is<HTMLStyleElement>(element) || !rel.contains("alternate"))
                        m_preferredStylesheetSetName = m_selectedStylesheetSetName = title;
                }
                if (title != m_preferredStylesheetSetName)
                    sheet = nullptr;
            }

            if (rel.contains("alternate") && title.isEmpty())
                sheet = nullptr;
        }
        if (sheet)
            sheets.append(sheet);
    }
}

AuthorStyleSheets::StyleResolverUpdateType AuthorStyleSheets::analyzeStyleSheetChange(UpdateFlag updateFlag, const Vector<RefPtr<CSSStyleSheet>>& newStylesheets, bool& requiresFullStyleRecalc)
{
    requiresFullStyleRecalc = true;
    
    // Stylesheets of <style> elements that @import stylesheets are active but loading. We need to trigger a full recalc when such loads are done.
    bool hasActiveLoadingStylesheet = false;
    unsigned newStylesheetCount = newStylesheets.size();
    for (auto& sheet : newStylesheets) {
        if (sheet->isLoading())
            hasActiveLoadingStylesheet = true;
    }
    if (m_hadActiveLoadingStylesheet && !hasActiveLoadingStylesheet) {
        m_hadActiveLoadingStylesheet = false;
        return Reconstruct;
    }
    m_hadActiveLoadingStylesheet = hasActiveLoadingStylesheet;

    if (updateFlag != OptimizedUpdate)
        return Reconstruct;
    if (!m_document.styleResolverIfExists())
        return Reconstruct;

    StyleResolver& styleResolver = *m_document.styleResolverIfExists();

    // Find out which stylesheets are new.
    unsigned oldStylesheetCount = m_activeStyleSheets.size();
    if (newStylesheetCount < oldStylesheetCount)
        return Reconstruct;

    Vector<StyleSheetContents*> addedSheets;
    unsigned newIndex = 0;
    for (unsigned oldIndex = 0; oldIndex < oldStylesheetCount; ++oldIndex) {
        if (newIndex >= newStylesheetCount)
            return Reconstruct;
        while (m_activeStyleSheets[oldIndex] != newStylesheets[newIndex]) {
            addedSheets.append(&newStylesheets[newIndex]->contents());
            ++newIndex;
            if (newIndex == newStylesheetCount)
                return Reconstruct;
        }
        ++newIndex;
    }
    bool hasInsertions = !addedSheets.isEmpty();
    while (newIndex < newStylesheetCount) {
        addedSheets.append(&newStylesheets[newIndex]->contents());
        ++newIndex;
    }
    // If all new sheets were added at the end of the list we can just add them to existing StyleResolver.
    // If there were insertions we need to re-add all the stylesheets so rules are ordered correctly.
    auto styleResolverUpdateType = hasInsertions ? Reset : Additive;

    // If we are already parsing the body and so may have significant amount of elements, put some effort into trying to avoid style recalcs.
    if (!m_document.bodyOrFrameset() || m_document.hasNodesWithPlaceholderStyle())
        return styleResolverUpdateType;

    StyleInvalidationAnalysis invalidationAnalysis(addedSheets, styleResolver.mediaQueryEvaluator());
    if (invalidationAnalysis.dirtiesAllStyle())
        return styleResolverUpdateType;
    invalidationAnalysis.invalidateStyle(m_document);
    requiresFullStyleRecalc = false;

    return styleResolverUpdateType;
}

static void filterEnabledNonemptyCSSStyleSheets(Vector<RefPtr<CSSStyleSheet>>& result, const Vector<RefPtr<StyleSheet>>& sheets)
{
    for (auto& sheet : sheets) {
        if (!is<CSSStyleSheet>(*sheet))
            continue;
        CSSStyleSheet& styleSheet = downcast<CSSStyleSheet>(*sheet);
        if (styleSheet.disabled())
            continue;
        if (!styleSheet.length())
            continue;
        result.append(&styleSheet);
    }
}

bool AuthorStyleSheets::updateActiveStyleSheets(UpdateFlag updateFlag)
{
    if (m_document.inStyleRecalc() || m_document.inRenderTreeUpdate()) {
        // Protect against deleting style resolver in the middle of a style resolution.
        // Crash stacks indicate we can get here when z resource load fails synchronously (for example due to content blocking).
        // FIXME: These kind of cases should be eliminated and this path replaced by an assert.
        m_pendingUpdateType = FullUpdate;
        m_document.scheduleForcedStyleRecalc();
        return false;

    }
    if (!m_document.hasLivingRenderTree())
        return false;

    Vector<RefPtr<StyleSheet>> activeStyleSheets;
    collectActiveStyleSheets(activeStyleSheets);

    Vector<RefPtr<CSSStyleSheet>> activeCSSStyleSheets;
    activeCSSStyleSheets.appendVector(m_document.extensionStyleSheets().injectedAuthorStyleSheets());
    activeCSSStyleSheets.appendVector(m_document.extensionStyleSheets().authorStyleSheetsForTesting());
    filterEnabledNonemptyCSSStyleSheets(activeCSSStyleSheets, activeStyleSheets);

    bool requiresFullStyleRecalc;
    auto styleResolverUpdateType = analyzeStyleSheetChange(updateFlag, activeCSSStyleSheets, requiresFullStyleRecalc);

    updateStyleResolver(activeCSSStyleSheets, styleResolverUpdateType);

    m_weakCopyOfActiveStyleSheetListForFastLookup = nullptr;
    m_activeStyleSheets.swap(activeCSSStyleSheets);
    m_styleSheetsForStyleSheetList.swap(activeStyleSheets);

    InspectorInstrumentation::activeStyleSheetsUpdated(m_document);

    for (const auto& sheet : m_activeStyleSheets) {
        if (sheet->contents().usesRemUnits())
            m_usesRemUnits = true;
        if (sheet->contents().usesStyleBasedEditability())
            m_usesStyleBasedEditability = true;
    }
    m_pendingUpdateType = NoUpdate;

    return requiresFullStyleRecalc;
}

void AuthorStyleSheets::updateStyleResolver(Vector<RefPtr<CSSStyleSheet>>& activeStyleSheets, StyleResolverUpdateType updateType)
{
    if (updateType == Reconstruct) {
        if (m_shadowRoot)
            m_shadowRoot->resetStyleResolver();
        else
            m_document.clearStyleResolver();
        return;
    }
    auto& styleResolver = m_document.ensureStyleResolver();
    auto& userAgentShadowTreeStyleResolver = m_document.userAgentShadowTreeStyleResolver();

    if (updateType == Reset) {
        styleResolver.ruleSets().resetAuthorStyle();
        styleResolver.appendAuthorStyleSheets(activeStyleSheets);
    } else {
        ASSERT(updateType == Additive);
        unsigned firstNewIndex = m_activeStyleSheets.size();
        Vector<RefPtr<CSSStyleSheet>> newStyleSheets;
        newStyleSheets.appendRange(activeStyleSheets.begin() + firstNewIndex, activeStyleSheets.end());
        styleResolver.appendAuthorStyleSheets(newStyleSheets);
    }

    userAgentShadowTreeStyleResolver.ruleSets().resetAuthorStyle();
    auto& authorRuleSet = styleResolver.ruleSets().authorStyle();
    if (authorRuleSet.hasShadowPseudoElementRules())
        userAgentShadowTreeStyleResolver.ruleSets().authorStyle().copyShadowPseudoElementRulesFrom(authorRuleSet);
}

const Vector<RefPtr<CSSStyleSheet>> AuthorStyleSheets::activeStyleSheetsForInspector() const
{
    Vector<RefPtr<CSSStyleSheet>> result;

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

bool AuthorStyleSheets::activeStyleSheetsContains(const CSSStyleSheet* sheet) const
{
    if (!m_weakCopyOfActiveStyleSheetListForFastLookup) {
        m_weakCopyOfActiveStyleSheetListForFastLookup = std::make_unique<HashSet<const CSSStyleSheet*>>();
        for (auto& activeStyleSheet : m_activeStyleSheets)
            m_weakCopyOfActiveStyleSheetListForFastLookup->add(activeStyleSheet.get());
    }
    return m_weakCopyOfActiveStyleSheetListForFastLookup->contains(sheet);
}

}
