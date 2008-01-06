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

#include "HTMLGenericFormElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLImageLoader;
class Selection;

class HTMLInputElement : public HTMLFormControlElementWithState {
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
        RANGE
    };

    HTMLInputElement(Document*, HTMLFormElement* = 0);
    HTMLInputElement(const QualifiedName& tagName, Document*, HTMLFormElement* = 0);
    virtual ~HTMLInputElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return inputType() != IMAGE; }
    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();
    virtual void updateFocusAppearance(bool restorePreviousSelection);
    virtual void aboutToUnload();
    virtual bool shouldUseInputMethod() const;

    virtual const AtomicString& name() const;

    bool autoComplete() const { return m_autocomplete; }

    // isChecked is used by the rendering tree/CSS while checked() is used by JS to determine checked state
    virtual bool isChecked() const { return checked() && (inputType() == CHECKBOX || inputType() == RADIO); }
    virtual bool isIndeterminate() const { return indeterminate(); }
    
    bool readOnly() const { return isReadOnlyControl(); }

    bool isTextButton() const { return m_type == SUBMIT || m_type == RESET || m_type == BUTTON; }
    virtual bool isRadioButton() const { return m_type == RADIO; }
    bool isTextField() const { return m_type == TEXT || m_type == PASSWORD || m_type == SEARCH || m_type == ISINDEX; }
    bool isSearchField() const { return m_type == SEARCH; }
    virtual bool isInputTypeHidden() const { return m_type == HIDDEN; }

    bool checked() const { return m_checked; }
    void setChecked(bool, bool sendChangeEvent = false);
    bool indeterminate() const { return m_indeterminate; }
    void setIndeterminate(bool);
    int maxLength() const { return m_maxLen; }
    int size() const { return m_size; }
    virtual const AtomicString& type() const;
    void setType(const String&);

    String value() const;
    void setValue(const String&);

    String valueWithDefault() const;

    void setValueFromRenderer(const String&);

    virtual bool saveState(String& value) const;
    virtual void restoreState(const String&);

    virtual bool canStartSelection() const;
    
    bool canHaveSelection() const;
    int selectionStart() const;
    int selectionEnd() const;
    void setSelectionStart(int);
    void setSelectionEnd(int);
    void select();
    void setSelectionRange(int start, int end);

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

    String accept() const;
    void setAccept(const String&);

    String accessKey() const;
    void setAccessKey(const String&);

    String align() const;
    void setAlign(const String&);

    String alt() const;
    void setAlt(const String&);

    void setSize(unsigned);

    String src() const;
    void setSrc(const String&);

    void setMaxLength(int);

    String useMap() const;
    void setUseMap(const String&);

    bool autofilled() const { return m_autofilled; }
    void setAutofilled(bool b = true) { m_autofilled = b; }
    
    void cacheSelection(int s, int e) { cachedSelStart = s; cachedSelEnd = e; };
    void addSearchResult();
    void onSearch();
    
    Selection selection() const;

    String constrainValue(const String& proposedValue) const;

    virtual void didRestoreFromCache();
    
protected:
    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();
    
    AtomicString m_name;

private:
    void init();
    bool storesValueSeparateFromAttribute() const;
    String constrainValue(const String& proposedValue, int maxLen) const;
    void recheckValue();

    String m_value;
    String m_originalValue;
    int xPos;
    int m_maxLen;
    short m_size;
    short yPos;

    short m_maxResults;

    OwnPtr<HTMLImageLoader> m_imageLoader;

    unsigned m_type : 4; // InputType 
    bool m_checked : 1;
    bool m_defaultChecked : 1;
    bool m_useDefaultChecked : 1;
    bool m_indeterminate : 1;
    bool m_haveType : 1;
    bool m_activeSubmit : 1;
    bool m_autocomplete : 1;
    bool m_autofilled : 1;
    bool m_inited : 1;
    
    int cachedSelStart;
    int cachedSelEnd;
};

} //namespace

#endif
