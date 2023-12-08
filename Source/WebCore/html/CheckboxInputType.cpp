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

#include "ChromeClient.h"
#include "EventHandler.h"
#include "HTMLInputElement.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
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

void CheckboxInputType::handleMouseDownEvent(MouseEvent& event)
{
    ASSERT(element());

    if (!event.isTrusted() || !isSwitch() || element()->isDisabledFormControl() || !element()->renderer())
        return;
    m_isSwitchVisuallyOn = element()->checked();
    startSwitchPointerTracking(event.absoluteLocation());
}

void CheckboxInputType::handleMouseMoveEvent(MouseEvent& event)
{
    ASSERT(element());

    if (!isSwitchPointerTracking())
        return;

    ASSERT(!element()->isDisabledFormControl());

    if (!event.isTrusted() || !isSwitch() || !element()->renderer()) {
        stopSwitchPointerTracking();
        return;
    }

    updateIsSwitchVisuallyOnFromAbsoluteLocation(event.absoluteLocation());
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

    if (isSwitchPointerTracking() && m_hasSwitchVisuallyOnChanged && m_isSwitchVisuallyOn == state.checked) {
        stopSwitchPointerTracking();
        return;
    }

    element()->setChecked(!state.checked, state.trusted ? WasSetByJavaScript::No : WasSetByJavaScript::Yes);

    if (isSwitch() && state.trusted && !(isSwitchPointerTracking() && m_hasSwitchVisuallyOnChanged && m_isSwitchVisuallyOn == !state.checked))
        performSwitchAnimation(SwitchAnimationType::VisuallyOn);

    stopSwitchPointerTracking();
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

void CheckboxInputType::startSwitchPointerTracking(LayoutPoint absoluteLocation)
{
    ASSERT(element());
    ASSERT(element()->renderer());
    if (RefPtr frame = element()->document().frame()) {
        frame->eventHandler().setCapturingMouseEventsElement(element());
        m_isSwitchVisuallyOn = element()->checked();
        m_switchPointerTrackingXPositionStart = element()->renderer()->absoluteToLocal(absoluteLocation, UseTransforms).x();
    }
}

void CheckboxInputType::stopSwitchPointerTracking()
{
    ASSERT(element());
    if (!isSwitchPointerTracking())
        return;

    if (RefPtr frame = element()->document().frame())
        frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    m_hasSwitchVisuallyOnChanged = false;
    m_switchPointerTrackingXPositionStart = std::nullopt;
}

bool CheckboxInputType::isSwitchPointerTracking() const
{
    return !!m_switchPointerTrackingXPositionStart;
}

bool CheckboxInputType::matchesIndeterminatePseudoClass() const
{
    ASSERT(element());
    return element()->indeterminate() && !isSwitch();
}

void CheckboxInputType::disabledStateChanged()
{
    ASSERT(element());
    if (isSwitch() && element()->isDisabledFormControl()) {
        stopSwitchAnimation(SwitchAnimationType::VisuallyOn);
        stopSwitchPointerTracking();
    }
}

void CheckboxInputType::willUpdateCheckedness(bool, WasSetByJavaScript wasCheckedByJavaScript)
{
    ASSERT(element());
    if (isSwitch() && wasCheckedByJavaScript == WasSetByJavaScript::Yes) {
        stopSwitchAnimation(SwitchAnimationType::VisuallyOn);
        stopSwitchPointerTracking();
    }
}

// FIXME: ideally CheckboxInputType would not be responsible for the timer specifics and instead
// ask a more knowledgable system for a refresh callback (perhaps passing a desired FPS).
static Seconds switchAnimationUpdateInterval(HTMLInputElement* element)
{
    if (auto* page = element->document().page())
        return page->preferredRenderingUpdateInterval();
    return 0_s;
}

static Seconds switchAnimationDuration(SwitchAnimationType)
{
    return RenderTheme::singleton().switchAnimationVisuallyOnDuration();
}

Seconds CheckboxInputType::switchAnimationStartTime(SwitchAnimationType) const
{
    return m_switchAnimationVisuallyOnStartTime;
}

void CheckboxInputType::setSwitchAnimationStartTime(SwitchAnimationType, Seconds time)
{
    m_switchAnimationVisuallyOnStartTime = time;
}

bool CheckboxInputType::isSwitchAnimating(SwitchAnimationType type) const
{
    return switchAnimationStartTime(type) != 0_s;
}

void CheckboxInputType::performSwitchAnimation(SwitchAnimationType type)
{
    ASSERT(isSwitch());
    ASSERT(element());
    ASSERT(element()->renderer());

    if (!element()->renderer()->style().hasEffectiveAppearance())
        return;

    auto updateInterval = switchAnimationUpdateInterval(element());
    auto duration = switchAnimationDuration(type);

    if (!m_switchAnimationTimer) {
        if (!(duration > 0_s && updateInterval > 0_s))
            return;
        m_switchAnimationTimer = makeUnique<Timer>(*this, &CheckboxInputType::switchAnimationTimerFired);
    }
    ASSERT(duration > 0_s);
    ASSERT(updateInterval > 0_s);
    ASSERT(m_switchAnimationTimer);

    auto currentTime = MonotonicTime::now().secondsSinceEpoch();
    auto remainingTime = currentTime - switchAnimationStartTime(type);
    auto startTimeOffset = 0_s;
    if (isSwitchAnimating(type) && remainingTime < duration)
        startTimeOffset = duration - remainingTime;

    setSwitchAnimationStartTime(type, MonotonicTime::now().secondsSinceEpoch() - startTimeOffset);
    m_switchAnimationTimer->startOneShot(updateInterval);
}

void CheckboxInputType::stopSwitchAnimation(SwitchAnimationType type)
{
    setSwitchAnimationStartTime(type, 0_s);
}

float CheckboxInputType::switchAnimationProgress(SwitchAnimationType type) const
{
    if (!isSwitchAnimating(type))
        return 1.0f;
    auto duration = switchAnimationDuration(type);
    return std::min((float)((MonotonicTime::now().secondsSinceEpoch() - switchAnimationStartTime(type)) / duration), 1.0f);
}

float CheckboxInputType::switchAnimationVisuallyOnProgress() const
{
    ASSERT(isSwitch());
    ASSERT(switchAnimationDuration(SwitchAnimationType::VisuallyOn) > 0_s);

    return switchAnimationProgress(SwitchAnimationType::VisuallyOn);
}

bool CheckboxInputType::isSwitchVisuallyOn() const
{
    ASSERT(element());
    ASSERT(isSwitch());
    return isSwitchPointerTracking() ? m_isSwitchVisuallyOn : element()->checked();
}

void CheckboxInputType::updateIsSwitchVisuallyOnFromAbsoluteLocation(LayoutPoint absoluteLocation)
{
    auto xPosition = element()->renderer()->absoluteToLocal(absoluteLocation, UseTransforms).x();
    auto isSwitchVisuallyOn = m_isSwitchVisuallyOn;
    auto isRTL = element()->computedStyle()->direction() == TextDirection::RTL;
    auto switchThumbIsLeft = (!isRTL && !isSwitchVisuallyOn) || (isRTL && isSwitchVisuallyOn);
    auto switchTrackRect = element()->renderer()->absoluteBoundingBoxRect();
    auto switchThumbLength = switchTrackRect.height();
    auto switchTrackWidth = switchTrackRect.width();

    auto changePosition = switchTrackWidth / 2;
    if (!m_hasSwitchVisuallyOnChanged) {
        auto switchTrackNoThumbWidth = switchTrackWidth - switchThumbLength;
        auto changeOffset = switchTrackWidth * RenderTheme::singleton().switchPointerTrackingMagnitudeProportion();
        if (switchThumbIsLeft && *m_switchPointerTrackingXPositionStart > switchTrackNoThumbWidth)
            changePosition = *m_switchPointerTrackingXPositionStart + changeOffset;
        else if (!switchThumbIsLeft && *m_switchPointerTrackingXPositionStart < switchTrackNoThumbWidth)
            changePosition = *m_switchPointerTrackingXPositionStart - changeOffset;
    }

    auto switchThumbIsLeftNow = xPosition < changePosition;
    if (switchThumbIsLeftNow != switchThumbIsLeft) {
        m_hasSwitchVisuallyOnChanged = true;
        m_isSwitchVisuallyOn = !m_isSwitchVisuallyOn;
        performSwitchAnimation(SwitchAnimationType::VisuallyOn);
        if (auto* page = element()->document().page())
            page->chrome().client().performSwitchHapticFeedback();
    }
}

void CheckboxInputType::switchAnimationTimerFired()
{
    ASSERT(m_switchAnimationTimer);
    if (!isSwitch() || !element() || !element()->renderer())
        return;

    auto updateInterval = switchAnimationUpdateInterval(element());
    if (!(updateInterval > 0_s))
        return;

    auto currentTime = MonotonicTime::now().secondsSinceEpoch();
    auto isVisuallyOnOngoing = currentTime - switchAnimationStartTime(SwitchAnimationType::VisuallyOn) < switchAnimationDuration(SwitchAnimationType::VisuallyOn);
    if (isVisuallyOnOngoing)
        m_switchAnimationTimer->startOneShot(updateInterval);
    else
        stopSwitchAnimation(SwitchAnimationType::VisuallyOn);

    element()->renderer()->repaint();
}

} // namespace WebCore
