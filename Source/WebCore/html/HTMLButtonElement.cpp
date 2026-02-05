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

#include "CommandEvent.h"
#include "CommonAtomStrings.h"
#include "DOMFormData.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "RenderButton.h"
#include "RenderStyle+GettersInlines.h"
#include "Settings.h"
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
#include "SpatialImageControls.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLButtonElement);

using namespace HTMLNames;

inline HTMLButtonElement::HTMLButtonElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLFormControlElement(tagName, document, form)
    , m_type(Type::Submit)
    , m_isActivatedSubmit(false)
{
    ASSERT(hasTagName(buttonTag));
}

Ref<HTMLButtonElement> HTMLButtonElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    return adoptRef(*new HTMLButtonElement(tagName, document, form));
}

Ref<HTMLButtonElement> HTMLButtonElement::create(Document& document)
{
    return adoptRef(*new HTMLButtonElement(buttonTag, document, nullptr));
}

RenderPtr<RenderElement> HTMLButtonElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& position)
{
    // https://html.spec.whatwg.org/multipage/rendering.html#button-layout
    if (style.isDisplayFlexibleOrGridFormattingContextBox())
        return HTMLFormControlElement::createElementRenderer(WTF::move(style), position);
    return createRenderer<RenderButton>(*this, WTF::move(style));
}

int HTMLButtonElement::defaultTabIndex() const
{
    return 0;
}

const AtomString& HTMLButtonElement::formControlType() const
{
    switch (m_type) {
    case Type::Submit:
        return submitAtom();
    case Type::Button:
        return HTMLNames::buttonTag->localName();
    case Type::Reset:
        return resetAtom();
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

void HTMLButtonElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == typeAttr)
        computeType(newValue);
    else if ((name == commandAttr || name == commandforAttr) && document().settings().commandAttributesEnabled())
        computeType(attributeWithoutSynchronization(HTMLNames::typeAttr));
    else
        HTMLFormControlElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

RefPtr<Element> HTMLButtonElement::commandForElement() const
{
    auto canInvoke = [](const HTMLFormControlElement& element) -> bool {
        if (!element.document().settings().commandAttributesEnabled())
            return false;
        return is<HTMLButtonElement>(element);
    };

    if (!canInvoke(*this))
        return nullptr;

    return elementForAttributeInternal(commandforAttr);
}

static const AtomString& togglePopoverAtom()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("toggle-popover"_s);
    return identifier;
}

static const AtomString& showPopoverAtom()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("show-popover"_s);
    return identifier;
}

static const AtomString& hidePopoverAtom()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("hide-popover"_s);
    return identifier;
}

static const AtomString& closeAtom()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("close"_s);
    return identifier;
}

static const AtomString& requestCloseAtom()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("request-close"_s);
    return identifier;
}

static const AtomString& showModalAtom()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("show-modal"_s);
    return identifier;
}

CommandType HTMLButtonElement::commandType() const
{
    auto action = attributeWithoutSynchronization(HTMLNames::commandAttr);
    if (action.isNull() || action.isEmpty())
        return CommandType::Invalid;

    if (equalIgnoringASCIICase(action, togglePopoverAtom()))
        return CommandType::TogglePopover;

    if (equalIgnoringASCIICase(action, showPopoverAtom()))
        return CommandType::ShowPopover;

    if (equalIgnoringASCIICase(action, hidePopoverAtom()))
        return CommandType::HidePopover;

    if (equalIgnoringASCIICase(action, showModalAtom()))
        return CommandType::ShowModal;

    if (equalIgnoringASCIICase(action, closeAtom()))
        return CommandType::Close;

    if (equalIgnoringASCIICase(action, requestCloseAtom()))
        return CommandType::RequestClose;

    if (action.startsWith("--"_s))
        return CommandType::Custom;

    return CommandType::Invalid;
}

void HTMLButtonElement::handleCommand()
{
    RefPtr invokee = commandForElement();
    if (!invokee)
        return;

    auto commandRaw = attributeWithoutSynchronization(HTMLNames::commandAttr);
    auto command = commandType();

    if (command == CommandType::Invalid)
        return;

    if (command != CommandType::Custom && !invokee->isValidCommandType(command))
        return;

    CommandEvent::Init init {
        { false, true, false },
        this,
        commandRaw.isNull() ? emptyAtom() : commandRaw,
    };
    Ref event = CommandEvent::create(eventNames().commandEvent, WTF::move(init),
        CommandEvent::IsTrusted::Yes);
    invokee->dispatchEvent(event);

    if (!event->defaultPrevented() && command != CommandType::Custom)
        invokee->handleCommandInternal(*this, command);
}

const AtomString& HTMLButtonElement::command() const
{
    switch (commandType()) {
    case CommandType::TogglePopover:
        return togglePopoverAtom();
    case CommandType::ShowPopover:
        return showPopoverAtom();
    case CommandType::HidePopover:
        return hidePopoverAtom();
    case CommandType::Close:
        return closeAtom();
    case CommandType::RequestClose:
        return requestCloseAtom();
    case CommandType::ShowModal:
        return showModalAtom();
    case CommandType::Custom:
        return attributeWithoutSynchronization(HTMLNames::commandAttr);
    case CommandType::Invalid:
        return emptyAtom();
    }

    ASSERT_NOT_REACHED();
    return nullAtom();
}

void HTMLButtonElement::defaultEventHandler(Event& event)
{
#if ENABLE(SERVICE_CONTROLS)
    if (ImageControlsMac::handleEvent(*this, event))
        return;
#endif
#if ENABLE(SPATIAL_IMAGE_CONTROLS)
    if (SpatialImageControls::handleEvent(*this, event))
        return;
#endif
    auto& eventNames = WebCore::eventNames();
    if (event.type() == eventNames.DOMActivateEvent && !isDisabledFormControl()) {
        if (form()) {
            // Update layout before processing form actions in case the style changes
            // the Form or button relationships.
            protect(document())->updateLayoutIgnorePendingStylesheets();

            if (RefPtr currentForm = form()) {
                if (m_type == Type::Submit)
                    currentForm->submitIfPossible(&event, this);

                if (m_type == Type::Reset)
                    currentForm->reset();
            }

            if (m_type == Type::Submit || m_type == Type::Reset) {
                event.setDefaultHandled();
                return;
            }

            if (m_type == Type::Button && !equalLettersIgnoringASCIICase(attributeWithoutSynchronization(HTMLNames::typeAttr), "button"_s))
                return;
        }

        if (commandForElement()) {
            handleCommand();
            return;
        }

        handlePopoverTargetAction(event.protectedTarget().get());
    }

    if (RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event)) {
        if (keyboardEvent->type() == eventNames.keydownEvent && keyboardEvent->keyIdentifier() == "U+0020"_s) {
            setActive(true);
            // No setDefaultHandled() - IE dispatches a keypress in this case.
            return;
        }
        if (keyboardEvent->type() == eventNames.keypressEvent) {
            switch (keyboardEvent->charCode()) {
                case '\r':
                    dispatchSimulatedClick(keyboardEvent.get());
                    keyboardEvent->setDefaultHandled();
                    return;
                case ' ':
                    // Prevent scrolling down the page.
                    keyboardEvent->setDefaultHandled();
                    return;
            }
        }
        if (keyboardEvent->type() == eventNames.keyupEvent && keyboardEvent->keyIdentifier() == "U+0020"_s) {
            if (active())
                dispatchSimulatedClick(keyboardEvent.get());
            keyboardEvent->setDefaultHandled();
            return;
        }
    }

    HTMLFormControlElement::defaultEventHandler(event);
}

bool HTMLButtonElement::willRespondToMouseClickEventsWithEditability(Editability) const
{
    return !isDisabledFormControl();
}

bool HTMLButtonElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names to be considered successful.
    // However, other browsers do not impose this constraint.
    return m_type == Type::Submit;
}

bool HTMLButtonElement::matchesDefaultPseudoClass() const
{
    RefPtr form = this->form();
    return isSuccessfulSubmitButton() && form && form->defaultButton() == this;
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
    if (m_type != Type::Submit || name().isEmpty() || !m_isActivatedSubmit)
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
    return m_type == Type::Submit && HTMLFormControlElement::computeWillValidate();
}

bool HTMLButtonElement::isSubmitButton() const
{
    return m_type == Type::Submit;
}

bool HTMLButtonElement::isExplicitlySetSubmitButton() const
{
    return isSubmitButton() && hasAttributeWithoutSynchronization(HTMLNames::typeAttr);
}

void HTMLButtonElement::computeType(const AtomString& typeAttrValue)
{
    auto oldType = m_type;
    if (equalLettersIgnoringASCIICase(typeAttrValue, "reset"_s))
        m_type = Type::Reset;
    else if (equalLettersIgnoringASCIICase(typeAttrValue, "button"_s))
        m_type = Type::Button;
    else if (equalLettersIgnoringASCIICase(typeAttrValue, "submit"_s))
        m_type = Type::Submit;
    else if (document().settings().commandAttributesEnabled()) {
        if (hasAttributeWithoutSynchronization(HTMLNames::commandAttr) || hasAttributeWithoutSynchronization(HTMLNames::commandforAttr))
            m_type = Type::Button;
        else
            m_type = Type::Submit;
    } else
        m_type = Type::Submit;
    if (oldType != m_type) {
        updateWillValidateAndValidity();
        if (RefPtr currentForm = form(); currentForm && (oldType == Type::Submit || m_type == Type::Submit))
            currentForm->resetDefaultButton();
    }
}

} // namespace
