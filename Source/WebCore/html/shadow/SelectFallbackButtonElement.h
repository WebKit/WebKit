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

#pragma once

#include "HTMLDivElement.h"
#include "RenderPtr.h"

namespace WebCore {

class HTMLSelectElement;
class RenderSelectFallbackButton;

class SelectFallbackButtonElement final : public HTMLDivElement {
    WTF_MAKE_TZONE_ALLOCATED(SelectFallbackButtonElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SelectFallbackButtonElement);
public:
    static Ref<SelectFallbackButtonElement> create(Document&);

    HTMLSelectElement& selectElement() const;
    void updateText();
#if !PLATFORM(COCOA)
    void setTextFromOption(int optionIndex);
#endif

private:
    explicit SelectFallbackButtonElement(Document&);

    bool isSelectFallbackButtonElement() const final { return true; }

    std::optional<Style::UnadjustedStyle> resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle* hostStyle) final;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SelectFallbackButtonElement)
    static bool isType(const WebCore::HTMLElement& element) { return element.isSelectFallbackButtonElement(); }
    static bool isType(const WebCore::Node& node)
    {
        auto* htmlElement = dynamicDowncast<WebCore::HTMLElement>(node);
        return htmlElement && isType(*htmlElement);
    }
SPECIALIZE_TYPE_TRAITS_END()
