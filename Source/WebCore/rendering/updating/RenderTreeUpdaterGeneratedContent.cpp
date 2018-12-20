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
#include "StyleTreeResolver.h"

namespace WebCore {

RenderTreeUpdater::GeneratedContent::GeneratedContent(RenderTreeUpdater& updater)
    : m_updater(updater)
{
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
        quote.updateRenderer(m_updater.m_builder, m_previousUpdatedQuote.get());
        m_previousUpdatedQuote = makeWeakPtr(quote);
        if (&quote == lastQuote)
            return;
    }
    ASSERT(!lastQuote);
}

static void createContentRenderers(RenderTreeBuilder& builder, RenderElement& pseudoRenderer, const RenderStyle& style)
{
    ASSERT(style.contentData());

    for (const ContentData* content = style.contentData(); content; content = content->next()) {
        auto child = content->createContentRenderer(pseudoRenderer.document(), style);
        if (pseudoRenderer.isChildAllowed(*child, style))
            builder.attach(pseudoRenderer, WTFMove(child));
    }
}

static void updateStyleForContentRenderers(RenderElement& pseudoRenderer, const RenderStyle& style)
{
    for (auto& contentRenderer : descendantsOfType<RenderElement>(pseudoRenderer)) {
        // We only manage the style for the generated content which must be images or text.
        if (!is<RenderImage>(contentRenderer) && !is<RenderQuote>(contentRenderer))
            continue;
        contentRenderer.setStyle(RenderStyle::createStyleInheritingFromPseudoStyle(style));
    }
}

void RenderTreeUpdater::GeneratedContent::updatePseudoElement(Element& current, const Optional<Style::ElementUpdate>& update, PseudoId pseudoId)
{
    PseudoElement* pseudoElement = pseudoId == PseudoId::Before ? current.beforePseudoElement() : current.afterPseudoElement();

    if (auto* renderer = pseudoElement ? pseudoElement->renderer() : nullptr)
        m_updater.renderTreePosition().invalidateNextSibling(*renderer);

    if (!needsPseudoElement(update)) {
        if (pseudoElement) {
            if (pseudoId == PseudoId::Before)
                removeBeforePseudoElement(current, m_updater.m_builder);
            else
                removeAfterPseudoElement(current, m_updater.m_builder);
        }
        return;
    }

    RefPtr<PseudoElement> newPseudoElement;
    if (!pseudoElement) {
        newPseudoElement = PseudoElement::create(current, pseudoId);
        pseudoElement = newPseudoElement.get();
    }

    if (update->change == Style::NoChange)
        return;

    if (newPseudoElement) {
        if (pseudoId == PseudoId::Before)
            current.setBeforePseudoElement(newPseudoElement.releaseNonNull());
        else
            current.setAfterPseudoElement(newPseudoElement.releaseNonNull());
    }

    if (update->style->display() == DisplayType::Contents) {
        // For display:contents we create an inline wrapper that inherits its
        // style from the display:contents style.
        auto contentsStyle = RenderStyle::createPtr();
        contentsStyle->setStyleType(pseudoId);
        contentsStyle->inheritFrom(*update->style);
        contentsStyle->copyContentFrom(*update->style);

        Style::ElementUpdate contentsUpdate { WTFMove(contentsStyle), update->change, update->recompositeLayer };
        m_updater.updateElementRenderer(*pseudoElement, contentsUpdate);
        pseudoElement->storeDisplayContentsStyle(RenderStyle::clonePtr(*update->style));
    } else {
        m_updater.updateElementRenderer(*pseudoElement, *update);
        ASSERT(!pseudoElement->hasDisplayContents());
    }

    auto* pseudoElementRenderer = pseudoElement->renderer();
    if (!pseudoElementRenderer)
        return;

    if (update->change == Style::Detach)
        createContentRenderers(m_updater.m_builder, *pseudoElementRenderer, *update->style);
    else
        updateStyleForContentRenderers(*pseudoElementRenderer, *update->style);

    if (m_updater.renderView().hasQuotesNeedingUpdate()) {
        for (auto& child : descendantsOfType<RenderQuote>(*pseudoElementRenderer))
            updateQuotesUpTo(&child);
    }
    m_updater.m_builder.updateAfterDescendants(*pseudoElementRenderer);
}

bool RenderTreeUpdater::GeneratedContent::needsPseudoElement(const Optional<Style::ElementUpdate>& update)
{
    if (!update)
        return false;
    if (!m_updater.renderTreePosition().parent().canHaveGeneratedChildren())
        return false;
    if (!pseudoElementRendererIsNeeded(update->style.get()))
        return false;
    return true;
}

void RenderTreeUpdater::GeneratedContent::removeBeforePseudoElement(Element& element, RenderTreeBuilder& builder)
{
    auto* pseudoElement = element.beforePseudoElement();
    if (!pseudoElement)
        return;
    tearDownRenderers(*pseudoElement, TeardownType::Full, builder);
    element.clearBeforePseudoElement();
}

void RenderTreeUpdater::GeneratedContent::removeAfterPseudoElement(Element& element, RenderTreeBuilder& builder)
{
    auto* pseudoElement = element.afterPseudoElement();
    if (!pseudoElement)
        return;
    tearDownRenderers(*pseudoElement, TeardownType::Full, builder);
    element.clearAfterPseudoElement();
}

}
