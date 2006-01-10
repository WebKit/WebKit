/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "config.h"
#include "HTMLFormElementImpl.h"
#include "html_imageimpl.h"
#include "html_documentimpl.h"
#include "csshelper.h" // For kthml::parseURL
#include "FormDataList.h"

#include "rendering/render_form.h"
#include "EventNames.h"

#include "MacFrame.h"

#include <qtextcodec.h>

using namespace khtml;

namespace DOM {

using namespace EventNames;
using namespace HTMLNames;

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

bool HTMLFormElementImpl::formData(FormData &form_data) const
{
    QCString enc_string = ""; // used for non-multipart data

    // find out the QTextcodec to use
    QString str = m_acceptcharset.qstring();
    str.replace(',', ' ');
    QStringList charsets = QStringList::split(' ', str);
    QTextCodec* codec = 0;
    Frame *frame = getDocument()->frame();
    for (QStringList::Iterator it = charsets.begin(); it != charsets.end(); ++it) {
        QString enc = (*it);
        if (enc.contains("UNKNOWN")) {
            // use standard document encoding
            enc = "ISO-8859-1";
            if (frame)
                enc = frame->encoding();
        }
        if ((codec = QTextCodec::codecForName(enc.latin1())))
            break;
    }

    if (!codec)
        codec = QTextCodec::codecForLocale();


    for (unsigned i = 0; i < formElements.count(); ++i) {
        HTMLGenericFormElementImpl* current = formElements[i];
        FormDataList lst(codec);

        if (!current->disabled() && current->appendFormData(lst, m_multipart)) {
            for (QValueListConstIterator<FormDataListItem> it = lst.begin(); it != lst.end(); ++it) {
                if (!m_multipart) {
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
                            QString mimeType = frame ? Mac(frame)->mimeTypeForFileName(path) : QString();
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

void HTMLFormElementImpl::parseEnctype(const DOMString& type)
{
    if(type.contains("multipart", false) || type.contains("form-data", false)) {
        m_enctype = "multipart/form-data";
        m_multipart = true;
    } else if (type.contains("text", false) || type.contains("plain", false)) {
        m_enctype = "text/plain";
        m_multipart = false;
    } else {
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
    Frame *frame = getDocument()->frame();
    if(m_insubmit || !frame || frame->onlyLocalReferences())
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
    Frame *frame = getDocument()->frame();
    if (!view || !frame) {
        return;
    }

    if ( m_insubmit ) {
        m_doingsubmit = true;
        return;
    }

    m_insubmit = true;

    HTMLGenericFormElementImpl* firstSuccessfulSubmitButton = 0;
    bool needButtonActivation = activateSubmitButton;	// do we need to activate a submit button?
    
    Mac(frame)->clearRecordedFormValues();
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
                Mac(frame)->recordFormValue(input->name().qstring(), input->value().qstring(), this);
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

    if (!m_post)
        m_multipart = false;
    
    FormData form_data;
    if (formData(form_data)) {
        if(m_post) {
            frame->submitForm( "post", m_url.qstring(), form_data,
                                      m_target.qstring(),
                                      enctype().qstring(),
                                      boundary().qstring() );
        }
        else {
            frame->submitForm( "get", m_url.qstring(), form_data,
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
    Frame *frame = getDocument()->frame();
    if(m_inreset || !frame) return;

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
        if (equalIgnoringCase(attr->value(), "post"))
            m_post = true;
        else if (equalIgnoringCase(attr->value(), "get"))
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
        m_autocomplete = !equalIgnoringCase(attr->value(), "off");
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
    DocumentImpl *doc = getDocument();
    if (doc && e->isRadioButton() && !e->name().isEmpty()) {
        HTMLGenericFormElementImpl* currentCheckedRadio = doc->checkedRadioButtonForGroup(e->name().impl(), (HTMLFormElementImpl*) 0);
        if (currentCheckedRadio == e)
            doc->removeRadioButtonGroup(e->name().impl(), (HTMLFormElementImpl*) 0);
        if (e->isChecked())
            doc->radioButtonChecked((HTMLInputElementImpl*) e, this);
    }
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
    
} // namespace
