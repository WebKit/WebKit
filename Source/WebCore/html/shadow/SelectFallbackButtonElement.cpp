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

#include "CSSPrimitiveValueMappings.h"
#include "CSSValueKeywords.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "RenderSelectFallbackButton.h"
#include "RenderStyle+SettersInlines.h"
#include "RenderTheme.h"
#include "ResolvedStyle.h"
#include "StyleResolver.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SelectFallbackButtonElement);

Ref<SelectFallbackButtonElement> SelectFallbackButtonElement::create(Document& document)
{
    return adoptRef(*new SelectFallbackButtonElement(document));
}

SelectFallbackButtonElement::SelectFallbackButtonElement(Document& document)
    : HTMLDivElement(HTMLNames::divTag, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

HTMLSelectElement& SelectFallbackButtonElement::selectElement() const
{
    return downcast<HTMLSelectElement>(*protect(containingShadowRoot())->host());
}

void SelectFallbackButtonElement::updateText()
{
    invalidateStyle();
    if (CheckedPtr buttonTextRenderer = dynamicDowncast<RenderSelectFallbackButton>(renderer()))
        buttonTextRenderer->updateFromElement();
}

#if !PLATFORM(COCOA)
void SelectFallbackButtonElement::setTextFromOption(int optionIndex)
{
    if (CheckedPtr buttonTextRenderer = dynamicDowncast<RenderSelectFallbackButton>(renderer()))
        buttonTextRenderer->setTextFromOption(optionIndex);
}
#endif

std::optional<Style::UnadjustedStyle> SelectFallbackButtonElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* hostStyle)
{
    if (!hostStyle)
        return std::nullopt;

    auto elementStyle = resolveStyle(resolutionContext);
    CheckedRef style = *elementStyle.style;

    style->setFlexGrow(1);
    style->setFlexShrink(1);
    // min-width: 0; is needed for correct shrinking.
    style->setLogicalMinWidth(0_css_px);

    // Set text-align based on the select's direction (not its text-align property).
    // This matches the old RenderMenuList behavior where text-align followed the
    // select's bidi direction, not its CSS text-align property.
    style->setTextAlign(hostStyle->writingMode().isBidiLTR() ? Style::TextAlign::Left : Style::TextAlign::Right);

    // Apply direction and unicodeBidi from the selected option for proper bidirectional text rendering.
    for (auto& item : protect(selectElement())->listItems()) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(item.get());
        if (!option || !option->selected())
            continue;

        if (CheckedPtr optionStyle = option->computedStyleForEditability()) {
            style->setDirection(optionStyle->writingMode().bidiDirection());
            style->setUnicodeBidi(optionStyle->unicodeBidi());
        }
        break;
    }

    switch (hostStyle->usedAppearance()) {
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton: {
        style->setMarginBefore(CSS::Keyword::Auto { });
        style->setMarginAfter(CSS::Keyword::Auto { });
        style->setAlignSelf(CSS::Keyword::FlexStart { });

        auto paddingBox = RenderTheme::singleton().popupInternalPaddingBox(*hostStyle);
        style->setPaddingBox(WTF::move(paddingBox));
        break;
    }
    default:
        break;
    }

    return elementStyle;
}

RenderPtr<RenderElement> SelectFallbackButtonElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSelectFallbackButton>(*this, WTF::move(style));
}

} // namespace WebCore
