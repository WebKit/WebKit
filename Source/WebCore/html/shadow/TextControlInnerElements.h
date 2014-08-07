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
 
#ifndef TextControlInnerElements_h
#define TextControlInnerElements_h

#include "HTMLDivElement.h"
#include <wtf/Forward.h>

namespace WebCore {

class RenderTextControlInnerBlock;

class TextControlInnerContainer final : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerContainer> create(Document&);
protected:
    TextControlInnerContainer(Document&);
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
};

class TextControlInnerElement final : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerElement> create(Document&);

protected:
    TextControlInnerElement(Document&);
    virtual PassRefPtr<RenderStyle> customStyleForRenderer(RenderStyle& parentStyle) override;

private:
    virtual bool isMouseFocusable() const override { return false; }
};

class TextControlInnerTextElement final : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerTextElement> create(Document&);

    virtual void defaultEventHandler(Event*) override;

    RenderTextControlInnerBlock* renderer() const;

private:
    TextControlInnerTextElement(Document&);
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer(RenderStyle& parentStyle) override;
    virtual bool isMouseFocusable() const override { return false; }
    virtual bool isTextControlInnerTextElement() const override { return true; }
};

inline bool isTextControlInnerTextElement(const HTMLElement& element) { return element.isTextControlInnerTextElement(); }
inline bool isTextControlInnerTextElement(const Node& node) { return node.isHTMLElement() && isTextControlInnerTextElement(toHTMLElement(node)); }
NODE_TYPE_CASTS(TextControlInnerTextElement)

class SearchFieldResultsButtonElement final : public HTMLDivElement {
public:
    static PassRefPtr<SearchFieldResultsButtonElement> create(Document&);

    virtual void defaultEventHandler(Event*) override;
#if !PLATFORM(IOS)
    virtual bool willRespondToMouseClickEvents() override;
#endif

private:
    SearchFieldResultsButtonElement(Document&);
    virtual bool isMouseFocusable() const override { return false; }
};

class SearchFieldCancelButtonElement final : public HTMLDivElement {
public:
    static PassRefPtr<SearchFieldCancelButtonElement> create(Document&);

    virtual void defaultEventHandler(Event*) override;
    virtual bool isSearchFieldCancelButtonElement() const override { return true; }
#if !PLATFORM(IOS)
    virtual bool willRespondToMouseClickEvents() override;
#endif

private:
    SearchFieldCancelButtonElement(Document&);
    virtual void willDetachRenderers() override;
    virtual bool isMouseFocusable() const override { return false; }

    bool m_capturing;
};

} // namespace

#endif
