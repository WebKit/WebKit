/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2010, 2012-2016 Apple Inc. All rights reserved.
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
#include "RadioNodeList.h"
#include "RenderTextControl.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include <limits>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLFormElement);

using namespace HTMLNames;

HTMLFormElement::HTMLFormElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
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
    document().formController().willDeleteForm(*this);
    if (!shouldAutocomplete())
        document().unregisterForDocumentSuspensionCallbacks(*this);

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

    DisplayType display = style.display();
    bool formIsTablePart = display == DisplayType::Table || display == DisplayType::InlineTable || display == DisplayType::TableRowGroup
        || display == DisplayType::TableHeaderGroup || display == DisplayType::TableFooterGroup || display == DisplayType::TableRow
        || display == DisplayType::TableColumnGroup || display == DisplayType::TableColumn || display == DisplayType::TableCell
        || display == DisplayType::TableCaption;

    return formIsTablePart;
}

Node::InsertedIntoAncestorResult HTMLFormElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        document().didAssociateFormControl(*this);
    return InsertedIntoAncestorResult::Done;
}

void HTMLFormElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    Node& root = traverseToRootNode(); // Do not rely on rootNode() because our IsInTreeScope is outdated.
    Vector<FormAssociatedElement*> associatedElements(m_associatedElements);
    for (auto& associatedElement : associatedElements)
        associatedElement->formOwnerRemovedFromTree(root);
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

void HTMLFormElement::handleLocalEvents(Event& event, EventInvokePhase phase)
{
    if (event.eventPhase() != Event::CAPTURING_PHASE && is<Node>(event.target()) && event.target() != this && (event.type() == eventNames().submitEvent || event.type() == eventNames().resetEvent)) {
        event.stopPropagation();
        return;
    }
    HTMLElement::handleLocalEvents(event, phase);
}

unsigned HTMLFormElement::length() const
{
    unsigned length = 0;
    for (auto& associatedElement : m_associatedElements) {
        if (associatedElement->isEnumeratable())
            ++length;
    }
    return length;
}

HTMLElement* HTMLFormElement::item(unsigned index)
{
    return elements()->item(index);
}

Optional<Variant<RefPtr<RadioNodeList>, RefPtr<Element>>> HTMLFormElement::namedItem(const AtomicString& name)
{
    auto namedItems = namedElements(name);

    if (namedItems.isEmpty())
        return WTF::nullopt;
    if (namedItems.size() == 1)
        return Variant<RefPtr<RadioNodeList>, RefPtr<Element>> { RefPtr<Element> { WTFMove(namedItems[0]) } };

    return Variant<RefPtr<RadioNodeList>, RefPtr<Element>> { RefPtr<RadioNodeList> { radioNodeList(name) } };
}

Vector<AtomicString> HTMLFormElement::supportedPropertyNames() const
{
    // FIXME: Should be implemented (only needed for enumeration with includeDontEnumProperties mode
    // since this class is annotated with LegacyUnenumerableNamedProperties).
    return { };
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
    if (fromImplicitSubmissionTrigger && (submissionTriggerCount == 1 || document().settings().allowMultiElementImplicitSubmission()))
        prepareForSubmission(event);
}

bool HTMLFormElement::validateInteractively()
{
    for (auto& associatedElement : m_associatedElements) {
        if (is<HTMLFormControlElement>(*associatedElement))
            downcast<HTMLFormControlElement>(*associatedElement).hideVisibleValidationMessage();
    }

    Vector<RefPtr<HTMLFormControlElement>> unhandledInvalidControls;
    if (!checkInvalidControlsAndCollectUnhandled(unhandledInvalidControls))
        return true;
    // Because the form has invalid controls, we abort the form submission and
    // show a validation message on a focusable form control.

    // Make sure layout is up-to-date in case we call isFocusable() (which
    // has !renderer()->needsLayout() assertion).
    ASSERT(!document().view() || !document().view()->needsLayout());

    Ref<HTMLFormElement> protectedThis(*this);

    // Focus on the first focusable control and show a validation message.
    for (auto& control : unhandledInvalidControls) {
        if (control->isConnected() && control->isFocusable()) {
            control->focusAndShowValidationMessage();
            break;
        }
    }

    // Warn about all of unfocusable controls.
    if (document().frame()) {
        for (auto& control : unhandledInvalidControls) {
            if (control->isConnected() && control->isFocusable())
                continue;
            String message = makeString("An invalid form control with name='", control->name(), "' is not focusable.");
            document().addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, message);
        }
    }

    return false;
}

void HTMLFormElement::prepareForSubmission(Event& event)
{
    if (!isConnected())
        return;

    RefPtr<Frame> frame = document().frame();
    if (m_isSubmittingOrPreparingForSubmission || !frame)
        return;

    m_isSubmittingOrPreparingForSubmission = true;
    m_shouldSubmit = false;

    bool shouldValidate = document().page() && document().page()->settings().interactiveFormValidationEnabled() && !noValidate();

    if (shouldValidate) {
        auto submitElement = findSubmitButton(&event);
        if (submitElement && submitElement->formNoValidate())
            shouldValidate = false;
    }

    // Interactive validation must be done before dispatching the submit event.
    if (shouldValidate && !validateInteractively()) {
        m_isSubmittingOrPreparingForSubmission = false;
        return;
    }

    auto targetFrame = frame->loader().findFrameForNavigation(effectiveTarget(&event), &document());
    if (!targetFrame)
        targetFrame = frame.get();
    auto formState = FormState::create(*this, textFieldValues(), document(), NotSubmittedByJavaScript);
    targetFrame->loader().client().dispatchWillSendSubmitEvent(WTFMove(formState));

    auto protectedThis = makeRef(*this);

    auto submitEvent = Event::create(eventNames().submitEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes);
    dispatchEvent(submitEvent);

    // Event handling could have resulted in m_shouldSubmit becoming true as a side effect, too.
    if (!submitEvent->defaultPrevented())
        m_shouldSubmit = true;

    m_isSubmittingOrPreparingForSubmission = false;

    if (m_shouldSubmit)
        submit(&event, true, true, NotSubmittedByJavaScript);
}

void HTMLFormElement::submit()
{
    submit(nullptr, false, true, NotSubmittedByJavaScript);
}

void HTMLFormElement::submitFromJavaScript()
{
    submit(nullptr, false, UserGestureIndicator::processingUserGesture(), SubmittedByJavaScript);
}

StringPairVector HTMLFormElement::textFieldValues() const
{
    StringPairVector result;
    result.reserveInitialCapacity(m_associatedElements.size());
    for (auto& associatedElement : m_associatedElements) {
        auto& element = associatedElement->asHTMLElement();
        if (!is<HTMLInputElement>(element))
            continue;
        auto& input = downcast<HTMLInputElement>(element);
        if (!input.isTextField())
            continue;
        result.uncheckedAppend({ input.name().string(), input.value() });
    }
    return result;
}

void HTMLFormElement::submit(Event* event, bool activateSubmitButton, bool processingUserGesture, FormSubmissionTrigger formSubmissionTrigger)
{
    RefPtr<FrameView> view = document().view();
    RefPtr<Frame> frame = document().frame();
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

    auto protectedThis = makeRef(*this); // Form submission can execute arbitary JavaScript.

    auto shouldLockHistory = processingUserGesture ? LockHistory::No : LockHistory::Yes;
    frame->loader().submitForm(FormSubmission::create(*this, m_attributes, event, shouldLockHistory, formSubmissionTrigger));

    if (needButtonActivation && firstSuccessfulSubmitButton)
        firstSuccessfulSubmitButton->setActivatedSubmit(false);

    m_shouldSubmit = false;
    m_isSubmittingOrPreparingForSubmission = false;
}

void HTMLFormElement::reset()
{
    if (m_isInResetFunction)
        return;

    RefPtr<Frame> protectedFrame = document().frame();
    if (!protectedFrame)
        return;

    Ref<HTMLFormElement> protectedThis(*this);

    SetForScope<bool> isInResetFunctionRestorer(m_isInResetFunction, true);

    auto event = Event::create(eventNames().resetEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes);
    dispatchEvent(event);
    if (!event->defaultPrevented())
        resetAssociatedFormControlElements();
}

void HTMLFormElement::resetAssociatedFormControlElements()
{
    // Event handling can cause associated elements to be added or deleted while iterating
    // over this collection. Protect these elements until we are done notifying them of
    // the reset operation.
    Vector<Ref<HTMLFormControlElement>> associatedFormControlElements;
    associatedFormControlElements.reserveInitialCapacity(m_associatedElements.size());
    for (auto* element : m_associatedElements) {
        if (is<HTMLFormControlElement>(element))
            associatedFormControlElements.uncheckedAppend(*downcast<HTMLFormControlElement>(element));
    }
    
    for (auto& associatedFormControlElement : associatedFormControlElements)
        associatedFormControlElement->reset();
}

#if ENABLE(IOS_AUTOCORRECT_AND_AUTOCAPITALIZE)

// FIXME: We should look to share this code with class HTMLFormControlElement instead of duplicating the logic.

bool HTMLFormElement::shouldAutocorrect() const
{
    const AtomicString& autocorrectValue = attributeWithoutSynchronization(autocorrectAttr);
    if (!autocorrectValue.isEmpty())
        return !equalLettersIgnoringASCIICase(autocorrectValue, "off");
    if (RefPtr<HTMLFormElement> form = this->form())
        return form->shouldAutocorrect();
    return true;
}

#endif

void HTMLFormElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == actionAttr) {
        m_attributes.parseAction(value);
        
        if (!m_attributes.action().isEmpty()) {
            if (RefPtr<Frame> f = document().frame()) {
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
            document().registerForDocumentSuspensionCallbacks(*this);
        else
            document().unregisterForDocumentSuspensionCallbacks(*this);
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
    if (associatedHTMLElement.hasAttributeWithoutSynchronization(formAttr) && associatedHTMLElement.isConnected()) {
        unsigned short position = compareDocumentPosition(associatedHTMLElement);
        ASSERT_WITH_SECURITY_IMPLICATION(!(position & DOCUMENT_POSITION_DISCONNECTED));
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

    if (!associatedHTMLElement.isDescendantOf(*this))
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
                control.invalidateStyleForSubtree();
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

    if (auto* nodeLists = this->nodeLists())
        nodeLists->invalidateCaches();

    if (e == m_defaultButton)
        resetDefaultButton();
}

void HTMLFormElement::registerInvalidAssociatedFormControl(const HTMLFormControlElement& formControlElement)
{
    ASSERT_WITH_MESSAGE(!is<HTMLFieldSetElement>(formControlElement), "FieldSet are never candidates for constraint validation.");
    ASSERT(static_cast<const Element&>(formControlElement).matchesInvalidPseudoClass());

    if (m_invalidAssociatedFormControls.isEmpty())
        invalidateStyleForSubtree();
    m_invalidAssociatedFormControls.add(&formControlElement);
}

void HTMLFormElement::removeInvalidAssociatedFormControlIfNeeded(const HTMLFormControlElement& formControlElement)
{
    if (m_invalidAssociatedFormControls.remove(&formControlElement)) {
        if (m_invalidAssociatedFormControls.isEmpty())
            invalidateStyleForSubtree();
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

String HTMLFormElement::effectiveTarget(const Event* event) const
{
    if (auto* submitButton = findSubmitButton(event)) {
        auto targetValue = submitButton->attributeWithoutSynchronization(formtargetAttr);
        if (!targetValue.isNull())
            return targetValue;
    }

    auto targetValue = target();
    if (!targetValue.isNull())
        return targetValue;

    return document().baseTarget();
}

bool HTMLFormElement::wasUserSubmitted() const
{
    return m_wasUserSubmitted;
}

HTMLFormControlElement* HTMLFormElement::findSubmitButton(const Event* event) const
{
    if (!event || !is<Node>(event->target()))
        return nullptr;
    auto& node = downcast<Node>(*event->target());
    auto* element = is<Element>(node) ? &downcast<Element>(node) : node.parentElement();
    return element ? lineageOfType<HTMLFormControlElement>(*element).first() : nullptr;
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

    RefPtr<HTMLFormControlElement> oldDefault = m_defaultButton;
    m_defaultButton = nullptr;
    defaultButton();
    if (m_defaultButton != oldDefault) {
        if (oldDefault)
            oldDefault->invalidateStyleForSubtree();
        if (m_defaultButton)
            m_defaultButton->invalidateStyleForSubtree();
    }
}

bool HTMLFormElement::checkValidity()
{
    Vector<RefPtr<HTMLFormControlElement>> controls;
    return !checkInvalidControlsAndCollectUnhandled(controls);
}

bool HTMLFormElement::checkInvalidControlsAndCollectUnhandled(Vector<RefPtr<HTMLFormControlElement>>& unhandledInvalidControls)
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

bool HTMLFormElement::reportValidity()
{
    Ref<HTMLFormElement> protectedThis(*this);

    // Update layout before processing form actions in case the style changes
    // the Form or button relationships.
    document().updateLayoutIgnorePendingStylesheets();

    return validateInteractively();
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

RefPtr<HTMLElement> HTMLFormElement::elementFromPastNamesMap(const AtomicString& pastName) const
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

    auto elementFromPast = elementFromPastNamesMap(name);
    if (namedItems.size() == 1 && namedItems.first().ptr() != elementFromPast)
        addToPastNamesMap(downcast<HTMLElement>(namedItems.first().get()).asFormNamedItem(), name);
    else if (elementFromPast && namedItems.isEmpty())
        namedItems.append(*elementFromPast);

    return namedItems;
}

void HTMLFormElement::resumeFromDocumentSuspension()
{
    ASSERT(!shouldAutocomplete());

    Ref<HTMLFormElement> protectedThis(*this);

    resetAssociatedFormControlElements();
}

void HTMLFormElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    if (!shouldAutocomplete()) {
        oldDocument.unregisterForDocumentSuspensionCallbacks(*this);
        newDocument.registerForDocumentSuspensionCallbacks(*this);
    }

    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
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

const Vector<FormAssociatedElement*>& HTMLFormElement::unsafeAssociatedElements() const
{
    ASSERT(ScriptDisallowedScope::InMainThread::hasDisallowedScope());
    return m_associatedElements;
}

Vector<Ref<FormAssociatedElement>> HTMLFormElement::copyAssociatedElementsVector() const
{
    return WTF::map(m_associatedElements, [] (auto* rawElement) {
        return Ref<FormAssociatedElement>(*rawElement);
    });
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
