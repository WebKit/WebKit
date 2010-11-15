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
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLDocument.h"
#include "HTMLFormCollection.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "RenderTextControl.h"
#include "ScriptEventListener.h"
#include "Settings.h"
#include "ValidityState.h"
#include <limits>

#if PLATFORM(WX)
#include <wx/defs.h>
#include <wx/filename.h>
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

HTMLFormElement::HTMLFormElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
    , m_associatedElementsBeforeIndex(0)
    , m_associatedElementsAfterIndex(0)
    , m_wasUserSubmitted(false)
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

    ContainerNode* node = parentNode();
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

    if (hasID())
        document()->resetFormElementsOwner(this);
}

void HTMLFormElement::removedFromDocument()
{
    if (document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->removeNamedItem(m_name);

    HTMLElement::removedFromDocument();

    if (hasID())
        document()->resetFormElementsOwner(0);
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
    if (!document()->page() || !document()->page()->settings()->interactiveFormValidationEnabled() || noValidate())
        return true;

    HTMLFormControlElement* submitElement = submitElementFromEvent(event);
    if (submitElement && submitElement->formNoValidate())
        return true;

    for (unsigned i = 0; i < m_associatedElements.size(); ++i)
        m_associatedElements[i]->hideVisibleValidationMessage();

    Vector<RefPtr<HTMLFormControlElement> > unhandledInvalidControls;
    collectUnhandledInvalidControls(unhandledInvalidControls);
    if (unhandledInvalidControls.isEmpty())
        return true;
    // If the form has invalid controls, abort submission.

    RefPtr<HTMLFormElement> protector(this);
    // Focus on the first focusable control and show a validation message.
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
                unhandled->updateVisibleValidationMessage();
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
        submit(event, true, true, NotSubmittedByJavaScript);

    return m_doingsubmit;
}

void HTMLFormElement::submit(Frame* javaScriptActiveFrame)
{
    if (javaScriptActiveFrame)
        submit(0, false, javaScriptActiveFrame->script()->anyPageIsProcessingUserGesture(), SubmittedByJavaScript);
    else
        submit(0, false, true, NotSubmittedByJavaScript);
}

void HTMLFormElement::submit(Event* event, bool activateSubmitButton, bool processingUserGesture, FormSubmissionTrigger formSubmissionTrigger)
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
    m_wasUserSubmitted = processingUserGesture;

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

    frame->loader()->submitForm(FormSubmission::create(this, m_attributes, event, !processingUserGesture, formSubmissionTrigger));

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
        m_attributes.parseAction(attr->value());
    else if (attr->name() == targetAttr)
        m_attributes.setTarget(attr->value());
    else if (attr->name() == methodAttr)
        m_attributes.parseMethodType(attr->value());
    else if (attr->name() == enctypeAttr)
        m_attributes.parseEncodingType(attr->value());
    else if (attr->name() == accept_charsetAttr)
        // space separated list of charsets the server
        // accepts - see rfc2045
        m_attributes.setAcceptCharset(attr->value());
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

unsigned HTMLFormElement::formElementIndexWithFormAttribute(HTMLFormControlElement* element)
{
    // Compares the position of the form element and the inserted element.
    // Updates the indeces in order to the relation of the position:
    unsigned short position = compareDocumentPosition(element);
    if (position & DOCUMENT_POSITION_CONTAINS)
        ++m_associatedElementsAfterIndex;
    else if (position & DOCUMENT_POSITION_PRECEDING) {
        ++m_associatedElementsBeforeIndex;
        ++m_associatedElementsAfterIndex;
    }

    if (m_associatedElements.isEmpty())
        return 0;

    // Does binary search on m_associatedElements in order to find the index
    // to be inserted.
    unsigned left = 0, right = m_associatedElements.size() - 1;
    while (left != right) {
        unsigned middle = left + ((right - left) / 2);
        position = element->compareDocumentPosition(m_associatedElements[middle]);
        if (position & DOCUMENT_POSITION_FOLLOWING)
            right = middle;
        else
            left = middle + 1;
    }

    position = element->compareDocumentPosition(m_associatedElements[left]);
    if (position & DOCUMENT_POSITION_FOLLOWING)
        return left;
    return left + 1;
}

unsigned HTMLFormElement::formElementIndex(HTMLFormControlElement* e)
{
    // Treats separately the case where this element has the form attribute
    // for performance consideration.
    if (e->fastHasAttribute(formAttr))
        return formElementIndexWithFormAttribute(e);

    // Check for the special case where this element is the very last thing in
    // the form's tree of children; we don't want to walk the entire tree in that
    // common case that occurs during parsing; instead we'll just return a value
    // that says "add this form element to the end of the array".
    if (e->traverseNextNode(this)) {
        unsigned i = m_associatedElementsBeforeIndex;
        for (Node* node = this; node; node = node->traverseNextNode(this)) {
            if (node == e) {
                ++m_associatedElementsAfterIndex;
                return i;
            }
            if (node->isHTMLElement()
                    && static_cast<Element*>(node)->isFormControlElement()
                    && static_cast<HTMLFormControlElement*>(node)->form() == this)
                ++i;
        }
    }
    return m_associatedElementsAfterIndex++;
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
    if (e->fastHasAttribute(formAttr)) {
        unsigned index;
        for (index = 0; index < m_associatedElements.size(); ++index)
            if (m_associatedElements[index] == e)
                break;
        ASSERT(index < m_associatedElements.size());
        if (index < m_associatedElementsBeforeIndex)
            --m_associatedElementsBeforeIndex;
        if (index < m_associatedElementsAfterIndex)
            --m_associatedElementsAfterIndex;
    } else
        --m_associatedElementsAfterIndex;
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

bool HTMLFormElement::noValidate() const
{
    return !getAttribute(novalidateAttr).isNull();
}

// FIXME: This function should be removed because it does not do the same thing as the
// JavaScript binding for action, which treats action as a URL attribute. Last time I
// (Darin Adler) removed this, someone added it back, so I am leaving it in for now.
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

bool HTMLFormElement::wasUserSubmitted() const
{
    return m_wasUserSubmitted;
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
        m_elementAliases = adoptPtr(new AliasMap);
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
