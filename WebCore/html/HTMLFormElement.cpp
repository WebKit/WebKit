/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLFormCollection.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "ScriptEventListener.h"
#include "MIMETypeRegistry.h"
#include "MappedAttribute.h"
#include "Page.h"
#include "RenderTextControl.h"
#include <limits>
#include <wtf/CurrentTime.h>
#include <wtf/RandomNumber.h>

#if PLATFORM(WX)
#include <wx/defs.h>
#include <wx/filename.h>
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static int64_t generateFormDataIdentifier()
{
    // Initialize to the current time to reduce the likelihood of generating
    // identifiers that overlap with those from past/future browser sessions.
    static int64_t nextIdentifier = static_cast<int64_t>(currentTime() * 1000000.0);
    return ++nextIdentifier;
}

HTMLFormElement::HTMLFormElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
    , m_elementAliases(0)
    , collectionInfo(0)
    , m_autocomplete(true)
    , m_insubmit(false)
    , m_doingsubmit(false)
    , m_inreset(false)
    , m_malformed(false)
    , m_demoted(false)
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

bool HTMLFormElement::rendererIsNeeded(RenderStyle* style)
{
    if (!isDemoted())
        return HTMLElement::rendererIsNeeded(style);
    
    Node* node = parentNode();
    RenderObject* parentRenderer = node->renderer();
    bool parentIsTableElementPart = (parentRenderer->isTable() && node->hasTagName(tableTag))
        || (parentRenderer->isTableRow() && node->hasTagName(trTag))
        || (parentRenderer->isTableSection() && node->hasTagName(tbodyTag))
        || (parentRenderer->isTableCol() && node->hasTagName(colTag)) 
        || (parentRenderer->isTableCell() && node->hasTagName(trTag));

    if (!parentIsTableElementPart)
        return true;

    EDisplay display = style->display();
    bool formIsTablePart = display == TABLE || display == INLINE_TABLE || display == TABLE_ROW_GROUP
        || display == TABLE_HEADER_GROUP || display == TABLE_FOOTER_GROUP || display == TABLE_ROW
        || display == TABLE_COLUMN_GROUP || display == TABLE_COLUMN || display == TABLE_CELL
        || display == TABLE_CAPTION;

    return formIsTablePart;
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
    Node* targetNode = event->target()->toNode();
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

TextEncoding HTMLFormElement::dataEncoding() const
{
    if (isMailtoForm())
        return UTF8Encoding();

    return m_formDataBuilder.dataEncoding(document());
}

PassRefPtr<FormData> HTMLFormElement::createFormData(const CString& boundary)
{
    Vector<char> encodedData;
    TextEncoding encoding = dataEncoding().encodingForFormSubmission();

    RefPtr<FormData> result = FormData::create();

    for (unsigned i = 0; i < formElements.size(); ++i) {
        HTMLFormControlElement* control = formElements[i];
        FormDataList list(encoding);

        if (!control->disabled() && control->appendFormData(list, m_formDataBuilder.isMultiPartForm())) {
            size_t formDataListSize = list.list().size();
            ASSERT(formDataListSize % 2 == 0);
            for (size_t j = 0; j < formDataListSize; j += 2) {
                const FormDataList::Item& key = list.list()[j];
                const FormDataList::Item& value = list.list()[j + 1];
                if (!m_formDataBuilder.isMultiPartForm()) {
                    // Omit the name "isindex" if it's the first form data element.
                    // FIXME: Why is this a good rule? Is this obsolete now?
                    if (encodedData.isEmpty() && key.data() == "isindex")
                        FormDataBuilder::encodeStringAsFormData(encodedData, value.data());
                    else
                        m_formDataBuilder.addKeyValuePairAsFormData(encodedData, key.data(), value.data());
                } else {
                    Vector<char> header;
                    m_formDataBuilder.beginMultiPartHeader(header, boundary, key.data());

                    bool shouldGenerateFile = false;
                    // if the current type is FILE, then we also need to include the filename
                    if (value.file()) {
                        const String& path = value.file()->path();
                        String fileName = value.file()->fileName();

                        // Let the application specify a filename if it's going to generate a replacement file for the upload.
                        if (!path.isEmpty()) {
                            if (Page* page = document()->page()) {
                                String generatedFileName;
                                shouldGenerateFile = page->chrome()->client()->shouldReplaceWithGeneratedFileForUpload(path, generatedFileName);
                                if (shouldGenerateFile)
                                    fileName = generatedFileName;
                            }
                        }

                        // We have to include the filename=".." part in the header, even if the filename is empty
                        m_formDataBuilder.addFilenameToMultiPartHeader(header, encoding, fileName);

                        if (!fileName.isEmpty()) {
                            // FIXME: The MIMETypeRegistry function's name makes it sound like it takes a path,
                            // not just a basename. But filename is not the path. But note that it's not safe to
                            // just use path instead since in the generated-file case it will not reflect the
                            // MIME type of the generated file.
                            String mimeType = MIMETypeRegistry::getMIMETypeForPath(fileName);
                            if (!mimeType.isEmpty())
                                m_formDataBuilder.addContentTypeToMultiPartHeader(header, mimeType.latin1());
                        }
                    }

                    m_formDataBuilder.finishMultiPartHeader(header);

                    // Append body
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

    if (m_formDataBuilder.isMultiPartForm())
        m_formDataBuilder.addBoundaryToMultiPartHeader(encodedData, boundary, true);

    result->appendData(encodedData.data(), encodedData.size());

    result->setIdentifier(generateFormDataIdentifier());
    return result;
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

    if (dispatchEvent(eventNames().submitEvent, true, true) && !m_doingsubmit)
        m_doingsubmit = true;

    m_insubmit = false;

    if (m_doingsubmit)
        submit(event, true);

    return m_doingsubmit;
}

static void transferMailtoPostFormDataToURL(RefPtr<FormData>& data, KURL& url, const String& encodingType)
{
    String body = data->flattenToString();
    data = FormData::create();

    if (equalIgnoringCase(encodingType, "text/plain")) {
        // Convention seems to be to decode, and s/&/\r\n/. Also, spaces are encoded as %20.
        body = decodeURLEscapeSequences(body.replace('&', "\r\n").replace('+', ' ') + "\r\n");
    }

    Vector<char> bodyData;
    bodyData.append("body=", 5);
    FormDataBuilder::encodeStringAsFormData(bodyData, body.utf8());
    body = String(bodyData.data(), bodyData.size()).replace('+', "%20");

    String query = url.query();
    if (!query.isEmpty())
        query.append('&');
    query.append(body);
    url.setQuery(query);
}

void HTMLFormElement::submit(Event* event, bool activateSubmitButton, bool lockHistory, bool lockBackForwardList)
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
    
    Vector<pair<String, String> > formValues;

    for (unsigned i = 0; i < formElements.size(); ++i) {
        HTMLFormControlElement* control = formElements[i];
        if (control->hasLocalName(inputTag)) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(control);
            if (input->isTextField()) {
                formValues.append(pair<String, String>(input->name(), input->value()));
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

    RefPtr<FormState> formState = FormState::create(this, formValues, frame);

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(true);
    
    if (m_url.isEmpty())
        m_url = document()->url().string();

    if (m_formDataBuilder.isPostMethod()) {
        if (m_formDataBuilder.isMultiPartForm() && isMailtoForm()) {
            setEnctype("application/x-www-form-urlencoded");
            ASSERT(!m_formDataBuilder.isMultiPartForm());
        }

        if (!m_formDataBuilder.isMultiPartForm()) {
            RefPtr<FormData> data = createFormData(CString());

            if (isMailtoForm()) {
                // Convert the form data into a string that we put into the URL.
                KURL url = document()->completeURL(m_url);
                transferMailtoPostFormDataToURL(data, url, m_formDataBuilder.encodingType());
                m_url = url.string();
            }

            frame->loader()->submitForm("POST", m_url, data.release(), m_target, m_formDataBuilder.encodingType(), String(), lockHistory, lockBackForwardList, event, formState.release());
        } else {
            Vector<char> boundary = m_formDataBuilder.generateUniqueBoundaryString();
            frame->loader()->submitForm("POST", m_url, createFormData(boundary.data()), m_target, m_formDataBuilder.encodingType(), boundary.data(), lockHistory, lockBackForwardList, event, formState.release());
        }
    } else {
        m_formDataBuilder.setIsMultiPartForm(false);
        frame->loader()->submitForm("GET", m_url, createFormData(CString()), m_target, String(), String(), lockHistory, lockBackForwardList, event, formState.release());
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
    if ( !dispatchEvent(eventNames().resetEvent,true, true) ) {
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
    else if (attr->name() == methodAttr)
        m_formDataBuilder.parseMethodType(attr->value());
    else if (attr->name() == enctypeAttr)
        m_formDataBuilder.parseEncodingType(attr->value());
    else if (attr->name() == accept_charsetAttr)
        // space separated list of charsets the server
        // accepts - see rfc2045
        m_formDataBuilder.setAcceptCharset(attr->value());
    else if (attr->name() == acceptAttr) {
        // ignore this one for the moment...
    } else if (attr->name() == autocompleteAttr) {
        m_autocomplete = !equalIgnoringCase(attr->value(), "off");
        if (!m_autocomplete)
            document()->registerForDocumentActivationCallbacks(this);    
        else
            document()->unregisterForDocumentActivationCallbacks(this);
    } else if (attr->name() == onsubmitAttr)
        setAttributeEventListener(eventNames().submitEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onresetAttr)
        setAttributeEventListener(eventNames().resetEvent, createAttributeEventListener(this, attr));
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
                    && static_cast<Element*>(node)->isFormControlElement()
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
    HTMLElement::willMoveToNewOwnerDocument();
}

void HTMLFormElement::didMoveToNewOwnerDocument()
{
    if (!m_autocomplete)
        document()->registerForDocumentActivationCallbacks(this);
    HTMLElement::didMoveToNewOwnerDocument();
}

} // namespace
