/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "Base64.h"
#include "CSSHelper.h"
#include "CString.h"
#include "Event.h"
#include "EventNames.h"
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

using namespace EventNames;
using namespace HTMLNames;

HTMLFormElement::HTMLFormElement(Document* doc)
    : HTMLElement(formTag, doc)
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
}

HTMLFormElement::~HTMLFormElement()
{
    delete m_elementAliases;
    delete collectionInfo;
    
    for (unsigned i = 0; i < formElements.size(); ++i)
        formElements[i]->formDestroyed();
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

void HTMLFormElement::handleLocalEvents(Event* event, bool useCapture)
{
    EventTargetNode* targetNode = event->target()->toNode();
    if (!useCapture && targetNode && targetNode != this && (event->type() == submitEvent || event->type() == resetEvent)) {
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
            HTMLInputElement *element = static_cast<HTMLInputElement *>(formElements[i]);
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

static DeprecatedCString encodeCString(const CString& cstr)
{
    DeprecatedCString e = cstr.deprecatedCString();

    // http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
    // same safe characters as Netscape for compatibility
    static const char *safe = "-._*";
    int elen = e.length();
    DeprecatedCString encoded((elen + e.contains('\n')) * 3 + 1);
    int enclen = 0;

    for (int pos = 0; pos < elen; pos++) {
        unsigned char c = e[pos];

        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr(safe, c))
            encoded[enclen++] = c;
        else if (c == ' ')
            encoded[enclen++] = '+';
        else if (c == '\n' || (c == '\r' && e[pos + 1] != '\n')) {
            encoded[enclen++] = '%';
            encoded[enclen++] = '0';
            encoded[enclen++] = 'D';
            encoded[enclen++] = '%';
            encoded[enclen++] = '0';
            encoded[enclen++] = 'A';
        } else if (c != '\r') {
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

// Warning: this helper doesn't currently have a reliable cross-platform behavior in certain edge cases
// (see basename(3) specification for examples).
// Consider this if it ever needs to become a general purpose method.
static String pathGetFilename(String path)
{
#if PLATFORM(QT)
    return QFileInfo(path).fileName();
#elif PLATFORM(WX)
    return wxFileName(path).GetFullName();
#elif PLATFORM(WIN_OS)
    return String(PathFindFileName(path.charactersWithNullTermination()));
#else
    return path.substring(path.reverseFind('/') + 1);
#endif
}

TextEncoding HTMLFormElement::dataEncoding() const
{
    if (isMailtoForm())
        return UTF8Encoding();

    TextEncoding encoding;
    String str = m_acceptcharset;
    str.replace(',', ' ');
    Vector<String> charsets = str.split(' ');
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
    DeprecatedCString enc_string = "";
    TextEncoding encoding = dataEncoding();

    RefPtr<FormData> result = new FormData;
    
    for (unsigned i = 0; i < formElements.size(); ++i) {
        HTMLGenericFormElement* current = formElements[i];
        FormDataList lst(encoding);

        if (!current->disabled() && current->appendFormData(lst, m_multipart)) {
            size_t ln = lst.list().size();
            for (size_t j = 0; j < ln; ++j) {
                const FormDataListItem& item = lst.list()[j];
                if (!m_multipart) {
                    // handle ISINDEX / <input name=isindex> special
                    // but only if its the first entry
                    if (enc_string.isEmpty() && item.m_data == "isindex") {
                        enc_string += encodeCString(lst.list()[j + 1].m_data);
                        ++j;
                    } else {
                        if (!enc_string.isEmpty())
                            enc_string += '&';

                        enc_string += encodeCString(item.m_data);
                        enc_string += "=";
                        enc_string += encodeCString(lst.list()[j + 1].m_data);
                        ++j;
                    }
                }
                else
                {
                    DeprecatedCString hstr("--");
                    hstr += boundary;
                    hstr += "\r\n";
                    hstr += "Content-Disposition: form-data; name=\"";
                    hstr += item.m_data.data();
                    hstr += "\"";

                    // if the current type is FILE, then we also need to
                    // include the filename
                    if (current->hasLocalName(inputTag) &&
                        static_cast<HTMLInputElement*>(current)->inputType() == HTMLInputElement::FILE) {
                        String path = static_cast<HTMLInputElement*>(current)->value();
                        String filename = pathGetFilename(path);

                        // FIXME: This won't work if the filename includes a " mark,
                        // or control characters like CR or LF. This also does strange
                        // things if the filename includes characters you can't encode
                        // in the website's character set.
                        hstr += "; filename=\"";
                        hstr += encoding.encode(reinterpret_cast<const UChar*>(filename.characters()), filename.length(), true).data();
                        hstr += "\"";

                        if (!static_cast<HTMLInputElement*>(current)->value().isEmpty()) {
                            DeprecatedString mimeType = MIMETypeRegistry::getMIMETypeForPath(path).deprecatedString();
                            if (!mimeType.isEmpty()) {
                                hstr += "\r\nContent-Type: ";
                                hstr += mimeType.ascii();
                            }
                        }
                    }

                    hstr += "\r\n\r\n";

                    // append body
                    result->appendData(hstr.data(), hstr.length());
                    const FormDataListItem& item = lst.list()[j + 1];
                    if (size_t dataSize = item.m_data.length())
                        result->appendData(item.m_data.data(), dataSize);
                    else if (!item.m_path.isEmpty())
                        result->appendFile(item.m_path);
                    result->appendData("\r\n", 2);

                    ++j;
                }
            }
        }
    }


    if (m_multipart) {
        enc_string = "--";
        enc_string += boundary;
        enc_string += "--\r\n";
    }

    result->appendData(enc_string.data(), enc_string.length());
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
    return m_url.startsWith("mailto:", false);
}

bool HTMLFormElement::prepareSubmit(Event* event)
{
    Frame* frame = document()->frame();
    if (m_insubmit || !frame)
        return m_insubmit;

    m_insubmit = true;
    m_doingsubmit = false;

    if (dispatchHTMLEvent(submitEvent, true, true) && !m_doingsubmit)
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
    FrameView *view = document()->view();
    Frame *frame = document()->frame();
    if (!view || !frame)
        return;

    if (m_insubmit) {
        m_doingsubmit = true;
        return;
    }

    m_insubmit = true;

    HTMLGenericFormElement* firstSuccessfulSubmitButton = 0;
    bool needButtonActivation = activateSubmitButton; // do we need to activate a submit button?
    
    frame->loader()->clearRecordedFormValues();
    for (unsigned i = 0; i < formElements.size(); ++i) {
        HTMLGenericFormElement* current = formElements[i];
        if (current->hasLocalName(inputTag)) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(current);
            if (input->isTextField()) {
                frame->loader()->recordFormValue(input->name(), input->value(), this);
                if (input->isSearchField())
                    input->addSearchResult();
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
    
    if (!m_url)
        m_url = document()->url();

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
                    body = KURL::decode_string(body.replace('&', "\r\n").replace('+', ' ').deprecatedString() + "\r\n");
                }
                data = new FormData((String("body=") + encodeCString(body.utf8())).replace('+', "%20").latin1());
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
    document()->checkedRadioButtons().removeButton(e);
    m_checkedRadioButtons.addButton(e);
    formElements.insert(formElementIndex(e), e);
}

void HTMLFormElement::removeFormElement(HTMLGenericFormElement* e)
{
    m_checkedRadioButtons.removeButton(e);
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

PassRefPtr<HTMLCollection> HTMLFormElement::elements()
{
    return new HTMLFormCollection(this);
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

PassRefPtr<HTMLGenericFormElement> HTMLFormElement::elementForAlias(const AtomicString& alias)
{
    if (alias.isEmpty() || !m_elementAliases)
        return 0;
    return m_elementAliases->get(alias.impl());
}

void HTMLFormElement::addElementAlias(HTMLGenericFormElement* element, const AtomicString& alias)
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
    RefPtr<HTMLGenericFormElement> aliasElem;
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
        addElementAlias(static_cast<HTMLGenericFormElement*>(namedItems.first().get()), name);        
}

void HTMLFormElement::CheckedRadioButtons::addButton(HTMLGenericFormElement* element)
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

void HTMLFormElement::CheckedRadioButtons::removeButton(HTMLGenericFormElement* element)
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
