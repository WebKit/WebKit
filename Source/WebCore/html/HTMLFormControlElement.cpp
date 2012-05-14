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
#include "HTMLFormControlElement.h"

#include "Attribute.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "ScriptEventListener.h"
#include "ValidationMessage.h"
#include "ValidityState.h"
#include <wtf/Vector.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
    : LabelableElement(tagName, document)
    , m_fieldSetAncestor(0)
    , m_legendAncestor(0)
    , m_fieldSetAncestorValid(false)
    , m_disabled(false)
    , m_readOnly(false)
    , m_required(false)
    , m_valueMatchesRenderer(false)
    , m_dataListAncestorState(Unknown)
    , m_willValidateInitialized(false)
    , m_willValidate(true)
    , m_isValid(true)
    , m_wasChangedSinceLastFormControlChangeEvent(false)
    , m_hasAutofocused(false)
{
    setForm(form ? form : findFormAncestor());
    setHasCustomWillOrDidRecalcStyle();
}

HTMLFormControlElement::~HTMLFormControlElement()
{
}

void HTMLFormControlElement::detach()
{
    m_validationMessage = nullptr;
    HTMLElement::detach();
}

String HTMLFormControlElement::formEnctype() const
{
    return FormSubmission::Attributes::parseEncodingType(fastGetAttribute(formenctypeAttr));
}

void HTMLFormControlElement::setFormEnctype(const String& value)
{
    setAttribute(formenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const
{
    return FormSubmission::Attributes::methodString(FormSubmission::Attributes::parseMethodType(fastGetAttribute(formmethodAttr)));
}

void HTMLFormControlElement::setFormMethod(const String& value)
{
    setAttribute(formmethodAttr, value);
}

bool HTMLFormControlElement::formNoValidate() const
{
    return fastHasAttribute(formnovalidateAttr);
}

void HTMLFormControlElement::updateFieldSetAndLegendAncestor() const
{
    m_fieldSetAncestor = 0;
    m_legendAncestor = 0;
    for (ContainerNode* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        if (!m_legendAncestor && ancestor->hasTagName(legendTag))
            m_legendAncestor = static_cast<HTMLLegendElement*>(ancestor);
        if (ancestor->hasTagName(fieldsetTag)) {
            m_fieldSetAncestor = static_cast<HTMLFieldSetElement*>(ancestor);
            break;
        }
    }
    m_fieldSetAncestorValid = true;
}

void HTMLFormControlElement::parseAttribute(Attribute* attr)
{
    if (attr->name() == formAttr)
        formAttributeChanged();
    else if (attr->name() == disabledAttr) {
        bool oldDisabled = m_disabled;
        m_disabled = !attr->isNull();
        if (oldDisabled != m_disabled)
            disabledAttributeChanged();
    } else if (attr->name() == readonlyAttr) {
        bool oldReadOnly = m_readOnly;
        m_readOnly = !attr->isNull();
        if (oldReadOnly != m_readOnly) {
            setNeedsStyleRecalc();
            if (renderer() && renderer()->style()->hasAppearance())
                renderer()->theme()->stateChanged(renderer(), ReadOnlyState);
        }
    } else if (attr->name() == requiredAttr) {
        bool oldRequired = m_required;
        m_required = !attr->isNull();
        if (oldRequired != m_required)
            requiredAttributeChanged();
    } else
        HTMLElement::parseAttribute(attr);
    setNeedsWillValidateCheck();
}

void HTMLFormControlElement::disabledAttributeChanged()
{
    setNeedsStyleRecalc();
    if (renderer() && renderer()->style()->hasAppearance())
        renderer()->theme()->stateChanged(renderer(), EnabledState);
}

void HTMLFormControlElement::requiredAttributeChanged()
{
    setNeedsValidityCheck();
    // Style recalculation is needed because style selectors may include
    // :required and :optional pseudo-classes.
    setNeedsStyleRecalc();
}

static bool shouldAutofocus(HTMLFormControlElement* element)
{
    if (!element->autofocus())
        return false;
    if (!element->renderer())
        return false;
    if (element->document()->ignoreAutofocus())
        return false;
    if (element->document()->isSandboxed(SandboxAutomaticFeatures))
        return false;
    if (element->hasAutofocused())
        return false;

    // FIXME: Should this set of hasTagName checks be replaced by a
    // virtual member function?
    if (element->hasTagName(inputTag))
        return !static_cast<HTMLInputElement*>(element)->isInputTypeHidden();
    if (element->hasTagName(selectTag))
        return true;
    if (element->hasTagName(keygenTag))
        return true;
    if (element->hasTagName(buttonTag))
        return true;
    if (element->hasTagName(textareaTag))
        return true;

    return false;
}

static void focusPostAttach(Node* element, unsigned)
{ 
    static_cast<Element*>(element)->focus(); 
    element->deref(); 
}

void HTMLFormControlElement::attach()
{
    ASSERT(!attached());

    suspendPostAttachCallbacks();

    HTMLElement::attach();

    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (renderer())
        renderer()->updateFromElement();

    if (shouldAutofocus(this)) {
        setAutofocused();
        ref();
        queuePostAttachCallback(focusPostAttach, this);
    }

    resumePostAttachCallbacks();
}

void HTMLFormControlElement::didMoveToNewDocument(Document* oldDocument)
{
    FormAssociatedElement::didMoveToNewDocument(oldDocument);
    HTMLElement::didMoveToNewDocument(oldDocument);
}

Node::InsertionNotificationRequest HTMLFormControlElement::insertedInto(Node* insertionPoint)
{
    m_dataListAncestorState = Unknown;
    setNeedsWillValidateCheck();
    HTMLElement::insertedInto(insertionPoint);
    FormAssociatedElement::insertedInto(insertionPoint);
    return InsertionDone;
}

void HTMLFormControlElement::removedFrom(Node* insertionPoint)
{
    m_fieldSetAncestorValid = false;
    m_dataListAncestorState = Unknown;
    HTMLElement::removedFrom(insertionPoint);
    FormAssociatedElement::removedFrom(insertionPoint);
}

const AtomicString& HTMLFormControlElement::formControlName() const
{
    const AtomicString& name = getNameAttribute();
    return name.isNull() ? emptyAtom : name;
}

void HTMLFormControlElement::setName(const AtomicString& value)
{
    setAttribute(nameAttr, value);
}

bool HTMLFormControlElement::wasChangedSinceLastFormControlChangeEvent() const
{
    return m_wasChangedSinceLastFormControlChangeEvent;
}

void HTMLFormControlElement::setChangedSinceLastFormControlChangeEvent(bool changed)
{
    m_wasChangedSinceLastFormControlChangeEvent = changed;
}

void HTMLFormControlElement::dispatchFormControlChangeEvent()
{
    HTMLElement::dispatchChangeEvent();
    setChangedSinceLastFormControlChangeEvent(false);
}

void HTMLFormControlElement::dispatchFormControlInputEvent()
{
    setChangedSinceLastFormControlChangeEvent(true);
    HTMLElement::dispatchInputEvent();
}

bool HTMLFormControlElement::disabled() const
{
    if (m_disabled)
        return true;

    if (!m_fieldSetAncestorValid)
        updateFieldSetAndLegendAncestor();

    // Form controls in the first legend element inside a fieldset are not affected by fieldset.disabled.
    if (m_fieldSetAncestor && m_fieldSetAncestor->disabled())
        return !(m_legendAncestor && m_legendAncestor == m_fieldSetAncestor->legend());
    return false;
}

void HTMLFormControlElement::setDisabled(bool b)
{
    setAttribute(disabledAttr, b ? "" : 0);
}

bool HTMLFormControlElement::autofocus() const
{
    return hasAttribute(autofocusAttr);
}

bool HTMLFormControlElement::required() const
{
    return m_required;
}

static void updateFromElementCallback(Node* node, unsigned)
{
    ASSERT_ARG(node, node->isElementNode());
    ASSERT_ARG(node, static_cast<Element*>(node)->isFormControlElement());
    ASSERT(node->renderer());
    if (RenderObject* renderer = node->renderer())
        renderer->updateFromElement();
}

void HTMLFormControlElement::didRecalcStyle(StyleChange)
{
    // updateFromElement() can cause the selection to change, and in turn
    // trigger synchronous layout, so it must not be called during style recalc.
    if (renderer())
        queuePostAttachCallback(updateFromElementCallback, this);
}

bool HTMLFormControlElement::supportsFocus() const
{
    return !m_disabled;
}

bool HTMLFormControlElement::isFocusable() const
{
    if (!renderer() || !renderer()->isBox() || toRenderBox(renderer())->size().isEmpty())
        return false;
    // HTMLElement::isFocusable handles visibility and calls suportsFocus which
    // will cover the disabled case.
    return HTMLElement::isFocusable();
}

bool HTMLFormControlElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (isFocusable())
        if (document()->frame())
            return document()->frame()->eventHandler()->tabsToAllFormControls(event);
    return false;
}

bool HTMLFormControlElement::isMouseFocusable() const
{
#if PLATFORM(GTK) || PLATFORM(QT)
    return HTMLElement::isMouseFocusable();
#else
    return false;
#endif
}

short HTMLFormControlElement::tabIndex() const
{
    // Skip the supportsFocus check in HTMLElement.
    return Element::tabIndex();
}

bool HTMLFormControlElement::recalcWillValidate() const
{
    if (m_dataListAncestorState == Unknown) {
        for (ContainerNode* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
            if (!m_legendAncestor && ancestor->hasTagName(datalistTag)) {
                m_dataListAncestorState = InsideDataList;
                break;
            }
        }
        if (m_dataListAncestorState == Unknown)
            m_dataListAncestorState = NotInsideDataList;
    }
    return m_dataListAncestorState == NotInsideDataList && !m_disabled && !m_readOnly;
}

bool HTMLFormControlElement::willValidate() const
{
    if (!m_willValidateInitialized || m_dataListAncestorState == Unknown) {
        m_willValidateInitialized = true;
        m_willValidate = recalcWillValidate();
    } else {
        // If the following assertion fails, setNeedsWillValidateCheck() is not
        // called correctly when something which changes recalcWillValidate() result
        // is updated.
        ASSERT(m_willValidate == recalcWillValidate());
    }
    return m_willValidate;
}

void HTMLFormControlElement::setNeedsWillValidateCheck()
{
    // We need to recalculate willValidate immediately because willValidate change can causes style change.
    bool newWillValidate = recalcWillValidate();
    if (m_willValidateInitialized && m_willValidate == newWillValidate)
        return;
    m_willValidateInitialized = true;
    m_willValidate = newWillValidate;
    setNeedsStyleRecalc();
    if (!m_willValidate)
        hideVisibleValidationMessage();
}

void HTMLFormControlElement::updateVisibleValidationMessage()
{
    Page* page = document()->page();
    if (!page)
        return;
    String message;
    if (renderer() && willValidate()) {
        message = validationMessage().stripWhiteSpace();
        // HTML5 specification doesn't ask UA to show the title attribute value
        // with the validationMessage.  However, this behavior is same as Opera
        // and the specification describes such behavior as an example.
        const AtomicString& title = getAttribute(titleAttr);
        if (!message.isEmpty() && !title.isEmpty()) {
            message.append('\n');
            message.append(title);
        }
    }
    if (message.isEmpty()) {
        hideVisibleValidationMessage();
        return;
    }
    if (!m_validationMessage) {
        m_validationMessage = ValidationMessage::create(this);
        m_validationMessage->setMessage(message);
    } else {
        // Call setMessage() even if m_validationMesage->message() == message
        // because the existing message might be to be hidden.
        m_validationMessage->setMessage(message);
    }
}

void HTMLFormControlElement::hideVisibleValidationMessage()
{
    if (m_validationMessage)
        m_validationMessage->requestToHideMessage();
}

String HTMLFormControlElement::visibleValidationMessage() const
{
    return m_validationMessage ? m_validationMessage->message() : String();
}

bool HTMLFormControlElement::checkValidity(Vector<RefPtr<FormAssociatedElement> >* unhandledInvalidControls)
{
    if (!willValidate() || isValidFormControlElement())
        return true;
    // An event handler can deref this object.
    RefPtr<HTMLFormControlElement> protector(this);
    RefPtr<Document> originalDocument(document());
    bool needsDefaultAction = dispatchEvent(Event::create(eventNames().invalidEvent, false, true));
    if (needsDefaultAction && unhandledInvalidControls && inDocument() && originalDocument == document())
        unhandledInvalidControls->append(this);
    return false;
}

bool HTMLFormControlElement::isValidFormControlElement()
{
    // If the following assertion fails, setNeedsValidityCheck() is not called
    // correctly when something which changes validity is updated.
    ASSERT(m_isValid == validity()->valid());
    return m_isValid;
}

void HTMLFormControlElement::setNeedsValidityCheck()
{
    bool newIsValid = validity()->valid();
    if (willValidate() && newIsValid != m_isValid) {
        // Update style for pseudo classes such as :valid :invalid.
        setNeedsStyleRecalc();
    }
    m_isValid = newIsValid;

    // Updates only if this control already has a validtion message.
    if (!visibleValidationMessage().isEmpty()) {
        // Calls updateVisibleValidationMessage() even if m_isValid is not
        // changed because a validation message can be chagned.
        updateVisibleValidationMessage();
    }
}

void HTMLFormControlElement::setCustomValidity(const String& error)
{
    FormAssociatedElement::setCustomValidity(error);
    setNeedsValidityCheck();
}

void HTMLFormControlElement::dispatchBlurEvent(PassRefPtr<Node> newFocusedNode)
{
    HTMLElement::dispatchBlurEvent(newFocusedNode);
    hideVisibleValidationMessage();
}

HTMLFormElement* HTMLFormControlElement::virtualForm() const
{
    return FormAssociatedElement::form();
}

bool HTMLFormControlElement::isDefaultButtonForForm() const
{
    return isSuccessfulSubmitButton() && form() && form()->defaultButton() == this;
}

HTMLFormControlElement* HTMLFormControlElement::enclosingFormControlElement(Node* node)
{
    for (; node; node = node->parentNode()) {
        if (node->isElementNode() && toElement(node)->isFormControlElement())
            return static_cast<HTMLFormControlElement*>(node);
    }
    return 0;
}

} // namespace Webcore
