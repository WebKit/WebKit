/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
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
 * $Id$
 */
#ifndef HTML_FORMIMPL_H
#define HTML_FORMIMPL_H

#include "html_elementimpl.h"
#include "html_element.h"

#include <qvaluelist.h>
#include <qlist.h>
#include <qcstring.h>
#include <qarray.h>

class KHTMLView;
class QTextCodec;

namespace khtml
{
    class RenderFormElement;
    class RenderTextArea;
    class RenderSelect;
    class RenderLineEdit;

    typedef QValueList<QCString> encodingList;
}

namespace DOM {

class HTMLFormElement;
class DOMString;
class HTMLGenericFormElementImpl;
class HTMLOptionElementImpl;

class HTMLFormElementImpl : public HTMLElementImpl
{
public:
    HTMLFormElementImpl(DocumentPtr *doc);
    virtual ~HTMLFormElementImpl();

    virtual const DOMString nodeName() const { return "FORM"; }
    virtual ushort id() const;

    long length() const;

    QByteArray formData( );

    DOMString enctype() const { return m_enctype; }
    void setEnctype( const DOMString & );

    DOMString boundary() const { return m_boundary; }
    void setBoundary( const DOMString & );

    bool autoComplete() const { return m_autocomplete; }

    virtual void parseAttribute(AttrImpl *attr);

    virtual void attach();
    virtual void detach();

    void radioClicked( HTMLGenericFormElementImpl *caller );

    void registerFormElement(khtml::RenderFormElement *);
    void removeFormElement(khtml::RenderFormElement *);

    void registerFormElement(HTMLGenericFormElementImpl *);
    void removeFormElement(HTMLGenericFormElementImpl *);

    /*
     * state() and restoreState() are complimentary functions.
     */
    virtual QString state() { return QString::null; }
    virtual void restoreState(const QString &) { };

    bool prepareSubmit();
    void submit();
    void reset();

    static void i18nData();

    friend class HTMLFormElement;
    friend class HTMLFormCollectionImpl;

    QList<HTMLGenericFormElementImpl> formElements;
    DOMString m_url;
    DOMString m_target;
    DOMString m_enctype;
    DOMString m_boundary;
    DOMString m_acceptcharset;
    QString m_encCharset;
    KHTMLView *view;
    bool m_post : 1;
    bool m_multipart : 1;
    bool m_autocomplete : 1;
    bool m_insubmit : 1;
    bool m_doingsubmit : 1;
    bool m_inreset : 1;
};

// -------------------------------------------------------------------------

class HTMLGenericFormElementImpl : public HTMLElementImpl
{
    friend class HTMLFormElementImpl;
    friend class khtml::RenderFormElement;

public:
    HTMLGenericFormElementImpl(DocumentPtr *doc);
    HTMLGenericFormElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);
    virtual ~HTMLGenericFormElementImpl();

    HTMLFormElementImpl *form() { return m_form; }

    virtual void parseAttribute(AttrImpl *attr);

    virtual void attach();
    virtual void detach();

    virtual void reset() {}

    void onBlur();
    void onFocus();
    void onSelect();
    void onChange();

    bool disabled() const { return m_disabled; }
    void setDisabled(bool _disabled) { m_disabled = _disabled; }

    virtual bool isSelectable() const;
    virtual bool isEnumeratable() const { return false; }

    bool readOnly() const { return m_readOnly; }
    void setReadOnly(bool _readOnly) { m_readOnly = _readOnly; }

    const DOMString &name() const { return _name; }
    void setForm(HTMLFormElementImpl *f) { m_form = f; }

    /*
     * override in derived classes to get the encoded name=value pair
     * for submitting
     * return true for a successful control (see 17.13.2)
     */
    virtual bool encoding(const QTextCodec*, khtml::encodingList&, bool) { return false; }

    virtual void setFocus(bool);
    virtual void setParent(NodeImpl *parent);

protected:
    HTMLFormElementImpl *getForm() const;

    DOMString _name;
    HTMLFormElementImpl *m_form;
    KHTMLView *view;
    bool m_disabled, m_readOnly;
};

// -------------------------------------------------------------------------

class HTMLButtonElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLButtonElementImpl(DocumentPtr *doc);
    HTMLButtonElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);

    virtual ~HTMLButtonElementImpl();

    enum typeEnum {
        SUBMIT,
        RESET,
        BUTTON
    };

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    DOMString type() const;

    void parseAttribute(AttrImpl *attr);

    virtual void attach();

    virtual void defaultEventHandler(EventImpl *evt);

    virtual bool encoding(const QTextCodec*, khtml::encodingList&, bool);

protected:
    DOMString m_value;
    QString   m_currValue;
    typeEnum  m_type : 2;
    bool      m_dirty : 1;
    bool      m_clicked : 1;
    bool      m_activeSubmit : 1;
};

// -------------------------------------------------------------------------

class HTMLFieldSetElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLFieldSetElementImpl(DocumentPtr *doc);
    HTMLFieldSetElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);

    virtual ~HTMLFieldSetElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
};

// -------------------------------------------------------------------------

class HTMLInputElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderLineEdit;

public:
    enum typeEnum {
        TEXT,
        PASSWORD,
        CHECKBOX,
        RADIO,
        SUBMIT,
        RESET,
        FILE,
        HIDDEN,
        IMAGE,
        BUTTON,
        ISINDEX
    };

    HTMLInputElementImpl(DocumentPtr *doc);
    HTMLInputElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);
    virtual ~HTMLInputElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual bool isEnumeratable() const { return inputType() != IMAGE; }

    bool autoComplete() const { return m_autocomplete; }

    bool checked() const { return m_checked; }
    void setChecked(bool);
    long maxLength() const { return m_maxLen; }
    int size() const { return m_size; }
    DOMString type() const;

    DOMString value() const;
    void setValue(DOMString val);

    DOMString filename() const { return m_filename; }
    void setFilename(DOMString _filename) { m_filename = _filename; }

    virtual QString state();
    virtual void restoreState(const QString &);

    void select();
    void click();

    virtual void parseAttribute(AttrImpl *attr);

    virtual void attach();

    virtual bool encoding(const QTextCodec*, khtml::encodingList&, bool);

    typeEnum inputType() const { return m_type; }
    virtual void reset();

    // used in case input type=image was clicked.
    int clickX() const { return xPos; }
    int clickY() const { return yPos; }

    virtual void defaultEventHandler(EventImpl *evt);

protected:
    DOMString m_value;
    DOMString m_filename;
    DOMString m_src;
    DOMString m_defaultValue;
    int       xPos;
    short     m_maxLen;
    short     m_size;
    short     yPos;

    typeEnum m_type : 4;
    bool m_clicked : 1 ;
    bool m_defaultChecked : 1;
    bool m_checked : 1;
    bool m_haveType : 1;
    bool m_firstAttach :1;
    bool m_activeSubmit : 1;
    bool m_autocomplete : 1;

private:
    void init();
};

// -------------------------------------------------------------------------

class HTMLLabelElementImpl : public HTMLElementImpl
{
public:
    HTMLLabelElementImpl(DocumentPtr *doc);
    virtual ~HTMLLabelElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *attr);

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
    HTMLLegendElementImpl(DocumentPtr *doc);
    HTMLLegendElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);
    virtual ~HTMLLegendElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
};


// -------------------------------------------------------------------------

class HTMLSelectElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLSelectElementImpl(DocumentPtr *doc);
    HTMLSelectElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);

    virtual const DOMString nodeName() const { return "SELECT"; }
    virtual ushort id() const;

    DOMString type() const;

    long selectedIndex() const;
    void setSelectedIndex( long index );
    
    virtual bool isEnumeratable() const { return true; }

    long length() const;

    long size() const { return m_size; }

    bool multiple() const { return m_multiple; }

    void add ( const HTMLElement &element, const HTMLElement &before );
    void remove ( long index );

    DOMString value();
    void setValue(DOMStringImpl* value);

    virtual QString state();
    virtual void restoreState(const QString &);

    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );

    virtual void parseAttribute(AttrImpl *attr);

    virtual void attach();
    virtual bool encoding(const QTextCodec*, khtml::encodingList&, bool);

    // get the actual listbox index of the optionIndexth option
    int optionToListIndex(int optionIndex) const;
    // reverse of optionToListIndex - get optionIndex from listboxIndex
    int listToOptionIndex(int listIndex) const;
    void recalcListItems();
    QArray<HTMLGenericFormElementImpl*> listItems() const { return m_listItems; }
    virtual void reset();
    void notifyOptionSelected(HTMLOptionElementImpl *selectedOption, bool selected);

protected:
    QArray<HTMLGenericFormElementImpl*> m_listItems;
    short m_size : 15;
    bool m_multiple : 1;
};


// -------------------------------------------------------------------------

class HTMLOptGroupElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLOptGroupElementImpl(DocumentPtr *doc);
    HTMLOptGroupElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);
    virtual ~HTMLOptGroupElementImpl();

    virtual const DOMString nodeName() const { return "OPTGROUP"; }
    virtual ushort id() const;

    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual void parseAttribute(AttrImpl *attr);
    void recalcSelectOptions();
    virtual void setChanged(bool);

};


// ---------------------------------------------------------------------------

class HTMLOptionElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderSelect;
    
public:
    HTMLOptionElementImpl(DocumentPtr *doc);
    HTMLOptionElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    DOMString text();

    long index() const;
    void setIndex( long );
    virtual void parseAttribute(AttrImpl *attr);
    DOMString value() const { return m_value; }

    bool selected() const { return m_selected; }
    void setSelected(bool _selected);

    HTMLSelectElementImpl *getSelect();

    virtual void setChanged(bool);

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

    HTMLTextAreaElementImpl(DocumentPtr *doc);
    HTMLTextAreaElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);

    virtual const DOMString nodeName() const { return "TEXTAREA"; }
    virtual ushort id() const;

    long cols() const { return m_cols; }

    long rows() const { return m_rows; }

    WrapMethod wrap() const { return m_wrap; }

    virtual bool isEnumeratable() const { return true; }

    DOMString type() const;

    virtual QString state();
    virtual void restoreState(const QString &);

    void select (  );

    virtual void parseAttribute(AttrImpl *attr);
    virtual void attach();
    virtual bool encoding(const QTextCodec*, khtml::encodingList&, bool);
    virtual void reset();
    DOMString value();
    void setValue(DOMString _value);
    DOMString defaultValue();
    void setDefaultValue(DOMString _defaultValue);


protected:
    int m_rows;
    int m_cols;
    WrapMethod m_wrap;
    QString m_value;
    bool m_dirtyvalue;
};

// -------------------------------------------------------------------------

class HTMLIsIndexElementImpl : public HTMLInputElementImpl
{
public:
    HTMLIsIndexElementImpl(DocumentPtr *doc);
    HTMLIsIndexElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f);

    ~HTMLIsIndexElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *attr);
    virtual void attach();

protected:
    DOMString m_prompt;
};


}; //namespace

#endif
