/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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
#include "HTMLButtonElement.h"

#include "DOMFormData.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "RenderButton.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLButtonElement);

using namespace HTMLNames;

inline HTMLButtonElement::HTMLButtonElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLFormControlElement(tagName, document, form)
    , m_type(SUBMIT)
    , m_isActivatedSubmit(false)
{
    ASSERT(hasTagName(buttonTag));
}

Ref<HTMLButtonElement> HTMLButtonElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    return adoptRef(*new HTMLButtonElement(tagName, document, form));
}

void HTMLButtonElement::setType(const AtomString& type)
{
    setAttributeWithoutSynchronization(typeAttr, type);
}

RenderPtr<RenderElement> HTMLButtonElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& position)
{
    // https://html.spec.whatwg.org/multipage/rendering.html#button-layout
    DisplayType display = style.display();
    if (display == DisplayType::InlineGrid || display == DisplayType::Grid || display == DisplayType::InlineFlex || display == DisplayType::Flex)
        return HTMLFormControlElement::createElementRenderer(WTFMove(style), position);
    return createRenderer<RenderButton>(*this, WTFMove(style));
}

int HTMLButtonElement::defaultTabIndex() const
{
    return 0;
}

const AtomString& HTMLButtonElement::formControlType() const
{
    switch (m_type) {
        case SUBMIT: {
            static MainThreadNeverDestroyed<const AtomString> submit("submit", AtomString::ConstructFromLiteral);
            return submit;
        }
        case BUTTON: {
            static MainThreadNeverDestroyed<const AtomString> button("button", AtomString::ConstructFromLiteral);
            return button;
        }
        case RESET: {
            static MainThreadNeverDestroyed<const AtomString> reset("reset", AtomString::ConstructFromLiteral);
            return reset;
        }
    }

    ASSERT_NOT_REACHED();
    return emptyAtom();
}

bool HTMLButtonElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == alignAttr) {
        // Don't map 'align' attribute.  This matches what Firefox and IE do, but not Opera.
        // See http://bugs.webkit.org/show_bug.cgi?id=12071
        return false;
    }

    return HTMLFormControlElement::hasPresentationalHintsForAttribute(name);
}

void HTMLButtonElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == typeAttr) {
        Type oldType = m_type;
        if (equalLettersIgnoringASCIICase(value, "reset"))
            m_type = RESET;
        else if (equalLettersIgnoringASCIICase(value, "button"))
            m_type = BUTTON;
        else
            m_type = SUBMIT;
        if (oldType != m_type) {
            updateWillValidateAndValidity();
            if (form() && (oldType == SUBMIT || m_type == SUBMIT))
                form()->resetDefaultButton();
        }
    } else
        HTMLFormControlElement::parseAttribute(name, value);
}

void HTMLButtonElement::defaultEventHandler(Event& event)
{
#if ENABLE(SERVICE_CONTROLS)
    if (ImageControlsMac::handleEvent(*this, event))
        return;
#endif
    if (event.type() == eventNames().DOMActivateEvent && !isDisabledFormControl()) {
        RefPtr<HTMLFormElement> protectedForm(form());

        if (protectedForm) {
            // Update layout before processing form actions in case the style changes
            // the Form or button relationships.
            document().updateLayoutIgnorePendingStylesheets();

            if (auto currentForm = form()) {
                if (m_type == SUBMIT) {
                    SetForScope<bool> activatedSubmitState(m_isActivatedSubmit, true);
                    currentForm->submitIfPossible(&event, this);
                }

                if (m_type == RESET)
                    currentForm->reset();
            }

            if (m_type == SUBMIT || m_type == RESET)
                event.setDefaultHandled();
        }
    }

    if (is<KeyboardEvent>(event)) {
        KeyboardEvent& keyboardEvent = downcast<KeyboardEvent>(event);
        if (keyboardEvent.type() == eventNames().keydownEvent && keyboardEvent.keyIdentifier() == "U+0020") {
            setActive(true, true);
            // No setDefaultHandled() - IE dispatches a keypress in this case.
            return;
        }
        if (keyboardEvent.type() == eventNames().keypressEvent) {
            switch (keyboardEvent.charCode()) {
                case '\r':
                    dispatchSimulatedClick(&keyboardEvent);
                    keyboardEvent.setDefaultHandled();
                    return;
                case ' ':
                    // Prevent scrolling down the page.
                    keyboardEvent.setDefaultHandled();
                    return;
            }
        }
        if (keyboardEvent.type() == eventNames().keyupEvent && keyboardEvent.keyIdentifier() == "U+0020") {
            if (active())
                dispatchSimulatedClick(&keyboardEvent);
            keyboardEvent.setDefaultHandled();
            return;
        }
    }

    HTMLFormControlElement::defaultEventHandler(event);
}

bool HTMLButtonElement::willRespondToMouseClickEvents()
{
    return !isDisabledFormControl();
}

bool HTMLButtonElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names to be considered successful.
    // However, other browsers do not impose this constraint.
    return m_type == SUBMIT && !isDisabledFormControl();
}

bool HTMLButtonElement::matchesDefaultPseudoClass() const
{
    return isSuccessfulSubmitButton() && form() && form()->defaultButton() == this;
}

bool HTMLButtonElement::isActivatedSubmit() const
{
    return m_isActivatedSubmit;
}

void HTMLButtonElement::setActivatedSubmit(bool flag)
{
    m_isActivatedSubmit = flag;
}

bool HTMLButtonElement::appendFormData(DOMFormData& formData)
{
    if (m_type != SUBMIT || name().isEmpty() || !m_isActivatedSubmit)
        return false;
    formData.append(name(), value());
    return true;
}

bool HTMLButtonElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == formactionAttr || HTMLFormControlElement::isURLAttribute(attribute);
}

const AtomString& HTMLButtonElement::value() const
{
    return attributeWithoutSynchronization(valueAttr);
}

bool HTMLButtonElement::computeWillValidate() const
{
    return m_type == SUBMIT && HTMLFormControlElement::computeWillValidate();
}

bool HTMLButtonElement::isSubmitButton() const
{
    return m_type == SUBMIT;
}

bool HTMLButtonElement::isExplicitlySetSubmitButton() const
{
    return isSubmitButton() && hasAttributeWithoutSynchronization(HTMLNames::typeAttr);
}

} // namespace
