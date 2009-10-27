/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

class FileList;
class HTMLDataListElement;
class HTMLImageLoader;
class HTMLOptionElement;
class KURL;
class VisibleSelection;

class HTMLInputElement : public HTMLTextFormControlElement, public InputElement {
public:
    enum InputType {
        TEXT,
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
        COLOR
    };
    
    enum AutoCompleteSetting {
        Uninitialized,
        On,
        Off
    };

    HTMLInputElement(const QualifiedName&, Document*, HTMLFormElement* = 0);
    virtual ~HTMLInputElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return inputType() != IMAGE; }
    virtual void updateFocusAppearance(bool restorePreviousSelection);
    virtual void aboutToUnload();
    virtual bool shouldUseInputMethod() const;

    virtual const AtomicString& formControlName() const;
 
    bool autoComplete() const;

    // isChecked is used by the rendering tree/CSS while checked() is used by JS to determine checked state
    virtual bool isChecked() const { return checked() && (inputType() == CHECKBOX || inputType() == RADIO); }
    virtual bool isIndeterminate() const { return indeterminate(); }
    
    bool readOnly() const { return isReadOnlyFormControl(); }

    virtual bool isTextFormControl() const { return isTextField(); }

    virtual bool valueMissing() const;
    virtual bool patternMismatch() const;
    virtual bool tooLong() const;
    // For ValidityState
    bool rangeUnderflow() const;
    bool rangeOverflow() const;
    // Returns the minimum value for type=range.  Don't call this for other types.
    double rangeMinimum() const;
    // Returns the maximum value for type=range.  Don't call this for other types.
    // This always returns a value which is <= rangeMinimum().
    double rangeMaximum() const;

    bool isTextButton() const { return m_type == SUBMIT || m_type == RESET || m_type == BUTTON; }
    virtual bool isRadioButton() const { return m_type == RADIO; }
    virtual bool isTextField() const { return m_type == TEXT || m_type == PASSWORD || m_type == SEARCH || m_type == ISINDEX || m_type == EMAIL || m_type == NUMBER || m_type == TELEPHONE || m_type == URL || m_type == COLOR; }
    virtual bool isSearchField() const { return m_type == SEARCH; }
    virtual bool isInputTypeHidden() const { return m_type == HIDDEN; }
    virtual bool isPasswordField() const { return m_type == PASSWORD; }

    bool checked() const { return m_checked; }
    void setChecked(bool, bool sendChangeEvent = false);
    bool indeterminate() const { return m_indeterminate; }
    void setIndeterminate(bool);
    virtual int size() const;
    virtual const AtomicString& formControlType() const;
    void setType(const String&);

    virtual String value() const;
    virtual void setValue(const String&);

    virtual String placeholder() const;
    virtual void setPlaceholder(const String&);

    virtual bool searchEventsShouldBeDispatched() const;

    String valueWithDefault() const;

    virtual void setValueFromRenderer(const String&);
    void setFileListFromRenderer(const Vector<String>&);

    virtual bool saveFormControlState(String& value) const;
    virtual void restoreFormControlState(const String&);

    virtual bool canStartSelection() const;
    
    bool canHaveSelection() const;
    virtual void select() { HTMLTextFormControlElement::select(); }

    virtual void accessKeyAction(bool sendToAnyElement);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void copyNonAttributeProperties(const Element* source);

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void detach();
    virtual bool appendFormData(FormDataList&, bool);

    virtual bool isSuccessfulSubmitButton() const;
    virtual bool isActivatedSubmit() const;
    virtual void setActivatedSubmit(bool flag);

    InputType inputType() const { return static_cast<InputType>(m_type); }
    void setInputType(const String&);
    
    // Report if this input type uses height & width attributes
    bool respectHeightAndWidthAttrs() const { return inputType() == IMAGE || inputType() == HIDDEN; }

    virtual void reset();

    virtual void* preDispatchEventHandler(Event*);
    virtual void postDispatchEventHandler(Event*, void* dataFromPreDispatch);
    virtual void defaultEventHandler(Event*);

    String altText() const;
    
    virtual bool isURLAttribute(Attribute*) const;

    int maxResults() const { return m_maxResults; }

    String defaultValue() const;
    void setDefaultValue(const String&);
    
    bool defaultChecked() const;
    void setDefaultChecked(bool);

    void setDefaultName(const AtomicString&);

    String accept() const;
    void setAccept(const String&);

    String accessKey() const;
    void setAccessKey(const String&);

    String align() const;
    void setAlign(const String&);

    String alt() const;
    void setAlt(const String&);

    void setSize(unsigned);

    KURL src() const;
    void setSrc(const String&);

#if ENABLE(DATALIST)
    HTMLElement* list() const;
    HTMLOptionElement* selectedOption() const;
#endif

    int maxLength() const;
    void setMaxLength(int, ExceptionCode&);

    bool multiple() const;
    void setMultiple(bool);

    String useMap() const;
    void setUseMap(const String&);

    virtual bool isAutofilled() const { return m_autofilled; }
    void setAutofilled(bool value = true);

    FileList* files();

    virtual void cacheSelection(int start, int end);
    void addSearchResult();
    void onSearch();

    virtual String sanitizeValue(const String&) const;

    virtual void documentDidBecomeActive();

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;
    
    virtual bool willValidate() const;

    // Converts the specified string to a floating number.
    // If the conversion fails, the return value is false. Take care that leading or trailing unnecessary characters make failures.  This returns false for an empty string input.
    // The double* parameter may be 0.
    static bool formStringToDouble(const String&, double*);
    
protected:
    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();

private:
    bool storesValueSeparateFromAttribute() const;

    bool needsActivationCallback();
    void registerForActivationCallbackIfNeeded();
    void unregisterForActivationCallbackIfNeeded();

    virtual bool supportsPlaceholder() const { return isTextField(); }
    virtual bool isEmptyValue() const { return value().isEmpty(); }
    virtual void handleFocusEvent();
    virtual void handleBlurEvent();
    virtual int cachedSelectionStart() const { return m_data.cachedSelectionStart(); }
    virtual int cachedSelectionEnd() const { return m_data.cachedSelectionEnd(); }

    virtual bool isOptionalFormControl() const { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const;

    PassRefPtr<HTMLFormElement> createTemporaryFormForIsIndex();

#if ENABLE(DATALIST)
    HTMLDataListElement* dataList() const;
#endif

    InputElementData m_data;
    int m_xPos;
    int m_yPos;
    short m_maxResults;
    OwnPtr<HTMLImageLoader> m_imageLoader;
    RefPtr<FileList> m_fileList;
    unsigned m_type : 5; // InputType 
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
};

} //namespace

#endif
