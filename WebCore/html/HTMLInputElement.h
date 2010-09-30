/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "HTMLFormControlElement.h"
#include "InputElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class DateComponents;
class FileList;
class HTMLDataListElement;
class HTMLImageLoader;
class HTMLOptionElement;
class InputType;
class KURL;
class VisibleSelection;

class HTMLInputElement : public HTMLTextFormControlElement, public InputElement {
public:
    static PassRefPtr<HTMLInputElement> create(const QualifiedName&, Document*, HTMLFormElement*);
    virtual ~HTMLInputElement();

    bool autoComplete() const;

    // For ValidityState
    bool typeMismatch(const String&) const;
    // valueMissing() ignores the specified string value for CHECKBOX and RADIO.
    bool valueMissing(const String&) const;
    bool patternMismatch(const String&) const;
    bool tooLong(const String&, NeedsToCheckDirtyFlag) const;
    bool rangeUnderflow(const String&) const;
    bool rangeOverflow(const String&) const;
    // Returns the minimum value for type=date, number, or range.  Don't call this for other types.
    double minimum() const;
    // Returns the maximum value for type=date, number, or range.  Don't call this for other types.
    // This always returns a value which is >= minimum().
    double maximum() const;
    // Sets the "allowed value step" defined in the HTML spec to the specified double pointer.
    // Returns false if there is no "allowed value step."
    bool getAllowedValueStep(double*) const;
    // For ValidityState.
    bool stepMismatch(const String&) const;

    // Implementations of HTMLInputElement::stepUp() and stepDown().
    void stepUp(int, ExceptionCode&);
    void stepDown(int, ExceptionCode&);
    void stepUp(ExceptionCode& ec) { stepUp(1, ec); }
    void stepDown(ExceptionCode& ec) { stepDown(1, ec); }
    // stepUp()/stepDown() for user-interaction.
    void stepUpFromRenderer(int);

    bool isTextButton() const { return deprecatedInputType() == SUBMIT || deprecatedInputType() == RESET || deprecatedInputType() == BUTTON; }

    virtual bool isRadioButton() const { return deprecatedInputType() == RADIO; }
    virtual bool isTextField() const;
    virtual bool isSearchField() const { return deprecatedInputType() == SEARCH; }
    virtual bool isInputTypeHidden() const { return deprecatedInputType() == HIDDEN; }
    virtual bool isPasswordField() const { return deprecatedInputType() == PASSWORD; }
    virtual bool isCheckbox() const { return deprecatedInputType() == CHECKBOX; }

    // FIXME: It's highly likely that any call site calling this function should instead
    // be using a different one. Many input elements behave like text fields, and in addition
    // any unknown input type is treated as text. Consider, for example, isTextField or
    // isTextField && !isPasswordField.
    bool isText() const { return deprecatedInputType() == TEXT; }

    bool isEmailField() const { return deprecatedInputType() == EMAIL; }
    bool isFileUpload() const { return deprecatedInputType() == FILE; }
    bool isImageButton() const { return deprecatedInputType() == IMAGE; }
    bool isNumberField() const { return deprecatedInputType() == NUMBER; }
    bool isSubmitButton() const { return deprecatedInputType() == SUBMIT; }
    bool isTelephoneField() const { return deprecatedInputType() == TELEPHONE; }
    bool isURLField() const { return deprecatedInputType() == URL; }

#if ENABLE(INPUT_SPEECH)
    virtual bool isSpeechEnabled() const;
#endif    

    bool checked() const { return m_checked; }
    void setChecked(bool, bool sendChangeEvent = false);

    // 'indeterminate' is a state independent of the checked state that causes the control to draw in a way that hides the actual state.
    bool indeterminate() const { return m_indeterminate; }
    void setIndeterminate(bool);

    virtual int size() const;

    void setType(const String&);

    virtual String value() const;
    virtual void setValue(const String&, bool sendChangeEvent = false);
    virtual void setValueForUser(const String&);
    // Checks if the specified string would be a valid value.
    // We should not call this for types with no string value such as CHECKBOX and RADIO.
    bool isValidValue(const String&) const;

    virtual const String& suggestedValue() const;
    void setSuggestedValue(const String&);

    double valueAsDate() const;
    void setValueAsDate(double, ExceptionCode&);

    double valueAsNumber() const;
    void setValueAsNumber(double, ExceptionCode&);

    virtual String placeholder() const;
    virtual void setPlaceholder(const String&);

    String valueWithDefault() const;

    virtual void setValueFromRenderer(const String&);
    void setFileListFromRenderer(const Vector<String>&);

    bool canHaveSelection() const;
    virtual void select() { HTMLTextFormControlElement::select(); }

    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void detach();

    // FIXME: For isActivatedSubmit and setActivatedSubmit, we should use the NVI-idiom here by making
    // it private virtual in all classes and expose a public method in HTMLFormControlElement to call
    // the private virtual method.
    virtual bool isActivatedSubmit() const;
    virtual void setActivatedSubmit(bool flag);

    String altText() const;

    int maxResults() const { return m_maxResults; }

    String defaultValue() const;
    void setDefaultValue(const String&);
    
    bool defaultChecked() const;

    void setDefaultName(const AtomicString&);

    String accept() const;
    String alt() const;

    void setSize(unsigned);

    KURL src() const;

    int maxLength() const;
    void setMaxLength(int, ExceptionCode&);

    bool multiple() const;

#if ENABLE(DIRECTORY_UPLOAD)
    bool webkitdirectory() const;
#endif

    virtual bool isAutofilled() const { return m_autofilled; }
    void setAutofilled(bool value = true);

    FileList* files();

    void addSearchResult();
    void onSearch();

#if ENABLE(DATALIST)
    HTMLElement* list() const;
    HTMLOptionElement* selectedOption() const;
#endif

#if ENABLE(WCSS)
    void setWapInputFormat(String& mask);
#endif

protected:
    HTMLInputElement(const QualifiedName&, Document*, HTMLFormElement* = 0);

    virtual void defaultEventHandler(Event*);

private:
    enum DeprecatedInputType {
        TEXT = 0, // TEXT must be 0.
        PASSWORD,
        ISINDEX,
        CHECKBOX,
        RADIO,
        SUBMIT,
        RESET,
        FILE,
        HIDDEN,
        IMAGE,
        BUTTON,
        SEARCH,
        RANGE,
        EMAIL,
        NUMBER,
        TELEPHONE,
        URL,
        COLOR,
        DATE,
        DATETIME,
        DATETIMELOCAL,
        MONTH,
        TIME,
        WEEK,
    };

    enum AutoCompleteSetting { Uninitialized, On, Off };

    typedef HashMap<String, HTMLInputElement::DeprecatedInputType, CaseFoldingHash> InputTypeMap;
    static PassOwnPtr<InputTypeMap> createTypeMap();
    DeprecatedInputType deprecatedInputType() const { return static_cast<DeprecatedInputType>(m_deprecatedTypeNumber); }

    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();

    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return deprecatedInputType() != IMAGE; }
    virtual void updateFocusAppearance(bool restorePreviousSelection);
    virtual void aboutToUnload();
    virtual bool shouldUseInputMethod() const;

    virtual const AtomicString& formControlName() const;
 
    // isChecked is used by the rendering tree/CSS while checked() is used by JS to determine checked state
    virtual bool isChecked() const { return checked() && (deprecatedInputType() == CHECKBOX || deprecatedInputType() == RADIO); }
    virtual bool isIndeterminate() const { return indeterminate(); }
    
    virtual bool isTextFormControl() const { return isTextField(); }

    virtual bool hasSpinButton() const { return deprecatedInputType() == NUMBER || deprecatedInputType() == DATE || deprecatedInputType() == DATETIME || deprecatedInputType() == DATETIMELOCAL || deprecatedInputType() == MONTH || deprecatedInputType() == TIME || deprecatedInputType() == WEEK; }
    virtual bool canTriggerImplicitSubmission() const { return isTextField(); }

    bool allowsIndeterminate() const { return deprecatedInputType() == CHECKBOX || deprecatedInputType() == RADIO; }

    virtual const AtomicString& formControlType() const;

    virtual bool searchEventsShouldBeDispatched() const;

    virtual bool saveFormControlState(String& value) const;
    virtual void restoreFormControlState(const String&);

    virtual bool canStartSelection() const;
    
    virtual void accessKeyAction(bool sendToAnyElement);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(Attribute*);

    virtual void copyNonAttributeProperties(const Element* source);

    virtual void attach();

    virtual bool appendFormData(FormDataList&, bool);

    virtual bool isSuccessfulSubmitButton() const;

    // Report if this input type uses height & width attributes
    bool respectHeightAndWidthAttrs() const { return deprecatedInputType() == IMAGE || deprecatedInputType() == HIDDEN; }

    virtual void reset();

    virtual void* preDispatchEventHandler(Event*);
    virtual void postDispatchEventHandler(Event*, void* dataFromPreDispatch);

    virtual bool isURLAttribute(Attribute*) const;

    virtual void cacheSelection(int start, int end);

    virtual bool isAcceptableValue(const String&) const;
    virtual String sanitizeValue(const String&) const;
    virtual bool hasUnacceptableValue() const;

    virtual void documentDidBecomeActive();

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    bool storesValueSeparateFromAttribute() const;

    bool needsActivationCallback();
    void registerForActivationCallbackIfNeeded();
    void unregisterForActivationCallbackIfNeeded();

    virtual bool supportsMaxLength() const { return isTextType(); }
    bool isTextType() const;

    virtual bool supportsPlaceholder() const { return isTextType() || deprecatedInputType() == ISINDEX; }
    virtual bool isEmptyValue() const { return value().isEmpty(); }
    virtual void handleFocusEvent();
    virtual void handleBlurEvent();
    virtual int cachedSelectionStart() const { return m_data.cachedSelectionStart(); }
    virtual int cachedSelectionEnd() const { return m_data.cachedSelectionEnd(); }

    virtual bool isOptionalFormControl() const { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const;
    virtual bool recalcWillValidate() const;

    void updateType();

    void updateCheckedRadioButtons();
    
    void handleBeforeTextInsertedEvent(Event*);
    void handleKeyEventForRange(KeyboardEvent*);
    PassRefPtr<HTMLFormElement> createTemporaryFormForIsIndex();
    // Helper for getAllowedValueStep();
    bool getStepParameters(double* defaultStep, double* stepScaleFactor) const;
    // Helper for stepUp()/stepDown().  Adds step value * count to the current value.
    void applyStep(double count, ExceptionCode&);
    // Helper for applyStepForNumberOrRange().
    double stepBase() const;

    // Parses the specified string as the DeprecatedInputType, and returns true if it is successfully parsed.
    // An instance pointed by the DateComponents* parameter will have parsed values and be
    // modified even if the parsing fails.  The DateComponents* parameter may be 0.
    static bool parseToDateComponents(DeprecatedInputType, const String&, DateComponents*);

    // Parses the specified string for the current type, and return
    // the double value for the parsing result if the parsing
    // succeeds; Returns defaultValue otherwise. This function can
    // return NaN or Infinity only if defaultValue is NaN or Infinity.
    double parseToDouble(const String&, double defaultValue) const;
    // Create a string representation of the specified double value for the
    // current input type. If NaN or Infinity is specified, this returns an
    // emtpy string. This should not be called for types without valueAsNumber.
    String serialize(double) const;
    // Create a string representation of the specified double value for the
    // current input type. The type must be one of DATE, DATETIME,
    // DATETIMELOCAL, MONTH, TIME, and WEEK.
    String serializeForDateTimeTypes(double) const;

#if ENABLE(DATALIST)
    HTMLDataListElement* dataList() const;
#endif

#if ENABLE(WCSS)
    virtual InputElementData data() const { return m_data; }
#endif

    InputElementData m_data;
    int m_xPos;
    int m_yPos;
    short m_maxResults;
    OwnPtr<HTMLImageLoader> m_imageLoader;
    RefPtr<FileList> m_fileList;
    unsigned m_deprecatedTypeNumber : 5; // DeprecatedInputType 
    bool m_checked : 1;
    bool m_defaultChecked : 1;
    bool m_useDefaultChecked : 1;
    bool m_indeterminate : 1;
    bool m_haveType : 1;
    bool m_activeSubmit : 1;
    unsigned m_autocomplete : 2; // AutoCompleteSetting
    bool m_autofilled : 1;
    bool m_inited : 1;
#if ENABLE(DATALIST)
    bool m_hasNonEmptyList : 1;
#endif
    OwnPtr<InputType> m_inputType;
};

} //namespace

#endif
