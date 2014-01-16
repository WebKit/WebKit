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
#include "SpeechInputListener.h"
#include <wtf/Forward.h>

namespace WebCore {

class RenderTextControlInnerBlock;
class SpeechInput;

class TextControlInnerContainer FINAL : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerContainer> create(Document&);
protected:
    TextControlInnerContainer(Document&);
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
};

class TextControlInnerElement FINAL : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerElement> create(Document&);

protected:
    TextControlInnerElement(Document&);
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() override;

private:
    virtual bool isMouseFocusable() const override { return false; }
};

class TextControlInnerTextElement FINAL : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerTextElement> create(Document&);

    virtual void defaultEventHandler(Event*) override;

    RenderTextControlInnerBlock* renderer() const;

private:
    TextControlInnerTextElement(Document&);
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() override;
    virtual bool isMouseFocusable() const override { return false; }
    virtual bool isTextControlInnerTextElement() const override { return true; }
};

inline bool isTextControlInnerTextElement(const HTMLElement& element) { return element.isTextControlInnerTextElement(); }
inline bool isTextControlInnerTextElement(const Node& node) { return node.isHTMLElement() && isTextControlInnerTextElement(toHTMLElement(node)); }
NODE_TYPE_CASTS(TextControlInnerTextElement)

class SearchFieldResultsButtonElement FINAL : public HTMLDivElement {
public:
    static PassRefPtr<SearchFieldResultsButtonElement> create(Document&);

    virtual void defaultEventHandler(Event*) override;
#if !PLATFORM(IOS)
    virtual bool willRespondToMouseClickEvents() override;
#endif

private:
    SearchFieldResultsButtonElement(Document&);
    virtual const AtomicString& shadowPseudoId() const override;
    virtual bool isMouseFocusable() const override { return false; }
};

class SearchFieldCancelButtonElement FINAL : public HTMLDivElement {
public:
    static PassRefPtr<SearchFieldCancelButtonElement> create(Document&);

    virtual void defaultEventHandler(Event*) override;
    virtual bool isSearchFieldCancelButtonElement() const override { return true; }
#if !PLATFORM(IOS)
    virtual bool willRespondToMouseClickEvents() override;
#endif

private:
    SearchFieldCancelButtonElement(Document&);
    virtual const AtomicString& shadowPseudoId() const override;
    virtual void willDetachRenderers() override;
    virtual bool isMouseFocusable() const override { return false; }

    bool m_capturing;
};

#if ENABLE(INPUT_SPEECH)

class InputFieldSpeechButtonElement FINAL
    : public HTMLDivElement,
      public SpeechInputListener {
public:
    enum SpeechInputState {
        Idle,
        Recording,
        Recognizing,
    };

    static PassRefPtr<InputFieldSpeechButtonElement> create(Document&);
    virtual ~InputFieldSpeechButtonElement();

    virtual void defaultEventHandler(Event*);
#if !PLATFORM(IOS)
    virtual bool willRespondToMouseClickEvents();
#endif
    virtual bool isInputFieldSpeechButtonElement() const { return true; }
    SpeechInputState state() const { return m_state; }
    void startSpeechInput();
    void stopSpeechInput();

    // SpeechInputListener methods.
    void didCompleteRecording(int);
    void didCompleteRecognition(int);
    void setRecognitionResult(int, const SpeechInputResultArray&);

private:
    InputFieldSpeechButtonElement(Document&);
    SpeechInput* speechInput();
    void setState(SpeechInputState state);
    virtual const AtomicString& shadowPseudoId() const;
    virtual bool isMouseFocusable() const override { return false; }
    virtual void willAttachRenderers() override;
    virtual void willDetachRenderers() override;


    bool m_capturing;
    SpeechInputState m_state;
    int m_listenerId;
    SpeechInputResultArray m_results;
};

inline InputFieldSpeechButtonElement* toInputFieldSpeechButtonElement(Element* element)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!element || element->isInputFieldSpeechButtonElement());
    return static_cast<InputFieldSpeechButtonElement*>(element);
}

#endif // ENABLE(INPUT_SPEECH)

} // namespace

#endif
