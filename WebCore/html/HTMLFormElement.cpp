/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "HTMLFormElement.h"

#include "CSSHelper.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "FileList.h"
#include "FileSystem.h"
#include "FormData.h"
#include "FormDataList.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLFormCollection.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "RenderTextControl.h"

#if PLATFORM(QT)
#include <QtCore/QFileInfo>
#endif

#if PLATFORM(WX)
#include <wx/defs.h>
#include <wx/filename.h>
#endif

#if PLATFORM(WIN_OS)
#include <shlwapi.h>
#endif

namespace WebCore {

using namespace HTMLNames;

static const char hexDigits[17] = "0123456789ABCDEF";

HTMLFormElement::HTMLFormElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
    , m_elementAliases(0)
    , collectionInfo(0)
    , m_enctype("application/x-www-form-urlencoded")
    , m_post(false)
    , m_multipart(false)
    , m_autocomplete(true)
    , m_insubmit(false)
    , m_doingsubmit(false)
    , m_inreset(false)
    , m_malformed(false)
{
    ASSERT(hasTagName(formTag));
}

HTMLFormElement::~HTMLFormElement()
{
    if (!m_autocomplete)
        document()->unregisterForDocumentActivationCallbacks(this);

    delete m_elementAliases;
    delete collectionInfo;

    for (unsigned i = 0; i < formElements.size(); ++i)
        formElements[i]->formDestroyed();
    for (unsigned i = 0; i < imgElements.size(); ++i)
        imgElements[i]->m_form = 0;
}

bool HTMLFormElement::formWouldHaveSecureSubmission(const String& url)
{
    return document()->completeURL(url).protocolIs("https");
}

void HTMLFormElement::attach()
{
    HTMLElement::attach();
}

void HTMLFormElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->addNamedItem(m_name);

    HTMLElement::insertedIntoDocument();
}

void HTMLFormElement::removedFromDocument()
{
    if (document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->removeNamedItem(m_name);
   
    HTMLElement::removedFromDocument();
}

void HTMLFormElement::handleLocalEvents(Event* event, bool useCapture)
{
    EventTargetNode* targetNode = event->target()->toNode();
    if (!useCapture && targetNode && targetNode != this && (event->type() == eventNames().submitEvent || event->type() == eventNames().resetEvent)) {
        event->stopPropagation();
        return;
    }
    HTMLElement::handleLocalEvents(event, useCapture);
}

unsigned HTMLFormElement::length() const
{
    int len = 0;
    for (unsigned i = 0; i < formElements.size(); ++i)
        if (formElements[i]->isEnumeratable())
            ++len;

    return len;
}

Node* HTMLFormElement::item(unsigned index)
{
    return elements()->item(index);
}

void HTMLFormElement::submitClick(Event* event)
{
    bool submitFound = false;
    for (unsigned i = 0; i < formElements.size(); ++i) {
        if (formElements[i]->hasLocalName(inputTag)) {
            HTMLInputElement* element = static_cast<HTMLInputElement*>(formElements[i]);
            if (element->isSuccessfulSubmitButton() && element->renderer()) {
                submitFound = true;
                element->dispatchSimulatedClick(event);
                break;
            }
        }
    }
    if (!submitFound) // submit the form without a submit or image input
        prepareSubmit(event);
}

static void appendString(Vector<char>& buffer, const char* string)
{
    buffer.append(string, strlen(string));
}

static void appendString(Vector<char>& buffer, const CString& string)
{
    buffer.append(string.data(), string.length());
}

static void appendEncodedString(Vector<char>& buffer, const CString& string)
{
    // http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
    int length = string.length();
    for (int i = 0; i < length; i++) {
        unsigned char c = string.data()[i];

        // Same safe characters as Netscape for compatibility.
        static const char safe[] = "-._*";
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr(safe, c))
            buffer.append(c);
        else if (c == ' ')
            buffer.append('+');
        else if (c == '\n' || (c == '\r' && (i + 1 >= length || string.data()[i + 1] != '\n')))
            appendString(buffer, "%0D%0A");
        else if (c != '\r') {
            buffer.append('%');
            buffer.append(hexDigits[c >> 4]);
            buffer.append(hexDigits[c & 0xF]);
        }
    }
}

// FIXME: Move to platform directory?
static int randomNumber()
{
    static bool randomSeeded = false;

#if PLATFORM(DARWIN)
    if (!randomSeeded) {
        srandomdev();
        randomSeeded = true;
    }
    return random();
#else
    if (!randomSeeded) {
        srand(static_cast<unsigned>(time(0)));
        randomSeeded = true;
    }
    return rand();
#endif
}

TextEncoding HTMLFormElement::dataEncoding() const
{
    if (isMailtoForm())
        return UTF8Encoding();

    TextEncoding encoding;
    String str = m_acceptcharset;
    str.replace(',', ' ');
    Vector<String> charsets;
    str.split(' ', charsets);
    Vector<String>::const_iterator end = charsets.end();
    for (Vector<String>::const_iterator it = charsets.begin(); it != end; ++it)
        if ((encoding = TextEncoding(*it)).isValid())
            return encoding;
    if (Frame* frame = document()->frame())
        return frame->loader()->encoding();
    return Latin1Encoding();
}

PassRefPtr<FormData> HTMLFormElement::formData(const char* boundary) const
{
    Vector<char> encodedData;
    TextEncoding encoding = dataEncoding().encodingForFormSubmission();

    RefPtr<FormData> result = FormData::create();
    
    for (unsigned i = 0; i < formElements.size(); ++i) {
        HTMLFormControlElement* control = formElements[i];
        FormDataList list(encoding);

        if (!control->disabled() && control->appendFormData(list, m_multipart)) {
            size_t formDataListSize = list.list().size();
            ASSERT(formDataListSize % 2 == 0);
            for (size_t j = 0; j < formDataListSize; j += 2) {
                const FormDataList::Item& key = list.list()[j];
                const FormDataList::Item& value = list.list()[j + 1];
                if (!m_multipart) {
                    // Omit the name "isindex" if it's the first form data element.
                    // FIXME: Why is this a good rule? Is this obsolete now?
                    if (encodedData.isEmpty() && key.data() == "isindex")
                        appendEncodedString(encodedData, value.data());
                    else {
                        if (!encodedData.isEmpty())
                            encodedData.append('&');
                        appendEncodedString(encodedData, key.data());
                        encodedData.append('=');
                        appendEncodedString(encodedData, value.data());
                    }
                } else {
                    Vector<char> header;
                    appendString(header, "--");
                    appendString(header, boundary);
                    appendString(header, "\r\n");
                    appendString(header, "Content-Disposition: form-data; name=\"");
                    header.append(key.data().data(), key.data().length());
                    header.append('"');

                    bool shouldGenerateFile = false;
                    // if the current type is FILE, then we also need to
                    // include the filename
                    if (value.file()) {
                        const String& path = value.file()->path();
                        String filename = value.file()->fileName();

                        // Let the application specify a filename if it's going to generate a replacement file for the upload.
                        if (!path.isEmpty()) {
                            if (Page* page = document()->page()) {
                                String generatedFilename;
                                shouldGenerateFile = page->chrome()->client()->shouldReplaceWithGeneratedFileForUpload(path, generatedFilename);
                                if (shouldGenerateFile)
                                    filename = generatedFilename;
                            }
                        }

                        // FIXME: This won't work if the filename includes a " mark,
                        // or control characters like CR or LF. This also does strange
                        // things if the filename includes characters you can't encode
                        // in the website's character set.
                        appendString(header, "; filename=\"");
                        appendString(header, encoding.encode(filename.characters(), filename.length(), QuestionMarksForUnencodables));
                        header.append('"');

                        if (!filename.isEmpty()) {
                            // FIXME: The MIMETypeRegistry function's name makes it sound like it takes a path,
                            // not just a basename. But filename is not the path. But note that it's not safe to
                            // just use path instead since in the generated-file case it will not reflect the
                            // MIME type of the generated file.
                            String mimeType = MIMETypeRegistry::getMIMETypeForPath(filename);
                            if (!mimeType.isEmpty()) {
                                appendString(header, "\r\nContent-Type: ");
                                appendString(header, mimeType.latin1());
                            }
                        }
                    }

                    appendString(header, "\r\n\r\n");

                    // append body
                    result->appendData(header.data(), header.size());
                    if (size_t dataSize = value.data().length())
                        result->appendData(value.data().data(), dataSize);
                    else if (value.file() && !value.file()->path().isEmpty())
                        result->appendFile(value.file()->path(), shouldGenerateFile);
                    result->appendData("\r\n", 2);
                }
            }
        }
    }


    if (m_multipart) {
        appendString(encodedData, "--");
        appendString(encodedData, boundary);
        appendString(encodedData, "--\r\n");
    }

    result->appendData(encodedData.data(), encodedData.size());
    return result;
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

bool HTMLFormElement::isMailtoForm() const
{
    return protocolIs(m_url, "mailto");
}

bool HTMLFormElement::prepareSubmit(Event* event)
{
    Frame* frame = document()->frame();
    if (m_insubmit || !frame)
        return m_insubmit;

    m_insubmit = true;
    m_doingsubmit = false;

    if (dispatchEventForType(eventNames().submitEvent, true, true) && !m_doingsubmit)
        m_doingsubmit = true;

    m_insubmit = false;

    if (m_doingsubmit)
        submit(event, true);

    return m_doingsubmit;
}

void HTMLFormElement::submit()
{
    submit(0, false);
}

// Returns a 0-terminated C string in the vector.
static void getUniqueBoundaryString(Vector<char>& boundary)
{
    // The RFC 2046 spec says the AlphaNumeric characters plus the following characters
    // are legal for boundaries:  '()+_,-./:=?
    // However the following characters, though legal, cause some sites to fail:
    // (),./:=
    // http://bugs.webkit.org/show_bug.cgi?id=13352
    static const char AlphaNumericEncMap[64] =
    {
      0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
      0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
      0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
      0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
      0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
      0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
      0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,
      0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x41
      // FIXME <rdar://problem/5252577> gmail does not accept legal characters in the form boundary
      // As stated above, some legal characters cause, sites to fail. Specifically
      // the / character which was the last character in the above array. I have
      // replaced the last character with another character already in the array
      // (notice the first and last values are both 0x41, A). Instead of picking
      // another unique legal character for boundary strings that, because it has
      // never been tested, may or may not break other sites, I simply
      // replaced / with A.  This means A is twice as likely to occur in our boundary
      // strings than any other character but I think this is fine for the time being.
      // The FIXME here is about restoring the / character once the aforementioned
      // radar has been resolved.
    };

    // Start with an informative prefix.
    const char boundaryPrefix[] = "----WebKitFormBoundary";
    boundary.append(boundaryPrefix, strlen(boundaryPrefix));

    // Append 16 random 7bit ascii AlphaNumeric characters.
    Vector<char> randomBytes;

    for (int i = 0; i < 4; ++i) {
        int randomness = randomNumber();
        randomBytes.append(AlphaNumericEncMap[(randomness >> 24) & 0x3F]);
        randomBytes.append(AlphaNumericEncMap[(randomness >> 16) & 0x3F]);
        randomBytes.append(AlphaNumericEncMap[(randomness >> 8) & 0x3F]);
        randomBytes.append(AlphaNumericEncMap[randomness & 0x3F]);
    }

    boundary.append(randomBytes);
    boundary.append(0); // Add a 0 at the end so we can use this as a C-style string.
}

void HTMLFormElement::submit(Event* event, bool activateSubmitButton)
{
    FrameView* view = document()->view();
    Frame* frame = document()->frame();
    if (!view || !frame)
        return;

    if (m_insubmit) {
        m_doingsubmit = true;
        return;
    }

    m_insubmit = true;

    HTMLFormControlElement* firstSuccessfulSubmitButton = 0;
    bool needButtonActivation = activateSubmitButton; // do we need to activate a submit button?
    
    frame->loader()->clearRecordedFormValues();
    frame->loader()->setFormAboutToBeSubmitted(this);
    for (unsigned i = 0; i < formElements.size(); ++i) {
        HTMLFormControlElement* control = formElements[i];
        if (control->hasLocalName(inputTag)) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(control);
            if (input->isTextField()) {
                frame->loader()->recordFormValue(input->name(), input->value());
                if (input->isSearchField())
                    input->addSearchResult();
            }
        }
        if (needButtonActivation) {
            if (control->isActivatedSubmit())
                needButtonActivation = false;
            else if (firstSuccessfulSubmitButton == 0 && control->isSuccessfulSubmitButton())
                firstSuccessfulSubmitButton = control;
        }
    }

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(true);
    
    if (m_url.isEmpty())
        m_url = document()->url().string();

    if (m_post) {
        if (m_multipart && isMailtoForm()) {
            setEnctype("application/x-www-form-urlencoded");
            m_multipart = false;
        }

        if (!m_multipart) {
            RefPtr<FormData> data = formData(0);
            if (isMailtoForm()) {
                String body = data->flattenToString();
                if (equalIgnoringCase(enctype(), "text/plain")) {
                    // Convention seems to be to decode, and s/&/\r\n/. Also, spaces are encoded as %20.
                    body = decodeURLEscapeSequences(body.replace('&', "\r\n").replace('+', ' ') + "\r\n");
                }
                Vector<char> bodyData;
                appendString(bodyData, "body=");
                appendEncodedString(bodyData, body.utf8());
                data = FormData::create(String(bodyData.data(), bodyData.size()).replace('+', "%20").latin1());
            }
            frame->loader()->submitForm("POST", m_url, data, m_target, enctype(), String(), event);
        } else {
            Vector<char> boundary;
            getUniqueBoundaryString(boundary);
            frame->loader()->submitForm("POST", m_url, formData(boundary.data()), m_target, enctype(), boundary.data(), event);
        }
    } else {
        m_multipart = false;
        frame->loader()->submitForm("GET", m_url, formData(0), m_target, String(), String(), event);
    }

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(false);
    
    m_doingsubmit = m_insubmit = false;
}

void HTMLFormElement::reset()
{
    Frame* frame = document()->frame();
    if (m_inreset || !frame)
        return;

    m_inreset = true;

    // ### DOM2 labels this event as not cancelable, however
    // common browsers( sick! ) allow it be cancelled.
    if ( !dispatchEventForType(eventNames().resetEvent,true, true) ) {
        m_inreset = false;
        return;
    }

    for (unsigned i = 0; i < formElements.size(); ++i)
        formElements[i]->reset();

    m_inreset = false;
}

void HTMLFormElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == actionAttr)
        m_url = parseURL(attr->value());
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
    } else if (attr->name() == autocompleteAttr) {
        m_autocomplete = !equalIgnoringCase(attr->value(), "off");
        if (!m_autocomplete)
            document()->registerForDocumentActivationCallbacks(this);    
        else
            document()->unregisterForDocumentActivationCallbacks(this);
    } else if (attr->name() == onsubmitAttr)
        setInlineEventListenerForTypeAndAttribute(eventNames().submitEvent, attr);
    else if (attr->name() == onresetAttr)
        setInlineEventListenerForTypeAndAttribute(eventNames().resetEvent, attr);
    else if (attr->name() == nameAttr) {
        const AtomicString& newName = attr->value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeNamedItem(m_name);
            document->addNamedItem(newName);
        }
        m_name = newName;
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

unsigned HTMLFormElement::formElementIndex(HTMLFormControlElement* e)
{
    // Check for the special case where this element is the very last thing in
    // the form's tree of children; we don't want to walk the entire tree in that
    // common case that occurs during parsing; instead we'll just return a value
    // that says "add this form element to the end of the array".
    if (e->traverseNextNode(this)) {
        unsigned i = 0;
        for (Node* node = this; node; node = node->traverseNextNode(this)) {
            if (node == e)
                return i;
            if (node->isHTMLElement()
                    && static_cast<HTMLElement*>(node)->isGenericFormElement()
                    && static_cast<HTMLFormControlElement*>(node)->form() == this)
                ++i;
        }
    }
    return formElements.size();
}

void HTMLFormElement::registerFormElement(HTMLFormControlElement* e)
{
    document()->checkedRadioButtons().removeButton(e);
    m_checkedRadioButtons.addButton(e);
    formElements.insert(formElementIndex(e), e);
}

void HTMLFormElement::removeFormElement(HTMLFormControlElement* e)
{
    m_checkedRadioButtons.removeButton(e);
    removeFromVector(formElements, e);
}

bool HTMLFormElement::isURLAttribute(Attribute* attr) const
{
    return attr->name() == actionAttr;
}

void HTMLFormElement::registerImgElement(HTMLImageElement* e)
{
    imgElements.append(e);
}

void HTMLFormElement::removeImgElement(HTMLImageElement* e)
{
    removeFromVector(imgElements, e);
}

PassRefPtr<HTMLCollection> HTMLFormElement::elements()
{
    return HTMLFormCollection::create(this);
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

PassRefPtr<HTMLFormControlElement> HTMLFormElement::elementForAlias(const AtomicString& alias)
{
    if (alias.isEmpty() || !m_elementAliases)
        return 0;
    return m_elementAliases->get(alias.impl());
}

void HTMLFormElement::addElementAlias(HTMLFormControlElement* element, const AtomicString& alias)
{
    if (alias.isEmpty())
        return;
    if (!m_elementAliases)
        m_elementAliases = new AliasMap;
    m_elementAliases->set(alias.impl(), element);
}

void HTMLFormElement::getNamedElements(const AtomicString& name, Vector<RefPtr<Node> >& namedItems)
{
    elements()->namedItems(name, namedItems);

    // see if we have seen something with this name before
    RefPtr<HTMLFormControlElement> aliasElem;
    if (aliasElem = elementForAlias(name)) {
        bool found = false;
        for (unsigned n = 0; n < namedItems.size(); n++) {
            if (namedItems[n] == aliasElem.get()) {
                found = true;
                break;
            }
        }
        if (!found)
            // we have seen it before but it is gone now. still, we need to return it.
            namedItems.append(aliasElem.get());
    }
    // name has been accessed, remember it
    if (namedItems.size() && aliasElem != namedItems.first())
        addElementAlias(static_cast<HTMLFormControlElement*>(namedItems.first().get()), name);        
}

void HTMLFormElement::documentDidBecomeActive()
{
    ASSERT(!m_autocomplete);
    
    for (unsigned i = 0; i < formElements.size(); ++i)
        formElements[i]->reset();
}

void HTMLFormElement::willMoveToNewOwnerDocument()
{
    if (!m_autocomplete)
        document()->unregisterForDocumentActivationCallbacks(this);
}

void HTMLFormElement::didMoveToNewOwnerDocument()
{
    if(m_autocomplete)
        document()->registerForDocumentActivationCallbacks(this);
}

void HTMLFormElement::CheckedRadioButtons::addButton(HTMLFormControlElement* element)
{
    // We only want to add radio buttons.
    if (!element->isRadioButton())
        return;

    // Without a name, there is no group.
    if (element->name().isEmpty())
        return;

    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(element);

    // We only track checked buttons.
    if (!inputElement->checked())
        return;

    if (!m_nameToCheckedRadioButtonMap)
        m_nameToCheckedRadioButtonMap.set(new NameToInputMap);

    pair<NameToInputMap::iterator, bool> result = m_nameToCheckedRadioButtonMap->add(element->name().impl(), inputElement);
    if (result.second)
        return;
    
    HTMLInputElement* oldCheckedButton = result.first->second;
    if (oldCheckedButton == inputElement)
        return;

    result.first->second = inputElement;
    oldCheckedButton->setChecked(false);
}

HTMLInputElement* HTMLFormElement::CheckedRadioButtons::checkedButtonForGroup(const AtomicString& name) const
{
    if (!m_nameToCheckedRadioButtonMap)
        return 0;
    
    return m_nameToCheckedRadioButtonMap->get(name.impl());
}

void HTMLFormElement::CheckedRadioButtons::removeButton(HTMLFormControlElement* element)
{
    if (element->name().isEmpty() || !m_nameToCheckedRadioButtonMap)
        return;
    
    NameToInputMap::iterator it = m_nameToCheckedRadioButtonMap->find(element->name().impl());
    if (it == m_nameToCheckedRadioButtonMap->end() || it->second != element)
        return;
    
    ASSERT(element->isRadioButton());
    ASSERT(element->isChecked());
    
    m_nameToCheckedRadioButtonMap->remove(it);
    if (m_nameToCheckedRadioButtonMap->isEmpty())
        m_nameToCheckedRadioButtonMap.clear();
}

} // namespace
