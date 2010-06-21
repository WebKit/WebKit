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

#include "Attribute.h"
#include "CSSHelper.h"
#include "DOMFormData.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "FileList.h"
#include "FileSystem.h"
#include "FormData.h"
#include "FormDataList.h"
#include "FormState.h"
#include "FormSubmission.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLDocument.h"
#include "HTMLFormCollection.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "MIMETypeRegistry.h"
#include "RenderTextControl.h"
#include "ScriptEventListener.h"
#include "ValidityState.h"
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

HTMLFormElement::HTMLFormElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
    , m_autocomplete(true)
    , m_insubmit(false)
    , m_doingsubmit(false)
    , m_inreset(false)
    , m_malformed(false)
    , m_demoted(false)
{
    ASSERT(hasTagName(formTag));
}

PassRefPtr<HTMLFormElement> HTMLFormElement::create(Document* document)
{
    return adoptRef(new HTMLFormElement(formTag, document));
}

PassRefPtr<HTMLFormElement> HTMLFormElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLFormElement(tagName, document));
}

HTMLFormElement::~HTMLFormElement()
{
    if (!m_autocomplete)
        document()->unregisterForDocumentActivationCallbacks(this);

    for (unsigned i = 0; i < m_associatedElements.size(); ++i)
        m_associatedElements[i]->formDestroyed();
    for (unsigned i = 0; i < m_imageElements.size(); ++i)
        m_imageElements[i]->m_form = 0;
}

bool HTMLFormElement::formWouldHaveSecureSubmission(const String& url)
{
    return document()->completeURL(url).protocolIs("https");
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

void HTMLFormElement::handleLocalEvents(Event* event)
{
    Node* targetNode = event->target()->toNode();
    if (event->eventPhase() != Event::CAPTURING_PHASE && targetNode && targetNode != this && (event->type() == eventNames().submitEvent || event->type() == eventNames().resetEvent)) {
        event->stopPropagation();
        return;
    }
    HTMLElement::handleLocalEvents(event);
}

unsigned HTMLFormElement::length() const
{
    unsigned len = 0;
    for (unsigned i = 0; i < m_associatedElements.size(); ++i)
        if (m_associatedElements[i]->isEnumeratable())
            ++len;
    return len;
}

Node* HTMLFormElement::item(unsigned index)
{
    return elements()->item(index);
}

void HTMLFormElement::submitImplicitly(Event* event, bool fromImplicitSubmissionTrigger)
{
    int submissionTriggerCount = 0;
    for (unsigned i = 0; i < m_associatedElements.size(); ++i) {
        HTMLFormControlElement* formElement = m_associatedElements[i];
        if (formElement->isSuccessfulSubmitButton()) {
            if (formElement->renderer()) {
                formElement->dispatchSimulatedClick(event);
                return;
            }
        } else if (formElement->canTriggerImplicitSubmission())
            ++submissionTriggerCount;
    }
    if (fromImplicitSubmissionTrigger && submissionTriggerCount == 1)
        prepareSubmit(event);
}

TextEncoding HTMLFormElement::dataEncoding() const
{
    if (isMailtoForm())
        return UTF8Encoding();

    return m_formDataBuilder.dataEncoding(document());
}

static void appendMailtoPostFormDataToURL(KURL& url, const FormData& data, const String& encodingType)
{
    String body = data.flattenToString();

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

PassRefPtr<FormSubmission> HTMLFormElement::prepareFormSubmission(Event* event, bool lockHistory, FormSubmissionTrigger trigger)
{
    if (m_formDataBuilder.isPostMethod()) {
        if (m_formDataBuilder.isMultiPartForm() && isMailtoForm()) {
            // FIXME: This may fire a DOM Mutation Event. Do we really want this here?
            setEnctype("application/x-www-form-urlencoded");
            ASSERT(!m_formDataBuilder.isMultiPartForm());
        }
    } else
        m_formDataBuilder.setIsMultiPartForm(false);

    RefPtr<DOMFormData> domFormData = DOMFormData::create(dataEncoding().encodingForFormSubmission());
    Vector<pair<String, String> > formValues;

    for (unsigned i = 0; i < m_associatedElements.size(); ++i) {
        HTMLFormControlElement* control = m_associatedElements[i];
        if (!control->disabled())
            control->appendFormData(*domFormData, m_formDataBuilder.isMultiPartForm());
        if (control->hasLocalName(inputTag)) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(control);
            if (input->isTextField()) {
                formValues.append(pair<String, String>(input->name(), input->value()));
                if (input->isSearchField())
                    input->addSearchResult();
            }
        }
    }

    RefPtr<FormData> formData;
    String boundary;
    // FIXME: Figure out whether m_url.isNull check is necessary.
    KURL actionURL = document()->completeURL(m_url.isNull() ? "" : m_url);

    if (m_formDataBuilder.isMultiPartForm()) {
        formData = FormData::createMultiPart(domFormData->items(), domFormData->encoding(), document());
        boundary = formData->boundary().data();
    } else {
        formData = FormData::create(domFormData->items(), domFormData->encoding());
        if (m_formDataBuilder.isPostMethod() && isMailtoForm()) {
            // Convert the form data into a string that we put into the URL.
            appendMailtoPostFormDataToURL(actionURL, *formData, m_formDataBuilder.encodingType());
            // FIXME: Why is setting m_url necessary here? This seems wrong if mail form is submitted multiple times?
            m_url = actionURL.string();

            formData = FormData::create();
        }
    }

    formData->setIdentifier(generateFormDataIdentifier());
    FormSubmission::Method method = m_formDataBuilder.isPostMethod() ? FormSubmission::PostMethod : FormSubmission::GetMethod;
    String targetOrBaseTarget = m_target.isEmpty() ? document()->baseTarget() : m_target;
    return FormSubmission::create(method, actionURL, targetOrBaseTarget, m_formDataBuilder.encodingType(), FormState::create(this, formValues, document()->frame(), trigger), formData.release(), boundary, lockHistory, event);
}

bool HTMLFormElement::isMailtoForm() const
{
    return protocolIs(m_url, "mailto");
}

static inline HTMLFormControlElement* submitElementFromEvent(const Event* event)
{
    Node* targetNode = event->target()->toNode();
    if (!targetNode || !targetNode->isElementNode())
        return 0;
    Element* targetElement = static_cast<Element*>(targetNode);
    if (!targetElement->isFormControlElement())
        return 0;
    return static_cast<HTMLFormControlElement*>(targetElement);
}

bool HTMLFormElement::validateInteractively(Event* event)
{
    ASSERT(event);
    if (!document()->inStrictMode() || noValidate())
        return true;

    HTMLFormControlElement* submitElement = submitElementFromEvent(event);
    if (submitElement && submitElement->formNoValidate())
        return true;

    Vector<RefPtr<HTMLFormControlElement> > unhandledInvalidControls;
    collectUnhandledInvalidControls(unhandledInvalidControls);
    if (unhandledInvalidControls.isEmpty())
        return true;
    // If the form has invalid controls, abort submission.

    RefPtr<HTMLFormElement> protector(this);
    // Focus on the first focusable control.
    for (unsigned i = 0; i < unhandledInvalidControls.size(); ++i) {
        HTMLFormControlElement* unhandled = unhandledInvalidControls[i].get();
        if (unhandled->isFocusable() && unhandled->inDocument()) {
            RefPtr<Document> originalDocument(unhandled->document());
            unhandled->scrollIntoViewIfNeeded(false);
            // scrollIntoViewIfNeeded() dispatches events, so the state
            // of 'unhandled' might be changed so it's no longer focusable or
            // moved to another document.
            if (unhandled->isFocusable() && unhandled->inDocument() && originalDocument == unhandled->document()) {
                unhandled->focus();
                break;
            }
        }
    }
    // Warn about all of unfocusable controls.
    if (Frame* frame = document()->frame()) {
        for (unsigned i = 0; i < unhandledInvalidControls.size(); ++i) {
            HTMLFormControlElement* unhandled = unhandledInvalidControls[i].get();
            if (unhandled->isFocusable() && unhandled->inDocument())
                continue;
            String message("An invalid form control with name='%name' is not focusable.");
            message.replace("%name", unhandled->name());
            frame->domWindow()->console()->addMessage(HTMLMessageSource, LogMessageType, ErrorMessageLevel, message, 0, document()->url().string());
        }
    }
    m_insubmit = false;
    return false;
}

bool HTMLFormElement::prepareSubmit(Event* event)
{
    Frame* frame = document()->frame();
    if (m_insubmit || !frame)
        return m_insubmit;

    m_insubmit = true;
    m_doingsubmit = false;

    // Interactive validation must be done before dispatching the submit event.
    if (!validateInteractively(event))
        return false;

    frame->loader()->client()->dispatchWillSendSubmitEvent(this);

    if (dispatchEvent(Event::create(eventNames().submitEvent, true, true)) && !m_doingsubmit)
        m_doingsubmit = true;

    m_insubmit = false;

    if (m_doingsubmit)
        submit(event, true, false, NotSubmittedByJavaScript);

    return m_doingsubmit;
}

void HTMLFormElement::submit(Frame* javaScriptActiveFrame)
{
    if (javaScriptActiveFrame)
        submit(0, false, !javaScriptActiveFrame->script()->anyPageIsProcessingUserGesture(), SubmittedByJavaScript);
    else
        submit(0, false, false, NotSubmittedByJavaScript);
}

void HTMLFormElement::submit(Event* event, bool activateSubmitButton, bool lockHistory, FormSubmissionTrigger formSubmissionTrigger)
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
    
    for (unsigned i = 0; i < m_associatedElements.size(); ++i) {
        HTMLFormControlElement* control = m_associatedElements[i];
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

    frame->loader()->submitForm(prepareFormSubmission(event, lockHistory, formSubmissionTrigger));

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
    if (!dispatchEvent(Event::create(eventNames().resetEvent, true, true))) {
        m_inreset = false;
        return;
    }

    for (unsigned i = 0; i < m_associatedElements.size(); ++i)
        m_associatedElements[i]->reset();

    m_inreset = false;
}

void HTMLFormElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == actionAttr)
        m_url = deprecatedParseURL(attr->value());
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
    return m_associatedElements.size();
}

void HTMLFormElement::registerFormElement(HTMLFormControlElement* e)
{
    document()->checkedRadioButtons().removeButton(e);
    m_checkedRadioButtons.addButton(e);
    m_associatedElements.insert(formElementIndex(e), e);
}

void HTMLFormElement::removeFormElement(HTMLFormControlElement* e)
{
    m_checkedRadioButtons.removeButton(e);
    removeFromVector(m_associatedElements, e);
}

bool HTMLFormElement::isURLAttribute(Attribute* attr) const
{
    return attr->name() == actionAttr;
}

void HTMLFormElement::registerImgElement(HTMLImageElement* e)
{
    ASSERT(m_imageElements.find(e) == notFound);
    m_imageElements.append(e);
}

void HTMLFormElement::removeImgElement(HTMLImageElement* e)
{
    ASSERT(m_imageElements.find(e) != notFound);
    removeFromVector(m_imageElements, e);
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

bool HTMLFormElement::noValidate() const
{
    return !getAttribute(novalidateAttr).isNull();
}

void HTMLFormElement::setNoValidate(bool novalidate)
{
    setAttribute(novalidateAttr, novalidate ? "" : 0);
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

HTMLFormControlElement* HTMLFormElement::defaultButton() const
{
    for (unsigned i = 0; i < m_associatedElements.size(); ++i) {
        HTMLFormControlElement* control = m_associatedElements[i];
        if (control->isSuccessfulSubmitButton())
            return control;
    }

    return 0;
}

bool HTMLFormElement::checkValidity()
{
    Vector<RefPtr<HTMLFormControlElement> > controls;
    collectUnhandledInvalidControls(controls);
    return controls.isEmpty();
}

void HTMLFormElement::collectUnhandledInvalidControls(Vector<RefPtr<HTMLFormControlElement> >& unhandledInvalidControls)
{
    RefPtr<HTMLFormElement> protector(this);
    // Copy m_associatedElements because event handlers called from
    // HTMLFormControlElement::checkValidity() might change m_associatedElements.
    Vector<RefPtr<HTMLFormControlElement> > elements;
    elements.reserveCapacity(m_associatedElements.size());
    for (unsigned i = 0; i < m_associatedElements.size(); ++i)
        elements.append(m_associatedElements[i]);
    for (unsigned i = 0; i < elements.size(); ++i) {
        if (elements[i]->form() == this)
            elements[i]->checkValidity(&unhandledInvalidControls);
    }
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
        m_elementAliases.set(new AliasMap);
    m_elementAliases->set(alias.impl(), element);
}

void HTMLFormElement::getNamedElements(const AtomicString& name, Vector<RefPtr<Node> >& namedItems)
{
    elements()->namedItems(name, namedItems);

    // see if we have seen something with this name before
    RefPtr<HTMLFormControlElement> aliasElem;
    if ((aliasElem = elementForAlias(name))) {
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
    
    for (unsigned i = 0; i < m_associatedElements.size(); ++i)
        m_associatedElements[i]->reset();
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
