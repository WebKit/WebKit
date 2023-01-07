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
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FormAssociatedElement.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "PseudoClassChangeInvalidation.h"
#include "Quirks.h"
#include "RenderBox.h"
#include "RenderTheme.h"
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
    , ValidatedFormListedElement(form)
    , m_isRequired(false)
    , m_valueMatchesRenderer(false)
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

Node::InsertedIntoAncestorResult HTMLFormControlElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    ValidatedFormListedElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLFormControlElement::didFinishInsertingNode()
{
    HTMLElement::didFinishInsertingNode();
    ValidatedFormListedElement::didFinishInsertingNode();
}

void HTMLFormControlElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    ValidatedFormListedElement::didMoveToNewDocument();
}

void HTMLFormControlElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    ValidatedFormListedElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

void HTMLFormControlElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == requiredAttr) {
        bool newRequired = !value.isNull();
        if (m_isRequired != newRequired) {
            Style::PseudoClassChangeInvalidation requiredInvalidation(*this, { { CSSSelector::PseudoClassRequired, newRequired }, { CSSSelector::PseudoClassOptional, !newRequired } });
            m_isRequired = newRequired;
            requiredStateChanged();
        }
    } else {
        LabelableElement::parseAttribute(name, value);
        ValidatedFormListedElement::parseAttribute(name, value);
    }
}

void HTMLFormControlElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    if (!FormController::ownerForm(*this))
        restoreFormControlStateIfNecessary();
}

void HTMLFormControlElement::disabledStateChanged()
{
    ValidatedFormListedElement::disabledStateChanged();
    if (renderer() && renderer()->style().hasEffectiveAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::States::Enabled);
}

void HTMLFormControlElement::readOnlyStateChanged()
{
    ValidatedFormListedElement::readOnlyStateChanged();

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
    setInteractedWithSinceLastFormSubmitEvent(true);
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

void HTMLFormControlElement::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    HTMLElement::dispatchBlurEvent(WTFMove(newFocusedElement));
    hideVisibleValidationMessage();
}

#if ENABLE(AUTOCORRECT)

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
