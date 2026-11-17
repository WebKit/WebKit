/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CheckboxInputType.h"

#include "CheckboxSwitch.h"
#include "ContainerNodeInlines.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLInputElementInlines.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "ScopedEventQueue.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "UserAgentParts.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(IOS_TOUCH_EVENTS)
#include "TouchEvent.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CheckboxInputType);

CheckboxInputType::CheckboxInputType(HTMLInputElement& element)
    : BaseCheckableInputType(Type::Checkbox, element)
{
}

CheckboxInputType::~CheckboxInputType() = default;

const AtomString& CheckboxInputType::formControlType() const
{
    return InputTypeNames::checkbox();
}

bool CheckboxInputType::valueMissing(StringView) const
{
    ASSERT(element());
    return element()->isRequired() && !element()->checked();
}

String CheckboxInputType::valueMissingText() const
{
    if (isSwitch())
        return validationMessageValueMissingForSwitchText();
    return validationMessageValueMissingForCheckboxText();
}

void CheckboxInputType::createShadowSubtree()
{
    ASSERT(needsShadowSubtree());
    ASSERT(element());
    Ref element = *this->element();
    ASSERT(element->userAgentShadowRoot());
    Ref shadowRoot = *element->userAgentShadowRoot();
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { shadowRoot };

    Ref document = element->document();
    Ref track = HTMLDivElement::create(document);
    {
        ScriptDisallowedScope::EventAllowedScope eventAllowedScopeBeforeAppend { track };
        track->setUserAgentPart(UserAgentParts::sliderTrack());
    }
    shadowRoot->appendChild(ContainerNode::ChildChange::Source::Parser, track);
    Ref thumb = HTMLDivElement::create(document);
    {
        ScriptDisallowedScope::EventAllowedScope eventAllowedScopeBeforeAppend { thumb };
        thumb->setUserAgentPart(UserAgentParts::sliderThumb());
    }
    shadowRoot->appendChild(ContainerNode::ChildChange::Source::Parser, thumb);
}

void CheckboxInputType::handleKeyupEvent(KeyboardEvent& event)
{
    const String& key = event.keyIdentifier();
    if (key != "U+0020"_s)
        return;
    dispatchSimulatedClickIfActive(event);
}

void CheckboxInputType::handleMouseDownEvent(MouseEvent& event)
{
    if (!event.isTrusted() || !isSwitch())
        return;

    protect(ensureSwitch())->handleMouseDownEvent(event);
}

void CheckboxInputType::handleMouseMoveEvent(MouseEvent& event)
{
    if (m_switch)
        protect(m_switch)->handleMouseMoveEvent(event);
}

void CheckboxInputType::handleMouseUpEvent(MouseEvent& event)
{
    if (m_switch)
        protect(m_switch)->handleMouseUpEvent(event);
}

#if ENABLE(IOS_TOUCH_EVENTS)
void CheckboxInputType::handleTouchEvent(TouchEvent& event)
{
    if (!isSwitch())
        return;

    if (protect(ensureSwitch())->handleTouchEvent(event))
        protect(element())->dispatchSimulatedClick(&event, SendNoEvents);
}
#endif

void CheckboxInputType::willDispatchClick(InputElementClickState& state)
{
    ASSERT(element());
    Ref element = *this->element();

    // An event handler can use preventDefault or "return false" to reverse the checking we do here.
    // The InputElementClickState object contains what we need to undo what we did here in didDispatchClick.

    state.checked = element->checked();
    state.indeterminate = element->indeterminate();

    if (state.indeterminate)
        element->setIndeterminate(false);

    if (m_switch && protect(m_switch)->willSkipToggleForClick(state.checked))
        return;

    element->setChecked(!state.checked, state.trusted ? WasSetByJavaScript::No : WasSetByJavaScript::Yes);

    if (isSwitch() && state.trusted)
        protect(ensureSwitch())->didToggleForClick(state.checked);
    else if (m_switch)
        protect(m_switch)->stopSwitchPointerTracking();
}

void CheckboxInputType::didDispatchClick(Event& event, const InputElementClickState& state)
{
    bool didSkipSwitchToggle = m_switch && protect(m_switch)->takeDidSkipSwitchToggle();
    if (event.defaultPrevented() || event.defaultHandled()) {
        ASSERT(element());
        Ref element = *this->element();
        element->setIndeterminate(state.indeterminate);
        element->setChecked(state.checked);
    } else if (!didSkipSwitchToggle)
        fireInputAndChangeEvents();

    // The work we did in willDispatchClick was default handling.
    event.setDefaultHandled();
}

bool CheckboxInputType::matchesIndeterminatePseudoClass() const
{
    ASSERT(element());
    return element()->indeterminate() && !isSwitch();
}

void CheckboxInputType::disabledStateChanged()
{
    if (!isSwitch())
        return;

    ASSERT(element());
    if (m_switch && element()->isDisabledFormControl())
        protect(m_switch)->stopAnimationsAndPointerTracking();

#if ENABLE(TOUCH_EVENTS)
    if (isSwitch())
        protect(element())->updateTouchEventHandler();
#endif
}

void CheckboxInputType::willUpdateCheckedness(bool, WasSetByJavaScript wasCheckedByJavaScript)
{
    ASSERT(element());
    if (m_switch && wasCheckedByJavaScript == WasSetByJavaScript::Yes)
        protect(m_switch)->stopAnimationsAndPointerTracking();
}

void CheckboxInputType::attributeChanged(const QualifiedName& name)
{
    if (m_switch && name == HTMLNames::switchAttr && !isSwitch())
        m_switch = nullptr;

    BaseCheckableInputType::attributeChanged(name);
}

CheckboxSwitch& CheckboxInputType::ensureSwitch()
{
    ASSERT(element());
    ASSERT(isSwitch());
    if (!m_switch)
        m_switch = makeUnique<CheckboxSwitch>(*protect(element()));
    return *m_switch;
}

float CheckboxInputType::switchAnimationVisuallyOnProgress() const
{
    ASSERT(isSwitch());
    return m_switch ? protect(m_switch)->switchAnimationVisuallyOnProgress() : 1.0f;
}

bool CheckboxInputType::isSwitchVisuallyOn() const
{
    ASSERT(element());
    ASSERT(isSwitch());
    return m_switch ? m_switch->isSwitchVisuallyOn() : element()->checked();
}

float CheckboxInputType::switchAnimationHeldProgress() const
{
    ASSERT(isSwitch());
    return m_switch ? protect(m_switch)->switchAnimationHeldProgress() : 1.0f;
}

bool CheckboxInputType::isSwitchHeld() const
{
    ASSERT(isSwitch());
    return m_switch && m_switch->isSwitchHeld();
}

} // namespace WebCore
