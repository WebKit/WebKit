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
#include "HTMLFormElement.h"

#include "EventNames.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLFormCollection.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "csshelper.h"
#include "html_imageimpl.h"
#include "RenderLineEdit.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLFormElement::HTMLFormElement(Document *doc)
    : HTMLElement(formTag, doc)
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
    m_malformed = false;
    m_preserveAcrossRemove = false;
}

HTMLFormElement::~HTMLFormElement()
{
    delete collectionInfo;
    
    for (unsigned i = 0; i < formElements.size(); ++i)
        formElements[i]->m_form = 0;
    for (unsigned i = 0; i < imgElements.size(); ++i)
        imgElements[i]->m_form = 0;
}

bool HTMLFormElement::formWouldHaveSecureSubmission(const String &url)
{
    if (url.isNull()) {
        return false;
    }
    return document()->completeURL(url.deprecatedString()).startsWith("https:", false);
}

void HTMLFormElement::attach()
{
    HTMLElement::attach();

    // note we don't deal with calling secureFormRemoved() on detach, because the timing
    // was such that it cleared our state too early
    if (formWouldHaveSecureSubmission(m_url))
        document()->secureFormAdded();
}

void HTMLFormElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument *doc = static_cast<HTMLDocument *>(document());
        doc->addNamedItem(oldNameAttr);
    }

    HTMLElement::insertedIntoDocument();
}

void HTMLFormElement::removedFromDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument *doc = static_cast<HTMLDocument *>(document());
        doc->removeNamedItem(oldNameAttr);
    }
   
    HTMLElement::removedFromDocument();
}

int HTMLFormElement::length() const
{
    int len = 0;
    for (unsigned i = 0; i < formElements.size(); ++i)
        if (formElements[i]->isEnumeratable())
            ++len;

    return len;
}


void HTMLFormElement::submitClick()
{
    bool submitFound = false;
    for (unsigned i = 0; i < formElements.size(); ++i) {
        if (formElements[i]->hasLocalName(inputTag)) {
            HTMLInputElement *element = static_cast<HTMLInputElement *>(formElements[i]);
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

static DeprecatedCString encodeCString(const DeprecatedCString& e)
{
    // http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
    // safe characters like NS handles them for compatibility
    static const char *safe = "-._*";
    int elen = e.length();
    DeprecatedCString encoded(( elen+e.contains( '\n' ) )*3+1);
    int enclen = 0;

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

bool HTMLFormElement::formData(FormData &form_data) const
{
    DeprecatedCString enc_string = ""; // used for non-multipart data

    DeprecatedString str = m_acceptcharset.deprecatedString();
    str.replace(',', ' ');
    DeprecatedStringList charsets = DeprecatedStringList::split(' ', str);
    TextEncoding encoding(InvalidEncoding);
    Frame *frame = document()->frame();
    for (DeprecatedStringList::Iterator it = charsets.begin(); it != charsets.end(); ++it) {
        if ((encoding = TextEncoding((*it).latin1())).isValid())
            break;
    }

    if (!encoding.isValid()) {
        if (frame)
            encoding = TextEncoding(frame->encoding().latin1());
        else
            encoding = TextEncoding(Latin1Encoding);
    }

    for (unsigned i = 0; i < formElements.size(); ++i) {
        HTMLGenericFormElement* current = formElements[i];
        FormDataList lst(encoding);

        if (!current->disabled() && current->appendFormData(lst, m_multipart)) {
            for (DeprecatedValueListConstIterator<FormDataListItem> it = lst.begin(); it != lst.end(); ++it) {
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
                    DeprecatedCString hstr("--");
                    hstr += m_boundary.deprecatedString().latin1();
                    hstr += "\r\n";
                    hstr += "Content-Disposition: form-data; name=\"";
                    hstr += (*it).m_data.data();
                    hstr += "\"";

                    // if the current type is FILE, then we also need to
                    // include the filename
                    if (current->hasLocalName(inputTag) &&
                        static_cast<HTMLInputElement*>(current)->inputType() == HTMLInputElement::FILE) {
                        DeprecatedString path = static_cast<HTMLInputElement*>(current)->value().deprecatedString();

                        // FIXME: This won't work if the filename includes a " mark,
                        // or control characters like CR or LF. This also does strange
                        // things if the filename includes characters you can't encode
                        // in the website's character set.
                        hstr += "; filename=\"";
                        hstr += encoding.fromUnicode(path.mid(path.findRev('/') + 1), true);
                        hstr += "\"";

                        if (!static_cast<HTMLInputElement*>(current)->value().isEmpty()) {
                            DeprecatedString mimeType = frame ? frame->mimeTypeForFileName(path).deprecatedString() : DeprecatedString();
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
        enc_string = ("--" + m_boundary.deprecatedString() + "--\r\n").ascii();

    form_data.appendData(enc_string.data(), enc_string.length());
    return true;
}

void HTMLFormElement::parseEnctype(const String& type)
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

void HTMLFormElement::setBoundary( const String& bound )
{
    m_boundary = bound;
}

bool HTMLFormElement::prepareSubmit()
{
    Frame *frame = document()->frame();
    if (m_insubmit || !frame)
        return m_insubmit;

    m_insubmit = true;
    m_doingsubmit = false;

    if (dispatchHTMLEvent(submitEvent, true, true) && !m_doingsubmit)
        m_doingsubmit = true;

    m_insubmit = false;

    if (m_doingsubmit)
        submit(true);

    return m_doingsubmit;
}

void HTMLFormElement::submit( bool activateSubmitButton )
{
    FrameView *view = document()->view();
    Frame *frame = document()->frame();
    if (!view || !frame)
        return;

    if ( m_insubmit ) {
        m_doingsubmit = true;
        return;
    }

    m_insubmit = true;

    HTMLGenericFormElement* firstSuccessfulSubmitButton = 0;
    bool needButtonActivation = activateSubmitButton; // do we need to activate a submit button?
    
    frame->clearRecordedFormValues();
    for (unsigned i = 0; i < formElements.size(); ++i) {
        HTMLGenericFormElement* current = formElements[i];
        // Our app needs to get form values for password fields for doing password autocomplete,
        // so we are more lenient in pushing values, and let the app decide what to save when.
        if (current->hasLocalName(inputTag)) {
            HTMLInputElement *input = static_cast<HTMLInputElement*>(current);
            if (input->isTextField()) {
                frame->recordFormValue(input->name().deprecatedString(), input->value().deprecatedString(), this);
                if (input->renderer() && input->inputType() == HTMLInputElement::SEARCH)
                    static_cast<RenderLineEdit*>(input->renderer())->addSearchResult();
            }
        }

        if (needButtonActivation) {
            if (current->isActivatedSubmit())
                needButtonActivation = false;
            else if (firstSuccessfulSubmitButton == 0 && current->isSuccessfulSubmitButton())
                firstSuccessfulSubmitButton = current;
        }
    }

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(true);

    if (!m_post)
        m_multipart = false;
    
    FormData form_data;
    if (formData(form_data)) {
        if(m_post)
            frame->submitForm("post", m_url, form_data, m_target, enctype(), boundary());
        else
            frame->submitForm("get", m_url, form_data, m_target);
    }

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(false);
    
    m_doingsubmit = m_insubmit = false;
}

void HTMLFormElement::reset()
{
    Frame *frame = document()->frame();
    if (m_inreset || !frame)
        return;

    m_inreset = true;

    // ### DOM2 labels this event as not cancelable, however
    // common browsers( sick! ) allow it be cancelled.
    if ( !dispatchHTMLEvent(resetEvent,true, true) ) {
        m_inreset = false;
        return;
    }

    for (unsigned i = 0; i < formElements.size(); ++i)
        formElements[i]->reset();

    m_inreset = false;
}

void HTMLFormElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == actionAttr) {
        bool oldURLWasSecure = formWouldHaveSecureSubmission(m_url);
        m_url = WebCore::parseURL(attr->value());
        bool newURLIsSecure = formWouldHaveSecureSubmission(m_url);

        if (m_attached && (oldURLWasSecure != newURLIsSecure))
            if (newURLIsSecure)
                document()->secureFormAdded();
            else
                document()->secureFormRemoved();
    }
    else if (attr->name() == targetAttr)
        m_target = attr->value();
    else if (attr->name() == methodAttr) {
        if (equalIgnoringCase(attr->value(), "post"))
            m_post = true;
        else if (equalIgnoringCase(attr->value(), "get"))
            m_post = false;
    } else if (attr->name() == enctypeAttr)
        parseEnctype(attr->value());
    else if (attr->name() == accept_charsetAttr)
        // space separated list of charsets the server
        // accepts - see rfc2045
        m_acceptcharset = attr->value();
    else if (attr->name() == acceptAttr) {
        // ignore this one for the moment...
    } else if (attr->name() == autocompleteAttr)
        m_autocomplete = !equalIgnoringCase(attr->value(), "off");
    else if (attr->name() == onsubmitAttr)
        setHTMLEventListener(submitEvent, attr);
    else if (attr->name() == onresetAttr)
        setHTMLEventListener(resetEvent, attr);
    else if (attr->name() == nameAttr) {
        String newNameAttr = attr->value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument *doc = static_cast<HTMLDocument *>(document());
            doc->removeNamedItem(oldNameAttr);
            doc->addNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLElement::parseMappedAttribute(attr);
}

template<class T, size_t n> static void removeFromVector(Vector<T*, n> & vec, T* item)
{
    size_t size = vec.size();
    for (size_t i = 0; i != size; ++i)
        if (vec[i] == item) {
            vec.remove(i);
            break;
        }
}

unsigned HTMLFormElement::formElementIndex(HTMLGenericFormElement *e)
{
    // Check for the special case where this element is the very last thing in
    // the form's tree of children; we don't want to walk the entire tree in that
    // common case that occurs during parsing; instead we'll just return a value
    // that says "add this form element to the end of the array".
    if (e->traverseNextNode(this)) {
        unsigned i = 0;
        for (Node *node = this; node; node = node->traverseNextNode(this)) {
            if (node == e)
                return i;
            if (node->isHTMLElement()
                    && static_cast<HTMLElement *>(node)->isGenericFormElement()
                    && static_cast<HTMLGenericFormElement *>(node)->form() == this)
                ++i;
        }
    }
    return formElements.size();
}

void HTMLFormElement::registerFormElement(HTMLGenericFormElement* e)
{
    Document* doc = document();
    if (e->isRadioButton() && !e->name().isEmpty()) {
        HTMLGenericFormElement* currentCheckedRadio = doc->checkedRadioButtonForGroup(e->name().impl(), 0);
        if (currentCheckedRadio == e)
            doc->removeRadioButtonGroup(e->name().impl(), 0);
        if (e->isChecked())
            doc->radioButtonChecked(static_cast<HTMLInputElement*>(e), this);
    }
    formElements.insert(formElementIndex(e), e);
}

void HTMLFormElement::removeFormElement(HTMLGenericFormElement* e)
{
    if (!e->name().isEmpty()) {
        HTMLGenericFormElement* currentCheckedRadio = document()->checkedRadioButtonForGroup(e->name().impl(), this);
        if (currentCheckedRadio == e)
            document()->removeRadioButtonGroup(e->name().impl(), this);
    }
    removeFromVector(formElements, e);
}

bool HTMLFormElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == actionAttr;
}

void HTMLFormElement::registerImgElement(HTMLImageElement *e)
{
    imgElements.append(e);
}

void HTMLFormElement::removeImgElement(HTMLImageElement *e)
{
    removeFromVector(imgElements, e);
}

RefPtr<HTMLCollection> HTMLFormElement::elements()
{
    return RefPtr<HTMLCollection>(new HTMLFormCollection(this));
}

String HTMLFormElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLFormElement::setName(const String &value)
{
    setAttribute(nameAttr, value);
}

String HTMLFormElement::acceptCharset() const
{
    return getAttribute(accept_charsetAttr);
}

void HTMLFormElement::setAcceptCharset(const String &value)
{
    setAttribute(accept_charsetAttr, value);
}

String HTMLFormElement::action() const
{
    return getAttribute(actionAttr);
}

void HTMLFormElement::setAction(const String &value)
{
    setAttribute(actionAttr, value);
}

void HTMLFormElement::setEnctype(const String &value)
{
    setAttribute(enctypeAttr, value);
}

String HTMLFormElement::method() const
{
    return getAttribute(methodAttr);
}

void HTMLFormElement::setMethod(const String &value)
{
    setAttribute(methodAttr, value);
}

String HTMLFormElement::target() const
{
    return getAttribute(targetAttr);
}

void HTMLFormElement::setTarget(const String &value)
{
    setAttribute(targetAttr, value);
}
    
} // namespace
