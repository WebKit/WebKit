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
#include "RenderCounter.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderQuote.h"
#include "RenderStyleInlines.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
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
        m_previousUpdatedQuote = quote;
        if (&quote == lastQuote)
            return;
    }
    ASSERT(!lastQuote || m_updater.m_builder.hasBrokenContinuation());
}

void RenderTreeUpdater::GeneratedContent::updateCounters()
{
    auto update = [&] {
        auto counters = m_updater.renderView().takeCountersNeedingUpdate();
        for (auto& counter : counters)
            counter.updateCounter();
    };
    // Update twice and hope it stabilizes.
    update();
    update();
}

static bool elementIsTargetedByKeyframeEffectRequiringPseudoElement(const Element* element, PseudoId pseudoId)
{
    if (auto* pseudoElement = dynamicDowncast<PseudoElement>(element))
        return elementIsTargetedByKeyframeEffectRequiringPseudoElement(pseudoElement->hostElement(), pseudoId);

    if (element) {
        if (auto* stack = element->keyframeEffectStack(pseudoId))
            return stack->requiresPseudoElement();
    }

    return false;
}

static void createContentRenderers(RenderTreeBuilder& builder, RenderElement& pseudoRenderer, const RenderStyle& style, PseudoId pseudoId)
{
    if (auto* contentData = style.contentData()) {
        for (const ContentData* content = contentData; content; content = content->next()) {
            auto child = content->createContentRenderer(pseudoRenderer.document(), style);
            if (pseudoRenderer.isChildAllowed(*child, style))
                builder.attach(pseudoRenderer, WTFMove(child));
        }
    } else {
        // The only valid scenario where this method is called without the "content" property being set
        // is the case where a pseudo-element has animations set on it via the Web Animations API.
        ASSERT_UNUSED(pseudoId, elementIsTargetedByKeyframeEffectRequiringPseudoElement(pseudoRenderer.element(), pseudoId));
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

void RenderTreeUpdater::GeneratedContent::updatePseudoElement(Element& current, const Style::ElementUpdate& elementUpdate, PseudoId pseudoId)
{
    PseudoElement* pseudoElement = pseudoId == PseudoId::Before ? current.beforePseudoElement() : current.afterPseudoElement();

    if (auto* renderer = pseudoElement ? pseudoElement->renderer() : nullptr)
        m_updater.renderTreePosition().invalidateNextSibling(*renderer);

    auto* updateStyle = elementUpdate.style ? elementUpdate.style->getCachedPseudoStyle(pseudoId) : nullptr;

    if (!needsPseudoElement(updateStyle) && !elementIsTargetedByKeyframeEffectRequiringPseudoElement(&current, pseudoId)) {
        if (pseudoElement) {
            if (pseudoId == PseudoId::Before)
                removeBeforePseudoElement(current, m_updater.m_builder);
            else
                removeAfterPseudoElement(current, m_updater.m_builder);
        }
        return;
    }

    if (!updateStyle)
        return;

    auto* existingStyle = pseudoElement ? pseudoElement->renderOrDisplayContentsStyle() : nullptr;

    auto styleChange = existingStyle ? Style::determineChange(*updateStyle, *existingStyle) : Style::Change::Renderer;
    if (styleChange == Style::Change::None)
        return;

    pseudoElement = &current.ensurePseudoElement(pseudoId);

    if (updateStyle->display() == DisplayType::Contents) {
        // For display:contents we create an inline wrapper that inherits its
        // style from the display:contents style.
        auto contentsStyle = RenderStyle::createPtr();
        contentsStyle->setPseudoElementType(pseudoId);
        contentsStyle->inheritFrom(*updateStyle);
        contentsStyle->copyContentFrom(*updateStyle);
        contentsStyle->copyPseudoElementsFrom(*updateStyle);

        Style::ElementUpdate contentsUpdate { WTFMove(contentsStyle), styleChange, elementUpdate.recompositeLayer };
        m_updater.updateElementRenderer(*pseudoElement, WTFMove(contentsUpdate));
        auto pseudoElementUpdateStyle = RenderStyle::cloneIncludingPseudoElements(*updateStyle);
        pseudoElement->storeDisplayContentsOrNoneStyle(makeUnique<RenderStyle>(WTFMove(pseudoElementUpdateStyle)));
    } else {
        auto pseudoElementUpdateStyle = RenderStyle::cloneIncludingPseudoElements(*updateStyle);
        Style::ElementUpdate pseudoElementUpdate { makeUnique<RenderStyle>(WTFMove(pseudoElementUpdateStyle)), styleChange, elementUpdate.recompositeLayer };
        m_updater.updateElementRenderer(*pseudoElement, WTFMove(pseudoElementUpdate));
        pseudoElement->clearDisplayContentsOrNoneStyle();
    }

    auto* pseudoElementRenderer = pseudoElement->renderer();
    if (!pseudoElementRenderer)
        return;

    if (styleChange == Style::Change::Renderer)
        createContentRenderers(m_updater.m_builder, *pseudoElementRenderer, *updateStyle, pseudoId);
    else
        updateStyleForContentRenderers(*pseudoElementRenderer, *updateStyle);

    if (m_updater.renderView().hasQuotesNeedingUpdate()) {
        for (auto& child : descendantsOfType<RenderQuote>(*pseudoElementRenderer))
            updateQuotesUpTo(&child);
    }
    m_updater.m_builder.updateAfterDescendants(*pseudoElementRenderer);
}

void RenderTreeUpdater::GeneratedContent::updateBackdropRenderer(RenderElement& renderer)
{
    auto destroyBackdropIfNeeded = [&renderer, this]() {
        if (WeakPtr backdropRenderer = renderer.backdropRenderer())
            m_updater.m_builder.destroy(*backdropRenderer);
    };

    // Intentionally bail out early here to avoid computing the style.
    if (!renderer.element() || !renderer.element()->isInTopLayer()) {
        destroyBackdropIfNeeded();
        return;
    }

    auto style = renderer.getCachedPseudoStyle(PseudoId::Backdrop, &renderer.style());
    if (!style || style->display() == DisplayType::None) {
        destroyBackdropIfNeeded();
        return;
    }

    auto newStyle = RenderStyle::clone(*style);
    if (auto backdropRenderer = renderer.backdropRenderer())
        backdropRenderer->setStyle(WTFMove(newStyle));
    else {
        auto newBackdropRenderer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, renderer.document(), WTFMove(newStyle));
        newBackdropRenderer->initializeStyle();
        renderer.setBackdropRenderer(*newBackdropRenderer.get());
        m_updater.m_builder.attach(renderer.view(), WTFMove(newBackdropRenderer));
    }
}

bool RenderTreeUpdater::GeneratedContent::needsPseudoElement(const RenderStyle* style)
{
    if (!style)
        return false;
    if (!m_updater.renderTreePosition().parent().canHaveGeneratedChildren())
        return false;
    if (!pseudoElementRendererIsNeeded(style))
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
