/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef HTML_FORMIMPL_H
#define HTML_FORMIMPL_H

#include "html/html_elementimpl.h"
#include "dom/html_element.h"
#include "html/html_miscimpl.h"

#include <qptrvector.h>
#include <qmemarray.h>

class KHTMLView;
class QTextCodec;

namespace khtml
{
    class FormData;
    class RenderFormElement;
    class RenderTextArea;
    class RenderSelect;
    class RenderLineEdit;
    class RenderRadioButton;
    class RenderFileButton;
#if APPLE_CHANGES
    class RenderSlider;
#endif
}

namespace DOM {

class DOMString;
class FormDataList;
class HTMLFormElement;
class HTMLGenericFormElementImpl;
class HTMLImageElementImpl;
class HTMLImageLoader;
class HTMLOptionElementImpl;
class HTMLOptionsCollectionImpl;

// -------------------------------------------------------------------------

class HTMLFormElementImpl : public HTMLElementImpl
{
public:
    HTMLFormElementImpl(DocumentPtr *doc);
    virtual ~HTMLFormElementImpl();

    virtual Id id() const;

    virtual void attach();
    virtual void detach();

    long length() const;

    DOMString enctype() const { return m_enctype; }
    void setEnctype( const DOMString & );

    DOMString boundary() const { return m_boundary; }
    void setBoundary( const DOMString & );

    bool autoComplete() const { return m_autocomplete; }

    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    void radioClicked( HTMLGenericFormElementImpl *caller );

    void registerFormElement(HTMLGenericFormElementImpl *);
    void removeFormElement(HTMLGenericFormElementImpl *);
    void makeFormElementDormant(HTMLGenericFormElementImpl *);
    void registerImgElement(HTMLImageElementImpl *);
    void removeImgElement(HTMLImageElementImpl *);

    bool prepareSubmit();
    void submit(bool activateSubmitButton);
    void reset();

    void setMalformed(bool malformed) { m_malformed = malformed; }
    virtual bool isMalformed() { return m_malformed; }
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;
    
#if APPLE_CHANGES
    void submitClick();
    bool formWouldHaveSecureSubmission(const DOMString &url);
#endif
   
    static void i18nData();

    friend class HTMLFormElement;
    friend class HTMLFormCollectionImpl;

    HTMLCollectionImpl::CollectionInfo *collectionInfo;

    QPtrVector<HTMLGenericFormElementImpl> formElements;
    QPtrVector<HTMLGenericFormElementImpl> dormantFormElements;
    QPtrVector<HTMLImageElementImpl> imgElements;
    DOMString m_url;
    DOMString m_target;
    DOMString m_enctype;
    DOMString m_boundary;
    DOMString m_acceptcharset;
    bool m_post : 1;
    bool m_multipart : 1;
    bool m_autocomplete : 1;
    bool m_insubmit : 1;
    bool m_doingsubmit : 1;
    bool m_inreset : 1;
    bool m_malformed : 1;

 private:
    bool formData(khtml::FormData &) const;

    QString oldIdAttr;
    QString oldNameAttr;
};

// -------------------------------------------------------------------------

class HTMLGenericFormElementImpl : public HTMLElementImpl
{
    friend class HTMLFormElementImpl;
    friend class khtml::RenderFormElement;

public:
    HTMLGenericFormElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    virtual ~HTMLGenericFormElementImpl();

    HTMLFormElementImpl *form() { return m_form; }

    virtual DOMString type() const = 0;

    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    virtual void attach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    virtual void reset() {}

    void onSelect();
    void onChange();

    virtual bool disabled() const;
    void setDisabled(bool _disabled);

    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return false; }

    bool readOnly() const { return m_readOnly; }
    void setReadOnly(bool _readOnly) { m_readOnly = _readOnly; }

    virtual void recalcStyle( StyleChange );

    DOMString name() const;
    void setName(const DOMString& name);

    virtual bool isGenericFormElement() const { return true; }

    /*
     * override in derived classes to get the encoded name=value pair
     * for submitting
     * return true for a successful control (see HTML4-17.13.2)
     */
    virtual bool appendFormData(FormDataList&, bool) { return false; }

    virtual void defaultEventHandler(EventImpl *evt);
    virtual bool isEditable();

    virtual QString state();
    QString findMatchingState(QStringList &states);

    virtual bool isSuccessfulSubmitButton() const { return false; }
    virtual bool isActivatedSubmit() const { return false; }
    virtual void setActivatedSubmit(bool flag) { }

protected:
    HTMLFormElementImpl *getForm() const;

    DOMStringImpl* m_name;
    HTMLFormElementImpl *m_form;
    bool m_disabled, m_readOnly;

    bool m_inited : 1;
    bool m_dormant : 1;
};

// -------------------------------------------------------------------------

class HTMLButtonElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLButtonElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);

    virtual ~HTMLButtonElementImpl();

    enum typeEnum {
        SUBMIT,
        RESET,
        BUTTON
    };

    virtual Id id() const;
    DOMString type() const;

    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    virtual void defaultEventHandler(EventImpl *evt);
    virtual bool appendFormData(FormDataList&, bool);

    virtual bool isSuccessfulSubmitButton() const;
    virtual bool isActivatedSubmit() const;
    virtual void setActivatedSubmit(bool flag);

    virtual void click(bool sendMouseEvents);
    virtual void accessKeyAction(bool sendToAnyElement);
    void blur();
    void focus();
    
protected:
    DOMString m_value;
    DOMString m_currValue;
    typeEnum  m_type : 2;
    bool      m_dirty : 1;
    bool      m_activeSubmit : 1;
};

// -------------------------------------------------------------------------

class HTMLFieldSetElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLFieldSetElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);

    virtual ~HTMLFieldSetElementImpl();

    virtual Id id() const;
    
    virtual bool isFocusable() const;
    
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    virtual DOMString type() const;
};

// -------------------------------------------------------------------------

class HTMLInputElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderLineEdit;
    friend class khtml::RenderRadioButton;
    friend class khtml::RenderFileButton;

#if APPLE_CHANGES
    friend class HTMLSelectElementImpl;
    friend class khtml::RenderSlider;
#endif

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
        BUTTON
#if APPLE_CHANGES
        ,SEARCH,
        RANGE
#endif
    };

    HTMLInputElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    virtual ~HTMLInputElementImpl();

    virtual Id id() const;

    virtual bool isEnumeratable() const { return inputType() != IMAGE; }

    bool autoComplete() const { return m_autocomplete; }

    bool checked() const { return m_checked; }
    void setChecked(bool);
    long maxLength() const { return m_maxLen; }
    int size() const { return m_size; }
    DOMString type() const;
    void setType(const DOMString& t);

    DOMString value() const;
    void setValue(const DOMString &);

    DOMString valueWithDefault() const;

    void setValueFromRenderer(const DOMString &);
    bool valueMatchesRenderer() const { return m_valueMatchesRenderer; }
    void setValueMatchesRenderer() { m_valueMatchesRenderer = true; }

    void blur();
    void focus();

    virtual bool maintainsState() { return m_type != PASSWORD; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    void select();
    
    virtual void click(bool sendMouseEvents);
    virtual void accessKeyAction(bool sendToAnyElement);

    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void detach();
    virtual bool appendFormData(FormDataList&, bool);

    virtual bool isSuccessfulSubmitButton() const;
    virtual bool isActivatedSubmit() const;
    virtual void setActivatedSubmit(bool flag);

    typeEnum inputType() const { return m_type; }
    virtual void reset();

    // used in case input type=image was clicked.
    int clickX() const { return xPos; }
    int clickY() const { return yPos; }

    virtual void defaultEventHandler(EventImpl *evt);
    virtual bool isEditable();

    DOMString altText() const;
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

#if APPLE_CHANGES
    long maxResults() const { return m_maxResults; }
#endif
    
protected:
    bool storesValueSeparateFromAttribute() const;

    DOMString m_value;
    int       xPos;
    short     m_maxLen;
    short     m_size;
    short     yPos;

#if APPLE_CHANGES
    short     m_maxResults;
#endif

    HTMLImageLoader* m_imageLoader;

    typeEnum m_type : 4;
    bool m_checked : 1;
    bool m_defaultChecked : 1;
    bool m_useDefaultChecked : 1;
    bool m_haveType : 1;
    bool m_activeSubmit : 1;
    bool m_autocomplete : 1;
    bool m_valueMatchesRenderer : 1;
};

// -------------------------------------------------------------------------

class HTMLLabelElementImpl : public HTMLElementImpl
{
public:
    HTMLLabelElementImpl(DocumentPtr *doc);
    virtual ~HTMLLabelElementImpl();

    virtual bool isFocusable() const;
    
    virtual Id id() const;

    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    virtual void accessKeyAction(bool sendToAnyElement);

    /**
     * the form element this label is associated to.
     */
    ElementImpl *formElement();

 private:
    DOMString m_formElementID;
};

// -------------------------------------------------------------------------

class HTMLLegendElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLLegendElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    virtual ~HTMLLegendElementImpl();

    virtual bool isFocusable() const;
    
    virtual Id id() const;
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    virtual DOMString type() const;
};

// -------------------------------------------------------------------------

class HTMLSelectElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderSelect;

public:
    HTMLSelectElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    ~HTMLSelectElementImpl();

    virtual Id id() const;
    DOMString type() const;

    virtual void recalcStyle( StyleChange );

    long selectedIndex() const;
    void setSelectedIndex( long index );

    virtual bool isEnumeratable() const { return true; }

    long length() const;

    long minWidth() const { return m_minwidth; }

    long size() const { return m_size; }

    bool multiple() const { return m_multiple; }

    void add ( HTMLElementImpl *element, HTMLElementImpl *before );
    void remove ( long index );
    void blur();
    void focus();

    DOMString value();
    void setValue(DOMStringImpl* value);
    
    HTMLOptionsCollectionImpl *options();

    virtual bool maintainsState() { return true; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual NodeImpl *addChild( NodeImpl* newChild );

    virtual void childrenChanged();

    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);

    // get the actual listbox index of the optionIndexth option
    int optionToListIndex(int optionIndex) const;
    // reverse of optionToListIndex - get optionIndex from listboxIndex
    int listToOptionIndex(int listIndex) const;

    void setRecalcListItems();

    QMemArray<HTMLGenericFormElementImpl*> listItems() const
     {
         if (m_recalcListItems) const_cast<HTMLSelectElementImpl*>(this)->recalcListItems();
         return m_listItems;
     }
    virtual void reset();
    void notifyOptionSelected(HTMLOptionElementImpl *selectedOption, bool selected);

#if APPLE_CHANGES
    virtual void defaultEventHandler(EventImpl *evt);
#endif

    virtual void accessKeyAction(bool sendToAnyElement);

private:
    void recalcListItems();

protected:
    mutable QMemArray<HTMLGenericFormElementImpl*> m_listItems;
    HTMLOptionsCollectionImpl *m_options;
    short m_minwidth;
    short m_size;
    bool m_multiple;
    bool m_recalcListItems;
};

// -------------------------------------------------------------------------

class HTMLKeygenElementImpl : public HTMLSelectElementImpl
{
public:
    HTMLKeygenElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);

    virtual Id id() const;

    DOMString type() const;

    // ### this is just a rough guess
    virtual bool isEnumeratable() const { return false; }

    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    virtual bool appendFormData(FormDataList&, bool);
protected:
    AtomicString m_challenge;
    AtomicString m_keyType;
};

// -------------------------------------------------------------------------

class HTMLOptGroupElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLOptGroupElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    virtual ~HTMLOptGroupElementImpl();

    virtual Id id() const;
    DOMString type() const;

    virtual bool isFocusable() const;
    
    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual NodeImpl *addChild( NodeImpl* newChild );
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    void recalcSelectOptions();

};


// ---------------------------------------------------------------------------

class HTMLOptionElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderSelect;
    friend class DOM::HTMLSelectElementImpl;

public:
    HTMLOptionElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);

    virtual bool isFocusable() const;
    
    virtual Id id() const;
    DOMString type() const;

    DOMString text() const;

    long index() const;
    void setIndex( long );
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    DOMString value() const;
    void setValue(DOMStringImpl* value);

    bool selected() const { return m_selected; }
    void setSelected(bool _selected);

    HTMLSelectElementImpl *getSelect() const;

    virtual void childrenChanged();

protected:
    DOMString m_value;
    bool m_selected;
};


// -------------------------------------------------------------------------

class HTMLTextAreaElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderTextArea;

public:
    enum WrapMethod {
        ta_NoWrap,
        ta_Virtual,
        ta_Physical
    };

    HTMLTextAreaElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    ~HTMLTextAreaElementImpl();

    virtual Id id() const;

    long cols() const { return m_cols; }

    long rows() const { return m_rows; }

    WrapMethod wrap() const { return m_wrap; }

    virtual bool isEnumeratable() const { return true; }

    DOMString type() const;

    virtual bool maintainsState() { return true; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    void select (  );

    virtual void childrenChanged();
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void detach();
    virtual bool appendFormData(FormDataList&, bool);
    virtual void reset();
    DOMString value();
    void setValue(const DOMString &value);
    DOMString defaultValue();
    void setDefaultValue(const DOMString &value);
    void blur();
    void focus();

    void invalidateValue() { m_valueIsValid = false; }
    void updateValue();

    bool valueMatchesRenderer() const { return m_valueMatchesRenderer; }
    void setValueMatchesRenderer() { m_valueMatchesRenderer = true; }

    virtual bool isEditable();
    
    virtual void accessKeyAction(bool sendToAnyElement);
    
protected:
    int m_rows;
    int m_cols;
    WrapMethod m_wrap;
    QString m_value;
    bool m_valueIsValid;
    bool m_valueMatchesRenderer;
};

// -------------------------------------------------------------------------

class HTMLIsIndexElementImpl : public HTMLInputElementImpl
{
public:
    HTMLIsIndexElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);

    virtual Id id() const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

protected:
    DOMString m_prompt;
};

// -------------------------------------------------------------------------

class HTMLOptionsCollectionImpl : public khtml::Shared<HTMLOptionsCollectionImpl>
{
public:
    HTMLOptionsCollectionImpl(HTMLSelectElementImpl *impl) : m_select(impl) { }

    unsigned long length() const;
    void setLength(unsigned long);
    NodeImpl *item(unsigned long index) const;
    NodeImpl *namedItem(const DOMString &name) const;

    void detach() { m_select = 0; }

private:
    HTMLSelectElementImpl *m_select;
};

} //namespace

#endif
