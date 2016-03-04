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
#include <memory>

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
    static Ref<HTMLInputElement> create(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);
    virtual ~HTMLInputElement();

    WEBCORE_EXPORT bool shouldAutocomplete() const override final;

    // For ValidityState
    bool hasBadInput() const override final;
    bool patternMismatch() const override final;
    bool rangeUnderflow() const override final;
    bool rangeOverflow() const override final;
    bool stepMismatch() const override final;
    bool tooLong() const override final;
    bool typeMismatch() const override final;
    bool valueMissing() const override final;
    String validationMessage() const override final;

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
    Optional<Decimal> findClosestTickMarkValue(const Decimal&);
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
    WEBCORE_EXPORT bool isTextField() const;
    WEBCORE_EXPORT bool isSearchField() const;
    bool isInputTypeHidden() const;
    WEBCORE_EXPORT bool isPasswordField() const;
    bool isCheckbox() const;
    bool isRangeControl() const;

#if ENABLE(INPUT_TYPE_COLOR)
    bool isColorControl() const;
#endif

    // FIXME: It's highly likely that any call site calling this function should instead
    // be using a different one. Many input elements behave like text fields, and in addition
    // any unknown input type is treated as text. Consider, for example, isTextField or
    // isTextField && !isPasswordField.
    WEBCORE_EXPORT bool isText() const;

    WEBCORE_EXPORT bool isEmailField() const;
    bool isFileUpload() const;
    bool isImageButton() const;
    WEBCORE_EXPORT bool isNumberField() const;
    bool isSubmitButton() const;
    WEBCORE_EXPORT bool isTelephoneField() const;
    WEBCORE_EXPORT bool isURLField() const;
    WEBCORE_EXPORT bool isDateField() const;
    WEBCORE_EXPORT bool isDateTimeField() const;
    WEBCORE_EXPORT bool isDateTimeLocalField() const;
    WEBCORE_EXPORT bool isMonthField() const;
    WEBCORE_EXPORT bool isTimeField() const;
    WEBCORE_EXPORT bool isWeekField() const;

#if PLATFORM(IOS)
    DateComponents::Type dateType() const;
#endif

    HTMLElement* containerElement() const;
    
    TextControlInnerTextElement* innerTextElement() const override final;
    Ref<RenderStyle> createInnerTextStyle(const RenderStyle&) const override;

    HTMLElement* innerBlockElement() const;
    HTMLElement* innerSpinButtonElement() const;
    HTMLElement* capsLockIndicatorElement() const;
    HTMLElement* resultsButtonElement() const;
    HTMLElement* cancelButtonElement() const;
    HTMLElement* sliderThumbElement() const;
    HTMLElement* sliderTrackElement() const;
    HTMLElement* placeholderElement() const override final;
    WEBCORE_EXPORT HTMLElement* autoFillButtonElement() const;

    bool checked() const { return m_isChecked; }
    void setChecked(bool, TextFieldEventBehavior = DispatchNoEvent);

    // 'indeterminate' is a state independent of the checked state that causes the control to draw in a way that hides the actual state.
    bool indeterminate() const { return m_isIndeterminate; }
    void setIndeterminate(bool);
    // shouldAppearChecked is used by the rendering tree/CSS while checked() is used by JS to determine checked state
    bool shouldAppearChecked() const;
    bool shouldAppearIndeterminate() const override final;

    unsigned size() const;
    bool sizeShouldIncludeDecoration(int& preferredSize) const;
    float decorationWidth() const;

    void setType(const AtomicString&);

    WEBCORE_EXPORT String value() const override final;
    void setValue(const String&, ExceptionCode&, TextFieldEventBehavior = DispatchNoEvent);
    WEBCORE_EXPORT void setValue(const String&, TextFieldEventBehavior = DispatchNoEvent);
    WEBCORE_EXPORT void setValueForUser(const String&);
    // Checks if the specified string would be a valid value.
    // We should not call this for types with no string value such as CHECKBOX and RADIO.
    bool isValidValue(const String&) const;
    bool hasDirtyValue() const { return !m_valueIfDirty.isNull(); };

    String sanitizeValue(const String&) const;

    String localizeValue(const String&) const;

    // The value which is drawn by a renderer.
    String visibleValue() const;

    WEBCORE_EXPORT void setEditingValue(const String&);

    double valueAsDate() const;
    void setValueAsDate(double, ExceptionCode&);

    WEBCORE_EXPORT double valueAsNumber() const;
    WEBCORE_EXPORT void setValueAsNumber(double, ExceptionCode&, TextFieldEventBehavior = DispatchNoEvent);

    String valueWithDefault() const;

    void setValueFromRenderer(const String&);

    bool canHaveSelection() const;

    bool rendererIsNeeded(const RenderStyle&) override final;
    RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override final;
    void willAttachRenderers() override final;
    void didAttachRenderers() override final;
    void didDetachRenderers() override final;

    // FIXME: For isActivatedSubmit and setActivatedSubmit, we should use the NVI-idiom here by making
    // it private virtual in all classes and expose a public method in HTMLFormControlElement to call
    // the private virtual method.
    bool isActivatedSubmit() const override final;
    void setActivatedSubmit(bool flag) override final;

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

    int maxLengthForBindings() const { return m_maxLength; }
    unsigned effectiveMaxLength() const;

    bool multiple() const;

    bool isAutoFilled() const { return m_isAutoFilled; }
    WEBCORE_EXPORT void setAutoFilled(bool = true);

    AutoFillButtonType autoFillButtonType() const { return (AutoFillButtonType)m_autoFillButtonType; }
    WEBCORE_EXPORT void setShowAutoFillButton(AutoFillButtonType);

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

    bool willRespondToMouseClickEvents() override;

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

    Color valueAsColor() const; // Returns transparent color if not type=color.
    WEBCORE_EXPORT void selectColor(const Color&); // Does nothing if not type=color. Simulates user selection of color; intended for testing.

    String defaultToolTip() const;

#if ENABLE(MEDIA_CAPTURE)
    bool shouldUseMediaCapture() const;
#endif

    static const unsigned maxEffectiveLength;

    unsigned height() const;
    unsigned width() const;
    void setHeight(unsigned);
    void setWidth(unsigned);

    void blur() override final;
    void defaultBlur();

    const AtomicString& name() const override final;

    void endEditing();

    static Vector<FileChooserFileInfo> filesFromFileInputFormControlState(const FormControlState&);

    bool matchesReadWritePseudoClass() const override final;
    void setRangeText(const String& replacement, ExceptionCode&) override final;
    void setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode, ExceptionCode&) override final;

    HTMLImageLoader* imageLoader() { return m_imageLoader.get(); }
    HTMLImageLoader& ensureImageLoader();

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    bool setupDateTimeChooserParameters(DateTimeChooserParameters&);
#endif

    void capsLockStateMayHaveChanged();

protected:
    HTMLInputElement(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);

    void defaultEventHandler(Event*) override;

private:
    enum AutoCompleteSetting { Uninitialized, On, Off };

    void didAddUserAgentShadowRoot(ShadowRoot*) override final;
    bool canHaveUserAgentShadowRoot() const override final { return true; }

    void willChangeForm() override final;
    void didChangeForm() override final;
    InsertionNotificationRequest insertedInto(ContainerNode&) override final;
    void finishedInsertingSubtree() override final;
    void removedFrom(ContainerNode&) override final;
    void didMoveToNewDocument(Document* oldDocument) override final;

    bool hasCustomFocusLogic() const override final;
    bool isKeyboardFocusable(KeyboardEvent*) const override final;
    bool isMouseFocusable() const override final;
    bool isEnumeratable() const override final;
    bool supportLabels() const override final;
    void updateFocusAppearance(SelectionRestorationMode, SelectionRevealMode) override final;
    bool shouldUseInputMethod() override final;

    bool isTextFormControl() const override final { return isTextField(); }

    bool canTriggerImplicitSubmission() const override final { return isTextField(); }

    const AtomicString& formControlType() const override final;

    bool shouldSaveAndRestoreFormControlState() const override final;
    FormControlState saveFormControlState() const override final;
    void restoreFormControlState(const FormControlState&) override final;

    bool canStartSelection() const override final;

    void accessKeyAction(bool sendMouseEvents) override final;

    void parseAttribute(const QualifiedName&, const AtomicString&) override final;
    bool isPresentationAttribute(const QualifiedName&) const override final;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override final;
    void finishParsingChildren() override final;
    void parserDidSetAttributes() override final;

    void copyNonAttributePropertiesFromElement(const Element&) override final;

    bool appendFormData(FormDataList&, bool) override final;

    bool isSuccessfulSubmitButton() const override final;

    void reset() override final;

    bool isURLAttribute(const Attribute&) const override final;
    bool isInRange() const override final;
    bool isOutOfRange() const override final;

    void resumeFromDocumentSuspension() override final;
#if ENABLE(INPUT_TYPE_COLOR)
    void prepareForDocumentSuspension() override final;
#endif

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override final;

    bool needsSuspensionCallback();
    void registerForSuspensionCallbackIfNeeded();
    void unregisterForSuspensionCallbackIfNeeded();

    bool supportsMaxLength() const { return isTextType(); }
    bool isTextType() const;
    bool tooLong(const String&, NeedsToCheckDirtyFlag) const;

    bool supportsPlaceholder() const override final;
    void updatePlaceholderText() override final;
    bool isEmptyValue() const override final;
    void handleFocusEvent(Node* oldFocusedNode, FocusDirection) override final;
    void handleBlurEvent() override final;

    bool isOptionalFormControl() const override final { return !isRequiredFormControl(); }
    bool isRequiredFormControl() const override final;
    bool computeWillValidate() const override final;
    void requiredAttributeChanged() override final;

    void initializeInputType();
    void updateType();
    void runPostTypeUpdateTasks();
    
    void subtreeHasChanged() override final;

#if ENABLE(DATALIST_ELEMENT)
    void resetListAttributeTargetObserver();
#endif
    void maxLengthAttributeChanged(const AtomicString& newValue);
    void updateValueIfNeeded();

    // Returns null if this isn't associated with any radio button group.
    CheckedRadioButtons* checkedRadioButtons() const;
    void addToRadioButtonGroup();
    void removeFromRadioButtonGroup();

    AtomicString m_name;
    String m_valueIfDirty;
    unsigned m_size;
    int m_maxLength;
    short m_maxResults;
    bool m_isChecked : 1;
    bool m_reflectsCheckedAttribute : 1;
    bool m_isIndeterminate : 1;
    bool m_hasType : 1;
    bool m_isActivatedSubmit : 1;
    unsigned m_autocomplete : 2; // AutoCompleteSetting
    bool m_isAutoFilled : 1;
    unsigned m_autoFillButtonType : 2; // AutoFillButtonType;
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
    std::unique_ptr<HTMLImageLoader> m_imageLoader;
#if ENABLE(DATALIST_ELEMENT)
    std::unique_ptr<ListAttributeTargetObserver> m_listAttributeTargetObserver;
#endif
};

} //namespace
#endif
