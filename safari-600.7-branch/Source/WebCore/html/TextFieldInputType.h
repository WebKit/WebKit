/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextFieldInputType_h
#define TextFieldInputType_h

#include "InputType.h"
#include "SpinButtonElement.h"

namespace WebCore {

class FormDataList;
class TextControlInnerTextElement;

// The class represents types of which UI contain text fields.
// It supports not only the types for BaseTextInputType but also type=number.
class TextFieldInputType : public InputType, protected SpinButtonElement::SpinButtonOwner {
protected:
    explicit TextFieldInputType(HTMLInputElement&);
    virtual ~TextFieldInputType();
    virtual bool canSetSuggestedValue() override;
    virtual void handleKeydownEvent(KeyboardEvent*) override;
    void handleKeydownEventForSpinButton(KeyboardEvent*);

    virtual HTMLElement* containerElement() const override;
    virtual HTMLElement* innerBlockElement() const override;
    virtual TextControlInnerTextElement* innerTextElement() const override;
    virtual HTMLElement* innerSpinButtonElement() const override;
#if ENABLE(INPUT_SPEECH)
    virtual HTMLElement* speechButtonElement() const override;
#endif

protected:
    virtual bool needsContainer() const;
    virtual bool shouldHaveSpinButton() const;
    virtual void createShadowSubtree() override;
    virtual void destroyShadowSubtree() override;
    virtual void attributeChanged() override;
    virtual void disabledAttributeChanged() override;
    virtual void readonlyAttributeChanged() override;
    virtual bool supportsReadOnly() const override;
    virtual void handleBlurEvent() override;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    virtual void updateInnerTextValue() override;
#if PLATFORM(IOS)
    virtual String sanitizeValue(const String&) const override;
#endif

    virtual String convertFromVisibleValue(const String&) const;
    enum ValueChangeState {
        ValueChangeStateNone,
        ValueChangeStateChanged
    };
    virtual void didSetValueByUserEdit(ValueChangeState);

private:
    virtual bool isKeyboardFocusable(KeyboardEvent*) const override;
    virtual bool isMouseFocusable() const override;
    virtual bool isTextField() const override;
    virtual bool valueMissing(const String&) const override;
    virtual void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) override;
    virtual void forwardEvent(Event*) override;
    virtual bool shouldSubmitImplicitly(Event*) override;
    virtual RenderPtr<RenderElement> createInputRenderer(PassRef<RenderStyle>) override;
    virtual bool shouldUseInputMethod() const override;
#if !PLATFORM(IOS)
    virtual String sanitizeValue(const String&) const override;
#endif
    virtual bool shouldRespectListAttribute() override;
    virtual HTMLElement* placeholderElement() const override;
    virtual void updatePlaceholderText() override;
    virtual bool appendFormData(FormDataList&, bool multipart) const override;
    virtual void subtreeHasChanged() override;

    // SpinButtonElement::SpinButtonOwner functions.
    virtual void focusAndSelectSpinButtonOwner() override;
    virtual bool shouldSpinButtonRespondToMouseEvents() override;
    virtual bool shouldSpinButtonRespondToWheelEvents() override;
    virtual void spinButtonStepDown() override;
    virtual void spinButtonStepUp() override;

    RefPtr<HTMLElement> m_container;
    RefPtr<HTMLElement> m_innerBlock;
    RefPtr<TextControlInnerTextElement> m_innerText;
    RefPtr<HTMLElement> m_placeholder;
    RefPtr<SpinButtonElement> m_innerSpinButton;
#if ENABLE(INPUT_SPEECH)
    RefPtr<HTMLElement> m_speechButton;
#endif
};

} // namespace WebCore

#endif // TextFieldInputType_h
