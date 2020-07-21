/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include "HTMLInputElement.h"
#include "HTMLTextFormControlElement.h"
#include "RenderPtr.h"
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if PLATFORM(IOS_FAMILY)
#include "DateComponents.h"
#endif

namespace WebCore {

class BeforeTextInsertedEvent;
class Chrome;
class DOMFormData;
class DateComponents;
class DragData;
class Event;
class FileList;
class HTMLElement;
class HTMLFormElement;
class Icon;
class KeyboardEvent;
class MouseEvent;
class Node;
class RenderStyle;
class StepRange;
class TextControlInnerTextElement;
class TouchEvent;

struct InputElementClickState;

enum class AnyStepHandling : bool;

// An InputType object represents the type-specific part of an HTMLInputElement.
// Do not expose instances of InputType and classes derived from it to classes
// other than HTMLInputElement.
class InputType : public RefCounted<InputType> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    static Ref<InputType> create(HTMLInputElement&, const AtomString&);
    static Ref<InputType> createText(HTMLInputElement&);
    virtual ~InputType();

    void detachFromElement() { m_element = nullptr; }

    static bool themeSupportsDataListUI(InputType*);

    virtual const AtomString& formControlType() const = 0;

    // Type query functions.

    // Any time we are using one of these functions it's best to refactor
    // to add a virtual function to allow the input type object to do the
    // work instead, or at least make a query function that asks a higher
    // level question. These functions make the HTMLInputElement class
    // inflexible because it's harder to add new input types if there is
    // scattered code with special cases for various types.

    virtual bool isCheckbox() const;
    virtual bool isColorControl() const;
    virtual bool isDateField() const;
    virtual bool isDateTimeField() const;
    virtual bool isDateTimeLocalField() const;
    virtual bool isEmailField() const;
    virtual bool isFileUpload() const;
    virtual bool isHiddenType() const;
    virtual bool isImageButton() const;
    virtual bool isInteractiveContent() const;
    virtual bool supportLabels() const;
    virtual bool isMonthField() const;
    virtual bool isNumberField() const;
    virtual bool isPasswordField() const;
    virtual bool isRadioButton() const;
    virtual bool isRangeControl() const;
    virtual bool isSearchField() const;
    virtual bool isSubmitButton() const;
    virtual bool isTelephoneField() const;
    virtual bool isTextButton() const;
    virtual bool isTextField() const;
    virtual bool isTextType() const;
    virtual bool isTimeField() const;
    virtual bool isURLField() const;
    virtual bool isWeekField() const;

    // Form value functions.

    virtual bool shouldSaveAndRestoreFormControlState() const;
    virtual FormControlState saveFormControlState() const;
    virtual void restoreFormControlState(const FormControlState&);
    virtual bool isFormDataAppendable() const;
    virtual bool appendFormData(DOMFormData&, bool multipart) const;

    // DOM property functions.

    virtual bool getTypeSpecificValue(String&); // Checked first, before internal storage or the value attribute.
    virtual String fallbackValue() const; // Checked last, if both internal storage and value attribute are missing.
    virtual String defaultValue() const; // Checked after even fallbackValue, only when the valueWithDefault function is called.
    virtual double valueAsDate() const;
    virtual ExceptionOr<void> setValueAsDate(double) const;
    virtual double valueAsDouble() const;
    virtual ExceptionOr<void> setValueAsDouble(double, TextFieldEventBehavior) const;
    virtual ExceptionOr<void> setValueAsDecimal(const Decimal&, TextFieldEventBehavior) const;

    // Validation functions.

    virtual String validationMessage() const;
    virtual bool supportsValidation() const;
    virtual bool typeMismatchFor(const String&) const;
    virtual bool supportsRequired() const;
    virtual bool valueMissing(const String&) const;
    virtual bool hasBadInput() const;
    virtual bool patternMismatch(const String&) const;
    bool rangeUnderflow(const String&) const;
    bool rangeOverflow(const String&) const;
    bool isInRange(const String&) const;
    bool isOutOfRange(const String&) const;
    virtual Decimal defaultValueForStepUp() const;
    double minimum() const;
    double maximum() const;
    virtual bool sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const;
    virtual float decorationWidth() const;
    bool stepMismatch(const String&) const;
    virtual bool getAllowedValueStep(Decimal*) const;
    virtual StepRange createStepRange(AnyStepHandling) const;
    virtual ExceptionOr<void> stepUp(int);
    virtual void stepUpFromRenderer(int);
    virtual String badInputText() const;
    virtual String typeMismatchText() const;
    virtual String valueMissingText() const;
    virtual bool canSetStringValue() const;
    virtual String localizeValue(const String&) const;
    virtual String visibleValue() const;
    virtual bool isEmptyValue() const;

    // Type check for the current input value. We do nothing for some types
    // though typeMismatchFor() does something for them because of value sanitization.
    virtual bool typeMismatch() const;

    // Return value of null string means "use the default value".
    // This function must be called only by HTMLInputElement::sanitizeValue().
    virtual String sanitizeValue(const String&) const;

    // Event handlers.

    virtual void handleClickEvent(MouseEvent&);
    virtual void handleMouseDownEvent(MouseEvent&);
    virtual void willDispatchClick(InputElementClickState&);
    virtual void didDispatchClick(Event&, const InputElementClickState&);
    virtual void handleDOMActivateEvent(Event&);

    enum ShouldCallBaseEventHandler { No, Yes };
    virtual ShouldCallBaseEventHandler handleKeydownEvent(KeyboardEvent&);

    virtual void handleKeypressEvent(KeyboardEvent&);
    virtual void handleKeyupEvent(KeyboardEvent&);
    virtual void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent&);
    virtual void forwardEvent(Event&);

#if ENABLE(TOUCH_EVENTS)
    virtual void handleTouchEvent(TouchEvent&);
#endif

    // Helpers for event handlers.

    virtual bool shouldSubmitImplicitly(Event&);
    virtual bool hasCustomFocusLogic() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;
    virtual bool shouldUseInputMethod() const;
    virtual void handleFocusEvent(Node* oldFocusedNode, FocusDirection);
    virtual void handleBlurEvent();
    virtual bool accessKeyAction(bool sendMouseEvents);
    virtual bool canBeSuccessfulSubmitButton();
    virtual void subtreeHasChanged();
    virtual void blur();

    virtual void elementDidBlur() { }

#if ENABLE(TOUCH_EVENTS)
    virtual bool hasTouchEventHandler() const;
#endif

    // Shadow tree handling.

    virtual void createShadowSubtree();
    virtual void destroyShadowSubtree();

    virtual HTMLElement* containerElement() const { return nullptr; }
    virtual HTMLElement* innerBlockElement() const { return nullptr; }
    virtual RefPtr<TextControlInnerTextElement> innerTextElement() const;
    virtual HTMLElement* innerSpinButtonElement() const { return nullptr; }
    virtual HTMLElement* capsLockIndicatorElement() const { return nullptr; }
    virtual HTMLElement* autoFillButtonElement() const { return nullptr; }
    virtual HTMLElement* resultsButtonElement() const { return nullptr; }
    virtual HTMLElement* cancelButtonElement() const { return nullptr; }
    virtual HTMLElement* sliderThumbElement() const { return nullptr; }
    virtual HTMLElement* sliderTrackElement() const { return nullptr; }
    virtual HTMLElement* placeholderElement() const;
#if ENABLE(DATALIST_ELEMENT)
    virtual HTMLElement* dataListButtonElement() const { return nullptr; }
#endif

    // Miscellaneous functions.

    virtual bool rendererIsNeeded();
    virtual RenderPtr<RenderElement> createInputRenderer(RenderStyle&&);
    virtual void addSearchResult();
    virtual void attach();
    virtual void detach();
    virtual bool shouldRespectAlignAttribute();
    virtual FileList* files();
    virtual void setFiles(RefPtr<FileList>&&);
    virtual Icon* icon() const;
    virtual bool shouldSendChangeEventAfterCheckedChanged();
    virtual bool canSetValue(const String&);
    virtual bool storesValueSeparateFromAttribute();
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior);
    virtual bool shouldResetOnDocumentActivation();
    virtual bool shouldRespectListAttribute();
    virtual bool isEnumeratable();
    virtual bool isCheckable();
    virtual bool isSteppable() const;
    virtual bool shouldRespectHeightAndWidthAttributes();
    virtual bool supportsPlaceholder() const;
    virtual bool supportsReadOnly() const;
    virtual void updateInnerTextValue();
    virtual void updatePlaceholderText();
    virtual void attributeChanged(const QualifiedName&) { }
    virtual void disabledStateChanged() { }
    virtual void readOnlyStateChanged() { }
    virtual void requiredStateChanged() { }
    virtual void capsLockStateMayHaveChanged();
    virtual void updateAutoFillButton();
    virtual String defaultToolTip() const;
    virtual bool matchesIndeterminatePseudoClass() const;
    virtual bool shouldAppearIndeterminate() const;
    virtual bool isPresentingAttachedView() const;
    virtual bool supportsSelectionAPI() const;
    virtual Color valueAsColor() const;
    virtual void selectColor(StringView);
    virtual Vector<Color> suggestedColors() const;

    // Parses the specified string for the type, and return
    // the Decimal value for the parsing result if the parsing
    // succeeds; Returns defaultValue otherwise. This function can
    // return NaN or Infinity only if defaultValue is NaN or Infinity.
    virtual Decimal parseToNumber(const String&, const Decimal& defaultValue) const;

    // Create a string representation of the specified Decimal value for the
    // input type. If NaN or Infinity is specified, this returns an empty
    // string. This should not be called for types without valueAsNumber.
    virtual String serialize(const Decimal&) const;

    // Gets width and height of the input element if the type of the
    // element is image. It returns 0 if the element is not image type.
    virtual unsigned height() const;
    virtual unsigned width() const;

    void dispatchSimulatedClickIfActive(KeyboardEvent&) const;

#if ENABLE(DATALIST_ELEMENT)
    virtual void dataListMayHaveChanged();
    virtual Optional<Decimal> findClosestTickMarkValue(const Decimal&);
#endif

#if ENABLE(DRAG_SUPPORT)
    virtual bool receiveDroppedFiles(const DragData&);
#endif

    virtual DateComponents::Type dateType() const;

    virtual String displayString() const;

protected:
    explicit InputType(HTMLInputElement& element)
        : m_element(makeWeakPtr(element)) { }
    HTMLInputElement* element() const { return m_element.get(); }
    Chrome* chrome() const;
    Decimal parseToNumberOrNaN(const String&) const;

private:
    // Helper for stepUp()/stepDown(). Adds step value * count to the current value.
    ExceptionOr<void> applyStep(int count, AnyStepHandling, TextFieldEventBehavior);

    // m_element is null if this InputType is no longer associated with an element (either the element died or changed input type).
    WeakPtr<HTMLInputElement> m_element;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_INPUT_TYPE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::InputType& input) { return input.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
