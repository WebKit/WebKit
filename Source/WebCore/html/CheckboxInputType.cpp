/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#include "HTMLInputElement.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "RenderElement.h"
#include "RenderStyleInlines.h"
#include "RenderTheme.h"
#include "ScopedEventQueue.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "SwitchThumbElement.h"
#include "SwitchTrackElement.h"

namespace WebCore {

const AtomString& CheckboxInputType::formControlType() const
{
    return InputTypeNames::checkbox();
}

bool CheckboxInputType::valueMissing(const String&) const
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
    ASSERT(element()->userAgentShadowRoot());

    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { *element()->userAgentShadowRoot() };

    Ref document = element()->document();
    element()->userAgentShadowRoot()->appendChild(ContainerNode::ChildChange::Source::Parser, SwitchTrackElement::create(document));
    element()->userAgentShadowRoot()->appendChild(ContainerNode::ChildChange::Source::Parser, SwitchThumbElement::create(document));
}

void CheckboxInputType::handleKeyupEvent(KeyboardEvent& event)
{
    const String& key = event.keyIdentifier();
    if (key != "U+0020"_s)
        return;
    dispatchSimulatedClickIfActive(event);
}

void CheckboxInputType::willDispatchClick(InputElementClickState& state)
{
    ASSERT(element());

    // An event handler can use preventDefault or "return false" to reverse the checking we do here.
    // The InputElementClickState object contains what we need to undo what we did here in didDispatchClick.

    state.checked = element()->checked();
    state.indeterminate = element()->indeterminate();

    if (state.indeterminate)
        element()->setIndeterminate(false);

    element()->setChecked(!state.checked, state.trusted ? WasSetByJavaScript::No : WasSetByJavaScript::Yes);
}

void CheckboxInputType::didDispatchClick(Event& event, const InputElementClickState& state)
{
    if (event.defaultPrevented() || event.defaultHandled()) {
        ASSERT(element());
        element()->setIndeterminate(state.indeterminate);
        element()->setChecked(state.checked);
    } else
        fireInputAndChangeEvents();

    // The work we did in willDispatchClick was default handling.
    event.setDefaultHandled();
}

bool CheckboxInputType::matchesIndeterminatePseudoClass() const
{
    return shouldAppearIndeterminate();
}

bool CheckboxInputType::shouldAppearIndeterminate() const
{
    ASSERT(element());
    return element()->indeterminate() && !isSwitch();
}

void CheckboxInputType::disabledStateChanged()
{
    stopSwitchCheckedChangeAnimation();
}

// FIXME: ideally CheckboxInputType would not be responsible for the timer specifics and instead
// ask a more knowledgable system for a refresh callback (perhaps passing a desired FPS).
static Seconds switchCheckedChangeAnimationUpdateInterval(HTMLInputElement* element)
{
    if (auto* page = element->document().page())
        return page->preferredRenderingUpdateInterval();
    return 0_s;
}

void CheckboxInputType::performSwitchCheckedChangeAnimation(WasSetByJavaScript wasCheckedByJavaScript)
{
    ASSERT(isSwitch());
    ASSERT(element());
    ASSERT(element()->renderer());
    ASSERT(element()->renderer()->style().hasEffectiveAppearance());

    if (wasCheckedByJavaScript == WasSetByJavaScript::Yes) {
        stopSwitchCheckedChangeAnimation();
        return;
    }

    auto updateInterval = switchCheckedChangeAnimationUpdateInterval(element());
    auto duration = RenderTheme::singleton().switchCheckedChangeAnimationDuration();

    if (!m_switchCheckedChangeAnimationTimer) {
        if (!(duration > 0_s && updateInterval > 0_s))
            return;
        m_switchCheckedChangeAnimationTimer = makeUnique<Timer>(*this, &CheckboxInputType::switchCheckedChangeAnimationTimerFired);
    }
    ASSERT(duration > 0_s);
    ASSERT(updateInterval > 0_s);
    ASSERT(m_switchCheckedChangeAnimationTimer);

    auto isAnimating = m_switchCheckedChangeAnimationStartTime != 0_s;
    auto currentTime = MonotonicTime::now().secondsSinceEpoch();
    auto remainingTime = currentTime - m_switchCheckedChangeAnimationStartTime;
    auto startTimeOffset = 0_s;
    if (isAnimating && remainingTime < duration)
        startTimeOffset = duration - remainingTime;

    m_switchCheckedChangeAnimationStartTime = MonotonicTime::now().secondsSinceEpoch() - startTimeOffset;
    m_switchCheckedChangeAnimationTimer->startOneShot(updateInterval);
}

void CheckboxInputType::stopSwitchCheckedChangeAnimation()
{
    m_switchCheckedChangeAnimationStartTime = 0_s;
}

float CheckboxInputType::switchCheckedChangeAnimationProgress() const
{
    ASSERT(isSwitch());

    auto isAnimating = m_switchCheckedChangeAnimationStartTime != 0_s;
    if (!isAnimating)
        return 1.0f;
    auto duration = RenderTheme::singleton().switchCheckedChangeAnimationDuration();
    return std::min((float)((MonotonicTime::now().secondsSinceEpoch() - m_switchCheckedChangeAnimationStartTime) / duration), 1.0f);
}

void CheckboxInputType::switchCheckedChangeAnimationTimerFired()
{
    ASSERT(m_switchCheckedChangeAnimationTimer);
    if (!isSwitch() || !element() || !element()->renderer())
        return;

    auto updateInterval = switchCheckedChangeAnimationUpdateInterval(element());
    if (!(updateInterval > 0_s))
        return;

    auto currentTime = MonotonicTime::now().secondsSinceEpoch();
    auto duration = RenderTheme::singleton().switchCheckedChangeAnimationDuration();
    if (currentTime - m_switchCheckedChangeAnimationStartTime < duration)
        m_switchCheckedChangeAnimationTimer->startOneShot(updateInterval);
    else
        stopSwitchCheckedChangeAnimation();

    element()->renderer()->repaint();
}

} // namespace WebCore
