/*
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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
#include "RenderTreeBuilderFormControls.h"

#include "ContainerNodeInlines.h"
#include "HTMLInputElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "InputType.h"
#include "RenderBlockFlow.h"
#include "RenderIterator.h"
#include "RenderMenuList.h"
#include "RenderTextFragment.h"
#include "RenderTreeUpdaterGeneratedContent.h"
#include "SelectPopoverElement.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::FormControls);

RenderTreeBuilder::FormControls::FormControls(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::FormControls::updateAfterDescendants(RenderElement& renderer)
{
    if (RefPtr inputElement = dynamicDowncast<HTMLInputElement>(renderer.element())) {
        if (renderer.usesButtonLayout())
            updateInputButtonText(downcast<RenderBlockFlow>(renderer), inputElement->valueWithDefault());
        else if (inputElement->isCheckable())
            updatePseudoElement(PseudoElementType::Checkmark, renderer, renderer.style().usedAppearance());
        return;
    }

    if (RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(renderer.element())) {
        RefPtr selectElement = optionElement->ownerSelectElement();
        if (!selectElement)
            return;

        RefPtr container = selectElement->optionContainer();
        CheckedPtr containerRenderer = container ? container->renderer() : nullptr;
        if (containerRenderer)
            updatePseudoElement(PseudoElementType::Checkmark, renderer, containerRenderer->style().usedAppearance(), PseudoElementPosition::FirstChild);

        return;
    }

    if (RefPtr select = dynamicDowncast<HTMLSelectElement>(renderer.element()); select && select->isDropdownBox(&renderer.style())) {
        updatePseudoElement(PseudoElementType::PickerIcon, renderer, renderer.style().usedAppearance());
        return;
    }
}

void RenderTreeBuilder::FormControls::updateInputButtonText(RenderBlockFlow& inputButton, const String& text)
{
    // https://html.spec.whatwg.org/multipage/rendering.html#the-input-element-as-a-button
    // "its contents are expected to be the text of the element's value attribute, if any, or text derived from the element's type attribute"
    // The text is the only content of an input button.
    WeakPtr textRenderer = RenderTraversal::firstWithin<RenderTextFragment>(inputButton);
    if (text.isEmpty()) {
        if (textRenderer) {
            // Like RenderTreeUpdater::tearDownTextRenderer() does for text nodes.
            textRenderer->repaint();
            inputButton.setChildNeedsLayout();
            inputButton.invalidateContentLogicalWidths();
            // The renderer is destroyed by this call, so it cannot be protected.
            SUPPRESS_UNCHECKED_ARG m_builder.destroy(*textRenderer);
        }
        return;
    }

    if (textRenderer) {
        textRenderer->setText(text);
        return;
    }

    m_builder.attach(inputButton, createRenderer<RenderTextFragment>(protect(inputButton.document()), text));
}

void RenderTreeBuilder::FormControls::updatePseudoElement(PseudoElementType type, RenderElement& renderer, StyleAppearance usedAppearance, PseudoElementPosition position)
{
    WeakPtr existingPseudoElement = renderer.pseudoElementRenderer(type);

    if (usedAppearance != StyleAppearance::Base && !existingPseudoElement)
        return;

    auto pseudoStyle = renderer.style().pseudoElementStyle({ type });
    if (!pseudoStyle)
        return;

    auto shouldHavePseudoElementRenderer = [&] -> bool {
        return usedAppearance == StyleAppearance::Base && pseudoStyle->display() != Style::DisplayType::None;
    };

    if (!shouldHavePseudoElementRenderer()) {
        if (existingPseudoElement)
            // The renderer is destroyed by this call, so it cannot be protected.
            SUPPRESS_UNCHECKED_ARG m_builder.destroy(*existingPseudoElement);
        return;
    }

    if (existingPseudoElement && existingPseudoElement->style().content() == pseudoStyle->content()) {
        auto pseudoElementStyle = Style::ComputedStyle::clone(*pseudoStyle);
        existingPseudoElement->setStyle(WTF::move(pseudoElementStyle));
        RenderTreeUpdater::GeneratedContent::updateStyleForContentRenderers(*existingPseudoElement, existingPseudoElement->style());
        return;
    }

    if (existingPseudoElement) {
        // The renderer is destroyed by this call, so it cannot be protected.
        SUPPRESS_UNCHECKED_ARG m_builder.destroy(*existingPseudoElement);
    }

    if (!renderer.canHaveChildren())
        return;

    Ref document = renderer.document();
    auto pseudoElementStyle = Style::ComputedStyle::clone(*pseudoStyle);

    RenderPtr<RenderBlockFlow> pseudoElement = createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, WTF::move(pseudoElementStyle));
    pseudoElement->initializeStyle();

    if (!renderer.isChildAllowed(*pseudoElement, pseudoElement->style()))
        return;

    if (pseudoElement->style().content().isData())
        RenderTreeUpdater::GeneratedContent::createContentRenderers(m_builder, *pseudoElement, pseudoElement->style(), type);

    renderer.setPseudoElementRenderer(type, *pseudoElement.get());

    auto* beforeChild = position == PseudoElementPosition::FirstChild ? renderer.firstChild() : nullptr;
    m_builder.attach(renderer, WTF::move(pseudoElement), beforeChild);
}

} // namespace WebCore
