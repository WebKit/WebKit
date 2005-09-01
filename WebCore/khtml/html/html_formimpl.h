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
#include "html/html_miscimpl.h"

#include <qptrvector.h>
#include <qmemarray.h>
#include "hashtable.h"
#include "pointerhash.h"

class KHTMLView;
class QTextCodec;

namespace khtml
{
    class FormData;
    class RenderFormElement;
    class RenderTextArea;
    class RenderSelect;
    class RenderLineEdit;
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
class HTMLInputElementImpl;
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

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 3; }

    virtual void attach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
 
    SharedPtr<HTMLCollectionImpl> elements();
    long length() const;

    DOMString enctype() const { return m_enctype; }
    void setEnctype(const DOMString &);

    DOMString boundary() const { return m_boundary; }
    void setBoundary(const DOMString &);

    bool autoComplete() const { return m_autocomplete; }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    void radioButtonChecked(HTMLInputElementImpl *caller);
    HTMLInputElementImpl* checkedRadioButtonForGroup(DOMStringImpl* name);
    void removeRadioButtonGroup(DOMStringImpl* name);

    void registerFormElement(HTMLGenericFormElementImpl *);
    void removeFormElement(HTMLGenericFormElementImpl *);
    void registerImgElement(HTMLImageElementImpl *);
    void removeImgElement(HTMLImageElementImpl *);

    bool prepareSubmit();
    void submit(bool activateSubmitButton = false);
    void reset();

    void setMalformed(bool malformed) { m_malformed = malformed; }
    virtual bool isMalformed() { return m_malformed; }
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;
    
    void submitClick();
    bool formWouldHaveSecureSubmission(const DOMString &url);

    DOMString name() const;
    void setName(const DOMString &);

    DOMString acceptCharset() const;
    void setAcceptCharset(const DOMString &);

    DOMString action() const;
    void setAction(const DOMString &);

    DOMString method() const;
    void setMethod(const DOMString &);

    DOMString target() const;
    void setTarget(const DOMString &);

    static void i18nData();

    friend class HTMLFormElement;
    friend class HTMLFormCollectionImpl;

    HTMLCollectionImpl::CollectionInfo *collectionInfo;

    QPtrVector<HTMLGenericFormElementImpl> formElements;
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

    khtml::HashMap<DOMStringImpl*, HTMLInputElementImpl*, khtml::PointerHash<DOMStringImpl*> >* m_selectedRadioButtons;
    
private:
    void parseEnctype(const DOMString &);
    bool formData(khtml::FormData &) const;

    unsigned formElementIndex(HTMLGenericFormElementImpl *);

    DOMString oldNameAttr;
};

// -------------------------------------------------------------------------

class HTMLGenericFormElementImpl : public HTMLElementImpl
{
    friend class HTMLFormElementImpl;
    friend class khtml::RenderFormElement;

public:
    HTMLGenericFormElementImpl(const QualifiedName& tagName, DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    virtual ~HTMLGenericFormElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    HTMLFormElementImpl *form() { return m_form; }

    virtual DOMString type() const = 0;

    virtual bool isControl() const { return true; }
    virtual bool isEnabled() const { return !disabled(); }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    virtual void attach();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);

    virtual void reset() {}

    void onSelect();
    void onChange();

    bool disabled() const;
    void setDisabled(bool _disabled);

    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return false; }

    bool readOnly() const { return m_readOnly; }
    void setReadOnly(bool _readOnly);

    virtual void recalcStyle( StyleChange );

    DOMString name() const;
    void setName(const DOMString& name);

    void setOverrideName(const DOMString& name);

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

    long tabIndex() const;
    void setTabIndex(long);

protected:
    HTMLFormElementImpl *getForm() const;

    DOMString m_overrideName;
    HTMLFormElementImpl *m_form;
    bool m_disabled : 1;
    bool m_readOnly: 1;
    bool m_inited : 1;
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

    DOMString type() const;

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    virtual void defaultEventHandler(EventImpl *evt);
    virtual bool appendFormData(FormDataList&, bool);

    virtual bool isSuccessfulSubmitButton() const;
    virtual bool isActivatedSubmit() const;
    virtual void setActivatedSubmit(bool flag);

    virtual void accessKeyAction(bool sendToAnyElement);

    DOMString accessKey() const;
    void setAccessKey(const DOMString &);

    DOMString value() const;
    void setValue(const DOMString &);

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
    
    virtual int tagPriority() const { return 3; }
    virtual bool checkDTD(const NodeImpl* newChild);

    virtual bool isFocusable() const;
    
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    virtual DOMString type() const;
};

// -------------------------------------------------------------------------

class HTMLInputElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderLineEdit;
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
    HTMLInputElementImpl(const QualifiedName& tagName, DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    virtual ~HTMLInputElementImpl();
    void init();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool isKeyboardFocusable() const;
    virtual bool isEnumeratable() const { return inputType() != IMAGE; }

    bool autoComplete() const { return m_autocomplete; }

    virtual bool isChecked() const { return checked(); }
    
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

    bool canHaveSelection();

    long selectionStart();
    long selectionEnd();
    void setSelectionStart(long);
    void setSelectionEnd(long);

    void select();
    void setSelectionRange(long, long);
    
    virtual void click(bool sendMouseEvents = false, bool showPressedLook = true);
    virtual void accessKeyAction(bool sendToAnyElement);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

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

    virtual void preDispatchEventHandler(EventImpl *evt);
    virtual void defaultEventHandler(EventImpl *evt);
    virtual bool isEditable();

    DOMString altText() const;
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    long maxResults() const { return m_maxResults; }

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

    void setSize(unsigned long);

    DOMString src() const;
    void setSrc(const DOMString &);

    void setMaxLength(long);

    DOMString useMap() const;
    void setUseMap(const DOMString &);

protected:
    bool storesValueSeparateFromAttribute() const;

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

    virtual int tagPriority() const { return 5; }

    virtual bool isFocusable() const;

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    virtual void accessKeyAction(bool sendToAnyElement);

    /**
     * the form element this label is associated to.
     */
    ElementImpl *formElement();

    HTMLFormElementImpl *form();

    DOMString accessKey() const;
    void setAccessKey(const DOMString &);

    DOMString htmlFor() const;
    void setHtmlFor(const DOMString &);

    void focus();

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
    
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    virtual DOMString type() const;

    virtual void accessKeyAction(bool sendToAnyElement);

    /**
     * The first form element in the legend's fieldset 
     */
    ElementImpl *formElement();

    DOMString accessKey() const;
    void setAccessKey(const DOMString &);

    DOMString align() const;
    void setAlign(const DOMString &);
    
    void focus();
};

// -------------------------------------------------------------------------

class HTMLSelectElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderSelect;

public:
    HTMLSelectElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    HTMLSelectElementImpl(const QualifiedName& tagName, DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    ~HTMLSelectElementImpl();
    void init();

    virtual int tagPriority() const { return 6; }
    virtual bool checkDTD(const NodeImpl* newChild);

    DOMString type() const;

    virtual void recalcStyle( StyleChange );

    long selectedIndex() const;
    void setSelectedIndex( long index );

    virtual bool isEnumeratable() const { return true; }

    long length() const;

    long minWidth() const { return m_minwidth; }

    long size() const { return m_size; }

    bool multiple() const { return m_multiple; }

    void add ( HTMLElementImpl *element, HTMLElementImpl *before, int &exceptioncode );
    void remove ( long index );
    void blur();
    void focus();

    DOMString value();
    void setValue(const DOMString &);
    
    HTMLOptionsCollectionImpl *options();
    SharedPtr<HTMLCollectionImpl> optionsHTMLCollection(); // FIXME: Remove this and migrate to options().

    virtual bool maintainsState() { return true; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual NodeImpl *addChild( NodeImpl* newChild );

    virtual void childrenChanged();

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual bool appendFormData(FormDataList&, bool);

    // get the actual listbox index of the optionIndexth option
    int optionToListIndex(int optionIndex) const;
    // reverse of optionToListIndex - get optionIndex from listboxIndex
    int listToOptionIndex(int listIndex) const;

    void setRecalcListItems();

    QMemArray<HTMLElementImpl*> listItems() const
     {
         if (m_recalcListItems) const_cast<HTMLSelectElementImpl*>(this)->recalcListItems();
         return m_listItems;
     }
    virtual void reset();
    void notifyOptionSelected(HTMLOptionElementImpl *selectedOption, bool selected);

    virtual void defaultEventHandler(EventImpl *evt);
    virtual void accessKeyAction(bool sendToAnyElement);

    void setMultiple(bool);

    void setSize(long);

private:
    void recalcListItems();

protected:
    mutable QMemArray<HTMLElementImpl*> m_listItems;
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

    virtual int tagPriority() const { return 0; }

    DOMString type() const;

    // ### this is just a rough guess
    virtual bool isEnumeratable() const { return false; }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
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

    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->hasTagName(HTMLNames::optionTag) || newChild->hasTagName(HTMLNames::hrTag); }

    DOMString type() const;

    virtual bool isFocusable() const;
    
    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual NodeImpl *addChild( NodeImpl* newChild );
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    void recalcSelectOptions();

    DOMString label() const;
    void setLabel(const DOMString &);
};


// ---------------------------------------------------------------------------

class HTMLOptionElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderSelect;
    friend class DOM::HTMLSelectElementImpl;

public:
    HTMLOptionElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 2; }
    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->isTextNode(); }

    virtual bool isFocusable() const;

    DOMString type() const;

    DOMString text() const;
    void setText(const DOMString &, int &exception);

    long index() const;
    void setIndex(long, int &exception);
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    DOMString value() const;
    void setValue(const DOMString &);

    bool selected() const { return m_selected; }
    void setSelected(bool _selected);

    HTMLSelectElementImpl *getSelect() const;

    virtual void childrenChanged();

    bool defaultSelected() const;
    void setDefaultSelected( bool );

    DOMString label() const;
    void setLabel( const DOMString & );

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

    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->isTextNode(); }

    long cols() const { return m_cols; }
    long rows() const { return m_rows; }

    WrapMethod wrap() const { return m_wrap; }

    virtual bool isEnumeratable() const { return true; }

    DOMString type() const;

    virtual bool maintainsState() { return true; }
    virtual QString state();
    virtual void restoreState(QStringList &);

    long selectionStart();
    long selectionEnd();

    void setSelectionStart(long);
    void setSelectionEnd(long);

    void select (  );
    void setSelectionRange(long, long);

    virtual void childrenChanged();
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void attach();
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
    
    DOMString accessKey() const;
    void setAccessKey(const DOMString &);

    void setCols(long);

    void setRows(long);

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

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    DOMString prompt() const;
    void setPrompt(const DOMString &);

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
