/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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
#include "HTMLDataListElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLTextAreaElement.h"
#include "PseudoClassChangeInvalidation.h"
#include "Quirks.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "ScriptDisallowedScope.h"
#include "SelectionRestorationMode.h"
#include "Settings.h"
#include "StyleTreeResolver.h"
#include "ValidationMessage.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLFormControlElement);

using namespace HTMLNames;

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : LabelableElement(tagName, document)
    , FormAssociatedElement(form)
    , m_disabled(false)
    , m_hasReadOnlyAttribute(false)
    , m_isRequired(false)
    , m_valueMatchesRenderer(false)
    , m_disabledByAncestorFieldset(false)
    , m_dataListAncestorState(Unknown)
    , m_willValidateInitialized(false)
    , m_willValidate(true)
    , m_isValid(true)
    , m_wasChangedSinceLastFormControlChangeEvent(false)
{
    setHasCustomStyleResolveCallbacks();
}

HTMLFormControlElement::~HTMLFormControlElement()
{
    clearForm();
}

String HTMLFormControlElement::formEnctype() const
{
    const AtomString& formEnctypeAttr = attributeWithoutSynchronization(formenctypeAttr);
    if (formEnctypeAttr.isNull())
        return emptyString();
    return FormSubmission::Attributes::parseEncodingType(formEnctypeAttr);
}

void HTMLFormControlElement::setFormEnctype(const AtomString& value)
{
    setAttributeWithoutSynchronization(formenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const
{
    auto& formMethodAttr = attributeWithoutSynchronization(formmethodAttr);
    if (formMethodAttr.isNull())
        return emptyString();
    bool dialogElementEnabled = document().settings().dialogElementEnabled();
    return FormSubmission::Attributes::methodString(FormSubmission::Attributes::parseMethodType(formMethodAttr, dialogElementEnabled), dialogElementEnabled);
}

void HTMLFormControlElement::setFormMethod(const AtomString& value)
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
        return document().url().string();
    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(value)).string();
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
    if (m_disabledByAncestorFieldset == isDisabled)
        return;

    Style::PseudoClassChangeInvalidation disabledInvalidation(*this, { { CSSSelector::PseudoClassDisabled, isDisabled }, { CSSSelector::PseudoClassEnabled, !isDisabled } });

    m_disabledByAncestorFieldset = isDisabled;
    disabledStateChanged();
}

void HTMLFormControlElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == formAttr)
        formAttributeChanged();
    else if (name == disabledAttr) {
        if (canBeActuallyDisabled()) {
            bool newDisabled = !value.isNull();
            if (m_disabled != newDisabled) {
                Style::PseudoClassChangeInvalidation disabledInvalidation(*this, { { CSSSelector::PseudoClassDisabled, newDisabled }, { CSSSelector::PseudoClassEnabled, !newDisabled } });
                m_disabled = newDisabled;
                disabledAttributeChanged();
            }
        }
    } else if (name == readonlyAttr) {
        bool newHasReadOnlyAttribute = !value.isNull();
        if (m_hasReadOnlyAttribute != newHasReadOnlyAttribute) {
            bool newMatchesReadWrite = supportsReadOnly() && !newHasReadOnlyAttribute;
            Style::PseudoClassChangeInvalidation readWriteInvalidation(*this, { { CSSSelector::PseudoClassReadWrite, newMatchesReadWrite }, { CSSSelector::PseudoClassReadOnly, !newMatchesReadWrite } });
            m_hasReadOnlyAttribute = newHasReadOnlyAttribute;
            readOnlyStateChanged();
        }
    } else if (name == requiredAttr) {
        bool newRequired = !value.isNull();
        if (m_isRequired != newRequired) {
            Style::PseudoClassChangeInvalidation requiredInvalidation(*this, { { CSSSelector::PseudoClassRequired, newRequired }, { CSSSelector::PseudoClassOptional, !newRequired } });
            m_isRequired = newRequired;
            requiredStateChanged();
        }
    } else
        HTMLElement::parseAttribute(name, value);
}

void HTMLFormControlElement::disabledAttributeChanged()
{
    disabledStateChanged();
}

void HTMLFormControlElement::disabledStateChanged()
{
    updateWillValidateAndValidity();
    if (renderer() && renderer()->style().hasEffectiveAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::States::Enabled);
}

void HTMLFormControlElement::readOnlyStateChanged()
{
    updateWillValidateAndValidity();

    // Some input pseudo classes like :in-range/out-of-range change based on the readonly state.
    // FIXME: Use PseudoClassChangeInvalidation instead for :has() support and more efficiency.
    invalidateStyleForSubtree();
}

void HTMLFormControlElement::requiredStateChanged()
{
    updateValidity();
}

void HTMLFormControlElement::didAttachRenderers()
{
    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (renderer())
        renderer()->updateFromElement();
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

    updateWillValidateAndValidity();
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
        updateWillValidateAndValidity();
}

void HTMLFormControlElement::setChangedSinceLastFormControlChangeEvent(bool changed)
{
    m_wasChangedSinceLastFormControlChangeEvent = changed;
}

void HTMLFormControlElement::dispatchChangeEvent()
{
    dispatchScopedEvent(Event::create(eventNames().changeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
}

void HTMLFormControlElement::dispatchCancelEvent()
{
    dispatchScopedEvent(Event::create(eventNames().cancelEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
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

void HTMLFormControlElement::didRecalcStyle(Style::Change)
{
    // updateFromElement() can cause the selection to change, and in turn
    // trigger synchronous layout, so it must not be called during style recalc.
    if (renderer()) {
        RefPtr<HTMLFormControlElement> element = this;
        Style::deprecatedQueuePostResolutionCallback([element] {
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
#if (PLATFORM(GTK) || PLATFORM(WPE))
    return HTMLElement::isMouseFocusable();
#else
    if (needsMouseFocusableQuirk())
        return HTMLElement::isMouseFocusable();
    return false;
#endif
}

void HTMLFormControlElement::runFocusingStepsForAutofocus()
{
    focus({ SelectionRestorationMode::PlaceCaretAtStart });
}

bool HTMLFormControlElement::matchesValidPseudoClass() const
{
    return willValidate() && isValidFormControlElement();
}

bool HTMLFormControlElement::matchesInvalidPseudoClass() const
{
    return willValidate() && !isValidFormControlElement();
}

void HTMLFormControlElement::endDelayingUpdateValidity()
{
    ASSERT(m_delayedUpdateValidityCount);
    if (!--m_delayedUpdateValidityCount)
        updateValidity();
}

bool HTMLFormControlElement::computeWillValidate() const
{
    if (m_dataListAncestorState == Unknown) {
#if ENABLE(DATALIST_ELEMENT)
        m_dataListAncestorState = (document().hasDataListElements() && ancestorsOfType<HTMLDataListElement>(*this).first()) ? InsideDataList : NotInsideDataList;
#else
        m_dataListAncestorState = NotInsideDataList;
#endif
    }
    // readonly bars constraint validation for *all* <input> elements, regardless of the <input> type, for compat reasons.
    return m_dataListAncestorState == NotInsideDataList && !isDisabledFormControl() && !m_hasReadOnlyAttribute;
}

bool HTMLFormControlElement::willValidate() const
{
    if (!m_willValidateInitialized || m_dataListAncestorState == Unknown) {
        m_willValidateInitialized = true;
        bool newWillValidate = computeWillValidate();
        if (m_willValidate != newWillValidate)
            m_willValidate = newWillValidate;
    } else {
        // If the following assertion fails, updateWillValidateAndValidity() is not
        // called correctly when something which changes computeWillValidate() result
        // is updated.
        ASSERT(m_willValidate == computeWillValidate());
    }
    return m_willValidate;
}

void HTMLFormControlElement::updateWillValidateAndValidity()
{
    // We need to recalculate willValidate immediately because willValidate change can causes style change.
    bool newWillValidate = computeWillValidate();
    if (m_willValidateInitialized && m_willValidate == newWillValidate)
        return;

    bool wasValid = m_isValid;

    m_willValidateInitialized = true;
    m_willValidate = newWillValidate;

    updateValidity();

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
        m_validationMessage = makeUnique<ValidationMessage>(*this);
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

bool HTMLFormControlElement::isFocusingWithValidationMessage() const
{
    return m_isFocusingWithValidationMessage;
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
        Ref protectedThis { *this };
        focusAndShowValidationMessage();
        return false;
    }

    if (document().frame()) {
        auto message = makeString("An invalid form control with name='", name(), "' is not focusable.");
        document().addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, message);
    }

    return false;
}

void HTMLFormControlElement::focusAndShowValidationMessage()
{
    SetForScope isFocusingWithValidationMessageScope(m_isFocusingWithValidationMessage, true);

    // Calling focus() will scroll the element into view.
    focus();

    // focus() will scroll the element into view and this scroll may happen asynchronously.
    // Because scrolling the view hides the validation message, we need to show the validation
    // message asynchronously as well.
    callOnMainThread([this, protectedThis = Ref { *this }] {
        updateVisibleValidationMessage();
    });
}

inline bool HTMLFormControlElement::isValidFormControlElement() const
{
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
    if (m_delayedUpdateValidityCount)
        return;

    bool willValidate = this->willValidate();
    bool newIsValid = this->computeValidity();

    if (newIsValid != m_isValid) {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, { { CSSSelector::PseudoClassValid, newIsValid }, { CSSSelector::PseudoClassInvalid, !newIsValid } });

        m_isValid = newIsValid;

        if (willValidate) {
            if (!m_isValid) {
                if (isConnected())
                    addInvalidElementToAncestorFromInsertionPoint(*this, parentNode());
                if (HTMLFormElement* form = this->form())
                    form->registerInvalidAssociatedFormControl(*this);
            } else {
                if (isConnected())
                    removeInvalidElementToAncestorFromInsertionPoint(*this, parentNode());
                if (HTMLFormElement* form = this->form())
                    form->removeInvalidAssociatedFormControlIfNeeded(*this);
            }
        }
    }

    // Updates only if this control already has a validation message.
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
        return !equalLettersIgnoringASCIICase(autocorrectValue, "off"_s);
    if (RefPtr<HTMLFormElement> form = this->form())
        return form->shouldAutocorrect();
    return true;
}

#endif

#if ENABLE(AUTOCAPITALIZE)

AutocapitalizeType HTMLFormControlElement::autocapitalizeType() const
{
    AutocapitalizeType type = HTMLElement::autocapitalizeType();
    if (type == AutocapitalizeType::Default) {
        if (RefPtr<HTMLFormElement> form = this->form())
            return form->autocapitalizeType();
    }
    return type;
}

#endif

String HTMLFormControlElement::autocomplete() const
{
    return autofillData().idlExposedValue;
}

void HTMLFormControlElement::setAutocomplete(const AtomString& value)
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

String HTMLFormControlElement::resultForDialogSubmit() const
{
    return attributeWithoutSynchronization(HTMLNames::valueAttr);
}

// FIXME: We should remove the quirk once <rdar://problem/47334655> is fixed.
bool HTMLFormControlElement::needsMouseFocusableQuirk() const
{
    return document().quirks().needsFormControlToBeMouseFocusable();
}

} // namespace Webcore
