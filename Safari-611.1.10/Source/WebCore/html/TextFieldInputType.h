/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "AutoFillButtonElement.h"
#include "DataListButtonElement.h"
#include "DataListSuggestionPicker.h"
#include "DataListSuggestionsClient.h"
#include "InputType.h"
#include "SpinButtonElement.h"

namespace WebCore {

class DOMFormData;
class TextControlInnerTextElement;

// The class represents types of which UI contain text fields.
// It supports not only the types for BaseTextInputType but also type=number.
class TextFieldInputType : public InputType, protected SpinButtonElement::SpinButtonOwner, protected AutoFillButtonElement::AutoFillButtonOwner
#if ENABLE(DATALIST_ELEMENT)
    , private DataListSuggestionsClient, protected DataListButtonElement::DataListButtonOwner
#endif
{
protected:
    explicit TextFieldInputType(HTMLInputElement&);
    virtual ~TextFieldInputType();
    ShouldCallBaseEventHandler handleKeydownEvent(KeyboardEvent&) override;
    void handleKeydownEventForSpinButton(KeyboardEvent&);
#if ENABLE(DATALIST_ELEMENT)
    void handleClickEvent(MouseEvent&) final;
#endif

    HTMLElement* containerElement() const final;
    HTMLElement* innerBlockElement() const final;
    RefPtr<TextControlInnerTextElement> innerTextElement() const final;
    HTMLElement* innerSpinButtonElement() const final;
    HTMLElement* capsLockIndicatorElement() const final;
    HTMLElement* autoFillButtonElement() const final;
#if ENABLE(DATALIST_ELEMENT)
    HTMLElement* dataListButtonElement() const final;
#endif

    virtual bool needsContainer() const;
    void createShadowSubtree() override;
    void destroyShadowSubtree() override;
    void attributeChanged(const QualifiedName&) override;
    void disabledStateChanged() final;
    void readOnlyStateChanged() final;
    bool supportsReadOnly() const final;
    void handleFocusEvent(Node* oldFocusedNode, FocusDirection) final;
    void handleBlurEvent() final;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    void updateInnerTextValue() final;
    String sanitizeValue(const String&) const override;

    virtual String convertFromVisibleValue(const String&) const;
    virtual void didSetValueByUserEdit();

private:
    bool isKeyboardFocusable(KeyboardEvent*) const final;
    bool isMouseFocusable() const final;
    bool isTextField() const final;
    bool isEmptyValue() const final;
    bool valueMissing(const String&) const final;
    void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent&) final;
    void forwardEvent(Event&) final;
    bool shouldSubmitImplicitly(Event&) final;
    RenderPtr<RenderElement> createInputRenderer(RenderStyle&&) override;
    bool shouldUseInputMethod() const override;
    bool shouldRespectListAttribute() override;
    HTMLElement* placeholderElement() const final;
    void updatePlaceholderText() final;
    bool appendFormData(DOMFormData&, bool multipart) const final;
    void subtreeHasChanged() final;
    void capsLockStateMayHaveChanged() final;
    void updateAutoFillButton() final;
    void elementDidBlur() final;

    // SpinButtonElement::SpinButtonOwner functions.
    void focusAndSelectSpinButtonOwner() final;
    bool shouldSpinButtonRespondToMouseEvents() final;
    bool shouldSpinButtonRespondToWheelEvents() final;
    void spinButtonStepDown() final;
    void spinButtonStepUp() final;

    // AutoFillButtonElement::AutoFillButtonOwner
    void autoFillButtonElementWasClicked() final;

    bool shouldHaveSpinButton() const;
    bool shouldHaveCapsLockIndicator() const;
    bool shouldDrawCapsLockIndicator() const;
    bool shouldDrawAutoFillButton() const;

    void createContainer();
    void createAutoFillButton(AutoFillButtonType);

#if ENABLE(DATALIST_ELEMENT)
    void createDataListDropdownIndicator();
    bool isPresentingAttachedView() const final;
    void dataListMayHaveChanged() final;
    void displaySuggestions(DataListSuggestionActivationType);
    void closeSuggestions();

    // DataListSuggestionsClient
    IntRect elementRectInRootViewCoordinates() const final;
    Vector<DataListSuggestion> suggestions() final;
    void didSelectDataListOption(const String&) final;
    void didCloseSuggestions() final;

    static bool shouldOnlyShowDataListDropdownButtonWhenFocusedOrEdited();

    void dataListButtonElementWasClicked() final;
    RefPtr<DataListButtonElement> m_dataListDropdownIndicator;

    std::pair<String, Vector<DataListSuggestion>> m_cachedSuggestions;
    std::unique_ptr<DataListSuggestionPicker> m_suggestionPicker;
#endif

    RefPtr<HTMLElement> m_container;
    RefPtr<HTMLElement> m_innerBlock;
    RefPtr<TextControlInnerTextElement> m_innerText;
    RefPtr<HTMLElement> m_placeholder;
    RefPtr<SpinButtonElement> m_innerSpinButton;
    RefPtr<HTMLElement> m_capsLockIndicator;
    RefPtr<HTMLElement> m_autoFillButton;
};

} // namespace WebCore
