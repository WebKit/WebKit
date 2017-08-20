/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderTreeUpdaterGeneratedContent.h"

#include "ContentData.h"
#include "InspectorInstrumentation.h"
#include "PseudoElement.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderQuote.h"
#include "RenderTreeUpdater.h"
#include "RenderTreeUpdaterListItem.h"
#include "StyleTreeResolver.h"

namespace WebCore {

RenderTreeUpdater::GeneratedContent::GeneratedContent(RenderTreeUpdater& updater)
    : m_updater(updater)
{
}

void RenderTreeUpdater::GeneratedContent::updateBeforePseudoElement(Element& element)
{
    updatePseudoElement(element, BEFORE);
}

void RenderTreeUpdater::GeneratedContent::updateAfterPseudoElement(Element& element)
{
    updatePseudoElement(element, AFTER);
}

void RenderTreeUpdater::GeneratedContent::updateRemainingQuotes()
{
    if (!m_updater.renderView().hasQuotesNeedingUpdate())
        return;
    updateQuotesUpTo(nullptr);
    m_previousUpdatedQuote = nullptr;
    m_updater.renderView().setHasQuotesNeedingUpdate(false);
}

void RenderTreeUpdater::GeneratedContent::updateQuotesUpTo(RenderQuote* lastQuote)
{
    auto quoteRenderers = descendantsOfType<RenderQuote>(m_updater.renderView());
    auto it = m_previousUpdatedQuote ? ++quoteRenderers.at(*m_previousUpdatedQuote) : quoteRenderers.begin();
    auto end = quoteRenderers.end();
    for (; it != end; ++it) {
        auto& quote = *it;
        // Quote character depends on quote depth so we chain the updates.
        quote.updateRenderer(m_previousUpdatedQuote);
        m_previousUpdatedQuote = &quote;
        if (&quote == lastQuote)
            return;
    }
    ASSERT(!lastQuote);
}

static void createContentRenderers(RenderElement& renderer)
{
    auto& style = renderer.style();
    if (style.hasFlowFrom())
        return;

    ASSERT(style.contentData());

    for (const ContentData* content = style.contentData(); content; content = content->next()) {
        auto child = content->createContentRenderer(renderer.document(), style);
        if (renderer.isChildAllowed(*child, style))
            renderer.addChild(child.leakPtr());
    }
}

static void updateStyleForContentRenderers(RenderElement& renderer)
{
    for (auto* child = renderer.nextInPreOrder(&renderer); child; child = child->nextInPreOrder(&renderer)) {
        // We only manage the style for the generated content which must be images or text.
        if (!is<RenderImage>(*child) && !is<RenderQuote>(*child))
            continue;
        auto createdStyle = RenderStyle::createStyleInheritingFromPseudoStyle(renderer.style());
        downcast<RenderElement>(*child).setStyle(WTFMove(createdStyle));
    }
}

void RenderTreeUpdater::GeneratedContent::updatePseudoElement(Element& current, PseudoId pseudoId)
{
    PseudoElement* pseudoElement = pseudoId == BEFORE ? current.beforePseudoElement() : current.afterPseudoElement();

    if (auto* renderer = pseudoElement ? pseudoElement->renderer() : nullptr)
        m_updater.renderTreePosition().invalidateNextSibling(*renderer);

    if (!needsPseudoElement(current, pseudoId)) {
        if (pseudoElement) {
            if (pseudoId == BEFORE)
                current.clearBeforePseudoElement();
            else
                current.clearAfterPseudoElement();
        }
        return;
    }

    RefPtr<PseudoElement> newPseudoElement;
    if (!pseudoElement) {
        newPseudoElement = PseudoElement::create(current, pseudoId);
        pseudoElement = newPseudoElement.get();
    }

    auto newStyle = RenderStyle::clonePtr(*current.renderer()->getCachedPseudoStyle(pseudoId, &current.renderer()->style()));

    auto elementUpdate = Style::TreeResolver::createAnimatedElementUpdate(WTFMove(newStyle), *pseudoElement, Style::NoChange);

    if (elementUpdate.change == Style::NoChange)
        return;

    if (newPseudoElement) {
        InspectorInstrumentation::pseudoElementCreated(m_updater.m_document.page(), *newPseudoElement);
        if (pseudoId == BEFORE)
            current.setBeforePseudoElement(newPseudoElement.releaseNonNull());
        else
            current.setAfterPseudoElement(newPseudoElement.releaseNonNull());
    }

    m_updater.updateElementRenderer(*pseudoElement, elementUpdate);

    auto* pseudoRenderer = pseudoElement->renderer();
    if (!pseudoRenderer)
        return;

    if (elementUpdate.change == Style::Detach)
        createContentRenderers(*pseudoRenderer);
    else
        updateStyleForContentRenderers(*pseudoRenderer);

    if (m_updater.renderView().hasQuotesNeedingUpdate()) {
        for (auto& child : descendantsOfType<RenderQuote>(*pseudoRenderer))
            updateQuotesUpTo(&child);
    }
    if (is<RenderListItem>(*pseudoRenderer))
        ListItem::updateMarker(downcast<RenderListItem>(*pseudoRenderer));
}

bool RenderTreeUpdater::GeneratedContent::needsPseudoElement(Element& current, PseudoId pseudoId)
{
    if (!current.renderer() || !current.renderer()->canHaveGeneratedChildren())
        return false;
    if (current.isPseudoElement())
        return false;
    if (!pseudoElementRendererIsNeeded(current.renderer()->getCachedPseudoStyle(pseudoId)))
        return false;
    return true;
}

}
