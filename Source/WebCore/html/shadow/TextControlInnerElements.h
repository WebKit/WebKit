/*
 * Copyright (C) 2006, 2008, 2010, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#pragma once

#include "HTMLDivElement.h"
#include <wtf/Forward.h>

namespace WebCore {

class RenderTextControlInnerBlock;

class TextControlInnerContainer final : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(TextControlInnerContainer);
public:
    static Ref<TextControlInnerContainer> create(Document&);
protected:
    TextControlInnerContainer(Document&);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    Optional<ElementStyle> resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle* shadowHostStyle) override;
};

class TextControlInnerElement final : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(TextControlInnerElement);
public:
    static Ref<TextControlInnerElement> create(Document&);

protected:
    TextControlInnerElement(Document&);
    Optional<ElementStyle> resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle* shadowHostStyle) override;

private:
    bool isMouseFocusable() const override { return false; }
};

class TextControlInnerTextElement final : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(TextControlInnerTextElement);
public:
    static Ref<TextControlInnerTextElement> create(Document&);

    void defaultEventHandler(Event&) override;

    RenderTextControlInnerBlock* renderer() const;

private:
    TextControlInnerTextElement(Document&);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    Optional<ElementStyle> resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle* shadowHostStyle) override;
    bool isMouseFocusable() const override { return false; }
    bool isTextControlInnerTextElement() const override { return true; }
};

class TextControlPlaceholderElement final : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(TextControlPlaceholderElement);
public:
    static Ref<TextControlPlaceholderElement> create(Document&);

private:
    TextControlPlaceholderElement(Document&);
    
    Optional<ElementStyle> resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle* shadowHostStyle) override;
};

class SearchFieldResultsButtonElement final : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(SearchFieldResultsButtonElement);
public:
    static Ref<SearchFieldResultsButtonElement> create(Document&);

    void defaultEventHandler(Event&) override;
#if !PLATFORM(IOS_FAMILY)
    bool willRespondToMouseClickEvents() override;
#endif

private:
    SearchFieldResultsButtonElement(Document&);
    bool isMouseFocusable() const override { return false; }
};

class SearchFieldCancelButtonElement final : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(SearchFieldCancelButtonElement);
public:
    static Ref<SearchFieldCancelButtonElement> create(Document&);

    void defaultEventHandler(Event&) override;
#if !PLATFORM(IOS_FAMILY)
    bool willRespondToMouseClickEvents() override;
#endif

private:
    SearchFieldCancelButtonElement(Document&);
    bool isMouseFocusable() const override { return false; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TextControlInnerTextElement)
    static bool isType(const WebCore::HTMLElement& element) { return element.isTextControlInnerTextElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::HTMLElement>(node) && isType(downcast<WebCore::HTMLElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
