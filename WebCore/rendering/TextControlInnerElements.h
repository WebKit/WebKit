/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

namespace WebCore {

class String;

class TextControlInnerElement : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerElement> create(Node* shadowParent);

    void attachInnerElement(Node*, PassRefPtr<RenderStyle>, RenderArena*);

protected:
    TextControlInnerElement(Document*, Node* shadowParent = 0);

private:
    virtual bool isMouseFocusable() const { return false; } 
    virtual bool isShadowNode() const { return m_shadowParent; }
    virtual Node* shadowParentNode() { return m_shadowParent; }
    void setShadowParentNode(Node* node) { m_shadowParent = node; }

    Node* m_shadowParent;
};

class TextControlInnerTextElement : public TextControlInnerElement {
public:
    static PassRefPtr<TextControlInnerTextElement> create(Document*, Node* shadowParent);

    virtual void defaultEventHandler(Event*);

private:
    TextControlInnerTextElement(Document*, Node* shadowParent);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);  
};

class SearchFieldResultsButtonElement : public TextControlInnerElement {
public:
    static PassRefPtr<SearchFieldResultsButtonElement> create(Document*);

    virtual void defaultEventHandler(Event*);

private:
    SearchFieldResultsButtonElement(Document*);
};

class SearchFieldCancelButtonElement : public TextControlInnerElement {
public:
    static PassRefPtr<SearchFieldCancelButtonElement> create(Document*);

    virtual void defaultEventHandler(Event*);

private:
    SearchFieldCancelButtonElement(Document*);

    virtual void detach();

    bool m_capturing;
};

class SpinButtonElement : public TextControlInnerElement {
public:
    static PassRefPtr<SpinButtonElement> create(Node*);

    // FIXME: "Spin button on up button" is not a phrase with a single clear meaning.
    // Need a name for this that makes it clearer.
    bool onUpButton() const { return m_onUpButton; }

private:
    SpinButtonElement(Node*);

    virtual bool isSpinButtonElement() const { return true; }
    virtual bool isEnabledFormControl() { return static_cast<Element*>(shadowAncestorNode())->isEnabledFormControl(); }
    virtual void defaultEventHandler(Event*);

    bool m_capturing;
    bool m_onUpButton;
};

#if ENABLE(INPUT_SPEECH)

class InputFieldSpeechButtonElement : public TextControlInnerElement {
public:
    static PassRefPtr<InputFieldSpeechButtonElement> create(Document*);

    virtual void defaultEventHandler(Event*);

private:
    InputFieldSpeechButtonElement(Document*);
};

#endif // ENABLE(INPUT_SPEECH)

} // namespace

#endif
