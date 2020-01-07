/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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
#include "HTMLFormControlElement.h"

#include "Autofill.h"
#include "ControlStates.h"
#include "ElementAncestorIterator.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "HTMLTextAreaElement.h"
#include "Quirks.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "StyleTreeResolver.h"
#include "ValidationMessage.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLFormControlElement);

using namespace HTMLNames;

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : LabelableElement(tagName, document)
    , FormAssociatedElement(form)
    , m_disabled(false)
    , m_isReadOnly(false)
    , m_isRequired(false)
    , m_valueMatchesRenderer(false)
    , m_disabledByAncestorFieldset(false)
    , m_dataListAncestorState(Unknown)
    , m_willValidateInitialized(false)
    , m_willValidate(true)
    , m_isValid(true)
    , m_wasChangedSinceLastFormControlChangeEvent(false)
    , m_hasAutofocused(false)
{
    setHasCustomStyleResolveCallbacks();
}

HTMLFormControlElement::~HTMLFormControlElement()
{
    // The calls willChangeForm() and didChangeForm() are virtual, we want the
    // form to be reset while this object still exists.
    setForm(nullptr);
}

String HTMLFormControlElement::formEnctype() const
{
    const AtomString& formEnctypeAttr = attributeWithoutSynchronization(formenctypeAttr);
    if (formEnctypeAttr.isNull())
        return emptyString();
    return FormSubmission::Attributes::parseEncodingType(formEnctypeAttr);
}

void HTMLFormControlElement::setFormEnctype(const String& value)
{
    setAttributeWithoutSynchronization(formenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const
{
    auto& formMethodAttr = attributeWithoutSynchronization(formmethodAttr);
    if (formMethodAttr.isNull())
        return emptyString();
    return FormSubmission::Attributes::methodString(FormSubmission::Attributes::parseMethodType(formMethodAttr));
}

void HTMLFormControlElement::setFormMethod(const String& value)
{
    setAttributeWithoutSynchronization(formmethodAttr, value);
}

bool HTMLFormControlElement::formNoValidate() const
{
    return hasAttributeWithoutSynchronization(formnovalidateAttr);
}

String HTMLFormControlElement::formAction() const
{
    const AtomString& value = attributeWithoutSynchronization(formactionAttr);
    if (value.isEmpty())
        return document().url();
    return getURLAttribute(formactionAttr);
}

void HTMLFormControlElement::setFormAction(const AtomString& value)
{
    setAttributeWithoutSynchronization(formactionAttr, value);
}

bool HTMLFormControlElement::computeIsDisabledByFieldsetAncestor() const
{
    RefPtr<Element> previousAncestor;
    for (RefPtr<Element> ancestor = parentElement(); ancestor; ancestor = ancestor->parentElement()) {
        if (is<HTMLFieldSetElement>(*ancestor) && ancestor->hasAttributeWithoutSynchronization(disabledAttr)) {
            HTMLFieldSetElement& fieldSetAncestor = downcast<HTMLFieldSetElement>(*ancestor);
            bool isInFirstLegend = is<HTMLLegendElement>(previousAncestor) && previousAncestor == fieldSetAncestor.legend();
            return !isInFirstLegend;
        }
        previousAncestor = ancestor;
    }
    return false;
}

void HTMLFormControlElement::setAncestorDisabled(bool isDisabled)
{
    ASSERT(computeIsDisabledByFieldsetAncestor() == isDisabled);
    bool oldValue = m_disabledByAncestorFieldset;
    m_disabledByAncestorFieldset = isDisabled;
    if (oldValue != m_disabledByAncestorFieldset)
        disabledStateChanged();
}

void HTMLFormControlElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == formAttr)
        formAttributeChanged();
    else if (name == disabledAttr) {
        if (canBeActuallyDisabled()) {
            bool oldDisabled = m_disabled;
            m_disabled = !value.isNull();
            if (oldDisabled != m_disabled)
                disabledAttributeChanged();
        }
    } else if (name == readonlyAttr) {
        bool wasReadOnly = m_isReadOnly;
        m_isReadOnly = !value.isNull();
        if (wasReadOnly != m_isReadOnly)
            readOnlyStateChanged();
    } else if (name == requiredAttr) {
        bool wasRequired = m_isRequired;
        m_isRequired = !value.isNull();
        if (wasRequired != m_isRequired)
            requiredStateChanged();
    } else
        HTMLElement::parseAttribute(name, value);
}

void HTMLFormControlElement::disabledAttributeChanged()
{
    disabledStateChanged();
}

void HTMLFormControlElement::disabledStateChanged()
{
    setNeedsWillValidateCheck();
    invalidateStyleForSubtree();
    if (renderer() && renderer()->style().hasAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::EnabledState);
}

void HTMLFormControlElement::readOnlyStateChanged()
{
    setNeedsWillValidateCheck();
    invalidateStyleForSubtree();
}

void HTMLFormControlElement::requiredStateChanged()
{
    updateValidity();
    // Style recalculation is needed because style selectors may include
    // :required and :optional pseudo-classes.
    invalidateStyleForSubtree();
}

static bool shouldAutofocus(HTMLFormControlElement* element)
{
    if (!element->renderer())
        return false;
    if (!element->hasAttributeWithoutSynchronization(autofocusAttr))
        return false;
    if (!element->isConnected() || !element->document().renderView())
        return false;
    if (element->document().isSandboxed(SandboxAutomaticFeatures)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        element->document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked autofocusing on a form control because the form's frame is sandboxed and the 'allow-scripts' permission is not set."_s);
        return false;
    }

    auto& document = element->document();
    if (!document.frame()->isMainFrame() && !document.topDocument().securityOrigin().canAccess(document.securityOrigin())) {
        document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked autofocusing on a form control in a cross-origin subframe."_s);
        return false;
    }

    if (element->hasAutofocused())
        return false;

    // FIXME: Should this set of hasTagName checks be replaced by a
    // virtual member function?
    if (is<HTMLInputElement>(*element))
        return !downcast<HTMLInputElement>(*element).isInputTypeHidden();
    if (element->hasTagName(selectTag))
        return true;
    if (element->hasTagName(keygenTag))
        return true;
    if (element->hasTagName(buttonTag))
        return true;
    if (is<HTMLTextAreaElement>(*element))
        return true;

    return false;
}

void HTMLFormControlElement::didAttachRenderers()
{
    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (renderer())
        renderer()->updateFromElement();

    if (shouldAutofocus(this)) {
        setAutofocused();

        RefPtr<HTMLFormControlElement> element = this;
        auto frameView = makeRefPtr(document().view());
        if (frameView && frameView->layoutContext().isInLayout()) {
            frameView->queuePostLayoutCallback([element] {
                element->focus();
            });
        } else {
            Style::queuePostResolutionCallback([element] {
                element->focus();
            });
        }
    }
}

void HTMLFormControlElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    FormAssociatedElement::didMoveToNewDocument(oldDocument);
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
}

static void addInvalidElementToAncestorFromInsertionPoint(const HTMLFormControlElement& element, ContainerNode* insertionPoint)
{
    if (!is<Element>(insertionPoint))
        return;

    for (auto& ancestor : lineageOfType<HTMLFieldSetElement>(downcast<Element>(*insertionPoint)))
        ancestor.addInvalidDescendant(element);
}

static void removeInvalidElementToAncestorFromInsertionPoint(const HTMLFormControlElement& element, ContainerNode* insertionPoint)
{
    if (!is<Element>(insertionPoint))
        return;

    for (auto& ancestor : lineageOfType<HTMLFieldSetElement>(downcast<Element>(*insertionPoint)))
        ancestor.removeInvalidDescendant(element);
}

Node::InsertedIntoAncestorResult HTMLFormControlElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    if (m_dataListAncestorState == NotInsideDataList)
        m_dataListAncestorState = Unknown;

    setNeedsWillValidateCheck();
    if (willValidate() && !isValidFormControlElement())
        addInvalidElementToAncestorFromInsertionPoint(*this, &parentOfInsertedTree);
    if (document().hasDisabledFieldsetElement())
        setAncestorDisabled(computeIsDisabledByFieldsetAncestor());
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    FormAssociatedElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLFormControlElement::didFinishInsertingNode()
{
    resetFormOwner();
}

void HTMLFormControlElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    bool wasMatchingInvalidPseudoClass = willValidate() && !isValidFormControlElement();

    m_validationMessage = nullptr;
    if (m_disabledByAncestorFieldset)
        setAncestorDisabled(computeIsDisabledByFieldsetAncestor());

    bool wasInsideDataList = false;
    if (m_dataListAncestorState == InsideDataList) {
        m_dataListAncestorState = Unknown;
        wasInsideDataList = true;
    }

    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    FormAssociatedElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (wasMatchingInvalidPseudoClass)
        removeInvalidElementToAncestorFromInsertionPoint(*this, &oldParentOfRemovedTree);

    if (wasInsideDataList)
        setNeedsWillValidateCheck();
}

void HTMLFormControlElement::setChangedSinceLastFormControlChangeEvent(bool changed)
{
    m_wasChangedSinceLastFormControlChangeEvent = changed;
}

void HTMLFormControlElement::dispatchChangeEvent()
{
    dispatchScopedEvent(Event::create(eventNames().changeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
}

void HTMLFormControlElement::dispatchFormControlChangeEvent()
{
    dispatchChangeEvent();
    setChangedSinceLastFormControlChangeEvent(false);
}

void HTMLFormControlElement::dispatchFormControlInputEvent()
{
    setChangedSinceLastFormControlChangeEvent(true);
    dispatchInputEvent();
}

bool HTMLFormControlElement::isDisabledFormControl() const
{
    return m_disabled || m_disabledByAncestorFieldset;
}

bool HTMLFormControlElement::isRequired() const
{
    return m_isRequired;
}

void HTMLFormControlElement::didRecalcStyle(Style::Change)
{
    // updateFromElement() can cause the selection to change, and in turn
    // trigger synchronous layout, so it must not be called during style recalc.
    if (renderer()) {
        RefPtr<HTMLFormControlElement> element = this;
        Style::queuePostResolutionCallback([element]{
            if (auto* renderer = element->renderer())
                renderer->updateFromElement();
        });
    }
}

bool HTMLFormControlElement::supportsFocus() const
{
    return !isDisabledFormControl();
}

bool HTMLFormControlElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    return isFocusable()
        && document().frame()
        && document().frame()->eventHandler().tabsToAllFormControls(event);
}

bool HTMLFormControlElement::isMouseFocusable() const
{
#if PLATFORM(GTK)
    return HTMLElement::isMouseFocusable();
#else
    if (needsMouseFocusableQuirk())
        return HTMLElement::isMouseFocusable();
    return false;
#endif
}

bool HTMLFormControlElement::matchesValidPseudoClass() const
{
    return willValidate() && isValidFormControlElement();
}

bool HTMLFormControlElement::matchesInvalidPseudoClass() const
{
    return willValidate() && !isValidFormControlElement();
}

bool HTMLFormControlElement::computeWillValidate() const
{
    if (m_dataListAncestorState == Unknown) {
        for (ContainerNode* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
            if (ancestor->hasTagName(datalistTag)) {
                m_dataListAncestorState = InsideDataList;
                break;
            }
        }
        if (m_dataListAncestorState == Unknown)
            m_dataListAncestorState = NotInsideDataList;
    }
    return m_dataListAncestorState == NotInsideDataList && !isDisabledOrReadOnly();
}

bool HTMLFormControlElement::willValidate() const
{
    if (!m_willValidateInitialized || m_dataListAncestorState == Unknown) {
        m_willValidateInitialized = true;
        bool newWillValidate = computeWillValidate();
        if (m_willValidate != newWillValidate)
            m_willValidate = newWillValidate;
    } else {
        // If the following assertion fails, setNeedsWillValidateCheck() is not
        // called correctly when something which changes computeWillValidate() result
        // is updated.
        ASSERT(m_willValidate == computeWillValidate());
    }
    return m_willValidate;
}

void HTMLFormControlElement::setNeedsWillValidateCheck()
{
    // We need to recalculate willValidate immediately because willValidate change can causes style change.
    bool newWillValidate = computeWillValidate();
    if (m_willValidateInitialized && m_willValidate == newWillValidate)
        return;

    bool wasValid = m_isValid;

    m_willValidateInitialized = true;
    m_willValidate = newWillValidate;

    updateValidity();
    invalidateStyleForSubtree();

    if (!m_willValidate && !wasValid) {
        removeInvalidElementToAncestorFromInsertionPoint(*this, parentNode());
        if (RefPtr<HTMLFormElement> form = this->form())
            form->removeInvalidAssociatedFormControlIfNeeded(*this);
    }

    if (!m_willValidate)
        hideVisibleValidationMessage();
}

void HTMLFormControlElement::updateVisibleValidationMessage()
{
    Page* page = document().page();
    if (!page)
        return;
    String message;
    if (renderer() && willValidate())
        message = validationMessage().stripWhiteSpace();
    if (!m_validationMessage)
        m_validationMessage = makeUnique<ValidationMessage>(this);
    m_validationMessage->updateValidationMessage(message);
}

void HTMLFormControlElement::hideVisibleValidationMessage()
{
    if (m_validationMessage)
        m_validationMessage->requestToHideMessage();
}

bool HTMLFormControlElement::checkValidity(Vector<RefPtr<HTMLFormControlElement>>* unhandledInvalidControls)
{
    if (!willValidate() || isValidFormControlElement())
        return true;
    // An event handler can deref this object.
    Ref<HTMLFormControlElement> protectedThis(*this);
    Ref<Document> originalDocument(document());
    auto event = Event::create(eventNames().invalidEvent, Event::CanBubble::No, Event::IsCancelable::Yes);
    dispatchEvent(event);
    if (!event->defaultPrevented() && unhandledInvalidControls && isConnected() && originalDocument.ptr() == &document())
        unhandledInvalidControls->append(this);
    return false;
}

bool HTMLFormControlElement::isShowingValidationMessage() const
{
    return m_validationMessage && m_validationMessage->isVisible();
}
    
bool HTMLFormControlElement::reportValidity()
{
    Vector<RefPtr<HTMLFormControlElement>> elements;
    if (checkValidity(&elements))
        return true;

    if (elements.isEmpty())
        return false;

    // Needs to update layout now because we'd like to call isFocusable(), which
    // has !renderer()->needsLayout() assertion.
    document().updateLayoutIgnorePendingStylesheets();

    if (isConnected() && isFocusable()) {
        focusAndShowValidationMessage();
        return false;
    }

    if (document().frame()) {
        String message = makeString("An invalid form control with name='", name(), "' is not focusable.");
        document().addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, message);
    }

    return false;
}

void HTMLFormControlElement::focusAndShowValidationMessage()
{
    // Calling focus() will scroll the element into view.
    focus();

    // focus() will scroll the element into view and this scroll may happen asynchronously.
    // Because scrolling the view hides the validation message, we need to show the validation
    // message asynchronously as well.
    callOnMainThread([this, protectedThis = makeRef(*this)] {
        updateVisibleValidationMessage();
    });
}

inline bool HTMLFormControlElement::isValidFormControlElement() const
{
    // If the following assertion fails, updateValidity() is not called
    // correctly when something which changes validity is updated.
    ASSERT(m_isValid == isValid());
    return m_isValid;
}

void HTMLFormControlElement::willChangeForm()
{
    if (HTMLFormElement* form = this->form())
        form->removeInvalidAssociatedFormControlIfNeeded(*this);
    FormAssociatedElement::willChangeForm();
}

void HTMLFormControlElement::didChangeForm()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    FormAssociatedElement::didChangeForm();
    if (auto* form = this->form()) {
        if (m_willValidateInitialized && m_willValidate && !isValidFormControlElement())
            form->registerInvalidAssociatedFormControl(*this);
    }
}

void HTMLFormControlElement::updateValidity()
{
    bool willValidate = this->willValidate();
    bool wasValid = m_isValid;

    m_isValid = isValid();

    if (willValidate && m_isValid != wasValid) {
        // Update style for pseudo classes such as :valid :invalid.
        invalidateStyleForSubtree();

        if (!m_isValid) {
            addInvalidElementToAncestorFromInsertionPoint(*this, parentNode());
            if (HTMLFormElement* form = this->form())
                form->registerInvalidAssociatedFormControl(*this);
        } else {
            removeInvalidElementToAncestorFromInsertionPoint(*this, parentNode());
            if (HTMLFormElement* form = this->form())
                form->removeInvalidAssociatedFormControlIfNeeded(*this);
        }
    }

    // Updates only if this control already has a validtion message.
    if (m_validationMessage && m_validationMessage->isVisible()) {
        // Calls updateVisibleValidationMessage() even if m_isValid is not
        // changed because a validation message can be chagned.
        updateVisibleValidationMessage();
    }
}

void HTMLFormControlElement::setCustomValidity(const String& error)
{
    FormAssociatedElement::setCustomValidity(error);
    updateValidity();
}

bool HTMLFormControlElement::validationMessageShadowTreeContains(const Node& node) const
{
    return m_validationMessage && m_validationMessage->shadowTreeContains(node);
}

void HTMLFormControlElement::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    HTMLElement::dispatchBlurEvent(WTFMove(newFocusedElement));
    hideVisibleValidationMessage();
}

#if ENABLE(AUTOCORRECT)

// FIXME: We should look to share this code with class HTMLFormElement instead of duplicating the logic.

bool HTMLFormControlElement::shouldAutocorrect() const
{
    const AtomString& autocorrectValue = attributeWithoutSynchronization(autocorrectAttr);
    if (!autocorrectValue.isEmpty())
        return !equalLettersIgnoringASCIICase(autocorrectValue, "off");
    if (RefPtr<HTMLFormElement> form = this->form())
        return form->shouldAutocorrect();
    return true;
}

#endif

#if ENABLE(AUTOCAPITALIZE)

AutocapitalizeType HTMLFormControlElement::autocapitalizeType() const
{
    AutocapitalizeType type = HTMLElement::autocapitalizeType();
    if (type == AutocapitalizeTypeDefault) {
        if (RefPtr<HTMLFormElement> form = this->form())
            return form->autocapitalizeType();
    }
    return type;
}

#endif

HTMLFormControlElement* HTMLFormControlElement::enclosingFormControlElement(Node* node)
{
    for (; node; node = node->parentNode()) {
        if (is<HTMLFormControlElement>(*node))
            return downcast<HTMLFormControlElement>(node);
    }
    return nullptr;
}

String HTMLFormControlElement::autocomplete() const
{
    return autofillData().idlExposedValue;
}

void HTMLFormControlElement::setAutocomplete(const String& value)
{
    setAttributeWithoutSynchronization(autocompleteAttr, value);
}

AutofillMantle HTMLFormControlElement::autofillMantle() const
{
    return is<HTMLInputElement>(*this) && downcast<HTMLInputElement>(this)->isInputTypeHidden() ? AutofillMantle::Anchor : AutofillMantle::Expectation;
}

AutofillData HTMLFormControlElement::autofillData() const
{
    // FIXME: We could cache the AutofillData if we we had an efficient way to invalidate the cache when
    // the autofill mantle changed (due to a type change on an <input> element) or the element's form
    // owner's autocomplete attribute changed or the form owner itself changed.

    return AutofillData::createFromHTMLFormControlElement(*this);
}

// FIXME: We should remove the quirk once <rdar://problem/47334655> is fixed.
bool HTMLFormControlElement::needsMouseFocusableQuirk() const
{
    return document().quirks().needsFormControlToBeMouseFocusable();
}

} // namespace Webcore
