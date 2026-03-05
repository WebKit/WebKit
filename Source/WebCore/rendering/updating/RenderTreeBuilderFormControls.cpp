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
#include "RenderBlockInlines.h"
#include "RenderButton.h"
#include "RenderMenuList.h"
#include "RenderTreeBuilderBlock.h"
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

void RenderTreeBuilder::FormControls::attach(RenderButton& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    m_builder.blockBuilder().attach(findOrCreateParentForChild(parent), WTF::move(child), beforeChild);
}

RenderPtr<RenderObject> RenderTreeBuilder::FormControls::detach(RenderButton& parent, RenderObject& child, RenderTreeBuilder::WillBeDestroyed willBeDestroyed)
{
    auto* innerRenderer = parent.innerRenderer();
    if (!innerRenderer || &child == innerRenderer || child.parent() == &parent) {
        ASSERT(&child == innerRenderer || !innerRenderer);
        return m_builder.blockBuilder().detach(parent, child, willBeDestroyed);
    }
    return m_builder.detach(*innerRenderer, child, willBeDestroyed);
}


RenderBlock& RenderTreeBuilder::FormControls::findOrCreateParentForChild(RenderButton& parent)
{
    auto* innerRenderer = parent.innerRenderer();
    if (innerRenderer)
        return *innerRenderer;

    auto wrapper = Block::createAnonymousBlockWithStyle(protect(parent.document()), parent.style());
    innerRenderer = wrapper.get();
    m_builder.blockBuilder().attach(parent, WTF::move(wrapper), nullptr);
    parent.setInnerRenderer(*innerRenderer);
    return *innerRenderer;
}

void RenderTreeBuilder::FormControls::updateAfterDescendants(RenderElement& renderer)
{
    if (RefPtr inputElement = dynamicDowncast<HTMLInputElement>(renderer.element())) {
        if (inputElement->isCheckable())
            updatePseudoElement(PseudoElementType::Checkmark, renderer, renderer.style().usedAppearance());
        return;
    }

    if (RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(renderer.element())) {
        RefPtr selectElement = optionElement->ownerSelectElement();
        if (!selectElement)
            return;

        RefPtr pickerElement = selectElement->pickerPopoverElement();
        if (!pickerElement)
            return;

        if (CheckedPtr pickerElementRenderer = pickerElement->renderer())
            updatePseudoElement(PseudoElementType::Checkmark, renderer, pickerElementRenderer->style().usedAppearance(), renderer.firstChild());

        return;
    }

    if (RefPtr select = dynamicDowncast<HTMLSelectElement>(renderer.element()); select && select->usesMenuList()) {
        updatePseudoElement(PseudoElementType::PickerIcon, renderer, renderer.style().usedAppearance());
        return;
    }
}

void RenderTreeBuilder::FormControls::updatePseudoElement(PseudoElementType type, RenderElement& renderer, StyleAppearance usedAppearance, RenderObject* beforeChild)
{
    CheckedPtr existingPseudoElement = renderer.pseudoElementRenderer(type).get();

    if (usedAppearance != StyleAppearance::Base && !existingPseudoElement)
        return;

    auto pseudoStyle = renderer.getCachedPseudoStyle({ type });
    if (!pseudoStyle)
        return;

    auto shouldHavePseudoElementRenderer = [&] -> bool {
        return usedAppearance == StyleAppearance::Base && pseudoStyle->display() != Style::DisplayType::None;
    };

    if (!shouldHavePseudoElementRenderer()) {
        if (existingPseudoElement)
            m_builder.destroy(*existingPseudoElement);
        return;
    }

    if (existingPseudoElement && existingPseudoElement->style().content() == pseudoStyle->content()) {
        auto pseudoElementStyle = RenderStyle::clone(*pseudoStyle);
        existingPseudoElement->setStyle(WTF::move(pseudoElementStyle));
        RenderTreeUpdater::GeneratedContent::updateStyleForContentRenderers(*existingPseudoElement, existingPseudoElement->style());
        return;
    }

    if (existingPseudoElement) {
        m_builder.destroy(*existingPseudoElement);
        existingPseudoElement = nullptr;
    }

    Ref document = renderer.document();
    auto pseudoElementStyle = RenderStyle::clone(*pseudoStyle);

    RenderPtr<RenderBlockFlow> pseudoElement = createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, WTF::move(pseudoElementStyle));
    pseudoElement->initializeStyle();

    if (pseudoElement->style().content().isData())
        RenderTreeUpdater::GeneratedContent::createContentRenderers(m_builder, *pseudoElement, pseudoElement->style(), type);

    renderer.setPseudoElementRenderer(type, *pseudoElement.get());

    m_builder.attach(renderer, WTF::move(pseudoElement), beforeChild);
}

} // namespace WebCore
