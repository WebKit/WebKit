/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "FileChooser.h"
#include "HTMLTextFormControlElement.h"
#include "StepRange.h"
#include <memory>
#include <wtf/WeakPtr.h>

#if PLATFORM(IOS)
#include "DateComponents.h"
#endif

namespace WebCore {

class DragData;
class FileList;
class HTMLDataListElement;
class HTMLImageLoader;
class Icon;
class InputType;
class ListAttributeTargetObserver;
class RadioButtonGroups;
class URL;

struct DateTimeChooserParameters;

struct InputElementClickState {
    bool stateful { false };
    bool checked { false };
    bool indeterminate { false };
    RefPtr<HTMLInputElement> checkedRadioButton;
};

class HTMLInputElement : public HTMLTextFormControlElement, public CanMakeWeakPtr<HTMLInputElement> {
    WTF_MAKE_ISO_ALLOCATED(HTMLInputElement);
public:
    static Ref<HTMLInputElement> create(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);
    virtual ~HTMLInputElement();

    WEBCORE_EXPORT bool shouldAutocomplete() const final;

    // For ValidityState
    bool hasBadInput() const final;
    bool patternMismatch() const final;
    bool rangeUnderflow() const final;
    bool rangeOverflow() const final;
    bool stepMismatch() const final;
    bool tooShort() const final;
    bool tooLong() const final;
    bool typeMismatch() const final;
    bool valueMissing() const final;
    bool isValid() const final;
    WEBCORE_EXPORT String validationMessage() const final;

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
    std::optional<Decimal> findClosestTickMarkValue(const Decimal&);
#endif

    WEBCORE_EXPORT ExceptionOr<void> stepUp(int = 1);
    WEBCORE_EXPORT ExceptionOr<void> stepDown(int = 1);

    bool isPresentingAttachedView() const;

    // stepUp()/stepDown() for user-interaction.
    bool isSteppable() const;

    bool isTextButton() const;

    bool isRadioButton() const;
    WEBCORE_EXPORT bool isTextField() const final;
    WEBCORE_EXPORT bool isSearchField() const;
    bool isInputTypeHidden() const;
    WEBCORE_EXPORT bool isPasswordField() const;
    bool isCheckbox() const;
    bool isRangeControl() const;

#if ENABLE(INPUT_TYPE_COLOR)
    WEBCORE_EXPORT bool isColorControl() const;
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
    
    RefPtr<TextControlInnerTextElement> innerTextElement() const final;
    RenderStyle createInnerTextStyle(const RenderStyle&) override;

    HTMLElement* innerBlockElement() const;
    HTMLElement* innerSpinButtonElement() const;
    HTMLElement* capsLockIndicatorElement() const;
    HTMLElement* resultsButtonElement() const;
    HTMLElement* cancelButtonElement() const;
    HTMLElement* sliderThumbElement() const;
    HTMLElement* sliderTrackElement() const;
    HTMLElement* placeholderElement() const final;
    WEBCORE_EXPORT HTMLElement* autoFillButtonElement() const;
#if ENABLE(DATALIST_ELEMENT)
    WEBCORE_EXPORT HTMLElement* dataListButtonElement() const;
#endif

    bool checked() const { return m_isChecked; }
    WEBCORE_EXPORT void setChecked(bool);

    // 'indeterminate' is a state independent of the checked state that causes the control to draw in a way that hides the actual state.
    bool indeterminate() const { return m_isIndeterminate; }
    WEBCORE_EXPORT void setIndeterminate(bool);
    // shouldAppearChecked is used by the rendering tree/CSS while checked() is used by JS to determine checked state
    bool shouldAppearChecked() const;
    bool matchesIndeterminatePseudoClass() const final;
    bool shouldAppearIndeterminate() const final;

    WEBCORE_EXPORT unsigned size() const;
    bool sizeShouldIncludeDecoration(int& preferredSize) const;
    float decorationWidth() const;

    WEBCORE_EXPORT void setType(const AtomicString&);

    WEBCORE_EXPORT String value() const final;
    WEBCORE_EXPORT ExceptionOr<void> setValue(const String&, TextFieldEventBehavior = DispatchNoEvent);
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

    WEBCORE_EXPORT double valueAsDate() const;
    WEBCORE_EXPORT ExceptionOr<void> setValueAsDate(double);

    WEBCORE_EXPORT double valueAsNumber() const;
    WEBCORE_EXPORT ExceptionOr<void> setValueAsNumber(double, TextFieldEventBehavior = DispatchNoEvent);

    String valueWithDefault() const;

    // This function dispatches 'input' event for non-textfield types. Callers
    // need to handle any DOM structure changes by event handlers, or need to
    // delay the 'input' event with EventQueueScope.
    void setValueFromRenderer(const String&);

    bool canHaveSelection() const;

    bool rendererIsNeeded(const RenderStyle&) final;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    void willAttachRenderers() final;
    void didAttachRenderers() final;
    void didDetachRenderers() final;

    // FIXME: For isActivatedSubmit and setActivatedSubmit, we should use the NVI-idiom here by making
    // it private virtual in all classes and expose a public method in HTMLFormControlElement to call
    // the private virtual method.
    bool isActivatedSubmit() const final;
    void setActivatedSubmit(bool flag) final;

    String altText() const;

    void willDispatchEvent(Event&, InputElementClickState&);
    void didDispatchClickEvent(Event&, const InputElementClickState&);

    void didBlur();

    int maxResults() const { return m_maxResults; }

    WEBCORE_EXPORT String defaultValue() const;
    WEBCORE_EXPORT void setDefaultValue(const String&);

    Vector<String> acceptMIMETypes();
    Vector<String> acceptFileExtensions();
    String accept() const;
    WEBCORE_EXPORT String alt() const;

    WEBCORE_EXPORT ExceptionOr<void> setSize(unsigned);

    URL src() const;

    unsigned effectiveMaxLength() const;

    bool multiple() const;

    bool isAutoFilled() const { return m_isAutoFilled; }
    WEBCORE_EXPORT void setAutoFilled(bool = true);

    AutoFillButtonType lastAutoFillButtonType() const { return static_cast<AutoFillButtonType>(m_lastAutoFillButtonType); }
    AutoFillButtonType autoFillButtonType() const { return static_cast<AutoFillButtonType>(m_autoFillButtonType); }
    WEBCORE_EXPORT void setShowAutoFillButton(AutoFillButtonType);

    bool hasAutoFillStrongPasswordButton() const  { return autoFillButtonType() == AutoFillButtonType::StrongPassword; }

    bool isAutoFillAvailable() const { return m_isAutoFillAvailable; }
    void setAutoFillAvailable(bool autoFillAvailable) { m_isAutoFillAvailable = autoFillAvailable; }

    WEBCORE_EXPORT FileList* files();
    WEBCORE_EXPORT void setFiles(RefPtr<FileList>&&);

#if ENABLE(DRAG_SUPPORT)
    // Returns true if the given DragData has more than one dropped files.
    bool receiveDroppedFiles(const DragData&);
#endif

    Icon* icon() const;
    String displayString() const;

    // These functions are used for rendering the input active during a
    // drag-and-drop operation.
    bool canReceiveDroppedFiles() const;
    void setCanReceiveDroppedFiles(bool);

    void addSearchResult();
    void onSearch();

    bool willRespondToMouseClickEvents() override;

#if ENABLE(DATALIST_ELEMENT)
    WEBCORE_EXPORT RefPtr<HTMLElement> list() const;
    RefPtr<HTMLDataListElement> dataList() const;
    void listAttributeTargetChanged();
#endif

    Vector<HTMLInputElement*> radioButtonGroup() const;
    HTMLInputElement* checkedRadioButtonForGroup() const;
    bool isInRequiredRadioButtonGroup();
    // Returns null if this isn't associated with any radio button group.
    RadioButtonGroups* radioButtonGroups() const;

    // Functions for InputType classes.
    void setValueInternal(const String&, TextFieldEventBehavior);
    bool isTextFormControlFocusable() const;
    bool isTextFormControlKeyboardFocusable(KeyboardEvent*) const;
    bool isTextFormControlMouseFocusable() const;
    bool valueAttributeWasUpdatedAfterParsing() const { return m_valueAttributeWasUpdatedAfterParsing; }

    void cacheSelectionInResponseToSetValue(int caretOffset) { cacheSelection(caretOffset, caretOffset, SelectionHasNoDirection); }

    Color valueAsColor() const; // Returns transparent color if not type=color.
    WEBCORE_EXPORT void selectColor(StringView); // Does nothing if not type=color. Simulates user selection of color; intended for testing.
    WEBCORE_EXPORT Vector<Color> suggestedColors() const;

    String defaultToolTip() const;

#if ENABLE(MEDIA_CAPTURE)
    MediaCaptureType mediaCaptureType() const;
#endif

    static const unsigned maxEffectiveLength;

    WEBCORE_EXPORT unsigned height() const;
    WEBCORE_EXPORT unsigned width() const;
    WEBCORE_EXPORT void setHeight(unsigned);
    WEBCORE_EXPORT void setWidth(unsigned);

    void blur() final;
    void defaultBlur();

    const AtomicString& name() const final;

    void endEditing();

    void setSpellcheckDisabledExceptTextReplacement(bool disabled) { m_isSpellcheckDisabledExceptTextReplacement = disabled; }
    bool isSpellcheckDisabledExceptTextReplacement() const { return m_isSpellcheckDisabledExceptTextReplacement; }

    static Vector<FileChooserFileInfo> filesFromFileInputFormControlState(const FormControlState&);

    bool matchesReadWritePseudoClass() const final;
    WEBCORE_EXPORT ExceptionOr<void> setRangeText(const String& replacement) final;
    WEBCORE_EXPORT ExceptionOr<void> setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode) final;

    HTMLImageLoader* imageLoader() { return m_imageLoader.get(); }
    HTMLImageLoader& ensureImageLoader();

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    bool setupDateTimeChooserParameters(DateTimeChooserParameters&);
#endif

    void capsLockStateMayHaveChanged();

    bool shouldTruncateText(const RenderStyle&) const;

    ExceptionOr<int> selectionStartForBindings() const;
    ExceptionOr<void> setSelectionStartForBindings(int);

    ExceptionOr<int> selectionEndForBindings() const;
    ExceptionOr<void> setSelectionEndForBindings(int);

    ExceptionOr<String> selectionDirectionForBindings() const;
    ExceptionOr<void> setSelectionDirectionForBindings(const String&);

    ExceptionOr<void> setSelectionRangeForBindings(int start, int end, const String& direction);

protected:
    HTMLInputElement(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);

    void defaultEventHandler(Event&) override;

private:
    enum AutoCompleteSetting { Uninitialized, On, Off };

    void didAddUserAgentShadowRoot(ShadowRoot&) final;

    void willChangeForm() final;
    void didChangeForm() final;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void didFinishInsertingNode() final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;

    bool hasCustomFocusLogic() const final;
    bool isKeyboardFocusable(KeyboardEvent*) const final;
    bool isMouseFocusable() const final;
    bool isEnumeratable() const final;
    bool supportLabels() const final;
    void updateFocusAppearance(SelectionRestorationMode, SelectionRevealMode) final;
    bool shouldUseInputMethod() final;

    bool isInnerTextElementEditable() const final { return !hasAutoFillStrongPasswordButton() && HTMLTextFormControlElement::isInnerTextElementEditable(); }

    bool canTriggerImplicitSubmission() const final { return isTextField(); }

    const AtomicString& formControlType() const final;

    bool shouldSaveAndRestoreFormControlState() const final;
    FormControlState saveFormControlState() const final;
    void restoreFormControlState(const FormControlState&) final;

    void resignStrongPasswordAppearance();

    bool canStartSelection() const final;

    void accessKeyAction(bool sendMouseEvents) final;

    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    bool isPresentationAttribute(const QualifiedName&) const final;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) final;
    void finishParsingChildren() final;
    void parserDidSetAttributes() final;

    void copyNonAttributePropertiesFromElement(const Element&) final;

    bool appendFormData(DOMFormData&, bool) final;

    bool isSuccessfulSubmitButton() const final;
    bool matchesDefaultPseudoClass() const final;

    void reset() final;

    bool isURLAttribute(const Attribute&) const final;
    bool isInRange() const final;
    bool isOutOfRange() const final;

    void resumeFromDocumentSuspension() final;
#if ENABLE(INPUT_TYPE_COLOR)
    void prepareForDocumentSuspension() final;
#endif

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    bool needsSuspensionCallback();
    void registerForSuspensionCallbackIfNeeded();
    void unregisterForSuspensionCallbackIfNeeded();

    bool supportsMinLength() const { return isTextType(); }
    bool supportsMaxLength() const { return isTextType(); }
    bool isTextType() const;
    bool tooShort(StringView, NeedsToCheckDirtyFlag) const;
    bool tooLong(StringView, NeedsToCheckDirtyFlag) const;

    bool supportsPlaceholder() const final;
    void updatePlaceholderText() final;
    bool isEmptyValue() const final;
    void handleFocusEvent(Node* oldFocusedNode, FocusDirection) final;
    void handleBlurEvent() final;

    bool isOptionalFormControl() const final { return !isRequiredFormControl(); }
    bool isRequiredFormControl() const final;
    bool computeWillValidate() const final;
    void requiredStateChanged() final;

    void initializeInputType();
    void updateType();
    void runPostTypeUpdateTasks();

    void subtreeHasChanged() final;
    void disabledStateChanged() final;
    void readOnlyStateChanged() final;

#if ENABLE(DATALIST_ELEMENT)
    void resetListAttributeTargetObserver();
#endif
    void maxLengthAttributeChanged(const AtomicString& newValue);
    void minLengthAttributeChanged(const AtomicString& newValue);
    void updateValueIfNeeded();

    void addToRadioButtonGroup();
    void removeFromRadioButtonGroup();

    AtomicString m_name;
    String m_valueIfDirty;
    unsigned m_size;
    short m_maxResults { -1 };
    bool m_isChecked : 1;
    bool m_reflectsCheckedAttribute : 1;
    bool m_isIndeterminate : 1;
    bool m_hasType : 1;
    bool m_isActivatedSubmit : 1;
    unsigned m_autocomplete : 2; // AutoCompleteSetting
    bool m_isAutoFilled : 1;
    unsigned m_autoFillButtonType : 3; // AutoFillButtonType
    unsigned m_lastAutoFillButtonType : 3; // AutoFillButtonType
    bool m_isAutoFillAvailable : 1;
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
    bool m_isSpellcheckDisabledExceptTextReplacement : 1;
    RefPtr<InputType> m_inputType;
    // The ImageLoader must be owned by this element because the loader code assumes
    // that it lives as long as its owning element lives. If we move the loader into
    // the ImageInput object we may delete the loader while this element lives on.
    std::unique_ptr<HTMLImageLoader> m_imageLoader;
#if ENABLE(DATALIST_ELEMENT)
    std::unique_ptr<ListAttributeTargetObserver> m_listAttributeTargetObserver;
#endif
};

}
