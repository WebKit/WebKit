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
#ifndef HTML_HTMLInputElementImplL_H
#define HTML_HTMLInputElementImplL_H

#include "HTMLGenericFormElementImpl.h"

namespace khtml {
    class RenderLineEdit;
    class RenderFileButton;
    class RenderSlider;
}

namespace DOM {

class HTMLImageLoader;

class HTMLInputElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderLineEdit;
    friend class khtml::RenderFileButton;

    friend class HTMLSelectElementImpl;
    friend class khtml::RenderSlider;

public:
    // do not change the order!
    enum typeEnum {
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

    HTMLInputElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f = 0);
    HTMLInputElementImpl(const QualifiedName& tagName, DocumentImpl *doc, HTMLFormElementImpl *f = 0);
    virtual ~HTMLInputElementImpl();
    void init();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool isKeyboardFocusable() const;
    virtual bool isEnumeratable() const { return inputType() != IMAGE; }

    virtual DOMString name() const;

    bool autoComplete() const { return m_autocomplete; }

    virtual bool isChecked() const { return checked(); }
    virtual bool isIndeterminate() const { return indeterminate(); }

    bool isTextButton() const { return m_type == SUBMIT || m_type == RESET || m_type == BUTTON; }
    virtual bool isRadioButton() const { return m_type == RADIO; }

    bool checked() const { return m_checked; }
    void setChecked(bool);
    bool indeterminate() const { return m_indeterminate; }
    void setIndeterminate(bool);
    int maxLength() const { return m_maxLen; }
    int size() const { return m_size; }
    DOMString type() const;
    void setType(const DOMString& t);

    DOMString value() const;
    void setValue(const DOMString &);

    DOMString valueWithDefault() const;

    void setValueFromRenderer(const DOMString &);
    bool valueMatchesRenderer() const { return m_valueMatchesRenderer; }
    void setValueMatchesRenderer() { m_valueMatchesRenderer = true; }

    virtual bool maintainsState() { return m_type != PASSWORD; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    bool canHaveSelection();

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);

    void select();
    void setSelectionRange(int, int);
    
    virtual void click(bool sendMouseEvents = false, bool showPressedLook = true);
    virtual void accessKeyAction(bool sendToAnyElement);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    virtual void copyNonAttributeProperties(const ElementImpl *source);

    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void detach();
    virtual bool appendFormData(FormDataList&, bool);

    virtual bool isSuccessfulSubmitButton() const;
    virtual bool isActivatedSubmit() const;
    virtual void setActivatedSubmit(bool flag);

    typeEnum inputType() const { return m_type; }
    void setInputType(const DOMString& value);

    virtual void reset();

    // used in case input type=image was clicked.
    int clickX() const { return xPos; }
    int clickY() const { return yPos; }

    virtual void* preDispatchEventHandler(EventImpl *evt);
    virtual void postDispatchEventHandler(EventImpl *evt, void* data);
    virtual void defaultEventHandler(EventImpl *evt);
    virtual bool isEditable();

    DOMString altText() const;
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    int maxResults() const { return m_maxResults; }

    DOMString defaultValue() const;
    void setDefaultValue(const DOMString &);
    
    bool defaultChecked() const;
    void setDefaultChecked(bool);

    DOMString accept() const;
    void setAccept(const DOMString &);

    DOMString accessKey() const;
    void setAccessKey(const DOMString &);

    DOMString align() const;
    void setAlign(const DOMString &);

    DOMString alt() const;
    void setAlt(const DOMString &);

    void setSize(unsigned);

    DOMString src() const;
    void setSrc(const DOMString &);

    void setMaxLength(int);

    DOMString useMap() const;
    void setUseMap(const DOMString &);

protected:
    bool storesValueSeparateFromAttribute() const;

    AtomicString m_name;
    DOMString m_value;
    int       xPos;
    short     m_maxLen;
    short     m_size;
    short     yPos;

    short     m_maxResults;

    HTMLImageLoader* m_imageLoader;

    typeEnum m_type : 4;
    bool m_checked : 1;
    bool m_defaultChecked : 1;
    bool m_useDefaultChecked : 1;
    bool m_indeterminate : 1;
    bool m_haveType : 1;
    bool m_activeSubmit : 1;
    bool m_autocomplete : 1;
    bool m_valueMatchesRenderer : 1;
};

} //namespace

#endif
