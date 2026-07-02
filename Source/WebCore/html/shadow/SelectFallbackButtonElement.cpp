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
#include "SelectFallbackButtonElement.h"

#include "ContainerNodeInlines.h"
#include "CSSValueKeywords.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "PlatformRenderTheme.h"
#include "RenderTheme.h"
#include "ResolvedStyle.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleKeyword+Mappings.h"
#include "StyleResolver.h"
#include "StyleTextAlign.h"
#include "Text.h"
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include "LocalizedStrings.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SelectFallbackButtonElement);

#if PLATFORM(IOS_FAMILY)
static size_t selectedOptionCount(const HTMLSelectElement& selectElement)
{
    size_t count = 0;
    for (auto& item : selectElement.listItems()) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(item.get()); option && option->selected())
            ++count;
    }
    return count;
}
#endif

Ref<SelectFallbackButtonElement> SelectFallbackButtonElement::create(Document& document)
{
    Ref element = adoptRef(*new SelectFallbackButtonElement(document));
    ScriptDisallowedScope::EventAllowedScope scope { element };
    element->appendChild(Text::create(document, "\n"_s));
    return element;
}

SelectFallbackButtonElement::SelectFallbackButtonElement(Document& document)
    : HTMLDivElement(document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

HTMLSelectElement& SelectFallbackButtonElement::selectElement() const
{
    return downcast<HTMLSelectElement>(*containingShadowRoot()->host());
}

void SelectFallbackButtonElement::updateText(HTMLOptionElement* selectedOption, int optionIndex)
{
    Ref selectElement = this->selectElement();

    if (optionIndex < 0)
        optionIndex = selectElement->selectedIndex();

    auto applyText = [&](const String& text) {
        setText(text);
        invalidateStyle();
        selectElement->didUpdateActiveOption(optionIndex);
    };

#if PLATFORM(IOS_FAMILY)
    if (selectElement->multiple()) {
        size_t count = selectedOptionCount(selectElement);
        if (count != 1) {
            applyText(htmlSelectMultipleItems(count));
            return;
        }
    }
#endif

    RefPtr option = selectedOption;
    if (!option) {
        auto& listItems = selectElement->listItems();
        int i = selectElement->optionToListIndex(optionIndex);
        if (i >= 0 && static_cast<unsigned>(i) < listItems.size())
            option = dynamicDowncast<HTMLOptionElement>(*listItems[i]);
    }

    applyText(option ? option->textIndentedToRespectGroupLabel().trim(deprecatedIsSpaceOrNewline) : emptyString());
}

void SelectFallbackButtonElement::setText(const String& text)
{
    String textToUse = text.isEmpty() ? "\n"_s : text;
    Ref textNode = downcast<Text>(*firstChild());
    if (textNode->data() != textToUse)
        textNode->setData(textToUse);
}

std::optional<Style::UnadjustedStyle> SelectFallbackButtonElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const Style::ComputedStyle* hostStyle)
{
    if (!hostStyle)
        return std::nullopt;

    auto elementStyle = resolveStyle(resolutionContext);
    CheckedRef style = *elementStyle.style;

    auto hostTextAlign = hostStyle->textAlign();
    if (hostTextAlign == Style::TextAlign::Start)
        style->setTextAlign(hostStyle->writingMode().isBidiLTR() ? Style::TextAlign::Left : Style::TextAlign::Right);
    else if (hostTextAlign == Style::TextAlign::End)
        style->setTextAlign(hostStyle->writingMode().isBidiLTR() ? Style::TextAlign::Right : Style::TextAlign::Left);
    else
        style->setTextAlign(hostTextAlign);

    // Apply direction and unicodeBidi from the selected option for proper bidirectional text rendering.
    Ref selectElement = this->selectElement();
    for (auto& item : selectElement->listItems()) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(item.get());
        if (!option || !option->selected())
            continue;

        if (CheckedPtr optionStyle = option->computedStyleForEditability()) {
            style->setDirection(optionStyle->writingMode().bidiDirection());
            style->setUnicodeBidi(optionStyle->unicodeBidi());
        }
        break;
    }

    if (hostStyle->usedAppearance() != StyleAppearance::Base) {
        style->setFlexGrow(1);
        style->setFlexShrink(1);
        // min-width: 0; is needed for correct shrinking.
        style->setLogicalMinWidth(0_css_px);

        style->setMarginBefore(CSS::Keyword::Auto { });
        style->setMarginAfter(CSS::Keyword::Auto { });
        style->setAlignSelf(CSS::Keyword::FlexStart { });

        auto paddingBox = RenderTheme::singleton().popupInternalPaddingBox(*hostStyle);
        style->setPaddingBox(WTF::move(paddingBox));
    }

    return elementStyle;
}

} // namespace WebCore
