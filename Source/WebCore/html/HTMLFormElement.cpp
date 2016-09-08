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

#include "AutocompleteErrorEvent.h"
#include "DOMFormData.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ElementIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "FormController.h"
#include "FormData.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLTableElement.h"
#include "NodeRareData.h"
#include "Page.h"
#include "RenderTextControl.h"
#include "ScriptController.h"
#include "Settings.h"
#include <limits>
#include <wtf/Ref.h>

namespace WebCore {

using namespace HTMLNames;

HTMLFormElement::HTMLFormElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_associatedElementsBeforeIndex(0)
    , m_associatedElementsAfterIndex(0)
    , m_wasUserSubmitted(false)
    , m_isSubmittingOrPreparingForSubmission(false)
    , m_shouldSubmit(false)
    , m_isInResetFunction(false)
    , m_wasDemoted(false)
#if ENABLE(REQUEST_AUTOCOMPLETE)
    , m_requestAutocompletetimer(*this, &HTMLFormElement::requestAutocompleteTimerFired)
#endif
{
    ASSERT(hasTagName(formTag));
}

Ref<HTMLFormElement> HTMLFormElement::create(Document& document)
{
    return adoptRef(*new HTMLFormElement(formTag, document));
}

Ref<HTMLFormElement> HTMLFormElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLFormElement(tagName, document));
}

HTMLFormElement::~HTMLFormElement()
{
    document().formController().willDeleteForm(this);
    if (!shouldAutocomplete())
        document().unregisterForDocumentSuspensionCallbacks(this);

    m_defaultButton = nullptr;
    for (auto& associatedElement : m_associatedElements)
        associatedElement->formWillBeDestroyed();
    for (auto& imageElement : m_imageElements)
        imageElement->m_form = nullptr;
}

bool HTMLFormElement::formWouldHaveSecureSubmission(const String& url)
{
    return document().completeURL(url).protocolIs("https");
}

bool HTMLFormElement::rendererIsNeeded(const RenderStyle& style)
{
    if (!m_wasDemoted)
        return HTMLElement::rendererIsNeeded(style);

    auto parent = parentNode();
    auto parentRenderer = parent->renderer();

    if (!parentRenderer)
        return false;

    // FIXME: Shouldn't we also check for table caption (see |formIsTablePart| below).
    bool parentIsTableElementPart = (parentRenderer->isTable() && is<HTMLTableElement>(*parent))
        || (parentRenderer->isTableRow() && parent->hasTagName(trTag))
        || (parentRenderer->isTableSection() && parent->hasTagName(tbodyTag))
        || (parentRenderer->isRenderTableCol() && parent->hasTagName(colTag))
        || (parentRenderer->isTableCell() && parent->hasTagName(trTag));

    if (!parentIsTableElementPart)
        return true;

    EDisplay display = style.display();
    bool formIsTablePart = display == TABLE || display == INLINE_TABLE || display == TABLE_ROW_GROUP
        || display == TABLE_HEADER_GROUP || display == TABLE_FOOTER_GROUP || display == TABLE_ROW
        || display == TABLE_COLUMN_GROUP || display == TABLE_COLUMN || display == TABLE_CELL
        || display == TABLE_CAPTION;

    return formIsTablePart;
}

Node::InsertionNotificationRequest HTMLFormElement::insertedInto(ContainerNode& insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint.inDocument())
        document().didAssociateFormControl(this);
    return InsertionDone;
}

static inline Node* findRoot(Node* n)
{
    Node* root = n;
    for (; n; n = n->parentNode())
        root = n;
    return root;
}

void HTMLFormElement::removedFrom(ContainerNode& insertionPoint)
{
    Node* root = findRoot(this);
    Vector<FormAssociatedElement*> associatedElements(m_associatedElements);
    for (auto& associatedElement : associatedElements)
        associatedElement->formRemovedFromTree(root);
    HTMLElement::removedFrom(insertionPoint);
}

void HTMLFormElement::handleLocalEvents(Event& event)
{
    Node* targetNode = event.target()->toNode();
    if (event.eventPhase() != Event::CAPTURING_PHASE && targetNode && targetNode != this && (event.type() == eventNames().submitEvent || event.type() == eventNames().resetEvent)) {
        event.stopPropagation();
        return;
    }
    HTMLElement::handleLocalEvents(event);
}

unsigned HTMLFormElement::length() const
{
    unsigned len = 0;
    for (auto& associatedElement : m_associatedElements) {
        if (associatedElement->isEnumeratable())
            ++len;
    }
    return len;
}

HTMLElement* HTMLFormElement::item(unsigned index)
{
    return elements()->item(index);
}

void HTMLFormElement::submitImplicitly(Event& event, bool fromImplicitSubmissionTrigger)
{
    unsigned submissionTriggerCount = 0;
    for (auto& formAssociatedElement : m_associatedElements) {
        if (!is<HTMLFormControlElement>(*formAssociatedElement))
            continue;
        HTMLFormControlElement& formElement = downcast<HTMLFormControlElement>(*formAssociatedElement);
        if (formElement.isSuccessfulSubmitButton()) {
            if (formElement.renderer()) {
                formElement.dispatchSimulatedClick(&event);
                return;
            }
        } else if (formElement.canTriggerImplicitSubmission())
            ++submissionTriggerCount;
    }

    if (!submissionTriggerCount)
        return;

    // Older iOS apps using WebViews expect the behavior of auto submitting multi-input forms.
    Settings* settings = document().settings();
    if (fromImplicitSubmissionTrigger && (submissionTriggerCount == 1 || (settings && settings->allowMultiElementImplicitSubmission())))
        prepareForSubmission(event);
}

static inline HTMLFormControlElement* submitElementFromEvent(const Event& event)
{
    for (Node* node = event.target()->toNode(); node; node = node->parentNode()) {
        if (is<HTMLFormControlElement>(*node))
            return downcast<HTMLFormControlElement>(node);
    }
    return nullptr;
}

bool HTMLFormElement::validateInteractively(Event& event)
{
    if (!document().page() || !document().page()->settings().interactiveFormValidationEnabled() || noValidate())
        return true;

    HTMLFormControlElement* submitElement = submitElementFromEvent(event);
    if (submitElement && submitElement->formNoValidate())
        return true;

    for (auto& associatedElement : m_associatedElements) {
        if (is<HTMLFormControlElement>(*associatedElement))
            downcast<HTMLFormControlElement>(*associatedElement).hideVisibleValidationMessage();
    }

    Vector<RefPtr<FormAssociatedElement>> unhandledInvalidControls;
    if (!checkInvalidControlsAndCollectUnhandled(unhandledInvalidControls))
        return true;
    // Because the form has invalid controls, we abort the form submission and
    // show a validation message on a focusable form control.

    // Needs to update layout now because we'd like to call isFocusable(), which
    // has !renderer()->needsLayout() assertion.
    document().updateLayoutIgnorePendingStylesheets();

    Ref<HTMLFormElement> protectedThis(*this);

    // Focus on the first focusable control and show a validation message.
    for (auto& control : unhandledInvalidControls) {
        HTMLElement& element = control->asHTMLElement();
        if (element.inDocument() && element.isFocusable()) {
            element.scrollIntoViewIfNeeded(false);
            element.focus();
            if (is<HTMLFormControlElement>(element))
                downcast<HTMLFormControlElement>(element).updateVisibleValidationMessage();
            break;
        }
    }

    // Warn about all of unfocusable controls.
    if (document().frame()) {
        for (auto& control : unhandledInvalidControls) {
            HTMLElement& element = control->asHTMLElement();
            if (element.inDocument() && element.isFocusable())
                continue;
            String message("An invalid form control with name='%name' is not focusable.");
            message.replace("%name", control->name());
            document().addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, message);
        }
    }

    return false;
}

void HTMLFormElement::prepareForSubmission(Event& event)
{
    Frame* frame = document().frame();
    if (m_isSubmittingOrPreparingForSubmission || !frame)
        return;

    m_isSubmittingOrPreparingForSubmission = true;
    m_shouldSubmit = false;

    // Interactive validation must be done before dispatching the submit event.
    if (!validateInteractively(event)) {
        m_isSubmittingOrPreparingForSubmission = false;
        return;
    }

    StringPairVector controlNamesAndValues;
    getTextFieldValues(controlNamesAndValues);
    auto formState = FormState::create(this, controlNamesAndValues, &document(), NotSubmittedByJavaScript);
    frame->loader().client().dispatchWillSendSubmitEvent(WTFMove(formState));

    Ref<HTMLFormElement> protectedThis(*this);
    // Event handling can result in m_shouldSubmit becoming true, regardless of dispatchEvent() return value.
    if (dispatchEvent(Event::create(eventNames().submitEvent, true, true)))
        m_shouldSubmit = true;

    m_isSubmittingOrPreparingForSubmission = false;

    if (m_shouldSubmit)
        submit(&event, true, true, NotSubmittedByJavaScript);
}

void HTMLFormElement::submit()
{
    submit(0, false, true, NotSubmittedByJavaScript);
}

void HTMLFormElement::submitFromJavaScript()
{
    submit(0, false, ScriptController::processingUserGesture(), SubmittedByJavaScript);
}

void HTMLFormElement::getTextFieldValues(StringPairVector& fieldNamesAndValues) const
{
    ASSERT_ARG(fieldNamesAndValues, fieldNamesAndValues.isEmpty());

    fieldNamesAndValues.reserveCapacity(m_associatedElements.size());
    for (auto& associatedElement : m_associatedElements) {
        HTMLElement& element = associatedElement->asHTMLElement();
        if (!is<HTMLInputElement>(element))
            continue;
        HTMLInputElement& input = downcast<HTMLInputElement>(element);
        if (!input.isTextField())
            continue;
        fieldNamesAndValues.append(std::make_pair(input.name().string(), input.value()));
    }
}

void HTMLFormElement::submit(Event* event, bool activateSubmitButton, bool processingUserGesture, FormSubmissionTrigger formSubmissionTrigger)
{
    FrameView* view = document().view();
    Frame* frame = document().frame();
    if (!view || !frame)
        return;

    if (m_isSubmittingOrPreparingForSubmission) {
        m_shouldSubmit = true;
        return;
    }

    m_isSubmittingOrPreparingForSubmission = true;
    m_wasUserSubmitted = processingUserGesture;

    RefPtr<HTMLFormControlElement> firstSuccessfulSubmitButton;
    bool needButtonActivation = activateSubmitButton; // do we need to activate a submit button?

    for (auto& associatedElement : m_associatedElements) {
        if (!is<HTMLFormControlElement>(*associatedElement))
            continue;
        if (needButtonActivation) {
            HTMLFormControlElement& control = downcast<HTMLFormControlElement>(*associatedElement);
            if (control.isActivatedSubmit())
                needButtonActivation = false;
            else if (!firstSuccessfulSubmitButton && control.isSuccessfulSubmitButton())
                firstSuccessfulSubmitButton = &control;
        }
    }

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(true);

    LockHistory lockHistory = processingUserGesture ? LockHistory::No : LockHistory::Yes;
    Ref<HTMLFormElement> protectedThis(*this); // Form submission can execute arbitary JavaScript.
    frame->loader().submitForm(FormSubmission::create(this, m_attributes, event, lockHistory, formSubmissionTrigger));

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(false);

    m_shouldSubmit = false;
    m_isSubmittingOrPreparingForSubmission = false;
}

void HTMLFormElement::reset()
{
    Frame* frame = document().frame();
    if (m_isInResetFunction || !frame)
        return;

    m_isInResetFunction = true;

    if (!dispatchEvent(Event::create(eventNames().resetEvent, true, true))) {
        m_isInResetFunction = false;
        return;
    }

    for (auto& associatedElement : m_associatedElements) {
        if (is<HTMLFormControlElement>(*associatedElement))
            downcast<HTMLFormControlElement>(*associatedElement).reset();
    }

    m_isInResetFunction = false;
}

#if ENABLE(IOS_AUTOCORRECT_AND_AUTOCAPITALIZE)

// FIXME: We should look to share this code with class HTMLFormControlElement instead of duplicating the logic.

bool HTMLFormElement::autocorrect() const
{
    const AtomicString& autocorrectValue = attributeWithoutSynchronization(autocorrectAttr);
    if (!autocorrectValue.isEmpty())
        return !equalLettersIgnoringASCIICase(autocorrectValue, "off");
    if (HTMLFormElement* form = this->form())
        return form->autocorrect();
    return true;
}

void HTMLFormElement::setAutocorrect(bool autocorrect)
{
    setAttributeWithoutSynchronization(autocorrectAttr, autocorrect ? AtomicString("on", AtomicString::ConstructFromLiteral) : AtomicString("off", AtomicString::ConstructFromLiteral));
}

WebAutocapitalizeType HTMLFormElement::autocapitalizeType() const
{
    return autocapitalizeTypeForAttributeValue(attributeWithoutSynchronization(autocapitalizeAttr));
}

const AtomicString& HTMLFormElement::autocapitalize() const
{
    return stringForAutocapitalizeType(autocapitalizeType());
}

void HTMLFormElement::setAutocapitalize(const AtomicString& value)
{
    setAttributeWithoutSynchronization(autocapitalizeAttr, value);
}

#endif

#if ENABLE(REQUEST_AUTOCOMPLETE)

void HTMLFormElement::requestAutocomplete()
{
    Frame* frame = document().frame();
    if (!frame)
        return;

    if (!shouldAutocomplete() || !ScriptController::processingUserGesture()) {
        finishRequestAutocomplete(AutocompleteResult::ErrorDisabled);
        return;
    }

    StringPairVector controlNamesAndValues;
    getTextFieldValues(controlNamesAndValues);

    RefPtr<FormState> formState = FormState::create(this, controlNamesAndValues, &document(), SubmittedByJavaScript);
    frame->loader().client().didRequestAutocomplete(formState.release());
}

void HTMLFormElement::finishRequestAutocomplete(AutocompleteResult result)
{
    RefPtr<Event> event;
    switch (result) {
    case AutocompleteResult::Success:
        event = Event::create(eventNames().autocompleteEvent, false, false);
        break;
    case AutocompleteResult::ErrorDisabled:
        event = AutocompleteErrorEvent::create("disabled");
        break;
    case AutocompleteResult::ErrorCancel:
        event = AutocompleteErrorEvent::create("cancel");
        break;
    case AutocompleteResult::ErrorInvalid:
        event = AutocompleteErrorEvent::create("invalid");
        break;
    }

    event->setTarget(this);
    m_pendingAutocompleteEvents.append(WTFMove(event));

    // Dispatch events later as this API is meant to work asynchronously in all situations and implementations.
    if (!m_requestAutocompleteTimer.isActive())
        m_requestAutocompleteTimer.startOneShot(0);
}

void HTMLFormElement::requestAutocompleteTimerFired()
{
    Vector<RefPtr<Event>> pendingEvents;
    m_pendingAutocompleteEvents.swap(pendingEvents);
    for (auto& pendingEvent : pendingEvents)
        dispatchEvent(pendingEvent.release());
}

#endif

void HTMLFormElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == actionAttr) {
        m_attributes.parseAction(value);
        
        if (!m_attributes.action().isEmpty()) {
            if (Frame* f = document().frame()) {
                Frame& topFrame = f->tree().top();
                topFrame.loader().mixedContentChecker().checkFormForMixedContent(topFrame.document()->securityOrigin(), document().completeURL(m_attributes.action()));
            }
        }
    } else if (name == targetAttr)
        m_attributes.setTarget(value);
    else if (name == methodAttr)
        m_attributes.updateMethodType(value);
    else if (name == enctypeAttr)
        m_attributes.updateEncodingType(value);
    else if (name == accept_charsetAttr)
        m_attributes.setAcceptCharset(value);
    else if (name == autocompleteAttr) {
        if (!shouldAutocomplete())
            document().registerForDocumentSuspensionCallbacks(this);
        else
            document().unregisterForDocumentSuspensionCallbacks(this);
    } else
        HTMLElement::parseAttribute(name, value);
}

unsigned HTMLFormElement::formElementIndexWithFormAttribute(Element* element, unsigned rangeStart, unsigned rangeEnd)
{
    if (m_associatedElements.isEmpty())
        return 0;

    ASSERT(rangeStart <= rangeEnd);

    if (rangeStart == rangeEnd)
        return rangeStart;

    unsigned left = rangeStart;
    unsigned right = rangeEnd - 1;
    unsigned short position;

    // Does binary search on m_associatedElements in order to find the index
    // to be inserted.
    while (left != right) {
        unsigned middle = left + ((right - left) / 2);
        ASSERT(middle < m_associatedElementsBeforeIndex || middle >= m_associatedElementsAfterIndex);
        position = element->compareDocumentPosition(m_associatedElements[middle]->asHTMLElement());
        if (position & DOCUMENT_POSITION_FOLLOWING)
            right = middle;
        else
            left = middle + 1;
    }
    
    ASSERT(left < m_associatedElementsBeforeIndex || left >= m_associatedElementsAfterIndex);
    position = element->compareDocumentPosition(m_associatedElements[left]->asHTMLElement());
    if (position & DOCUMENT_POSITION_FOLLOWING)
        return left;
    return left + 1;
}

unsigned HTMLFormElement::formElementIndex(FormAssociatedElement* associatedElement)
{
    ASSERT(associatedElement);

    HTMLElement& associatedHTMLElement = associatedElement->asHTMLElement();

    // Treats separately the case where this element has the form attribute
    // for performance consideration.
    if (associatedHTMLElement.hasAttributeWithoutSynchronization(formAttr)) {
        unsigned short position = compareDocumentPosition(associatedHTMLElement);
        if (position & DOCUMENT_POSITION_PRECEDING) {
            ++m_associatedElementsBeforeIndex;
            ++m_associatedElementsAfterIndex;
            return HTMLFormElement::formElementIndexWithFormAttribute(&associatedHTMLElement, 0, m_associatedElementsBeforeIndex - 1);
        }
        if (position & DOCUMENT_POSITION_FOLLOWING && !(position & DOCUMENT_POSITION_CONTAINED_BY))
            return HTMLFormElement::formElementIndexWithFormAttribute(&associatedHTMLElement, m_associatedElementsAfterIndex, m_associatedElements.size());
    }

    unsigned currentAssociatedElementsAfterIndex = m_associatedElementsAfterIndex;
    ++m_associatedElementsAfterIndex;

    if (!associatedHTMLElement.isDescendantOf(this))
        return currentAssociatedElementsAfterIndex;

    // Check for the special case where this element is the very last thing in
    // the form's tree of children; we don't want to walk the entire tree in that
    // common case that occurs during parsing; instead we'll just return a value
    // that says "add this form element to the end of the array".
    auto descendants = descendantsOfType<HTMLElement>(*this);
    auto it = descendants.beginAt(associatedHTMLElement);
    auto end = descendants.end();
    if (++it == end)
        return currentAssociatedElementsAfterIndex;

    unsigned i = m_associatedElementsBeforeIndex;
    for (auto& element : descendants) {
        if (&element == &associatedHTMLElement)
            return i;
        if (!is<HTMLFormControlElement>(element) && !is<HTMLObjectElement>(element))
            continue;
        if (element.form() != this)
            continue;
        ++i;
    }
    return currentAssociatedElementsAfterIndex;
}

void HTMLFormElement::registerFormElement(FormAssociatedElement* e)
{
    m_associatedElements.insert(formElementIndex(e), e);

    if (is<HTMLFormControlElement>(e)) {
        HTMLFormControlElement& control = downcast<HTMLFormControlElement>(*e);
        if (control.isSuccessfulSubmitButton()) {
            if (!m_defaultButton)
                control.setNeedsStyleRecalc();
            else
                resetDefaultButton();
        }
    }
}

void HTMLFormElement::removeFormElement(FormAssociatedElement* e)
{
    unsigned index = m_associatedElements.find(e);
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_associatedElements.size());
    if (index < m_associatedElementsBeforeIndex)
        --m_associatedElementsBeforeIndex;
    if (index < m_associatedElementsAfterIndex)
        --m_associatedElementsAfterIndex;
    removeFromPastNamesMap(e);
    m_associatedElements.remove(index);

    if (e == m_defaultButton)
        resetDefaultButton();
}

void HTMLFormElement::registerInvalidAssociatedFormControl(const HTMLFormControlElement& formControlElement)
{
    ASSERT_WITH_MESSAGE(!is<HTMLFieldSetElement>(formControlElement), "FieldSet are never candidates for constraint validation.");
    ASSERT(static_cast<const Element&>(formControlElement).matchesInvalidPseudoClass());

    if (m_invalidAssociatedFormControls.isEmpty())
        setNeedsStyleRecalc();
    m_invalidAssociatedFormControls.add(&formControlElement);
}

void HTMLFormElement::removeInvalidAssociatedFormControlIfNeeded(const HTMLFormControlElement& formControlElement)
{
    if (m_invalidAssociatedFormControls.remove(&formControlElement)) {
        if (m_invalidAssociatedFormControls.isEmpty())
            setNeedsStyleRecalc();
    }
}

bool HTMLFormElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == actionAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLFormElement::registerImgElement(HTMLImageElement* e)
{
    ASSERT(m_imageElements.find(e) == notFound);
    m_imageElements.append(e);
}

void HTMLFormElement::removeImgElement(HTMLImageElement* e)
{
    removeFromPastNamesMap(e);
    bool removed = m_imageElements.removeFirst(e);
    ASSERT_UNUSED(removed, removed);
}

Ref<HTMLFormControlsCollection> HTMLFormElement::elements()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLFormControlsCollection>(*this, FormControls);
}

Ref<HTMLCollection> HTMLFormElement::elementsForNativeBindings()
{
    return elements();
}

String HTMLFormElement::name() const
{
    return getNameAttribute();
}

bool HTMLFormElement::noValidate() const
{
    return hasAttributeWithoutSynchronization(novalidateAttr);
}

// FIXME: This function should be removed because it does not do the same thing as the
// JavaScript binding for action, which treats action as a URL attribute. Last time I
// (Darin Adler) removed this, someone added it back, so I am leaving it in for now.
String HTMLFormElement::action() const
{
    return attributeWithoutSynchronization(actionAttr);
}

void HTMLFormElement::setAction(const String &value)
{
    setAttributeWithoutSynchronization(actionAttr, value);
}

void HTMLFormElement::setEnctype(const String &value)
{
    setAttributeWithoutSynchronization(enctypeAttr, value);
}

String HTMLFormElement::method() const
{
    return FormSubmission::Attributes::methodString(m_attributes.method());
}

void HTMLFormElement::setMethod(const String &value)
{
    setAttributeWithoutSynchronization(methodAttr, value);
}

String HTMLFormElement::target() const
{
    return attributeWithoutSynchronization(targetAttr);
}

bool HTMLFormElement::wasUserSubmitted() const
{
    return m_wasUserSubmitted;
}

HTMLFormControlElement* HTMLFormElement::defaultButton() const
{
    if (!m_defaultButton) {
        for (auto& associatedElement : m_associatedElements) {
            if (!is<HTMLFormControlElement>(*associatedElement))
                continue;
            HTMLFormControlElement& control = downcast<HTMLFormControlElement>(*associatedElement);
            if (control.isSuccessfulSubmitButton()) {
                m_defaultButton = &control;
                break;
            }
        }
    }
    return m_defaultButton;
}

void HTMLFormElement::resetDefaultButton()
{
    if (!m_defaultButton) {
        // Computing the default button is not cheap, we don't want to do it unless needed.
        // If there was no default button set, the only style to invalidate is the element
        // being added to the form. This is done explicitely in registerFormElement().
        return;
    }

    HTMLFormControlElement* oldDefault = m_defaultButton;
    m_defaultButton = nullptr;
    defaultButton();
    if (m_defaultButton != oldDefault) {
        if (oldDefault)
            oldDefault->setNeedsStyleRecalc();
        if (m_defaultButton)
            m_defaultButton->setNeedsStyleRecalc();
    }
}

bool HTMLFormElement::checkValidity()
{
    Vector<RefPtr<FormAssociatedElement>> controls;
    return !checkInvalidControlsAndCollectUnhandled(controls);
}

bool HTMLFormElement::checkInvalidControlsAndCollectUnhandled(Vector<RefPtr<FormAssociatedElement>>& unhandledInvalidControls)
{
    Ref<HTMLFormElement> protectedThis(*this);
    // Copy m_associatedElements because event handlers called from
    // HTMLFormControlElement::checkValidity() might change m_associatedElements.
    Vector<RefPtr<FormAssociatedElement>> elements;
    elements.reserveCapacity(m_associatedElements.size());
    for (auto& associatedElement : m_associatedElements)
        elements.append(associatedElement);
    bool hasInvalidControls = false;
    for (auto& element : elements) {
        if (element->form() == this && is<HTMLFormControlElement>(*element)) {
            HTMLFormControlElement& control = downcast<HTMLFormControlElement>(*element);
            if (!control.checkValidity(&unhandledInvalidControls) && control.form() == this)
                hasInvalidControls = true;
        }
    }
    return hasInvalidControls;
}

#ifndef NDEBUG
void HTMLFormElement::assertItemCanBeInPastNamesMap(FormNamedItem* item) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(item);
    HTMLElement& element = item->asHTMLElement();
    ASSERT_WITH_SECURITY_IMPLICATION(element.form() == this);

    if (item->isFormAssociatedElement()) {
        ASSERT_WITH_SECURITY_IMPLICATION(m_associatedElements.find(static_cast<FormAssociatedElement*>(item)) != notFound);
        return;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(element.hasTagName(imgTag));
    ASSERT_WITH_SECURITY_IMPLICATION(m_imageElements.find(&downcast<HTMLImageElement>(element)) != notFound);
}
#else
inline void HTMLFormElement::assertItemCanBeInPastNamesMap(FormNamedItem*) const
{
}
#endif

HTMLElement* HTMLFormElement::elementFromPastNamesMap(const AtomicString& pastName) const
{
    if (pastName.isEmpty() || !m_pastNamesMap)
        return nullptr;
    FormNamedItem* item = m_pastNamesMap->get(pastName.impl());
    if (!item)
        return nullptr;
    assertItemCanBeInPastNamesMap(item);
    return &item->asHTMLElement();
}

void HTMLFormElement::addToPastNamesMap(FormNamedItem* item, const AtomicString& pastName)
{
    assertItemCanBeInPastNamesMap(item);
    if (pastName.isEmpty())
        return;
    if (!m_pastNamesMap)
        m_pastNamesMap = std::make_unique<PastNamesMap>();
    m_pastNamesMap->set(pastName.impl(), item);
}

void HTMLFormElement::removeFromPastNamesMap(FormNamedItem* item)
{
    ASSERT(item);
    if (!m_pastNamesMap)
        return;

    for (auto& pastName : m_pastNamesMap->values()) {
        if (pastName == item)
            pastName = nullptr; // Keep looping. Single element can have multiple names.
    }
}

bool HTMLFormElement::matchesValidPseudoClass() const
{
    return m_invalidAssociatedFormControls.isEmpty();
}

bool HTMLFormElement::matchesInvalidPseudoClass() const
{
    return !m_invalidAssociatedFormControls.isEmpty();
}

// FIXME: Use Ref<HTMLElement> for the function result since there are no non-HTML elements returned here.
Vector<Ref<Element>> HTMLFormElement::namedElements(const AtomicString& name)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/forms.html#dom-form-nameditem
    Vector<Ref<Element>> namedItems = elements()->namedItems(name);

    HTMLElement* elementFromPast = elementFromPastNamesMap(name);
    if (namedItems.size() == 1 && namedItems.first().ptr() != elementFromPast)
        addToPastNamesMap(downcast<HTMLElement>(namedItems.first().get()).asFormNamedItem(), name);
    else if (elementFromPast && namedItems.isEmpty())
        namedItems.append(*elementFromPast);

    return namedItems;
}

void HTMLFormElement::resumeFromDocumentSuspension()
{
    ASSERT(!shouldAutocomplete());

    for (auto& associatedElement : m_associatedElements) {
        if (is<HTMLFormControlElement>(*associatedElement))
            downcast<HTMLFormControlElement>(*associatedElement).reset();
    }
}

void HTMLFormElement::didMoveToNewDocument(Document* oldDocument)
{
    if (!shouldAutocomplete()) {
        if (oldDocument)
            oldDocument->unregisterForDocumentSuspensionCallbacks(this);
        document().registerForDocumentSuspensionCallbacks(this);
    }

    HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLFormElement::shouldAutocomplete() const
{
    return !equalLettersIgnoringASCIICase(attributeWithoutSynchronization(autocompleteAttr), "off");
}

void HTMLFormElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    document().formController().restoreControlStateIn(*this);
}

void HTMLFormElement::copyNonAttributePropertiesFromElement(const Element& source)
{
    m_wasDemoted = static_cast<const HTMLFormElement&>(source).m_wasDemoted;
    HTMLElement::copyNonAttributePropertiesFromElement(source);
}

HTMLFormElement* HTMLFormElement::findClosestFormAncestor(const Element& startElement)
{
    return const_cast<HTMLFormElement*>(ancestorsOfType<HTMLFormElement>(startElement).first());
}

void HTMLFormElement::setAutocomplete(const AtomicString& value)
{
    setAttributeWithoutSynchronization(autocompleteAttr, value);
}

const AtomicString& HTMLFormElement::autocomplete() const
{
    static NeverDestroyed<AtomicString> on("on", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> off("off", AtomicString::ConstructFromLiteral);

    return equalIgnoringASCIICase(attributeWithoutSynchronization(autocompleteAttr), "off") ? off : on;
}

} // namespace
