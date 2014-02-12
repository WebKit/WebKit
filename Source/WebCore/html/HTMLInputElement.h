/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLInputElement_h
#define HTMLInputElement_h

#include "FileChooser.h"
#include "HTMLTextFormControlElement.h"
#include "StepRange.h"

#if PLATFORM(IOS)
#include "DateComponents.h"
#endif

namespace WebCore {

class CheckedRadioButtons;
class DragData;
class FileList;
class HTMLDataListElement;
class HTMLImageLoader;
class HTMLOptionElement;
class Icon;
class InputType;
class ListAttributeTargetObserver;
class TextControlInnerTextElement;
class URL;
struct DateTimeChooserParameters;

struct InputElementClickState {
    InputElementClickState()
        : stateful(false)
        , checked(false)
        , indeterminate(false)
    { }
    bool stateful;
    bool checked;
    bool indeterminate;
    RefPtr<HTMLInputElement> checkedRadioButton;
};

class HTMLInputElement : public HTMLTextFormControlElement {
public:
    static PassRefPtr<HTMLInputElement> create(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);
    virtual ~HTMLInputElement();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitspeechchange);

    virtual HTMLInputElement* toInputElement() override { return this; }

    virtual bool shouldAutocomplete() const override;

    // For ValidityState
    virtual bool hasBadInput() const override;
    virtual bool patternMismatch() const override;
    virtual bool rangeUnderflow() const override;
    virtual bool rangeOverflow() const override;
    virtual bool stepMismatch() const override;
    virtual bool tooLong() const override;
    virtual bool typeMismatch() const override;
    virtual bool valueMissing() const override;
    virtual String validationMessage() const override;

    // Returns the minimum value for type=date, number, or range.  Don't call this for other types.
    double minimum() const;
    // Returns the maximum value for type=date, number, or range.  Don't call this for other types.
    // This always returns a value which is >= minimum().
    double maximum() const;
    // Sets the "allowed value step" defined in the HTML spec to the specified double pointer.
    // Returns false if there is no "allowed value step."
    bool getAllowedValueStep(Decimal*) const;
    StepRange createStepRange(AnyStepHandling) const;

#if ENABLE(DATALIST_ELEMENT)
    Decimal findClosestTickMarkValue(const Decimal&);
#endif

    // Implementations of HTMLInputElement::stepUp() and stepDown().
    void stepUp(int, ExceptionCode&);
    void stepDown(int, ExceptionCode&);
    void stepUp(ExceptionCode& ec) { stepUp(1, ec); }
    void stepDown(ExceptionCode& ec) { stepDown(1, ec); }
    // stepUp()/stepDown() for user-interaction.
    bool isSteppable() const;

    bool isTextButton() const;

    bool isRadioButton() const;
    bool isTextField() const;
    bool isSearchField() const;
    bool isInputTypeHidden() const;
    bool isPasswordField() const;
    bool isCheckbox() const;
    bool isRangeControl() const;

#if ENABLE(INPUT_TYPE_COLOR)
    bool isColorControl() const;
#endif

    // FIXME: It's highly likely that any call site calling this function should instead
    // be using a different one. Many input elements behave like text fields, and in addition
    // any unknown input type is treated as text. Consider, for example, isTextField or
    // isTextField && !isPasswordField.
    bool isText() const;

    bool isEmailField() const;
    bool isFileUpload() const;
    bool isImageButton() const;
    bool isNumberField() const;
    bool isSubmitButton() const;
    bool isTelephoneField() const;
    bool isURLField() const;
    bool isDateField() const;
    bool isDateTimeField() const;
    bool isDateTimeLocalField() const;
    bool isMonthField() const;
    bool isTimeField() const;
    bool isWeekField() const;

#if ENABLE(INPUT_SPEECH)
    bool isSpeechEnabled() const;
#endif

#if PLATFORM(IOS)
    DateComponents::Type dateType() const;
#endif

    HTMLElement* containerElement() const;
    virtual TextControlInnerTextElement* innerTextElement() const override;
    HTMLElement* innerBlockElement() const;
    HTMLElement* innerSpinButtonElement() const;
    HTMLElement* resultsButtonElement() const;
    HTMLElement* cancelButtonElement() const;
#if ENABLE(INPUT_SPEECH)
    HTMLElement* speechButtonElement() const;
#endif
    HTMLElement* sliderThumbElement() const;
    HTMLElement* sliderTrackElement() const;
    virtual HTMLElement* placeholderElement() const override;

    bool checked() const { return m_isChecked; }
    void setChecked(bool, TextFieldEventBehavior = DispatchNoEvent);

    // 'indeterminate' is a state independent of the checked state that causes the control to draw in a way that hides the actual state.
    bool indeterminate() const { return m_isIndeterminate; }
    void setIndeterminate(bool);
    // shouldAppearChecked is used by the rendering tree/CSS while checked() is used by JS to determine checked state
    bool shouldAppearChecked() const;
    virtual bool shouldAppearIndeterminate() const override;

    int size() const;
    bool sizeShouldIncludeDecoration(int& preferredSize) const;
    float decorationWidth() const;

    void setType(const String&);

    virtual String value() const override;
    void setValue(const String&, ExceptionCode&, TextFieldEventBehavior = DispatchNoEvent);
    void setValue(const String&, TextFieldEventBehavior = DispatchNoEvent);
    void setValueForUser(const String&);
    // Checks if the specified string would be a valid value.
    // We should not call this for types with no string value such as CHECKBOX and RADIO.
    bool isValidValue(const String&) const;
    bool hasDirtyValue() const { return !m_valueIfDirty.isNull(); };

    String sanitizeValue(const String&) const;

    String localizeValue(const String&) const;

    // The value which is drawn by a renderer.
    String visibleValue() const;

    const String& suggestedValue() const;
    void setSuggestedValue(const String&);

    void setEditingValue(const String&);

    double valueAsDate() const;
    void setValueAsDate(double, ExceptionCode&);

    double valueAsNumber() const;
    void setValueAsNumber(double, ExceptionCode&, TextFieldEventBehavior = DispatchNoEvent);

    String valueWithDefault() const;

    void setValueFromRenderer(const String&);

    bool canHaveSelection() const;

    virtual bool rendererIsNeeded(const RenderStyle&) override;
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual void willAttachRenderers() override;
    virtual void didAttachRenderers() override;
    virtual void didDetachRenderers() override;

    // FIXME: For isActivatedSubmit and setActivatedSubmit, we should use the NVI-idiom here by making
    // it private virtual in all classes and expose a public method in HTMLFormControlElement to call
    // the private virtual method.
    virtual bool isActivatedSubmit() const override;
    virtual void setActivatedSubmit(bool flag) override;

    String altText() const;

    void willDispatchEvent(Event&, InputElementClickState&);
    void didDispatchClickEvent(Event&, const InputElementClickState&);

    int maxResults() const { return m_maxResults; }

    String defaultValue() const;
    void setDefaultValue(const String&);

    Vector<String> acceptMIMETypes();
    Vector<String> acceptFileExtensions();
    String accept() const;
    String alt() const;

    void setSize(unsigned);
    void setSize(unsigned, ExceptionCode&);

    URL src() const;

    virtual int maxLength() const override;
    void setMaxLength(int, ExceptionCode&);

    bool multiple() const;

    bool isAutofilled() const { return m_isAutofilled; }
    void setAutofilled(bool = true);

    FileList* files();
    void setFiles(PassRefPtr<FileList>);

#if ENABLE(DRAG_SUPPORT)
    // Returns true if the given DragData has more than one dropped files.
    bool receiveDroppedFiles(const DragData&);
#endif

    Icon* icon() const;
#if PLATFORM(IOS)
    String displayString() const;
#endif
    // These functions are used for rendering the input active during a
    // drag-and-drop operation.
    bool canReceiveDroppedFiles() const;
    void setCanReceiveDroppedFiles(bool);

    void addSearchResult();
    void onSearch();

    void updateClearButtonVisibility();

    virtual bool willRespondToMouseClickEvents() override;

#if ENABLE(DATALIST_ELEMENT)
    HTMLElement* list() const;
    HTMLDataListElement* dataList() const;
    void listAttributeTargetChanged();
#endif

    HTMLInputElement* checkedRadioButtonForGroup() const;
    bool isInRequiredRadioButtonGroup();

    // Functions for InputType classes.
    void setValueInternal(const String&, TextFieldEventBehavior);
    bool isTextFormControlFocusable() const;
    bool isTextFormControlKeyboardFocusable(KeyboardEvent*) const;
    bool isTextFormControlMouseFocusable() const;
    bool valueAttributeWasUpdatedAfterParsing() const { return m_valueAttributeWasUpdatedAfterParsing; }

    void cacheSelectionInResponseToSetValue(int caretOffset) { cacheSelection(caretOffset, caretOffset, SelectionHasNoDirection); }

#if ENABLE(INPUT_TYPE_COLOR)
    // For test purposes.
    void selectColorInColorChooser(const Color&);
#endif

    String defaultToolTip() const;

#if ENABLE(MEDIA_CAPTURE)
    bool shouldUseMediaCapture() const;
#endif

    static const int maximumLength;

    unsigned height() const;
    unsigned width() const;
    void setHeight(unsigned);
    void setWidth(unsigned);

    virtual void blur() override;
    void defaultBlur();

    virtual const AtomicString& name() const override;

    void endEditing();

    static Vector<FileChooserFileInfo> filesFromFileInputFormControlState(const FormControlState&);

    virtual bool matchesReadOnlyPseudoClass() const override;
    virtual bool matchesReadWritePseudoClass() const override;
    virtual void setRangeText(const String& replacement, ExceptionCode&) override;
    virtual void setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode, ExceptionCode&) override;

    bool hasImageLoader() const { return m_imageLoader; }
    HTMLImageLoader* imageLoader();

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    bool setupDateTimeChooserParameters(DateTimeChooserParameters&);
#endif

protected:
    HTMLInputElement(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);

    virtual void defaultEventHandler(Event*) override;

private:
    enum AutoCompleteSetting { Uninitialized, On, Off };

    // FIXME: Author shadows should be allowed
    // https://bugs.webkit.org/show_bug.cgi?id=92608
    virtual bool areAuthorShadowsAllowed() const override { return false; }

    virtual void didAddUserAgentShadowRoot(ShadowRoot*) override;

    virtual void willChangeForm() override;
    virtual void didChangeForm() override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;
    virtual void didMoveToNewDocument(Document* oldDocument) override;

    virtual bool hasCustomFocusLogic() const override;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const override;
    virtual bool isMouseFocusable() const override;
    virtual bool isEnumeratable() const override;
    virtual bool supportLabels() const override;
    virtual void updateFocusAppearance(bool restorePreviousSelection) override;
    virtual bool shouldUseInputMethod() override final;

    virtual bool isTextFormControl() const override { return isTextField(); }

    virtual bool canTriggerImplicitSubmission() const override { return isTextField(); }

    virtual const AtomicString& formControlType() const override;

    virtual bool shouldSaveAndRestoreFormControlState() const override;
    virtual FormControlState saveFormControlState() const override;
    virtual void restoreFormControlState(const FormControlState&) override;

    virtual bool canStartSelection() const override;

    virtual void accessKeyAction(bool sendMouseEvents) override;

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    virtual void finishParsingChildren() override;

    virtual void copyNonAttributePropertiesFromElement(const Element&) override;

    virtual bool appendFormData(FormDataList&, bool) override;

    virtual bool isSuccessfulSubmitButton() const override;

    virtual void reset() override;

    virtual bool isURLAttribute(const Attribute&) const override;
    virtual bool isInRange() const override;
    virtual bool isOutOfRange() const override;

    virtual void documentDidResumeFromPageCache() override;
#if ENABLE(INPUT_TYPE_COLOR)
    virtual void documentWillSuspendForPageCache() override;
#endif

    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    bool needsSuspensionCallback();
    void registerForSuspensionCallbackIfNeeded();
    void unregisterForSuspensionCallbackIfNeeded();

    bool supportsMaxLength() const { return isTextType(); }
    bool isTextType() const;
    bool tooLong(const String&, NeedsToCheckDirtyFlag) const;

    virtual bool supportsPlaceholder() const override;
    virtual void updatePlaceholderText() override;
    virtual bool isEmptyValue() const override { return innerTextValue().isEmpty(); }
    virtual bool isEmptySuggestedValue() const override { return suggestedValue().isEmpty(); }
    virtual void handleFocusEvent(Node* oldFocusedNode, FocusDirection) override;
    virtual void handleBlurEvent() override;

    virtual bool isOptionalFormControl() const override { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const override;
    virtual bool recalcWillValidate() const override;
    virtual void requiredAttributeChanged() override;

    void updateType();
    
    virtual void subtreeHasChanged() override;

#if ENABLE(DATALIST_ELEMENT)
    void resetListAttributeTargetObserver();
#endif
    void parseMaxLengthAttribute(const AtomicString&);
    void updateValueIfNeeded();

    // Returns null if this isn't associated with any radio button group.
    CheckedRadioButtons* checkedRadioButtons() const;
    void addToRadioButtonGroup();
    void removeFromRadioButtonGroup();

    AtomicString m_name;
    String m_valueIfDirty;
    String m_suggestedValue;
    int m_size;
    int m_maxLength;
    short m_maxResults;
    bool m_isChecked : 1;
    bool m_reflectsCheckedAttribute : 1;
    bool m_isIndeterminate : 1;
    bool m_hasType : 1;
    bool m_isActivatedSubmit : 1;
    unsigned m_autocomplete : 2; // AutoCompleteSetting
    bool m_isAutofilled : 1;
#if ENABLE(DATALIST_ELEMENT)
    bool m_hasNonEmptyList : 1;
#endif
    bool m_stateRestored : 1;
    bool m_parsingInProgress : 1;
    bool m_valueAttributeWasUpdatedAfterParsing : 1;
    bool m_wasModifiedByUser : 1;
    bool m_canReceiveDroppedFiles : 1;
#if ENABLE(TOUCH_EVENTS)
    bool m_hasTouchEventHandler : 1;
#endif
    std::unique_ptr<InputType> m_inputType;
    // The ImageLoader must be owned by this element because the loader code assumes
    // that it lives as long as its owning element lives. If we move the loader into
    // the ImageInput object we may delete the loader while this element lives on.
    OwnPtr<HTMLImageLoader> m_imageLoader;
#if ENABLE(DATALIST_ELEMENT)
    OwnPtr<ListAttributeTargetObserver> m_listAttributeTargetObserver;
#endif
};

NODE_TYPE_CASTS(HTMLInputElement)

} //namespace
#endif
