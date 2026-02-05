/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RenderSelectFallbackButton.h"

#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "RenderText.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SelectFallbackButtonElement.h"
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include "LocalizedStrings.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSelectFallbackButton);

RenderSelectFallbackButton::RenderSelectFallbackButton(SelectFallbackButtonElement& element, RenderStyle&& style)
    : RenderBlockFlow(Type::SelectFallbackButton, element, WTF::move(style))
{
}

SelectFallbackButtonElement& RenderSelectFallbackButton::selectFallbackButtonElement() const
{
    return downcast<SelectFallbackButtonElement>(nodeForNonAnonymous());
}

void RenderSelectFallbackButton::insertedIntoTree()
{
    RenderBlockFlow::insertedIntoTree();
    updateFromElement();
}

#if PLATFORM(IOS_FAMILY)
static size_t selectedOptionCount(HTMLSelectElement& selectElement)
{
    const auto& listItems = selectElement.listItems();
    size_t numberOfItems = listItems.size();

    size_t count = 0;
    for (size_t i = 0; i < numberOfItems; ++i) {
        auto* option = dynamicDowncast<HTMLOptionElement>(*listItems[i]);
        if (option && option->selected())
            ++count;
    }
    return count;
}
#endif

void RenderSelectFallbackButton::updateFromElement()
{
    Ref selectElement = protect(selectFallbackButtonElement())->selectElement();
    int optionIndex = selectElement->selectedIndex();

    const auto& listItems = selectElement->listItems();
    int size = listItems.size();

    int i = selectElement->optionToListIndex(optionIndex);
    String text = emptyString();
    if (i >= 0 && i < size) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*listItems[i]))
            text = option->textIndentedToRespectGroupLabel();
    }

#if PLATFORM(IOS_FAMILY)
    if (selectElement->multiple()) {
        size_t count = selectedOptionCount(selectElement.get());
        if (count != 1)
            text = htmlSelectMultipleItems(count);
    }
#endif

    setText(text.trim(deprecatedIsSpaceOrNewline));

    selectElement->didUpdateActiveOption(optionIndex);
}

#if !PLATFORM(COCOA)
void RenderSelectFallbackButton::setTextFromOption(int optionIndex)
{
    Ref selectElement = protect(selectFallbackButtonElement())->selectElement();

    const auto& listItems = selectElement->listItems();
    int size = listItems.size();

    int i = selectElement->optionToListIndex(optionIndex);
    String text = emptyString();
    if (i >= 0 && i < size) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*listItems[i]))
            text = option->textIndentedToRespectGroupLabel();
    }

    setText(text.trim(deprecatedIsSpaceOrNewline));

    selectElement->didUpdateActiveOption(optionIndex);
}
#endif

void RenderSelectFallbackButton::setText(const String& s)
{
    String textToUse = s.isEmpty() ? "\n"_str : s;

    if (CheckedPtr buttonText = m_buttonText.get()) {
        buttonText->setText(textToUse.impl(), true);
        return;
    }

    Ref document = this->document();
    auto newButtonText = createRenderer<RenderText>(Type::Text, document, textToUse);
    m_buttonText = *newButtonText;

    // FIXME: This mutation should go through the normal RenderTreeBuilder path.
    if (RenderTreeBuilder::current())
        RenderTreeBuilder::current()->attach(*this, WTF::move(newButtonText));
    else
        RenderTreeBuilder(*document->renderView()).attach(*this, WTF::move(newButtonText));
}

} // namespace WebCore
