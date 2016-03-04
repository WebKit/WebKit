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

#include "AutoFillButtonElement.h"
#include "InputType.h"
#include "SpinButtonElement.h"

namespace WebCore {

class FormDataList;
class TextControlInnerTextElement;

// The class represents types of which UI contain text fields.
// It supports not only the types for BaseTextInputType but also type=number.
class TextFieldInputType : public InputType, protected SpinButtonElement::SpinButtonOwner, protected AutoFillButtonElement::AutoFillButtonOwner {
protected:
    explicit TextFieldInputType(HTMLInputElement&);
    virtual ~TextFieldInputType();
    void handleKeydownEvent(KeyboardEvent*) override;
    void handleKeydownEventForSpinButton(KeyboardEvent*);

    HTMLElement* containerElement() const override final;
    HTMLElement* innerBlockElement() const override final;
    TextControlInnerTextElement* innerTextElement() const override final;
    HTMLElement* innerSpinButtonElement() const override final;
    HTMLElement* capsLockIndicatorElement() const override final;
    HTMLElement* autoFillButtonElement() const override final;

protected:
    virtual bool needsContainer() const;
    void createShadowSubtree() override;
    void destroyShadowSubtree() override;
    void attributeChanged() override final;
    void disabledAttributeChanged() override final;
    void readonlyAttributeChanged() override final;
    bool supportsReadOnly() const override final;
    void handleFocusEvent(Node* oldFocusedNode, FocusDirection) override final;
    void handleBlurEvent() override final;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    void updateInnerTextValue() override final;
    String sanitizeValue(const String&) const override;

    virtual String convertFromVisibleValue(const String&) const;
    virtual void didSetValueByUserEdit();

private:
    bool isKeyboardFocusable(KeyboardEvent*) const override final;
    bool isMouseFocusable() const override final;
    bool isTextField() const override final;
    bool isEmptyValue() const override final;
    bool valueMissing(const String&) const override final;
    void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) override final;
    void forwardEvent(Event*) override final;
    bool shouldSubmitImplicitly(Event*) override final;
    RenderPtr<RenderElement> createInputRenderer(Ref<RenderStyle>&&) override;
    bool shouldUseInputMethod() const override;
    bool shouldRespectListAttribute() override;
    HTMLElement* placeholderElement() const override final;
    void updatePlaceholderText() override final;
    bool appendFormData(FormDataList&, bool multipart) const override final;
    void subtreeHasChanged() override final;
    void capsLockStateMayHaveChanged() override final;
    void updateAutoFillButton() override final;

    // SpinButtonElement::SpinButtonOwner functions.
    void focusAndSelectSpinButtonOwner() override final;
    bool shouldSpinButtonRespondToMouseEvents() override final;
    bool shouldSpinButtonRespondToWheelEvents() override final;
    void spinButtonStepDown() override final;
    void spinButtonStepUp() override final;

    // AutoFillButtonElement::AutoFillButtonOwner
    void autoFillButtonElementWasClicked() override final;

    bool shouldHaveSpinButton() const;
    bool shouldHaveCapsLockIndicator() const;
    bool shouldDrawCapsLockIndicator() const;
    bool shouldDrawAutoFillButton() const;

    void createContainer();
    void createAutoFillButton(AutoFillButtonType);

    RefPtr<HTMLElement> m_container;
    RefPtr<HTMLElement> m_innerBlock;
    RefPtr<TextControlInnerTextElement> m_innerText;
    RefPtr<HTMLElement> m_placeholder;
    RefPtr<SpinButtonElement> m_innerSpinButton;
    RefPtr<HTMLElement> m_capsLockIndicator;
    RefPtr<HTMLElement> m_autoFillButton;
};

} // namespace WebCore

#endif // TextFieldInputType_h
