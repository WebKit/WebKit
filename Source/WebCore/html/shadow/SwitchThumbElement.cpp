/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SwitchThumbElement.h"

#include "ResolvedStyle.h"
#include "ScriptDisallowedScope.h"

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(SwitchThumbElement);

Ref<SwitchThumbElement> SwitchThumbElement::create(Document& document)
{
    auto element = adoptRef(*new SwitchThumbElement(document));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    return element;
}

SwitchThumbElement::SwitchThumbElement(Document& document)
    : HTMLDivElement(HTMLNames::divTag, document, CreateSwitchThumbElement)
{
}

std::optional<Style::ResolvedStyle> SwitchThumbElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* hostStyle)
{
    if (!hostStyle)
        return std::nullopt;

    auto elementStyle = resolveStyle(resolutionContext);
    if (hostStyle->effectiveAppearance() == StyleAppearance::Auto) {
        elementStyle.style->setEffectiveAppearance(StyleAppearance::SwitchThumb);
        elementStyle.style->setWidth({ 38, LengthType::Fixed });
        elementStyle.style->setHeight({ 24, LengthType::Fixed });
        elementStyle.style->setMarginTop({ -24, LengthType::Fixed });
    }

    return elementStyle;
}

} // namespace WebCore
