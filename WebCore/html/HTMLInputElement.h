/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef HTML_HTMLInputElement_H
#define HTML_HTMLInputElement_H

#include "HTMLGenericFormElement.h"

namespace WebCore {

class HTMLImageLoader;

class HTMLInputElement : public HTMLGenericFormElement
{
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

    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return inputType() != IMAGE; }
    virtual void focus();
    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();

    virtual const AtomicString& name() const;

    bool autoComplete() const { return m_autocomplete; }

    virtual bool isChecked() const { return checked(); }
    virtual bool isIndeterminate() const { return indeterminate(); }
    
    bool readOnly() const { return isReadOnlyControl(); }

    bool isTextButton() const { return m_type == SUBMIT || m_type == RESET || m_type == BUTTON; }
    virtual bool isRadioButton() const { return m_type == RADIO; }
    bool isTextField() const { return m_type == TEXT || m_type == PASSWORD || m_type == SEARCH; }
    // FIXME: When other text fields switch to the non-NSView implementation, we should add them here.
    // Once all text fields switch over, we should merge this with isTextField.
    bool isNonWidgetTextField() const { return m_type == TEXT; }

    bool checked() const { return m_checked; }
    void setChecked(bool);
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
    bool valueMatchesRenderer() const { return m_valueMatchesRenderer; }
    void setValueMatchesRenderer() { m_valueMatchesRenderer = true; }

    virtual String stateValue() const;
    virtual void restoreState(const String&);

    bool canHaveSelection() const;
    int selectionStart() const;
    int selectionEnd() const;
    void setSelectionStart(int);
    void setSelectionEnd(int);
    void select();
    void setSelectionRange(int start, int end);
    
    virtual void click(bool sendMouseEvents = false, bool showPressedLook = true);
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

    virtual void reset();

    // used in case input type=image was clicked.
    int clickX() const { return xPos; }
    int clickY() const { return yPos; }

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

protected:
    AtomicString m_name;

private:
    void init();
    bool storesValueSeparateFromAttribute() const;
    String constrainValue(const String& proposedValue) const;
    String constrainValue(const String& proposedValue, int maxLen) const;
    void recheckValue();

    String m_value;
    int xPos;
    short m_maxLen;
    short m_size;
    short yPos;

    short m_maxResults;

    HTMLImageLoader* m_imageLoader;

    unsigned m_type : 4; // InputType 
    bool m_checked : 1;
    bool m_defaultChecked : 1;
    bool m_useDefaultChecked : 1;
    bool m_indeterminate : 1;
    bool m_haveType : 1;
    bool m_activeSubmit : 1;
    bool m_autocomplete : 1;
    bool m_valueMatchesRenderer : 1;
    bool m_autofilled : 1;
    bool m_inited : 1;
};

} //namespace

#endif
