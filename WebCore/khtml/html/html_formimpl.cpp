/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005 Apple Computer, Inc.
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

#undef FORMS_DEBUG
//#define FORMS_DEBUG

#include "config.h"
#include "html/html_formimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "html/html_documentimpl.h"
#include "html_imageimpl.h"
#include "khtml_settings.h"
#include "misc/formdata.h"

#include "dom/dom_exception.h"
#include "css/cssstyleselector.h"
#include "css/cssproperties.h"
#include "css/csshelper.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/EventNames.h"
#include "khtml_ext.h"

#include "rendering/render_form.h"
#include "render_button.h"
#include "render_theme.h"

#include <kcharsets.h>
#include <kdebug.h>
#include <klocale.h>
#include <qfile.h>
#include <qtextcodec.h>

// for keygen
#include <qstring.h>
#include <ksslkeygen.h>

#include <assert.h>

using namespace khtml;

namespace DOM {

using namespace EventNames;
using namespace HTMLNames;

struct FormDataListItem {
    FormDataListItem(const QCString &data) : m_data(data) { }
    FormDataListItem(const QString &path) : m_path(path) { }

    QString m_path;
    QCString m_data;
};

class FormDataList {
public:
    FormDataList(QTextCodec *);

    void appendData(const DOMString &key, const DOMString &value)
        { appendString(key.qstring()); appendString(value.qstring()); }
    void appendData(const DOMString &key, const QString &value)
        { appendString(key.qstring()); appendString(value); }
    void appendData(const DOMString &key, const QCString &value)
        { appendString(key.qstring()); appendString(value); }
    void appendData(const DOMString &key, int value)
        { appendString(key.qstring()); appendString(QString::number(value)); }
    void appendFile(const DOMString &key, const DOMString &filename);

    QValueListConstIterator<FormDataListItem> begin() const
        { return m_list.begin(); }
    QValueListConstIterator<FormDataListItem> end() const
        { return m_list.end(); }

private:
    void appendString(const QCString &s);
    void appendString(const QString &s);

    QTextCodec *m_codec;
    QValueList<FormDataListItem> m_list;
};

HTMLFormElementImpl::HTMLFormElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(formTag, doc)
{
    collectionInfo = 0;
    m_post = false;
    m_multipart = false;
    m_autocomplete = true;
    m_insubmit = false;
    m_doingsubmit = false;
    m_inreset = false;
    m_enctype = "application/x-www-form-urlencoded";
    m_boundary = "----------0xKhTmLbOuNdArY";
    m_acceptcharset = "UNKNOWN";
    m_malformed = false;
}

HTMLFormElementImpl::~HTMLFormElementImpl()
{
    delete collectionInfo;
    
    for (unsigned i = 0; i < formElements.count(); ++i)
        formElements[i]->m_form = 0;
    for (unsigned i = 0; i < imgElements.count(); ++i)
        imgElements[i]->m_form = 0;
}


bool HTMLFormElementImpl::formWouldHaveSecureSubmission(const DOMString &url)
{
    if (url.isNull()) {
        return false;
    }
    return getDocument()->completeURL(url.qstring()).startsWith("https:", false);
}


void HTMLFormElementImpl::attach()
{
    HTMLElementImpl::attach();

    // note we don't deal with calling secureFormRemoved() on detach, because the timing
    // was such that it cleared our state too early
    if (formWouldHaveSecureSubmission(m_url))
        getDocument()->secureFormAdded();
}

void HTMLFormElementImpl::insertedIntoDocument()
{
    if (getDocument()->isHTMLDocument()) {
	HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
	document->addNamedItem(oldNameAttr);
    }

    HTMLElementImpl::insertedIntoDocument();
}

void HTMLFormElementImpl::removedFromDocument()
{
    if (getDocument()->isHTMLDocument()) {
	HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
	document->removeNamedItem(oldNameAttr);
    }
   
    HTMLElementImpl::removedFromDocument();
}

int HTMLFormElementImpl::length() const
{
    int len = 0;
    for (unsigned i = 0; i < formElements.count(); ++i)
	if (formElements[i]->isEnumeratable())
	    ++len;

    return len;
}


void HTMLFormElementImpl::submitClick()
{
    bool submitFound = false;
    for (unsigned i = 0; i < formElements.count(); ++i) {
        if (formElements[i]->hasLocalName(inputTag)) {
            HTMLInputElementImpl *element = static_cast<HTMLInputElementImpl *>(formElements[i]);
            if (element->isSuccessfulSubmitButton() && element->renderer()) {
                submitFound = true;
                element->click(false);
                break;
            }
        }
    }
    if (!submitFound) // submit the form without a submit or image input
        prepareSubmit();
}


static QCString encodeCString(const QCString& e)
{
    // http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
    // safe characters like NS handles them for compatibility
    static const char *safe = "-._*";
    int elen = e.length();
    QCString encoded(( elen+e.contains( '\n' ) )*3+1);
    int enclen = 0;

    //QCString orig(e.data(), e.size());

    for(int pos = 0; pos < elen; pos++) {
        unsigned char c = e[pos];

        if ( (( c >= 'A') && ( c <= 'Z')) ||
             (( c >= 'a') && ( c <= 'z')) ||
             (( c >= '0') && ( c <= '9')) ||
             (strchr(safe, c))
            )
            encoded[enclen++] = c;
        else if ( c == ' ' )
            encoded[enclen++] = '+';
        else if ( c == '\n' || ( c == '\r' && e[pos+1] != '\n' ) )
        {
            encoded[enclen++] = '%';
            encoded[enclen++] = '0';
            encoded[enclen++] = 'D';
            encoded[enclen++] = '%';
            encoded[enclen++] = '0';
            encoded[enclen++] = 'A';
        }
        else if ( c != '\r' )
        {
            encoded[enclen++] = '%';
            unsigned int h = c / 16;
            h += (h > 9) ? ('A' - 10) : '0';
            encoded[enclen++] = h;

            unsigned int l = c % 16;
            l += (l > 9) ? ('A' - 10) : '0';
            encoded[enclen++] = l;
        }
    }
    encoded[enclen++] = '\0';
    encoded.truncate(enclen);

    return encoded;
}

// Change plain CR and plain LF to CRLF pairs.
static QCString fixLineBreaks(const QCString &s)
{
    // Compute the length.
    unsigned newLen = 0;
    const char *p = s.data();
    while (char c = *p++) {
        if (c == '\r') {
            // Safe to look ahead because of trailing '\0'.
            if (*p != '\n') {
                // Turn CR into CRLF.
                newLen += 2;
            }
        } else if (c == '\n') {
            // Turn LF into CRLF.
            newLen += 2;
        } else {
            // Leave other characters alone.
            newLen += 1;
        }
    }
    if (newLen == s.length()) {
        return s;
    }
    
    // Make a copy of the string.
    p = s.data();
    QCString result(newLen + 1);
    char *q = result.data();
    while (char c = *p++) {
        if (c == '\r') {
            // Safe to look ahead because of trailing '\0'.
            if (*p != '\n') {
                // Turn CR into CRLF.
                *q++ = '\r';
                *q++ = '\n';
            }
        } else if (c == '\n') {
            // Turn LF into CRLF.
            *q++ = '\r';
            *q++ = '\n';
        } else {
            // Leave other characters alone.
            *q++ = c;
        }
    }
    return result;
}


bool HTMLFormElementImpl::formData(FormData &form_data) const
{
    QCString enc_string = ""; // used for non-multipart data

    // find out the QTextcodec to use
    QString str = m_acceptcharset.qstring();
    str.replace(',', ' ');
    QStringList charsets = QStringList::split(' ', str);
    QTextCodec* codec = 0;
    KHTMLPart *part = getDocument()->part();
    for ( QStringList::Iterator it = charsets.begin(); it != charsets.end(); ++it )
    {
        QString enc = (*it);
        if(enc.contains("UNKNOWN"))
        {
            // use standard document encoding
            enc = "ISO-8859-1";
            if (part)
                enc = part->encoding();
        }
        if((codec = QTextCodec::codecForName(enc.latin1())))
            break;
    }

    if(!codec)
        codec = QTextCodec::codecForLocale();


    for (unsigned i = 0; i < formElements.count(); ++i) {
        HTMLGenericFormElementImpl* current = formElements[i];
        FormDataList lst(codec);

        if (!current->disabled() && current->appendFormData(lst, m_multipart))
        {
            //kdDebug(6030) << "adding name " << current->name().qstring() << endl;
            for(QValueListConstIterator<FormDataListItem> it = lst.begin(); it != lst.end(); ++it )
            {
                if (!m_multipart)
                {
                    // handle ISINDEX / <input name=isindex> special
                    // but only if its the first entry
                    if ( enc_string.isEmpty() && (*it).m_data == "isindex" ) {
                        ++it;
                        enc_string += encodeCString( (*it).m_data );
                    }
                    else {
                        if(!enc_string.isEmpty())
                            enc_string += '&';

                        enc_string += encodeCString((*it).m_data);
                        enc_string += "=";
                        ++it;
                        enc_string += encodeCString((*it).m_data);
                    }
                }
                else
                {
                    QCString hstr("--");
                    hstr += m_boundary.qstring().latin1();
                    hstr += "\r\n";
                    hstr += "Content-Disposition: form-data; name=\"";
                    hstr += (*it).m_data.data();
                    hstr += "\"";

                    // if the current type is FILE, then we also need to
                    // include the filename
                    if (current->hasLocalName(inputTag) &&
                        static_cast<HTMLInputElementImpl*>(current)->inputType() == HTMLInputElementImpl::FILE)
                    {
                        QString path = static_cast<HTMLInputElementImpl*>(current)->value().qstring();

                        // FIXME: This won't work if the filename includes a " mark,
                        // or control characters like CR or LF. This also does strange
                        // things if the filename includes characters you can't encode
                        // in the website's character set.
                        hstr += "; filename=\"";
                        hstr += codec->fromUnicode(path.mid(path.findRev('/') + 1), true);
                        hstr += "\"";

                        if(!static_cast<HTMLInputElementImpl*>(current)->value().isEmpty())
                        {
                            QString mimeType = part ? KWQ(part)->mimeTypeForFileName(path) : QString();
                            if (!mimeType.isEmpty()) {
                                hstr += "\r\nContent-Type: ";
                                hstr += mimeType.ascii();
                            }
                        }
                    }

                    hstr += "\r\n\r\n";
                    ++it;

                    // append body
                    form_data.appendData(hstr.data(), hstr.length());
                    const FormDataListItem &item = *it;
                    size_t dataSize = item.m_data.size();
                    if (dataSize != 0)
                        form_data.appendData(item.m_data, dataSize - 1);
                    else if (!item.m_path.isEmpty())
                        form_data.appendFile(item.m_path);
                    form_data.appendData("\r\n", 2);
                }
            }
        }
    }


    if (m_multipart)
        enc_string = ("--" + m_boundary.qstring() + "--\r\n").ascii();

    form_data.appendData(enc_string.data(), enc_string.length());
    return true;
}

void HTMLFormElementImpl::parseEnctype( const DOMString& type )
{
    if(type.qstring().find("multipart", 0, false) != -1 || type.qstring().find("form-data", 0, false) != -1)
    {
        m_enctype = "multipart/form-data";
        m_multipart = true;
        m_post = true;
    } else if (type.qstring().find("text", 0, false) != -1 || type.qstring().find("plain", 0, false) != -1)
    {
        m_enctype = "text/plain";
        m_multipart = false;
    }
    else
    {
        m_enctype = "application/x-www-form-urlencoded";
        m_multipart = false;
    }
}

void HTMLFormElementImpl::setBoundary( const DOMString& bound )
{
    m_boundary = bound;
}

bool HTMLFormElementImpl::prepareSubmit()
{
    KHTMLPart *part = getDocument()->part();
    if(m_insubmit || !part || part->onlyLocalReferences())
        return m_insubmit;

    m_insubmit = true;
    m_doingsubmit = false;

    if ( dispatchHTMLEvent(submitEvent,false,true) && !m_doingsubmit )
        m_doingsubmit = true;

    m_insubmit = false;

    if ( m_doingsubmit )
        submit(true);

    return m_doingsubmit;
}

void HTMLFormElementImpl::submit( bool activateSubmitButton )
{
    KHTMLView *view = getDocument()->view();
    KHTMLPart *part = getDocument()->part();
    if (!view || !part) {
        return;
    }

    if ( m_insubmit ) {
        m_doingsubmit = true;
        return;
    }

    m_insubmit = true;

    HTMLGenericFormElementImpl* firstSuccessfulSubmitButton = 0;
    bool needButtonActivation = activateSubmitButton;	// do we need to activate a submit button?
    
    KWQ(part)->clearRecordedFormValues();
    for (unsigned i = 0; i < formElements.count(); ++i) {
        HTMLGenericFormElementImpl* current = formElements[i];
        // Our app needs to get form values for password fields for doing password autocomplete,
        // so we are more lenient in pushing values, and let the app decide what to save when.
        if (current->hasLocalName(inputTag)) {
            HTMLInputElementImpl *input = static_cast<HTMLInputElementImpl*>(current);
            if (input->inputType() == HTMLInputElementImpl::TEXT
                || input->inputType() ==  HTMLInputElementImpl::PASSWORD
                || input->inputType() == HTMLInputElementImpl::SEARCH)
            {
                KWQ(part)->recordFormValue(input->name().qstring(), input->value().qstring(), this);
                if (input->renderer() && input->inputType() == HTMLInputElementImpl::SEARCH)
                    static_cast<RenderLineEdit*>(input->renderer())->addSearchResult();
            }
        }

        if (needButtonActivation) {
            if (current->isActivatedSubmit()) {
                needButtonActivation = false;
            } else if (firstSuccessfulSubmitButton == 0 && current->isSuccessfulSubmitButton()) {
                firstSuccessfulSubmitButton = current;
            }
        }
    }

    if (needButtonActivation && firstSuccessfulSubmitButton) {
        firstSuccessfulSubmitButton->setActivatedSubmit(true);
    }

    FormData form_data;
    if (formData(form_data)) {
        if(m_post) {
            part->submitForm( "post", m_url.qstring(), form_data,
                                      m_target.qstring(),
                                      enctype().qstring(),
                                      boundary().qstring() );
        }
        else {
            part->submitForm( "get", m_url.qstring(), form_data,
                                      m_target.qstring() );
        }
    }

    if (needButtonActivation && firstSuccessfulSubmitButton) {
        firstSuccessfulSubmitButton->setActivatedSubmit(false);
    }
    
    m_doingsubmit = m_insubmit = false;
}

void HTMLFormElementImpl::reset(  )
{
    KHTMLPart *part = getDocument()->part();
    if(m_inreset || !part) return;

    m_inreset = true;

    // ### DOM2 labels this event as not cancelable, however
    // common browsers( sick! ) allow it be cancelled.
    if ( !dispatchHTMLEvent(resetEvent,true, true) ) {
        m_inreset = false;
        return;
    }

    for (unsigned i = 0; i < formElements.count(); ++i)
        formElements[i]->reset();

    m_inreset = false;
}

void HTMLFormElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == actionAttr) 
    {
        bool oldURLWasSecure = formWouldHaveSecureSubmission(m_url);
        m_url = khtml::parseURL(attr->value());
        bool newURLIsSecure = formWouldHaveSecureSubmission(m_url);

        if (m_attached && (oldURLWasSecure != newURLIsSecure))
            if (newURLIsSecure)
                getDocument()->secureFormAdded();
            else
                getDocument()->secureFormRemoved();
    }
    else if (attr->name() == targetAttr) {
        m_target = attr->value();
    } else if (attr->name() == methodAttr) {
        if ( strcasecmp( attr->value(), "post" ) == 0 )
            m_post = true;
        else if ( strcasecmp( attr->value(), "get" ) == 0 )
            m_post = false;
    } else if (attr->name() == enctypeAttr) {
        parseEnctype(attr->value());
    } else if (attr->name() == accept_charsetAttr) {
        // space separated list of charsets the server
        // accepts - see rfc2045
        m_acceptcharset = attr->value();
    } else if (attr->name() == acceptAttr) {
        // ignore this one for the moment...
    } else if (attr->name() == autocompleteAttr) {
        m_autocomplete = strcasecmp( attr->value(), "off" );
    } else if (attr->name() == onsubmitAttr) {
        setHTMLEventListener(submitEvent, attr);
    } else if (attr->name() == onresetAttr) {
        setHTMLEventListener(resetEvent, attr);
    } else if (attr->name() == nameAttr) {
        DOMString newNameAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
            document->removeNamedItem(oldNameAttr);
            document->addNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

template<class T> static void insertIntoVector(QPtrVector<T> &vec, unsigned pos, T* item)
{
    unsigned size = vec.size();
    unsigned count = vec.count();
    assert(pos <= count);
    if (size == count)
        vec.resize(size == 0 ? 8 : size * 3 / 2);
    for (unsigned i = count; i > pos; --i)
        vec.insert(i, vec[i - 1]);    
    vec.insert(pos, item);
}

template<class T> static void appendToVector(QPtrVector<T> &vec, T *item)
{
    unsigned size = vec.size();
    unsigned count = vec.count();
    if (size == count)
        vec.resize(size == 0 ? 8 : size * 3 / 2);
    vec.insert(count, item);
}

template<class T> static void removeFromVector(QPtrVector<T> &vec, T *item)
{
    int pos = vec.findRef(item);
    if (pos < 0)
        return;
    unsigned count = vec.count();
    for (unsigned i = pos; i < count - 1; ++i)
        vec.insert(i, vec[i + 1]);
    vec.remove(count - 1);
}

unsigned HTMLFormElementImpl::formElementIndex(HTMLGenericFormElementImpl *e)
{
    // Check for the special case where this element is the very last thing in
    // the form's tree of children; we don't want to walk the entire tree in that
    // common case that occurs during parsing; instead we'll just return a value
    // that says "add this form element to the end of the array".
    if (e->traverseNextNode(this)) {
        unsigned i = 0;
        for (NodeImpl *node = this; node; node = node->traverseNextNode(this)) {
            if (node == e)
                return i;
            if (node->isHTMLElement()
                    && static_cast<HTMLElementImpl *>(node)->isGenericFormElement()
                    && static_cast<HTMLGenericFormElementImpl *>(node)->form() == this)
                ++i;
        }
    }
    return formElements.count();
}

void HTMLFormElementImpl::registerFormElement(HTMLGenericFormElementImpl *e)
{
    insertIntoVector(formElements, formElementIndex(e), e);
}

void HTMLFormElementImpl::removeFormElement(HTMLGenericFormElementImpl *e)
{
    if (!e->name().isEmpty() && getDocument()) {
        HTMLGenericFormElementImpl* currentCheckedRadio = getDocument()->checkedRadioButtonForGroup(e->name().impl(), this);
        if (currentCheckedRadio == e)
            getDocument()->removeRadioButtonGroup(e->name().impl(), this);
    }
    removeFromVector(formElements, e);
}

bool HTMLFormElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == actionAttr;
}

void HTMLFormElementImpl::registerImgElement(HTMLImageElementImpl *e)
{
    appendToVector(imgElements, e);
}

void HTMLFormElementImpl::removeImgElement(HTMLImageElementImpl *e)
{
    removeFromVector(imgElements, e);
}

RefPtr<HTMLCollectionImpl> HTMLFormElementImpl::elements()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLFormCollectionImpl(this));
}

DOMString HTMLFormElementImpl::name() const
{
    return getAttribute(nameAttr);
}

void HTMLFormElementImpl::setName(const DOMString &value)
{
    setAttribute(nameAttr, value);
}

DOMString HTMLFormElementImpl::acceptCharset() const
{
    return getAttribute(accept_charsetAttr);
}

void HTMLFormElementImpl::setAcceptCharset(const DOMString &value)
{
    setAttribute(accept_charsetAttr, value);
}

DOMString HTMLFormElementImpl::action() const
{
    return getAttribute(actionAttr);
}

void HTMLFormElementImpl::setAction(const DOMString &value)
{
    setAttribute(actionAttr, value);
}

void HTMLFormElementImpl::setEnctype(const DOMString &value)
{
    setAttribute(enctypeAttr, value);
}

DOMString HTMLFormElementImpl::method() const
{
    return getAttribute(methodAttr);
}

void HTMLFormElementImpl::setMethod(const DOMString &value)
{
    setAttribute(methodAttr, value);
}

DOMString HTMLFormElementImpl::target() const
{
    return getAttribute(targetAttr);
}

void HTMLFormElementImpl::setTarget(const DOMString &value)
{
    setAttribute(targetAttr, value);
}

// -------------------------------------------------------------------------

HTMLGenericFormElementImpl::HTMLGenericFormElementImpl(const QualifiedName& tagName, DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLElementImpl(tagName, doc)
{
    m_disabled = m_readOnly = false;

    if (f)
	m_form = f;
    else
	m_form = getForm();
    if (m_form)
        m_form->registerFormElement(this);
}

HTMLGenericFormElementImpl::~HTMLGenericFormElementImpl()
{
    if (m_form)
        m_form->removeFormElement(this);
}

void HTMLGenericFormElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == nameAttr) {
        // Do nothing.
    } else if (attr->name() == disabledAttr) {
        bool oldDisabled = m_disabled;
        m_disabled = !attr->isNull();
        if (oldDisabled != m_disabled) {
            setChanged();
            if (renderer() && renderer()->style()->hasAppearance())
                theme()->stateChanged(renderer(), EnabledState);
        }
    } else if (attr->name() == readonlyAttr) {
        bool oldReadOnly = m_readOnly;
        m_readOnly = !attr->isNull();
        if (oldReadOnly != m_readOnly) setChanged();
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

void HTMLGenericFormElementImpl::attach()
{
    assert(!attached());

    HTMLElementImpl::attach();

    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (m_render) {
        m_render->updateFromElement();
    
        // Delayed attachment in order to prevent FOUC can result in an object being
        // programmatically focused before it has a render object.  If we have been focused
        // (i.e., if we are the focusNode) then go ahead and focus our corresponding native widget.
        // (Attach/detach can also happen as a result of display type changes, e.g., making a widget
        // block instead of inline, and focus should be restored in that case as well.)
        if (getDocument()->focusNode() == this && m_render->isWidget() && 
            static_cast<RenderWidget*>(renderer())->widget())
            static_cast<RenderWidget*>(renderer())->widget()->setFocus();
    }
}

void HTMLGenericFormElementImpl::insertedIntoTree(bool deep)
{
    if (!m_form) {
        // This handles the case of a new form element being created by
        // JavaScript and inserted inside a form.  In the case of the parser
        // setting a form, we will already have a non-null value for m_form, 
        // and so we don't need to do anything.
        m_form = getForm();
	if (m_form)
	    m_form->registerFormElement(this);
    }

    HTMLElementImpl::insertedIntoTree(deep);
}

void HTMLGenericFormElementImpl::removedFromTree(bool deep)
{
    // When being removed we have to possibly null out our form and remove ourselves from
    // the form's list of elements.
    if (m_form) {
        // Attempt to re-acquire the form.  If we can't re-acquire, then
        // remove ourselves from the form.
        NodeImpl* form = parentNode();
        NodeImpl* root = this;
        while (form) {
            if (form->hasTagName(formTag))
                break;
            root = form;
            form = form->parentNode();
        }
        
        // If the form has been demoted to a leaf, then we won't find it as an ancestor.
        // Check to see if the root node of our current form and our root node
        // match.  If so, preserve the connection to the form.
        if (!form) {
            NodeImpl* formRoot = m_form;
            while (formRoot->parent()) formRoot = formRoot->parent();
            if (formRoot == root)
                form = m_form;
        } 
        
        if (!form) {
            m_form->removeFormElement(this);
            m_form = 0;
        } else
            assert(form == m_form); // The re-acquired form should be the same. If not, something really strange happened.
    }
   
    HTMLElementImpl::removedFromTree(deep);
}

HTMLFormElementImpl *HTMLGenericFormElementImpl::getForm() const
{
    NodeImpl *p = parentNode();
    while(p) {
        if (p->hasTagName(formTag))
            return static_cast<HTMLFormElementImpl *>(p);
        p = p->parentNode();
    }
    return 0;
}

DOMString HTMLGenericFormElementImpl::name() const
{
    DOMString n = getAttribute(nameAttr);
    return n.isNull() ? "" : n;
}

void HTMLGenericFormElementImpl::setName(const DOMString &value)
{
    setAttribute(nameAttr, value);
}

void HTMLGenericFormElementImpl::onSelect()
{
    // ### make this work with new form events architecture
    dispatchHTMLEvent(selectEvent,true,false);
}

void HTMLGenericFormElementImpl::onChange()
{
    // ### make this work with new form events architecture
    dispatchHTMLEvent(changeEvent,true,false);
}

bool HTMLGenericFormElementImpl::disabled() const
{
    return m_disabled;
}

void HTMLGenericFormElementImpl::setDisabled(bool b)
{
    setAttribute(disabledAttr, b ? "" : 0);
}

void HTMLGenericFormElementImpl::setReadOnly(bool b)
{
    setAttribute(readonlyAttr, b ? "" : 0);
}

void HTMLGenericFormElementImpl::recalcStyle( StyleChange ch )
{
    //bool changed = changed();
    HTMLElementImpl::recalcStyle( ch );

    if (m_render /*&& changed*/)
        m_render->updateFromElement();
}

bool HTMLGenericFormElementImpl::isFocusable() const
{
    if (disabled() || !m_render || 
        (m_render->style() && m_render->style()->visibility() != VISIBLE) || 
        m_render->width() == 0 || m_render->height() == 0)
        return false;
    return true;
}

bool HTMLGenericFormElementImpl::isKeyboardFocusable() const
{
    if (isFocusable()) {
        if (m_render->isWidget()) {
            return static_cast<RenderWidget*>(m_render)->widget() &&
                (static_cast<RenderWidget*>(m_render)->widget()->focusPolicy() & QWidget::TabFocus);
        }
	if (getDocument()->part())
	    return getDocument()->part()->tabsToAllControls();
    }
    return false;
}

bool HTMLGenericFormElementImpl::isMouseFocusable() const
{
    if (isFocusable()) {
        if (m_render->isWidget()) {
            return static_cast<RenderWidget*>(m_render)->widget() &&
                (static_cast<RenderWidget*>(m_render)->widget()->focusPolicy() & QWidget::ClickFocus);
        }
        // For <input type=image> and <button>, we will assume no mouse focusability.  This is
        // consistent with OS X behavior for buttons.
        return false;
    }
    return false;
}

bool HTMLGenericFormElementImpl::isEditable()
{
    return false;
}

// Special chars used to encode form state strings.
// We pick chars that are unlikely to be used in an HTML attr, so we rarely have to really encode.
const char stateSeparator = '&';
const char stateEscape = '<';
static const char stateSeparatorMarker[] = "<A";
static const char stateEscapeMarker[] = "<<";

// Encode an element name so we can put it in a state string without colliding
// with our separator char.
static QString encodedElementName(QString str)
{
    int sepLoc = str.find(stateSeparator);
    int escLoc = str.find(stateSeparator);
    if (sepLoc >= 0 || escLoc >= 0) {
        QString newStr = str;
        //   replace "<" with "<<"
        while (escLoc >= 0) {
            newStr.replace(escLoc, 1, stateEscapeMarker);
            escLoc = str.find(stateSeparator, escLoc+1);
        }
        //   replace "&" with "<A"
        while (sepLoc >= 0) {
            newStr.replace(sepLoc, 1, stateSeparatorMarker);
            sepLoc = str.find(stateSeparator, sepLoc+1);
        }
        return newStr;
    } else {
        return str;
    }
}

QString HTMLGenericFormElementImpl::state( )
{
    // Build a string that contains ElementName&ElementType&
    return encodedElementName(name().qstring()) + stateSeparator + type().qstring() + stateSeparator;
}

QString HTMLGenericFormElementImpl::findMatchingState(QStringList &states)
{
    QString encName = encodedElementName(name().qstring());
    QString typeStr = type().qstring();
    for (QStringList::Iterator it = states.begin(); it != states.end(); ++it) {
        QString state = *it;
        int sep1 = state.find(stateSeparator);
        int sep2 = state.find(stateSeparator, sep1+1);
        assert(sep1 >= 0);
        assert(sep2 >= 0);

        QString nameAndType = state.left(sep2);
        if (encName.length() + typeStr.length() + 1 == (uint)sep2
            && nameAndType.startsWith(encName)
            && nameAndType.endsWith(typeStr))
        {
            states.remove(it);
            return state.mid(sep2+1);
        }
    }
    return QString::null;
}

int HTMLGenericFormElementImpl::tabIndex() const
{
    return getAttribute(tabindexAttr).toInt();
}

void HTMLGenericFormElementImpl::setTabIndex(int value)
{
    setAttribute(tabindexAttr, QString::number(value));
}

// -------------------------------------------------------------------------

HTMLButtonElementImpl::HTMLButtonElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(buttonTag, doc, f)
{
    m_type = SUBMIT;
    m_dirty = true;
    m_activeSubmit = false;
}

HTMLButtonElementImpl::~HTMLButtonElementImpl()
{
}

RenderObject* HTMLButtonElementImpl::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderButton(this);
}

DOMString HTMLButtonElementImpl::type() const
{
    return getAttribute(typeAttr);
}

void HTMLButtonElementImpl::blur()
{
    if(getDocument()->focusNode() == this)
	getDocument()->setFocusNode(0);
}

void HTMLButtonElementImpl::focus()
{
    getDocument()->setFocusNode(this);
}

void HTMLButtonElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() ==  typeAttr) {
        if ( strcasecmp( attr->value(), "submit" ) == 0 )
            m_type = SUBMIT;
        else if ( strcasecmp( attr->value(), "reset" ) == 0 )
            m_type = RESET;
        else if ( strcasecmp( attr->value(), "button" ) == 0 )
            m_type = BUTTON;
    } else if (attr->name() == valueAttr) {
        m_value = attr->value();
        m_currValue = m_value;
    } else if (attr->name() == accesskeyAttr) {
        // Do nothing.
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

void HTMLButtonElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (m_type != BUTTON && (evt->type() == DOMActivateEvent)) {

        if(m_form && m_type == SUBMIT) {
            m_activeSubmit = true;
            m_form->prepareSubmit();
            m_activeSubmit = false; // in case we were canceled
        }
        if(m_form && m_type == RESET) m_form->reset();
    }
    HTMLGenericFormElementImpl::defaultEventHandler(evt);
}

bool HTMLButtonElementImpl::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names
    // to be considered successful. However, other browsers
    // do not impose this constraint. Therefore, we behave
    // differently and can use different buttons than the 
    // author intended. 
    // Remove the name constraint for now.
    // Was: m_type == SUBMIT && !m_disabled && !name().isEmpty()
    return m_type == SUBMIT && !m_disabled;
}

bool HTMLButtonElementImpl::isActivatedSubmit() const
{
    return m_activeSubmit;
}

void HTMLButtonElementImpl::setActivatedSubmit(bool flag)
{
    m_activeSubmit = flag;
}

bool HTMLButtonElementImpl::appendFormData(FormDataList& encoding, bool /*multipart*/)
{
    if (m_type != SUBMIT || name().isEmpty() || !m_activeSubmit)
        return false;
    encoding.appendData(name(), m_currValue);
    return true;
}

void HTMLButtonElementImpl::accessKeyAction(bool sendToAnyElement)
{   
    // send the mouse button events iff the
    // caller specified sendToAnyElement
    click(sendToAnyElement);
}

DOMString HTMLButtonElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLButtonElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

DOMString HTMLButtonElementImpl::value() const
{
    return getAttribute(valueAttr);
}

void HTMLButtonElementImpl::setValue(const DOMString &value)
{
    setAttribute(valueAttr, value);
}

// -------------------------------------------------------------------------

HTMLFieldSetElementImpl::HTMLFieldSetElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
   : HTMLGenericFormElementImpl(fieldsetTag, doc, f)
{
}

HTMLFieldSetElementImpl::~HTMLFieldSetElementImpl()
{
}

bool HTMLFieldSetElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(legendTag) || HTMLElementImpl::checkDTD(newChild);
}

bool HTMLFieldSetElementImpl::isFocusable() const
{
    return false;
}

DOMString HTMLFieldSetElementImpl::type() const
{
    return "fieldset";
}

RenderObject* HTMLFieldSetElementImpl::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderFieldset(this);
}

// -------------------------------------------------------------------------

HTMLInputElementImpl::HTMLInputElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(inputTag, doc, f)
{
    init();
}

HTMLInputElementImpl::HTMLInputElementImpl(const QualifiedName& tagName, DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(tagName, doc, f)
{
    init();
}

void HTMLInputElementImpl::init()
{
    m_imageLoader = 0;
    m_valueMatchesRenderer = false;
    m_type = TEXT;
    m_maxLen = -1;
    m_size = 20;
    m_checked = false;
    m_defaultChecked = false;
    m_useDefaultChecked = true;
    m_indeterminate = false;

    m_haveType = false;
    m_activeSubmit = false;
    m_autocomplete = true;
    m_inited = false;

    xPos = 0;
    yPos = 0;

    m_maxResults = -1;

    if (m_form)
        m_autocomplete = m_form->autoComplete();
}

HTMLInputElementImpl::~HTMLInputElementImpl()
{
    if (getDocument()) getDocument()->deregisterMaintainsState(this);
    delete m_imageLoader;
}

DOMString HTMLInputElementImpl::name() const
{
    return m_name.isNull() ? "" : m_name;
}

bool HTMLInputElementImpl::isKeyboardFocusable() const
{
    // If the base class says we can't be focused, then we can stop now.
    if (!HTMLGenericFormElementImpl::isKeyboardFocusable())
        return false;

    if (m_type == RADIO) {
        // Unnamed radio buttons are never focusable (matches WinIE).
        if (name().isEmpty())
            return false;

        // Never allow keyboard tabbing to leave you in the same radio group.  Always
        // skip any other elements in the group.
        NodeImpl* currentFocusNode = getDocument()->focusNode();
        if (currentFocusNode && currentFocusNode->hasTagName(inputTag)) {
            HTMLInputElementImpl* focusedInput = static_cast<HTMLInputElementImpl*>(currentFocusNode);
            if (focusedInput->inputType() == RADIO && focusedInput->form() == m_form &&
                focusedInput->name() == name())
                return false;
        }
        
        // Allow keyboard focus if we're checked or if nothing in the group is checked.
        return checked() || !getDocument()->checkedRadioButtonForGroup(name().impl(), m_form);
    }
    
    return true;
}

void HTMLInputElementImpl::setType(const DOMString& t)
{
    if (t.isEmpty()) {
        int exccode;
        removeAttribute(typeAttr, exccode);
    }
    else
        setAttribute(typeAttr, t);
}

void HTMLInputElementImpl::setInputType(const DOMString& t)
{
    typeEnum newType;
    
    if ( strcasecmp( t, "password" ) == 0 )
        newType = PASSWORD;
    else if ( strcasecmp( t, "checkbox" ) == 0 )
        newType = CHECKBOX;
    else if ( strcasecmp( t, "radio" ) == 0 )
        newType = RADIO;
    else if ( strcasecmp( t, "submit" ) == 0 )
        newType = SUBMIT;
    else if ( strcasecmp( t, "reset" ) == 0 )
        newType = RESET;
    else if ( strcasecmp( t, "file" ) == 0 )
        newType = FILE;
    else if ( strcasecmp( t, "hidden" ) == 0 )
        newType = HIDDEN;
    else if ( strcasecmp( t, "image" ) == 0 )
        newType = IMAGE;
    else if ( strcasecmp( t, "button" ) == 0 )
        newType = BUTTON;
    else if ( strcasecmp( t, "khtml_isindex" ) == 0 )
        newType = ISINDEX;
    else if ( strcasecmp( t, "search" ) == 0 )
        newType = SEARCH;
    else if ( strcasecmp( t, "range" ) == 0 )
        newType = RANGE;
    else
        newType = TEXT;

    // ### IMPORTANT: Don't allow the type to be changed to FILE after the first
    // type change, otherwise a JavaScript programmer would be able to set a text
    // field's value to something like /etc/passwd and then change it to a file field.
    if (m_type != newType) {
        if (newType == FILE && m_haveType) {
            // Set the attribute back to the old value.
            // Useful in case we were called from inside parseMappedAttribute.
            setAttribute(typeAttr, type());
        } else {
            if (m_type == RADIO && !name().isEmpty()) {
                if (getDocument()->checkedRadioButtonForGroup(name().impl(), m_form) == this)
                    getDocument()->removeRadioButtonGroup(name().impl(), m_form);
            }
            bool wasAttached = m_attached;
            if (wasAttached)
                detach();
            bool didStoreValue = storesValueSeparateFromAttribute();
            m_type = newType;
            bool willStoreValue = storesValueSeparateFromAttribute();
            if (didStoreValue && !willStoreValue && !m_value.isNull()) {
                setAttribute(valueAttr, m_value);
                m_value = DOMString();
            }
            if (!didStoreValue && willStoreValue) {
                m_value = getAttribute(valueAttr);
            }
            if (wasAttached)
                attach();
                
            // If our type morphs into a radio button and we are checked, then go ahead
            // and signal this to the form.
            if (m_type == RADIO && checked())
                getDocument()->radioButtonChecked(this, m_form);
        }
    }
    m_haveType = true;
    
    if (m_type != IMAGE && m_imageLoader) {
        delete m_imageLoader;
        m_imageLoader = 0;
    }
}

DOMString HTMLInputElementImpl::type() const
{
    // needs to be lowercase according to DOM spec
    switch (m_type) {
    case TEXT: return "text";
    case PASSWORD: return "password";
    case CHECKBOX: return "checkbox";
    case RADIO: return "radio";
    case SUBMIT: return "submit";
    case RESET: return "reset";
    case FILE: return "file";
    case HIDDEN: return "hidden";
    case IMAGE: return "image";
    case BUTTON: return "button";
    case SEARCH: return "search";
    case RANGE: return "range";
    case ISINDEX: return "";
    }
    return "";
}

QString HTMLInputElementImpl::state( )
{
    assert(m_type != PASSWORD);		// should never save/restore password fields

    QString state = HTMLGenericFormElementImpl::state();
    switch (m_type) {
    case CHECKBOX:
    case RADIO:
        return state + (checked() ? "on" : "off");
    default:
        return state + value().qstring()+'.'; // Make sure the string is not empty!
    }
}

void HTMLInputElementImpl::restoreState(QStringList &states)
{
    assert(m_type != PASSWORD);		// should never save/restore password fields
    
    QString state = HTMLGenericFormElementImpl::findMatchingState(states);
    if (state.isNull()) return;

    switch (m_type) {
    case CHECKBOX:
    case RADIO:
        setChecked((state == "on"));
        break;
    default:
        setValue(DOMString(state.left(state.length()-1)));
        break;
    }
}

bool HTMLInputElementImpl::canHaveSelection()
{
    switch (m_type) {
        case TEXT:
        case PASSWORD:
        case SEARCH:
            return true;
        default:
            break;
    }
    return false;
}

int HTMLInputElementImpl::selectionStart()
{
    if (!m_render) return 0;
    
    switch (m_type) {
        case PASSWORD:
        case SEARCH:
        case TEXT:
            return static_cast<RenderLineEdit *>(m_render)->selectionStart();
        default:
            break;
    }
    return 0;
}

int HTMLInputElementImpl::selectionEnd()
{
    if (!m_render) return 0;
    
    switch (m_type) {
        case PASSWORD:
        case SEARCH:
        case TEXT:
            return static_cast<RenderLineEdit *>(m_render)->selectionEnd();
        default:
            break;
    }
    return 0;
}

void HTMLInputElementImpl::setSelectionStart(int start)
{
    if (!m_render) return;
    
    switch (m_type) {
        case PASSWORD:
        case SEARCH:
        case TEXT:
            static_cast<RenderLineEdit *>(m_render)->setSelectionStart(start);
            break;
        default:
            break;
    }
}

void HTMLInputElementImpl::setSelectionEnd(int end)
{
    if (!m_render) return;
    
    switch (m_type) {
        case PASSWORD:
        case SEARCH:
        case TEXT:
            static_cast<RenderLineEdit *>(m_render)->setSelectionEnd(end);
            break;
        default:
            break;
    }
}

void HTMLInputElementImpl::select(  )
{
    if(!m_render) return;

    switch (m_type) {
        case FILE:
            static_cast<RenderFileButton*>(m_render)->select();
            break;
        case PASSWORD:
        case SEARCH:
        case TEXT:
            static_cast<RenderLineEdit*>(m_render)->select();
            break;
        case BUTTON:
        case CHECKBOX:
        case HIDDEN:
        case IMAGE:
        case ISINDEX:
        case RADIO:
        case RANGE:
        case RESET:
        case SUBMIT:
            break;
    }
}

void HTMLInputElementImpl::setSelectionRange(int start, int end)
{
    if (!m_render) return;
    
    switch (m_type) {
        case PASSWORD:
        case SEARCH:
        case TEXT:
            static_cast<RenderLineEdit *>(m_render)->setSelectionRange(start, end);
            break;
        default:
            break;
    }
}

void HTMLInputElementImpl::click(bool sendMouseEvents, bool showPressedLook)
{
    switch (inputType()) {
        case HIDDEN:
            // a no-op for this type
            break;
        case SUBMIT:
        case RESET:
        case BUTTON:
            HTMLGenericFormElementImpl::click(sendMouseEvents, showPressedLook);
            break;
        case FILE:
            if (renderer()) {
                static_cast<RenderFileButton *>(renderer())->click(sendMouseEvents);
                break;
            }
            HTMLGenericFormElementImpl::click(sendMouseEvents, showPressedLook);
            break;
        case CHECKBOX:
        case RADIO:
        case IMAGE:
        case ISINDEX:
        case PASSWORD:
        case SEARCH:
        case RANGE:
        case TEXT:
            HTMLGenericFormElementImpl::click(sendMouseEvents, showPressedLook);
            break;
    }
}

void HTMLInputElementImpl::accessKeyAction(bool sendToAnyElement)
{
    switch (inputType()) {
        case HIDDEN:
            // a no-op for this type
            break;
        case TEXT:
        case PASSWORD:
        case SEARCH:
        case ISINDEX:
            focus();
            break;
        case CHECKBOX:
        case RADIO:
        case SUBMIT:
        case RESET:
        case IMAGE:
        case BUTTON:
        case FILE:
        case RANGE:
            // focus
            focus();

            // send the mouse button events iff the
            // caller specified sendToAnyElement
            click(sendToAnyElement);
            break;
    }
}

bool HTMLInputElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr ||
        attrName == heightAttr ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr) {
        result = eUniversal;
        return false;
    } 
    
    if (attrName == alignAttr) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLInputElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == nameAttr) {
        if (m_type == RADIO && checked() && m_form) {
            // Remove the radio from its old group.
            if (m_type == RADIO && !m_name.isEmpty())
                getDocument()->removeRadioButtonGroup(m_name.impl(), m_form);
        }
        
        // Update our cached reference to the name.
        m_name = attr->value();
        
        // Add it to its new group.
        if (m_type == RADIO && checked())
            getDocument()->radioButtonChecked(this, m_form);
    } else if (attr->name() == autocompleteAttr) {
        m_autocomplete = strcasecmp( attr->value(), "off" );
    } else if (attr->name() ==  typeAttr) {
        setInputType(attr->value());
    } else if (attr->name() == valueAttr) {
        // We only need to setChanged if the form is looking at the default value right now.
        if (m_value.isNull())
            setChanged();
        m_valueMatchesRenderer = false;
    } else if (attr->name() == checkedAttr) {
        m_defaultChecked = !attr->isNull();
        if (m_useDefaultChecked) {
            setChecked(m_defaultChecked);
            m_useDefaultChecked = true;
        }
    } else if (attr->name() == maxlengthAttr) {
        m_maxLen = !attr->isNull() ? attr->value().toInt() : -1;
        setChanged();
    } else if (attr->name() == sizeAttr) {
        m_size = !attr->isNull() ? attr->value().toInt() : 20;
    } else if (attr->name() == altAttr) {
        if (m_render && m_type == IMAGE)
            static_cast<RenderImage*>(m_render)->updateAltText();
    } else if (attr->name() == srcAttr) {
        if (m_render && m_type == IMAGE) {
            if (!m_imageLoader)
                m_imageLoader = new HTMLImageLoader(this);
            m_imageLoader->updateFromElement();
        }
    } else if (attr->name() == usemapAttr ||
               attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == alignAttr) {
        addHTMLAlignment(attr);
    } else if (attr->name() == widthAttr) {
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == heightAttr) {
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else if (attr->name() == onselectAttr) {
        setHTMLEventListener(selectEvent, attr);
    } else if (attr->name() == onchangeAttr) {
        setHTMLEventListener(changeEvent, attr);
    } else if (attr->name() == oninputAttr) {
        setHTMLEventListener(inputEvent, attr);
    }
    // Search field and slider attributes all just cause updateFromElement to be called through style
    // recalcing.
    else if (attr->name() == onsearchAttr) {
        setHTMLEventListener(searchEvent, attr);
    } else if (attr->name() == resultsAttr) {
        m_maxResults = !attr->isNull() ? attr->value().toInt() : -1;
        setChanged();
    } else if (attr->name() == autosaveAttr ||
               attr->name() == incrementalAttr ||
               attr->name() == placeholderAttr ||
               attr->name() == minAttr ||
               attr->name() == maxAttr ||
               attr->name() == precisionAttr) {
        setChanged();
    } else
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

bool HTMLInputElementImpl::rendererIsNeeded(RenderStyle *style)
{
    switch(m_type)
    {
    case TEXT:
    case PASSWORD:
    case SEARCH:
    case RANGE:
    case ISINDEX:
    case CHECKBOX:
    case RADIO:
    case SUBMIT:
    case IMAGE:
    case RESET:
    case FILE:
    case BUTTON:   return HTMLGenericFormElementImpl::rendererIsNeeded(style);
    case HIDDEN:   return false;
    }
    assert(false);
    return false;
}

RenderObject *HTMLInputElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    switch(m_type)
    {
    case TEXT:
    case PASSWORD:
    case SEARCH:
    case ISINDEX:  return new (arena) RenderLineEdit(this);
    case CHECKBOX:
    case RADIO:
        return RenderObject::createObject(this, style);
    case SUBMIT:
    case RESET:
    case BUTTON:
        return new (arena) RenderButton(this);
    case IMAGE:    return new (arena) RenderImageButton(this);
    case FILE:     return new (arena) RenderFileButton(this);
    case RANGE:    return new (arena) RenderSlider(this);
    case HIDDEN:   break;
    }
    assert(false);
    return 0;
}

void HTMLInputElementImpl::attach()
{
    if (!m_inited) {
        if (!m_haveType)
            setInputType(getAttribute(typeAttr));

        // FIXME: This needs to be dynamic, doesn't it, since someone could set this
        // after attachment?
        DOMString val = getAttribute(valueAttr);
        if ((uint) m_type <= ISINDEX && !val.isEmpty()) {
            // remove newline stuff..
            QString nvalue;
            for (unsigned int i = 0; i < val.length(); ++i)
                if (val[i] >= ' ')
                    nvalue += val[i];

            if (val.length() != nvalue.length())
                setAttribute(valueAttr, nvalue);
        }

        m_inited = true;
    }

    // Disallow the width attribute on inputs other than HIDDEN and IMAGE.
    // Dumb Web sites will try to set the width as an attribute on form controls that aren't
    // images or hidden.
    if (hasMappedAttributes() && m_type != HIDDEN && m_type != IMAGE && !getAttribute(widthAttr).isEmpty()) {
        int excCode;
        removeAttribute(widthAttr, excCode);
    }

    HTMLGenericFormElementImpl::attach();

    if (m_type == IMAGE) {
        if (!m_imageLoader)
            m_imageLoader = new HTMLImageLoader(this);
        m_imageLoader->updateFromElement();
        if (renderer()) {
            RenderImage* imageObj = static_cast<RenderImage*>(renderer());
            imageObj->setImage(m_imageLoader->image());    
        }
    }

    // note we don't deal with calling passwordFieldRemoved() on detach, because the timing
    // was such that it cleared our state too early
    if (m_type == PASSWORD)
        getDocument()->passwordFieldAdded();
}

void HTMLInputElementImpl::detach()
{
    HTMLGenericFormElementImpl::detach();
    m_valueMatchesRenderer = false;
}

DOMString HTMLInputElementImpl::altText() const
{
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    // note this is intentionally different to HTMLImageElementImpl::altText()
    DOMString alt = getAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
    if (alt.isNull())
        alt = getAttribute(valueAttr);
    if (alt.isEmpty())
        alt = inputElementAltText();

    return alt;
}

bool HTMLInputElementImpl::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names
    // to be considered successful. However, other browsers
    // do not impose this constraint. Therefore, we behave
    // differently and can use different buttons than the 
    // author intended. 
    // Was: (m_type == SUBMIT && !name().isEmpty())
    return !m_disabled && (m_type == IMAGE || m_type == SUBMIT);
}

bool HTMLInputElementImpl::isActivatedSubmit() const
{
    return m_activeSubmit;
}

void HTMLInputElementImpl::setActivatedSubmit(bool flag)
{
    m_activeSubmit = flag;
}

bool HTMLInputElementImpl::appendFormData(FormDataList &encoding, bool multipart)
{
    // image generates its own names
    if (name().isEmpty() && m_type != IMAGE) return false;

    switch (m_type) {
        case HIDDEN:
        case TEXT:
        case SEARCH:
        case RANGE:
        case PASSWORD:
            // always successful
            encoding.appendData(name(), value());
            return true;

        case CHECKBOX:
        case RADIO:
            if (checked()) {
                encoding.appendData(name(), value());
                return true;
            }
            break;

        case BUTTON:
        case RESET:
            // those buttons are never successful
            return false;

        case IMAGE:
            if (m_activeSubmit)
            {
                encoding.appendData(name().isEmpty() ? QString::fromLatin1("x") : (name().qstring() + ".x"), clickX());
                encoding.appendData(name().isEmpty() ? QString::fromLatin1("y") : (name().qstring() + ".y"), clickY());
                if (!name().isEmpty() && !value().isEmpty())
                    encoding.appendData(name(), value());
                return true;
            }
            break;

        case SUBMIT:
            if (m_activeSubmit)
            {
                QString enc_str = valueWithDefault().qstring();
                if (!enc_str.isEmpty()) {
                    encoding.appendData(name(), enc_str);
                    return true;
                }
            }
            break;

        case FILE:
        {
            // can't submit file on GET
            // don't submit if display: none or display: hidden
            if(!multipart || !renderer() || renderer()->style()->visibility() != khtml::VISIBLE)
                return false;

            // if no filename at all is entered, return successful, however empty
            // null would be more logical but netscape posts an empty file. argh.
            if (value().isEmpty()) {
                encoding.appendData(name(), QString(""));
                return true;
            }

            encoding.appendFile(name(), value());
            return true;
        }
        case ISINDEX:
            encoding.appendData(name(), value());
            return true;
    }
    return false;
}

void HTMLInputElementImpl::reset()
{
    if (storesValueSeparateFromAttribute())
        setValue(DOMString());
    setChecked(m_defaultChecked);
    m_useDefaultChecked = true;
}

void HTMLInputElementImpl::setChecked(bool _checked)
{
    // WinIE does not allow unnamed radio buttons to even be checked.
    if (checked() == _checked || (m_type == RADIO && name().isEmpty()))
        return;

    if (m_type == RADIO && _checked)
        getDocument()->radioButtonChecked(this, m_form);

    m_useDefaultChecked = false;
    m_checked = _checked;
    setChanged();
    if (renderer() && renderer()->style()->hasAppearance())
        theme()->stateChanged(renderer(), CheckedState);
    onChange();
}

void HTMLInputElementImpl::setIndeterminate(bool _indeterminate)
{
    // Only checkboxes honor indeterminate.
    if (m_type != CHECKBOX || indeterminate() == _indeterminate)
        return;

    m_indeterminate = _indeterminate;

    setChanged();

    if (renderer() && renderer()->style()->hasAppearance())
        theme()->stateChanged(renderer(), CheckedState);
}

DOMString HTMLInputElementImpl::value() const
{
    DOMString value = m_value;

    // It's important *not* to fall back to the value attribute for file inputs,
    // because that would allow a malicious web page to upload files by setting the
    // value attribute in markup.
    if (value.isNull() && m_type != FILE)
        value = getAttribute(valueAttr);

    // If no attribute exists, then just use "on" or "" based off the checked() state of the control.
    if (value.isNull() && (m_type == CHECKBOX || m_type == RADIO))
        return DOMString(checked() ? "on" : "");

    return value;
}

DOMString HTMLInputElementImpl::valueWithDefault() const
{
    DOMString v = value();
    if (v.isEmpty()) {
        switch (m_type) {
            case RESET:
                v = resetButtonDefaultLabel();
                break;

            case SUBMIT:
                v = submitButtonDefaultLabel();
                break;

            case BUTTON:
            case CHECKBOX:
            case FILE:
            case HIDDEN:
            case IMAGE:
            case ISINDEX:
            case PASSWORD:
            case RADIO:
            case RANGE:
            case SEARCH:
            case TEXT:
                break;
        }
    }
    return v;
}

void HTMLInputElementImpl::setValue(const DOMString &value)
{
    if (m_type == FILE) return;

    m_valueMatchesRenderer = false;
    if (storesValueSeparateFromAttribute()) {
        m_value = value;
        if (m_render)
            m_render->updateFromElement();
        setChanged();
    } else {
        setAttribute(valueAttr, value);
    }
}

void HTMLInputElementImpl::setValueFromRenderer(const DOMString &value)
{
    m_value = value;
    m_valueMatchesRenderer = true;
    
    // Fire the "input" DOM event.
    dispatchHTMLEvent(inputEvent, true, false);
}

bool HTMLInputElementImpl::storesValueSeparateFromAttribute() const
{
    switch (m_type) {
        case BUTTON:
        case CHECKBOX:
        case FILE:
        case HIDDEN:
        case IMAGE:
        case RADIO:
        case RANGE:
        case RESET:
        case SUBMIT:
            return false;
        case ISINDEX:
        case PASSWORD:
        case SEARCH:
        case TEXT:
            return true;
    }
    return false;
}

void HTMLInputElementImpl::blur()
{
    if(getDocument()->focusNode() == this)
	getDocument()->setFocusNode(0);
}

void HTMLInputElementImpl::focus()
{
    getDocument()->setFocusNode(this);
}

void* HTMLInputElementImpl::preDispatchEventHandler(EventImpl *evt)
{
    // preventDefault or "return false" are used to reverse the automatic checking/selection we do here.
    // This result gives us enough info to perform the "undo" in postDispatch of the action we take here.
    void* result = 0; 
    if ((m_type == CHECKBOX || m_type == RADIO) && evt->isMouseEvent() && evt->type() == clickEvent && 
        static_cast<MouseEventImpl*>(evt)->button() == 0) {
        if (m_type == CHECKBOX) {
            // As a way to store the state, we return 0 if we were unchecked, 1 if we were checked, and 2 for
            // indeterminate.
            if (indeterminate()) {
                result = (void*)0x2;
                setIndeterminate(false);
            } else {
                if (checked())
                    result = (void*)0x1;
                setChecked(!checked());
            }
        } else {
            // For radio buttons, store the current selected radio object.
            if (name().isEmpty() || checked())
                return 0; // Unnamed radio buttons dont get checked. Checked buttons just stay checked.
                          // FIXME: Need to learn to work without a form.

            // We really want radio groups to end up in sane states, i.e., to have something checked.
            // Therefore if nothing is currently selected, we won't allow this action to be "undone", since
            // we want some object in the radio group to actually get selected.
            HTMLInputElementImpl* currRadio = getDocument()->checkedRadioButtonForGroup(name().impl(), m_form);
            if (currRadio) {
                // We have a radio button selected that is not us.  Cache it in our result field and ref it so
                // that it can't be destroyed.
                currRadio->ref();
                result = currRadio;
            }
            setChecked(true);
        }
    }
    return result;
}

void HTMLInputElementImpl::postDispatchEventHandler(EventImpl *evt, void* data)
{
    if ((m_type == CHECKBOX || m_type == RADIO) && evt->isMouseEvent() && evt->type() == clickEvent && 
        static_cast<MouseEventImpl*>(evt)->button() == 0) {
        if (m_type == CHECKBOX) {
            // Reverse the checking we did in preDispatch.
            if (evt->defaultPrevented() || evt->defaultHandled()) {
                if (data == (void*)0x2)
                    setIndeterminate(true);
                else
                    setChecked(data);
            }
        } else if (data) {
            HTMLInputElementImpl* input = static_cast<HTMLInputElementImpl*>(data);
            if (evt->defaultPrevented() || evt->defaultHandled()) {
                // Restore the original selected radio button if possible.
                // Make sure it is still a radio button and only do the restoration if it still
                // belongs to our group.
                if (input->form() == form() && input->inputType() == RADIO && input->name() == name()) {
                    // Ok, the old radio button is still in our form and in our group and is still a 
                    // radio button, so it's safe to restore selection to it.
                    input->setChecked(true);
                }
            }
            input->deref();
        }
    }
}

void HTMLInputElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (m_type == IMAGE && evt->isMouseEvent() && evt->type() == clickEvent && m_render) {
        // record the mouse position for when we get the DOMActivate event
        MouseEventImpl *me = static_cast<MouseEventImpl*>(evt);
        int offsetX, offsetY;
        m_render->absolutePosition(offsetX,offsetY);
        xPos = me->clientX()-offsetX;
        yPos = me->clientY()-offsetY;

	me->setDefaultHandled();
    }

    // DOMActivate events cause the input to be "activated" - in the case of image and submit inputs, this means
    // actually submitting the form. For reset inputs, the form is reset. These events are sent when the user clicks
    // on the element, or presses enter while it is the active element. Javacsript code wishing to activate the element
    // must dispatch a DOMActivate event - a click event will not do the job.
    if (evt->type() == DOMActivateEvent) {
        if (m_type == IMAGE || m_type == SUBMIT || m_type == RESET) {
            if (!m_form)
                return;
            if (m_type == RESET)
                m_form->reset();
            else {
                m_activeSubmit = true;
                if (!m_form->prepareSubmit()) {
                    xPos = 0;
                    yPos = 0;
                }
                m_activeSubmit = false;
            }
        } 
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == keypressEvent && evt->isKeyboardEvent()) {
        bool clickElement = false;
        bool clickDefaultFormButton = false;

        DOMString key = static_cast<KeyboardEventImpl *>(evt)->keyIdentifier();

        if (key == "U+000020") {
            switch (m_type) {
                case BUTTON:
                case CHECKBOX:
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                    // Simulate mouse click for spacebar for these types of elements.
                    // The AppKit already does this for some, but not all, of them.
                    clickElement = true;
                    break;
                case RADIO:
                    // If an unselected radio is tabbed into (because the entire group has nothing
                    // checked, or because of some explicit .focus() call), then allow space to check it.
                    if (!checked())
                        clickElement = true;
                    break;
                case HIDDEN:
                case ISINDEX:
                case PASSWORD:
                case RANGE:
                case SEARCH:
                case TEXT:
                    break;
            }
        }

        if (key == "Enter") {
            switch (m_type) {
                case BUTTON:
                case CHECKBOX:
                case HIDDEN:
                case ISINDEX:
                case PASSWORD:
                case RANGE:
                case SEARCH:
                case TEXT:
                    // Simulate mouse click on the default form button for enter for these types of elements.
                    clickDefaultFormButton = true;
                    break;
                case FILE:
                case IMAGE:
                case RESET:
                case SUBMIT:
                    // Simulate mouse click for enter for these types of elements.
                    clickElement = true;
                    break;
                case RADIO:
                    break; // Don't do anything for enter on a radio button.
            }
        }

        if (m_type == RADIO && (key == "Up" || key == "Down" || key == "Left" || key == "Right")) {
            // Left and up mean "previous radio button".
            // Right and down mean "next radio button".
            // Tested in WinIE, and even for RTL, left still means previous radio button (and so moves
            // to the right).  Seems strange, but we'll match it.
            bool forward = (key == "Down" || key == "Right");
            
            // We can only stay within the form's children if the form hasn't been demoted to a leaf because
            // of malformed HTML.
            NodeImpl* n = this;
            while ((n = (forward ? n->traverseNextNode() : n->traversePreviousNode()))) {
                // Once we encounter a form element, we know we're through.
                if (n->hasTagName(formTag))
                    break;
                    
                // Look for more radio buttons.
                if (n->hasTagName(inputTag)) {
                    HTMLInputElementImpl* elt = static_cast<HTMLInputElementImpl*>(n);
                    if (elt->form() != m_form)
                        break;
                    if (n->hasTagName(inputTag)) {
                        HTMLInputElementImpl* inputElt = static_cast<HTMLInputElementImpl*>(n);
                        if (inputElt->inputType() == RADIO && inputElt->name() == name() &&
                            inputElt->isFocusable()) {
                            inputElt->setChecked(true);
                            getDocument()->setFocusNode(inputElt);
                            inputElt->click(false, false);
                            evt->setDefaultHandled();
                            break;
                        }
                    }
                }
            }
        }

        if (clickElement) {
            click(false);
            evt->setDefaultHandled();
        } else if (clickDefaultFormButton && m_form) {
            m_form->submitClick();
            evt->setDefaultHandled();
        }
    }

    HTMLGenericFormElementImpl::defaultEventHandler(evt);
}

bool HTMLInputElementImpl::isEditable()
{
    return ((m_type == TEXT) || (m_type == PASSWORD) ||
            (m_type == SEARCH) || (m_type == ISINDEX) || (m_type == FILE));
}

bool HTMLInputElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return (attr->name() == srcAttr);
}

DOMString HTMLInputElementImpl::defaultValue() const
{
    return getAttribute(valueAttr);
}

void HTMLInputElementImpl::setDefaultValue(const DOMString &value)
{
    setAttribute(valueAttr, value);
}

bool HTMLInputElementImpl::defaultChecked() const
{
    return !getAttribute(checkedAttr).isNull();
}

void HTMLInputElementImpl::setDefaultChecked(bool defaultChecked)
{
    setAttribute(checkedAttr, defaultChecked ? "" : 0);
}

DOMString HTMLInputElementImpl::accept() const
{
    return getAttribute(acceptAttr);
}

void HTMLInputElementImpl::setAccept(const DOMString &value)
{
    setAttribute(acceptAttr, value);
}

DOMString HTMLInputElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLInputElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

DOMString HTMLInputElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLInputElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLInputElementImpl::alt() const
{
    return getAttribute(altAttr);
}

void HTMLInputElementImpl::setAlt(const DOMString &value)
{
    setAttribute(altAttr, value);
}

void HTMLInputElementImpl::setMaxLength(int _maxLength)
{
    setAttribute(maxlengthAttr, QString::number(_maxLength));
}

void HTMLInputElementImpl::setSize(unsigned _size)
{
    setAttribute(sizeAttr, QString::number(_size));
}

DOMString HTMLInputElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(srcAttr));
}

void HTMLInputElementImpl::setSrc(const DOMString &value)
{
    setAttribute(srcAttr, value);
}

DOMString HTMLInputElementImpl::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLInputElementImpl::setUseMap(const DOMString &value)
{
    setAttribute(usemapAttr, value);
}

// -------------------------------------------------------------------------

HTMLLabelElementImpl::HTMLLabelElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(labelTag, doc)
{
}

HTMLLabelElementImpl::~HTMLLabelElementImpl()
{
}

bool HTMLLabelElementImpl::isFocusable() const
{
    return false;
}

void HTMLLabelElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

ElementImpl *HTMLLabelElementImpl::formElement()
{
    DOMString formElementId = getAttribute(forAttr);
    if (formElementId.isNull()) {
        // Search children of the label element for a form element.
        NodeImpl *node = this;
        while ((node = node->traverseNextNode(this))) {
            if (node->isHTMLElement()) {
                HTMLElementImpl *element = static_cast<HTMLElementImpl *>(node);
                if (element->isGenericFormElement())
                    return element;
            }
        }
        return 0;
    }
    if (formElementId.isEmpty())
        return 0;
    return getDocument()->getElementById(formElementId);
}

void HTMLLabelElementImpl::focus()
{
    if (ElementImpl *element = formElement())
        getDocument()->setFocusNode(element);
}

void HTMLLabelElementImpl::accessKeyAction(bool sendToAnyElement)
{
    ElementImpl *element = formElement();
    if (element)
        element->accessKeyAction(sendToAnyElement);
}

HTMLFormElementImpl *HTMLLabelElementImpl::form()
{
    for (NodeImpl *p = parentNode(); p != 0; p = p->parentNode()) {
        if (p->hasTagName(formTag))
            return static_cast<HTMLFormElementImpl *>(p);
    }
    
    return 0;
}

DOMString HTMLLabelElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLLabelElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

DOMString HTMLLabelElementImpl::htmlFor() const
{
    return getAttribute(forAttr);
}

void HTMLLabelElementImpl::setHtmlFor(const DOMString &value)
{
    setAttribute(forAttr, value);
}

// -------------------------------------------------------------------------

HTMLLegendElementImpl::HTMLLegendElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
: HTMLGenericFormElementImpl(legendTag, doc, f)
{
}

HTMLLegendElementImpl::~HTMLLegendElementImpl()
{
}

bool HTMLLegendElementImpl::isFocusable() const
{
    return false;
}

RenderObject* HTMLLegendElementImpl::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderLegend(this);
}

DOMString HTMLLegendElementImpl::type() const
{
    return "legend";
}

DOMString HTMLLegendElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLLegendElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

DOMString HTMLLegendElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLLegendElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

ElementImpl *HTMLLegendElementImpl::formElement()
{
    // Check if there's a fieldset belonging to this legend.
    NodeImpl *fieldset = parentNode();
    while (fieldset && !fieldset->hasTagName(fieldsetTag))
        fieldset = fieldset->parentNode();
    if (!fieldset)
        return 0;

    // Find first form element inside the fieldset.
    // FIXME: Should we care about tabindex?
    NodeImpl *node = fieldset;
    while ((node = node->traverseNextNode(fieldset))) {
        if (node->isHTMLElement()) {
            HTMLElementImpl *element = static_cast<HTMLElementImpl *>(node);
            if (!element->hasLocalName(legendTag) && element->isGenericFormElement())
                return element;
        }
    }

    return 0;
}

void HTMLLegendElementImpl::focus()
{
    if (ElementImpl *element = formElement())
        getDocument()->setFocusNode(element);
}

void HTMLLegendElementImpl::accessKeyAction(bool sendToAnyElement)
{
    if (ElementImpl *element = formElement())
        element->accessKeyAction(sendToAnyElement);
}

// -------------------------------------------------------------------------

HTMLSelectElementImpl::HTMLSelectElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(selectTag, doc, f), m_options(0)
{
    init();
}

HTMLSelectElementImpl::HTMLSelectElementImpl(const QualifiedName& tagName, DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(tagName, doc, f), m_options(0)
{
    init();
}

void HTMLSelectElementImpl::init()
{
    m_options = 0;
    m_multiple = false;
    m_recalcListItems = false;
    // 0 means invalid (i.e. not set)
    m_size = 0;
    m_minwidth = 0;
}

HTMLSelectElementImpl::~HTMLSelectElementImpl()
{
    if (getDocument()) getDocument()->deregisterMaintainsState(this);
    if (m_options) {
        m_options->detach();
        m_options->deref();
    }
}

bool HTMLSelectElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->isTextNode() || newChild->hasTagName(optionTag) || newChild->hasTagName(optgroupTag) || newChild->hasTagName(hrTag) ||
           newChild->hasTagName(scriptTag);
}

void HTMLSelectElementImpl::recalcStyle( StyleChange ch )
{
    if (hasChangedChild() && m_render) {
        static_cast<khtml::RenderSelect*>(m_render)->setOptionsChanged(true);
    }

    HTMLGenericFormElementImpl::recalcStyle( ch );
}


DOMString HTMLSelectElementImpl::type() const
{
    return (m_multiple ? "select-multiple" : "select-one");
}

int HTMLSelectElementImpl::selectedIndex() const
{
    // return the number of the first option selected
    uint o = 0;
    QMemArray<HTMLElementImpl*> items = listItems();
    for (unsigned int i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            if (static_cast<HTMLOptionElementImpl*>(items[i])->selected())
                return o;
            o++;
        }
    }
    Q_ASSERT(m_multiple);
    return -1;
}

void HTMLSelectElementImpl::setSelectedIndex( int  index )
{
    // deselect all other options and select only the new one
    QMemArray<HTMLElementImpl*> items = listItems();
    int listIndex;
    for (listIndex = 0; listIndex < int(items.size()); listIndex++) {
        if (items[listIndex]->hasLocalName(optionTag))
            static_cast<HTMLOptionElementImpl*>(items[listIndex])->setSelected(false);
    }
    listIndex = optionToListIndex(index);
    if (listIndex >= 0)
        static_cast<HTMLOptionElementImpl*>(items[listIndex])->setSelected(true);

    setChanged(true);
}

int HTMLSelectElementImpl::length() const
{
    int len = 0;
    uint i;
    QMemArray<HTMLElementImpl*> items = listItems();
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag))
            len++;
    }
    return len;
}

void HTMLSelectElementImpl::add( HTMLElementImpl *element, HTMLElementImpl *before, int &exceptioncode )
{
    RefPtr<HTMLElementImpl> protectNewChild(element); // make sure the element is ref'd and deref'd so we don't leak it

    if (!element || !(element->hasLocalName(optionTag) || element->hasLocalName(hrTag)))
        return;

    insertBefore(element, before, exceptioncode);
    if (!exceptioncode)
        setRecalcListItems();
}

void HTMLSelectElementImpl::remove( int index )
{
    int exceptioncode = 0;
    int listIndex = optionToListIndex(index);

    QMemArray<HTMLElementImpl*> items = listItems();
    if(listIndex < 0 || index >= int(items.size()))
        return; // ### what should we do ? remove the last item?

    NodeImpl *item = items[listIndex];
    item->ref();
    removeChild(item, exceptioncode);
    item->deref();
    if( !exceptioncode )
        setRecalcListItems();
}

void HTMLSelectElementImpl::blur()
{
    if(getDocument()->focusNode() == this)
	getDocument()->setFocusNode(0);
}

void HTMLSelectElementImpl::focus()
{
    getDocument()->setFocusNode(this);
}

DOMString HTMLSelectElementImpl::value()
{
    uint i;
    QMemArray<HTMLElementImpl*> items = listItems();
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag) && static_cast<HTMLOptionElementImpl*>(items[i])->selected())
            return static_cast<HTMLOptionElementImpl*>(items[i])->value();
    }
    return DOMString("");
}

void HTMLSelectElementImpl::setValue(const DOMString &value)
{
    if (value.isNull())
        return;
    // find the option with value() matching the given parameter
    // and make it the current selection.
    QMemArray<HTMLElementImpl*> items = listItems();
    for (unsigned i = 0; i < items.size(); i++)
        if (items[i]->hasLocalName(optionTag) && static_cast<HTMLOptionElementImpl*>(items[i])->value() == value) {
            static_cast<HTMLOptionElementImpl*>(items[i])->setSelected(true);
            return;
        }
}

QString HTMLSelectElementImpl::state()
{
    QMemArray<HTMLElementImpl*> items = listItems();

    int l = items.count();

    QChar stateChars[l];
    
    for(int i = 0; i < l; i++)
        if(items[i]->hasLocalName(optionTag) && static_cast<HTMLOptionElementImpl*>(items[i])->selected())
            stateChars[i] = 'X';
        else
            stateChars[i] = '.';
    QString state(stateChars, l);

    return HTMLGenericFormElementImpl::state() + state;
}

void HTMLSelectElementImpl::restoreState(QStringList &_states)
{
    QString _state = HTMLGenericFormElementImpl::findMatchingState(_states);
    if (_state.isNull()) return;

    recalcListItems();

    QString state = _state;
    if(!state.isEmpty() && !state.contains('X') && !m_multiple) {
        qWarning("should not happen in restoreState!");
        // KWQString doesn't support this operation. Should never get here anyway.
        //state[0] = 'X';
    }

    QMemArray<HTMLElementImpl*> items = listItems();

    int l = items.count();
    for(int i = 0; i < l; i++) {
        if(items[i]->hasLocalName(optionTag)) {
            HTMLOptionElementImpl* oe = static_cast<HTMLOptionElementImpl*>(items[i]);
            oe->setSelected(state[i] == 'X');
        }
    }
    setChanged(true);
}

NodeImpl *HTMLSelectElementImpl::insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::insertBefore(newChild,refChild, exceptioncode );
    if (!exceptioncode)
        setRecalcListItems();
    return result;
}

NodeImpl *HTMLSelectElementImpl::replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::replaceChild(newChild,oldChild, exceptioncode);
    if( !exceptioncode )
        setRecalcListItems();
    return result;
}

NodeImpl *HTMLSelectElementImpl::removeChild ( NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::removeChild(oldChild, exceptioncode);
    if( !exceptioncode )
        setRecalcListItems();
    return result;
}

NodeImpl *HTMLSelectElementImpl::appendChild ( NodeImpl *newChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::appendChild(newChild, exceptioncode);
    if( !exceptioncode )
        setRecalcListItems();
    setChanged(true);
    return result;
}

NodeImpl* HTMLSelectElementImpl::addChild(NodeImpl* newChild)
{
    setRecalcListItems();
    return HTMLGenericFormElementImpl::addChild(newChild);
}

void HTMLSelectElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == sizeAttr) {
        m_size = kMax(attr->value().toInt(), 1);
    } else if (attr->name() == widthAttr) {
        m_minwidth = kMax( attr->value().toInt(), 0 );
    } else if (attr->name() == multipleAttr) {
        m_multiple = (!attr->isNull());
    } else if (attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else if (attr->name() == onchangeAttr) {
        setHTMLEventListener(changeEvent, attr);
    } else
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

RenderObject *HTMLSelectElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderSelect(this);
}

bool HTMLSelectElementImpl::appendFormData(FormDataList& encoded_values, bool)
{
    bool successful = false;
    QMemArray<HTMLElementImpl*> items = listItems();

    uint i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[i]);
            if (option->selected()) {
                encoded_values.appendData(name(), option->value());
                successful = true;
            }
        }
    }

    // ### this case should not happen. make sure that we select the first option
    // in any case. otherwise we have no consistency with the DOM interface. FIXME!
    // we return the first one if it was a combobox select
    if (!successful && !m_multiple && m_size <= 1 && items.size() &&
        (items[0]->hasLocalName(optionTag))) {
        HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[0]);
        if (option->value().isNull())
            encoded_values.appendData(name(), option->text().qstring().stripWhiteSpace());
        else
            encoded_values.appendData(name(), option->value());
        successful = true;
    }

    return successful;
}

int HTMLSelectElementImpl::optionToListIndex(int optionIndex) const
{
    QMemArray<HTMLElementImpl*> items = listItems();
    if (optionIndex < 0 || optionIndex >= int(items.size()))
        return -1;

    int listIndex = 0;
    int optionIndex2 = 0;
    for (;
         optionIndex2 < int(items.size()) && optionIndex2 <= optionIndex;
         listIndex++) { // not a typo!
        if (items[listIndex]->hasLocalName(optionTag))
            optionIndex2++;
    }
    listIndex--;
    return listIndex;
}

int HTMLSelectElementImpl::listToOptionIndex(int listIndex) const
{
    QMemArray<HTMLElementImpl*> items = listItems();
    if (listIndex < 0 || listIndex >= int(items.size()) ||
        !items[listIndex]->hasLocalName(optionTag))
        return -1;

    int optionIndex = 0; // actual index of option not counting OPTGROUP entries that may be in list
    int i;
    for (i = 0; i < listIndex; i++)
        if (items[i]->hasLocalName(optionTag))
            optionIndex++;
    return optionIndex;
}

// FIXME 4197997: this method is used by the public API -[DOMHTMLSelectElement options], but always returns
// an empty list.
HTMLOptionsCollectionImpl *HTMLSelectElementImpl::options()
{
    if (!m_options) {
        m_options = new HTMLOptionsCollectionImpl(this);
        m_options->ref();
    }
    return m_options;
}

// FIXME: Delete once the above function is working well enough to use for real.
RefPtr<HTMLCollectionImpl> HTMLSelectElementImpl::optionsHTMLCollection()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::SELECT_OPTIONS));
}

void HTMLSelectElementImpl::recalcListItems()
{
    NodeImpl* current = firstChild();
    m_listItems.resize(0);
    HTMLOptionElementImpl* foundSelected = 0;
    while(current) {
        if (current->hasTagName(optgroupTag) && current->firstChild()) {
            // ### what if optgroup contains just comments? don't want one of no options in it...
            m_listItems.resize(m_listItems.size()+1);
            m_listItems[m_listItems.size()-1] = static_cast<HTMLElementImpl*>(current);
            current = current->firstChild();
        }
        if (current->hasTagName(optionTag)) {
            m_listItems.resize(m_listItems.size()+1);
            m_listItems[m_listItems.size()-1] = static_cast<HTMLElementImpl*>(current);
            if (!foundSelected && !m_multiple && m_size <= 1) {
                foundSelected = static_cast<HTMLOptionElementImpl*>(current);
                foundSelected->m_selected = true;
            }
            else if (foundSelected && !m_multiple && static_cast<HTMLOptionElementImpl*>(current)->selected()) {
                foundSelected->m_selected = false;
                foundSelected = static_cast<HTMLOptionElementImpl*>(current);
            }
        }
        if (current->hasTagName(hrTag)) {
            m_listItems.resize(m_listItems.size()+1);
            m_listItems[m_listItems.size()-1] = static_cast<HTMLElementImpl*>(current);
        }
        NodeImpl *parent = current->parentNode();
        current = current->nextSibling();
        if (!current) {
            if (parent != this)
                current = parent->nextSibling();
        }
    }
    m_recalcListItems = false;
}

void HTMLSelectElementImpl::childrenChanged()
{
    setRecalcListItems();

    HTMLGenericFormElementImpl::childrenChanged();
}

void HTMLSelectElementImpl::setRecalcListItems()
{
    m_recalcListItems = true;
    if (m_render)
        static_cast<khtml::RenderSelect*>(m_render)->setOptionsChanged(true);
    setChanged();
}

void HTMLSelectElementImpl::reset()
{
    QMemArray<HTMLElementImpl*> items = listItems();
    uint i;
    for (i = 0; i < items.size(); i++) {
        if (items[i]->hasLocalName(optionTag)) {
            HTMLOptionElementImpl *option = static_cast<HTMLOptionElementImpl*>(items[i]);
            bool selected = (!option->getAttribute(selectedAttr).isNull());
            option->setSelected(selected);
        }
    }
    if ( m_render )
        static_cast<RenderSelect*>(m_render)->setSelectionChanged(true);
    setChanged( true );
}

void HTMLSelectElementImpl::notifyOptionSelected(HTMLOptionElementImpl *selectedOption, bool selected)
{
    if (selected && !m_multiple) {
        // deselect all other options
        QMemArray<HTMLElementImpl*> items = listItems();
        uint i;
        for (i = 0; i < items.size(); i++) {
            if (items[i]->hasLocalName(optionTag))
                static_cast<HTMLOptionElementImpl*>(items[i])->m_selected = (items[i] == selectedOption);
        }
    }
    if (m_render)
        static_cast<RenderSelect*>(m_render)->setSelectionChanged(true);

    setChanged(true);
}

void HTMLSelectElementImpl::defaultEventHandler(EventImpl *evt)
{
    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == keypressEvent) {
    
        if (!m_form || !m_render || !evt->isKeyboardEvent())
            return;
        
        DOMString key = static_cast<KeyboardEventImpl *>(evt)->keyIdentifier();
        
        if (key == "Enter") {
            m_form->submitClick();
            evt->setDefaultHandled();
        }
    }
    HTMLGenericFormElementImpl::defaultEventHandler(evt);
}

void HTMLSelectElementImpl::accessKeyAction(bool sendToAnyElement)
{
    focus();
}

void HTMLSelectElementImpl::setMultiple(bool multiple)
{
    setAttribute(multipleAttr, multiple ? "" : 0);
}

void HTMLSelectElementImpl::setSize(int size)
{
    setAttribute(sizeAttr, QString::number(size));
}

// -------------------------------------------------------------------------

HTMLKeygenElementImpl::HTMLKeygenElementImpl(DocumentImpl* doc, HTMLFormElementImpl* f)
    : HTMLSelectElementImpl(keygenTag, doc, f)
{
    QStringList keys = KSSLKeyGen::supportedKeySizes();
    for (QStringList::Iterator i = keys.begin(); i != keys.end(); ++i) {
        HTMLOptionElementImpl* o = new HTMLOptionElementImpl(doc, form());
        addChild(o);
        o->addChild(new TextImpl(doc, DOMString(*i)));
    }
}

DOMString HTMLKeygenElementImpl::type() const
{
    return "keygen";
}

void HTMLKeygenElementImpl::parseMappedAttribute(MappedAttributeImpl* attr)
{
    if (attr->name() == challengeAttr)
        m_challenge = attr->value();
    else if (attr->name() == keytypeAttr)
        m_keyType = attr->value();
    else
        // skip HTMLSelectElementImpl parsing!
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

bool HTMLKeygenElementImpl::appendFormData(FormDataList& encoded_values, bool)
{
    // Only RSA is supported at this time.
    if (!m_keyType.isNull() && strcasecmp(m_keyType, "rsa")) {
        return false;
    }
    QString value = KSSLKeyGen::signedPublicKeyAndChallengeString(selectedIndex(), m_challenge.qstring(), getDocument()->part()->baseURL());
    if (value.isNull()) {
        return false;
    }
    encoded_values.appendData(name(), value.utf8());
    return true;
}

// -------------------------------------------------------------------------

HTMLOptGroupElementImpl::HTMLOptGroupElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(optgroupTag, doc, f)
{
}

HTMLOptGroupElementImpl::~HTMLOptGroupElementImpl()
{
}

bool HTMLOptGroupElementImpl::isFocusable() const
{
    return false;
}

DOMString HTMLOptGroupElementImpl::type() const
{
    return "optgroup";
}

NodeImpl *HTMLOptGroupElementImpl::insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::insertBefore(newChild,refChild, exceptioncode);
    if ( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::replaceChild(newChild,oldChild, exceptioncode);
    if(!exceptioncode)
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::removeChild ( NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::removeChild(oldChild, exceptioncode);
    if( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::appendChild ( NodeImpl *newChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::appendChild(newChild, exceptioncode);
    if( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl* HTMLOptGroupElementImpl::addChild(NodeImpl* newChild)
{
    recalcSelectOptions();

    return HTMLGenericFormElementImpl::addChild(newChild);
}

void HTMLOptGroupElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    HTMLGenericFormElementImpl::parseMappedAttribute(attr);
    recalcSelectOptions();
}

void HTMLOptGroupElementImpl::recalcSelectOptions()
{
    NodeImpl *select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();
    if (select)
        static_cast<HTMLSelectElementImpl*>(select)->setRecalcListItems();
}

DOMString HTMLOptGroupElementImpl::label() const
{
    return getAttribute(labelAttr);
}

void HTMLOptGroupElementImpl::setLabel(const DOMString &value)
{
    setAttribute(labelAttr, value);
}

// -------------------------------------------------------------------------

HTMLOptionElementImpl::HTMLOptionElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(optionTag, doc, f)
{
    m_selected = false;
}

bool HTMLOptionElementImpl::isFocusable() const
{
    return false;
}

DOMString HTMLOptionElementImpl::type() const
{
    return "option";
}

DOMString HTMLOptionElementImpl::text() const
{
    DOMString text;

    // WinIE does not use the label attribute, so as a quirk, we ignore it.
    if (getDocument() && !getDocument()->inCompatMode()) {
        DOMString text = getAttribute(labelAttr);
        if (!text.isEmpty())
            return text;
    }

    const NodeImpl *n = this->firstChild();
    while (n) {
        if (n->nodeType() == Node::TEXT_NODE || n->nodeType() == Node::CDATA_SECTION_NODE)
            text += n->nodeValue();
        // skip script content
        if (n->isElementNode() && n->hasTagName(HTMLNames::scriptTag))
            n = n->traverseNextSibling(this);
        else
            n = n->traverseNextNode(this);
    }

    return text;
}

void HTMLOptionElementImpl::setText(const DOMString &text, int &exception)
{
    // Handle the common special case where there's exactly 1 child node, and it's a text node.
    NodeImpl *child = firstChild();
    if (child && child->isTextNode() && !child->nextSibling()) {
        static_cast<TextImpl *>(child)->setData(text, exception);
        return;
    }

    removeChildren();
    appendChild(new TextImpl(getDocument(), text), exception);
}

int HTMLOptionElementImpl::index() const
{
    // Let's do this dynamically. Might be a bit slow, but we're sure
    // we won't forget to update a member variable in some cases...
    HTMLSelectElementImpl *select = getSelect();
    if (select) {
        QMemArray<HTMLElementImpl*> items = select->listItems();
        int l = items.count();
        int optionIndex = 0;
        for(int i = 0; i < l; i++) {
            if (items[i]->hasLocalName(optionTag)) {
                if (static_cast<HTMLOptionElementImpl*>(items[i]) == this)
                    return optionIndex;
                optionIndex++;
            }
        }
    }
    kdWarning() << "HTMLOptionElementImpl::index(): option not found!" << endl;
    return 0;
}

void HTMLOptionElementImpl::setIndex(int, int &exception)
{
    exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    kdWarning() << "Unimplemented HTMLOptionElementImpl::setIndex(int) called" << endl;
    // ###
}

void HTMLOptionElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == selectedAttr)
        m_selected = (!attr->isNull());
    else if (attr->name() == valueAttr)
        m_value = attr->value();
    else
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLOptionElementImpl::value() const
{
    if ( !m_value.isNull() )
        return m_value;
    // Use the text if the value wasn't set.
    return text().qstring().stripWhiteSpace();
}

void HTMLOptionElementImpl::setValue(const DOMString &value)
{
    setAttribute(valueAttr, value);
}

void HTMLOptionElementImpl::setSelected(bool _selected)
{
    if(m_selected == _selected)
        return;
    m_selected = _selected;
    HTMLSelectElementImpl *select = getSelect();
    if (select)
        select->notifyOptionSelected(this,_selected);
}

void HTMLOptionElementImpl::childrenChanged()
{
   HTMLSelectElementImpl *select = getSelect();
   if (select)
       select->childrenChanged();
}

HTMLSelectElementImpl *HTMLOptionElementImpl::getSelect() const
{
    NodeImpl *select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();
    return static_cast<HTMLSelectElementImpl*>(select);
}

bool HTMLOptionElementImpl::defaultSelected() const
{
    return !getAttribute(selectedAttr).isNull();
}

void HTMLOptionElementImpl::setDefaultSelected(bool b)
{
    setAttribute(selectedAttr, b ? "" : 0);
}

DOMString HTMLOptionElementImpl::label() const
{
    return getAttribute(labelAttr);
}

void HTMLOptionElementImpl::setLabel(const DOMString &value)
{
    setAttribute(labelAttr, value);
}

// -------------------------------------------------------------------------

HTMLTextAreaElementImpl::HTMLTextAreaElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(textareaTag, doc, f), m_valueIsValid(false), m_valueMatchesRenderer(false)
{
    // DTD requires rows & cols be specified, but we will provide reasonable defaults
    m_rows = 2;
    m_cols = 20;
    m_wrap = ta_Virtual;
}

HTMLTextAreaElementImpl::~HTMLTextAreaElementImpl()
{
    if (getDocument()) getDocument()->deregisterMaintainsState(this);
}

DOMString HTMLTextAreaElementImpl::type() const
{
    return "textarea";
}

QString HTMLTextAreaElementImpl::state( )
{
    // Make sure the string is not empty!
    return HTMLGenericFormElementImpl::state() + value().qstring()+'.';
}

void HTMLTextAreaElementImpl::restoreState(QStringList &states)
{
    QString state = HTMLGenericFormElementImpl::findMatchingState(states);
    if (state.isNull()) return;
    setDefaultValue(state.left(state.length()-1));
    // the close() in the rendertree will take care of transferring defaultvalue to 'value'
}

int HTMLTextAreaElementImpl::selectionStart()
{
    if (m_render)
        return static_cast<RenderTextArea *>(m_render)->selectionStart();
    return 0;
}

int HTMLTextAreaElementImpl::selectionEnd()
{
    if (m_render)
        return static_cast<RenderTextArea *>(m_render)->selectionEnd();
    return 0;
}

void HTMLTextAreaElementImpl::setSelectionStart(int start)
{
    if (m_render)
        static_cast<RenderTextArea *>(m_render)->setSelectionStart(start);
}

void HTMLTextAreaElementImpl::setSelectionEnd(int end)
{
    if (m_render)
        static_cast<RenderTextArea *>(m_render)->setSelectionEnd(end);
}

void HTMLTextAreaElementImpl::select(  )
{
    if (m_render)
        static_cast<RenderTextArea*>(m_render)->select();
}

void HTMLTextAreaElementImpl::setSelectionRange(int start, int end)
{
    if (m_render)
        static_cast<RenderTextArea *>(m_render)->setSelectionRange(start, end);
}

void HTMLTextAreaElementImpl::childrenChanged()
{
    setValue(defaultValue());
}
    
void HTMLTextAreaElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == rowsAttr) {
        m_rows = !attr->isNull() ? attr->value().toInt() : 3;
        if (renderer())
            renderer()->setNeedsLayoutAndMinMaxRecalc();
    } else if (attr->name() == colsAttr) {
        m_cols = !attr->isNull() ? attr->value().toInt() : 60;
        if (renderer())
            renderer()->setNeedsLayoutAndMinMaxRecalc();
    } else if (attr->name() == wrapAttr) {
        // virtual / physical is Netscape extension of HTML 3.0, now deprecated
        // soft/ hard / off is recommendation for HTML 4 extension by IE and NS 4
        if ( strcasecmp( attr->value(), "virtual" ) == 0  || strcasecmp( attr->value(), "soft") == 0)
            m_wrap = ta_Virtual;
        else if ( strcasecmp ( attr->value(), "physical" ) == 0 || strcasecmp( attr->value(), "hard") == 0)
            m_wrap = ta_Physical;
        else if(strcasecmp( attr->value(), "on" ) == 0)
            m_wrap = ta_Physical;
        else if(strcasecmp( attr->value(), "off") == 0)
            m_wrap = ta_NoWrap;
        if (renderer())
            renderer()->setNeedsLayoutAndMinMaxRecalc();
    } else if (attr->name() == accesskeyAttr) {
        // ignore for the moment
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else if (attr->name() == onselectAttr) {
        setHTMLEventListener(selectEvent, attr);
    } else if (attr->name() == onchangeAttr) {
        setHTMLEventListener(changeEvent, attr);
    } else
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

RenderObject *HTMLTextAreaElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderTextArea(this);
}

bool HTMLTextAreaElementImpl::appendFormData(FormDataList& encoding, bool)
{
    if (name().isEmpty()) return false;
    encoding.appendData(name(), value());
    return true;
}

void HTMLTextAreaElementImpl::reset()
{
    setValue(defaultValue());
}

void HTMLTextAreaElementImpl::updateValue()
{
    if ( !m_valueIsValid ) {
        if ( m_render ) {
            m_value = static_cast<RenderTextArea*>( m_render )->text();
            m_valueMatchesRenderer = true;
        } else {
            m_value = defaultValue().qstring();
            m_valueMatchesRenderer = false;
        }
        m_valueIsValid = true;
    }
}

DOMString HTMLTextAreaElementImpl::value()
{
    updateValue();
    return m_value.isNull() ? DOMString("") : m_value;
}

void HTMLTextAreaElementImpl::setValue(const DOMString &value)
{
    m_value = value.qstring();
    m_valueMatchesRenderer = false;
    m_valueIsValid = true;
    if (m_render)
        m_render->updateFromElement();
    // FIXME: Force reload from renderer, as renderer may have normalized line endings.
    m_valueIsValid = false;
    setChanged(true);
}


DOMString HTMLTextAreaElementImpl::defaultValue()
{
    DOMString val = "";
    // there may be comments - just grab the text nodes
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            val += static_cast<TextImpl*>(n)->data();
    
    if (val[0] == '\r' && val[1] == '\n') {
        val = val.copy();
        val.remove(0,2);
    } else if (val[0] == '\r' || val[0] == '\n') {
        val = val.copy();
        val.remove(0,1);
    }

    return val;
}

void HTMLTextAreaElementImpl::setDefaultValue(const DOMString &defaultValue)
{
    // there may be comments - remove all the text nodes and replace them with one
    QPtrList<NodeImpl> toRemove;
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            toRemove.append(n);
    QPtrListIterator<NodeImpl> it(toRemove);
    int exceptioncode = 0;
    for (; it.current(); ++it) {
        NodeImpl *n = it.current();
        n->ref();
        removeChild(n, exceptioncode);
        n->deref();
    }
    insertBefore(getDocument()->createTextNode(defaultValue),firstChild(), exceptioncode);
    setValue(defaultValue);
}

void HTMLTextAreaElementImpl::blur()
{
    if(getDocument()->focusNode() == this)
	getDocument()->setFocusNode(0);
}

void HTMLTextAreaElementImpl::focus()
{
    getDocument()->setFocusNode(this);
}

bool HTMLTextAreaElementImpl::isEditable()
{
    return true;
}

void HTMLTextAreaElementImpl::accessKeyAction(bool sendToAnyElement)
{
    focus();
}

void HTMLTextAreaElementImpl::attach()
{
    m_valueIsValid = true;
    HTMLGenericFormElementImpl::attach();
    updateValue();
}

void HTMLTextAreaElementImpl::detach()
{
    HTMLGenericFormElementImpl::detach();
    m_valueMatchesRenderer = false;
}

DOMString HTMLTextAreaElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLTextAreaElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

void HTMLTextAreaElementImpl::setCols(int cols)
{
    setAttribute(colsAttr, QString::number(cols));
}

void HTMLTextAreaElementImpl::setRows(int rows)
{
    setAttribute(rowsAttr, QString::number(rows));
}

// -------------------------------------------------------------------------

HTMLIsIndexElementImpl::HTMLIsIndexElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLInputElementImpl(isindexTag, doc, f)
{
    m_type = TEXT;
    m_name = "isindex";
}

void HTMLIsIndexElementImpl::parseMappedAttribute(MappedAttributeImpl* attr)
{
    if (attr->name() == promptAttr)
	setValue(attr->value());
    else
        // don't call HTMLInputElement::parseMappedAttribute here, as it would
        // accept attributes this element does not support
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLIsIndexElementImpl::prompt() const
{
    return getAttribute(promptAttr);
}

void HTMLIsIndexElementImpl::setPrompt(const DOMString &value)
{
    setAttribute(promptAttr, value);
}

// -------------------------------------------------------------------------

unsigned HTMLOptionsCollectionImpl::length() const
{
    // Not yet implemented.
    return 0;
}

void HTMLOptionsCollectionImpl::setLength(unsigned length)
{
    // Not yet implemented.
}

NodeImpl *HTMLOptionsCollectionImpl::item(unsigned index) const
{
    // Not yet implemented.
    return 0;
}

NodeImpl *HTMLOptionsCollectionImpl::namedItem(const DOMString &name) const
{
    // Not yet implemented.
    return 0;
}

// -------------------------------------------------------------------------

FormDataList::FormDataList(QTextCodec *c)
    : m_codec(c)
{
}

void FormDataList::appendString(const QCString &s)
{
    m_list.append(s);
}

void FormDataList::appendString(const QString &s)
{
    QCString cstr = fixLineBreaks(m_codec->fromUnicode(s, true));
    cstr.truncate(cstr.length());
    m_list.append(cstr);
}

void FormDataList::appendFile(const DOMString &key, const DOMString &filename)
{
    appendString(key.qstring());
    m_list.append(filename.qstring());
}

} // namespace
