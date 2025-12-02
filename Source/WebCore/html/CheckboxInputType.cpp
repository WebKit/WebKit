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

#include "Chrome.h"
#include "ChromeClient.h"
#include "ContainerNodeInlines.h"
#include "DocumentPage.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLInputElementInlines.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "LayoutPoint.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NodeDocument.h"
#include "RenderElement.h"
#include "RenderStyleInlines.h"
#include "RenderTheme.h"
#include "ScopedEventQueue.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "UserAgentParts.h"
#include "UserGestureIndicator.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(IOS_TOUCH_EVENTS)
#include "TouchEvent.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CheckboxInputType);

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
    Ref element = *this->element();
    ASSERT(element->userAgentShadowRoot());
    Ref shadowRoot = *element->userAgentShadowRoot();
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { shadowRoot };

    Ref document = element->document();
    Ref track = HTMLDivElement::create(document);
    {
        ScriptDisallowedScope::EventAllowedScope eventAllowedScopeBeforeAppend { track };
        track->setUserAgentPart(UserAgentParts::track());
    }
    shadowRoot->appendChild(ContainerNode::ChildChange::Source::Parser, track);
    Ref thumb = HTMLDivElement::create(document);
    {
        ScriptDisallowedScope::EventAllowedScope eventAllowedScopeBeforeAppend { thumb };
        thumb->setUserAgentPart(UserAgentParts::thumb());
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

    ASSERT(element());
    Ref element = *this->element();
    if (element->isDisabledFormControl() || !element->renderer())
        return;
    startSwitchPointerTracking(LayoutPoint(event.absoluteLocation()));
}

void CheckboxInputType::handleMouseMoveEvent(MouseEvent& event)
{
    if (!isSwitchPointerTracking())
        return;

    ASSERT(element());
    ASSERT(!element()->isDisabledFormControl());

    if (!event.isTrusted() || !isSwitch() || !protectedElement()->renderer()) {
        stopSwitchPointerTracking();
        return;
    }

    updateIsSwitchVisuallyOnFromAbsoluteLocation(LayoutPoint(event.absoluteLocation()));
}

#if ENABLE(IOS_TOUCH_EVENTS)
// FIXME: Share these functions with SliderThumbElement somehow?
static Touch* findTouchWithIdentifier(TouchList& list, unsigned identifier)
{
    unsigned length = list.length();
    for (unsigned i = 0; i < length; ++i) {
        auto* touch = list.item(i);
        if (touch->identifier() == identifier)
            return touch;
    }
    return nullptr;
}

Touch* CheckboxInputType::subsequentTouchEventTouch(const TouchEvent& event) const
{
    if (!m_switchPointerTrackingTouchIdentifier)
        return nullptr;

    RefPtr targetTouches = event.targetTouches();
    if (!targetTouches)
        return nullptr;

    return findTouchWithIdentifier(*targetTouches, *m_switchPointerTrackingTouchIdentifier);
}

void CheckboxInputType::handleTouchEvent(TouchEvent& event)
{
    ASSERT(element());
    Ref element = *this->element();

    if (!event.isTrusted() || !isSwitch() || element->isDisabledFormControl() || !element->renderer()) {
        stopSwitchPointerTracking();
        return;
    }

    const AtomString& eventType = event.type();
    auto& eventNames = WebCore::eventNames();
    if (eventType == eventNames.touchstartEvent) {
        RefPtr targetTouches = event.targetTouches();
        if (!targetTouches || targetTouches->length() != 1)
            return;
        RefPtr touch = targetTouches->item(0);

        m_switchPointerTrackingTouchIdentifier = touch->identifier();
        if (!m_switchHeldTimer) {
            m_switchHeldTimer = makeUnique<Timer>([protectedThis = Ref { *this }, touch] {
                if (!protectedThis->isSwitch() || !protectedThis->element() || !protectedThis->element()->renderer())
                    return;
                protectedThis->startSwitchPointerTracking({ static_cast<float>(touch->pageX()), static_cast<float>(touch->pageY()) });
                protectedThis->setIsSwitchHeld(true);
            });
        }
        constexpr Seconds switchHeldDelay = 200_ms;
        m_switchHeldTimer->startOneShot(switchHeldDelay);
        event.setDefaultHandled();
    } else if (eventType == eventNames.touchmoveEvent) {
        if (!isSwitchPointerTracking())
            return;
        RefPtr touch = subsequentTouchEventTouch(event);
        if (!touch)
            return;

        updateIsSwitchVisuallyOnFromAbsoluteLocation({ touch->pageX(), touch->pageY() });
        event.setDefaultHandled();
    } else if (eventType == eventNames.touchendEvent || eventType == eventNames.touchcancelEvent) {
        // If our touch still exists, this is not our touchend/touchcancel.
        RefPtr touch = subsequentTouchEventTouch(event);
        if (touch)
            return;

        m_switchPointerTrackingTouchIdentifier = { };
        if (m_switchHeldTimer)
            m_switchHeldTimer->stop();
        if (std::exchange(m_isSwitchHeld, false))
            performSwitchAnimation(SwitchAnimationType::Held);
        element->dispatchSimulatedClick(&event, SendNoEvents);
    }
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

    if (isSwitchPointerTracking() && m_hasSwitchVisuallyOnChanged && m_isSwitchVisuallyOn == state.checked) {
        stopSwitchPointerTracking();
        return;
    }

    element->setChecked(!state.checked, state.trusted ? WasSetByJavaScript::No : WasSetByJavaScript::Yes);

    if (isSwitch() && state.trusted && !(isSwitchPointerTracking() && m_hasSwitchVisuallyOnChanged && m_isSwitchVisuallyOn == !state.checked))
        performSwitchVisuallyOnAnimation(SwitchTrigger::Click);

    stopSwitchPointerTracking();
}

void CheckboxInputType::didDispatchClick(Event& event, const InputElementClickState& state)
{
    if (event.defaultPrevented() || event.defaultHandled()) {
        ASSERT(element());
        Ref element = *this->element();
        element->setIndeterminate(state.indeterminate);
        element->setChecked(state.checked);
    } else
        fireInputAndChangeEvents();

    // The work we did in willDispatchClick was default handling.
    event.setDefaultHandled();
}

static int switchPointerTrackingLogicalLeftPosition(Element& element, LayoutPoint absoluteLocation)
{
    CheckedRef renderer = *element.renderer();
    auto isVertical = !renderer->writingMode().isHorizontal();
    auto localLocation = renderer->absoluteToLocal(absoluteLocation, UseTransforms);
    return isVertical ? localLocation.y() : localLocation.x();
}

void CheckboxInputType::startSwitchPointerTracking(LayoutPoint absoluteLocation)
{
    ASSERT(element());
    Ref element = *this->element();
    ASSERT(element->renderer());
    if (RefPtr frame = element->protectedDocument()->frame()) {
        frame->eventHandler().setCapturingMouseEventsElement(element.ptr());
        m_isSwitchVisuallyOn = element->checked();
        m_switchPointerTrackingLogicalLeftPositionStart = switchPointerTrackingLogicalLeftPosition(element.get(), absoluteLocation);
    }
}

void CheckboxInputType::stopSwitchPointerTracking()
{
    ASSERT(element());
    if (!isSwitchPointerTracking())
        return;

    if (RefPtr frame = protectedElement()->protectedDocument()->frame())
        frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    m_hasSwitchVisuallyOnChanged = false;
    m_switchPointerTrackingLogicalLeftPositionStart = { };
}

bool CheckboxInputType::isSwitchPointerTracking() const
{
    return !!m_switchPointerTrackingLogicalLeftPositionStart;
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
    if (protectedElement()->isDisabledFormControl()) {
        stopSwitchAnimation(SwitchAnimationType::VisuallyOn);
        stopSwitchAnimation(SwitchAnimationType::Held);
        stopSwitchPointerTracking();
    }

#if ENABLE(TOUCH_EVENTS)
    if (isSwitch())
        protectedElement()->updateTouchEventHandler();
#endif
}

void CheckboxInputType::willUpdateCheckedness(bool, WasSetByJavaScript wasCheckedByJavaScript)
{
    ASSERT(element());
    if (isSwitch() && wasCheckedByJavaScript == WasSetByJavaScript::Yes) {
        stopSwitchAnimation(SwitchAnimationType::VisuallyOn);
        stopSwitchAnimation(SwitchAnimationType::Held);
        stopSwitchPointerTracking();
    }
}

// FIXME: ideally CheckboxInputType would not be responsible for the timer specifics and instead
// ask a more knowledgable system for a refresh callback (perhaps passing a desired FPS).
static Seconds switchAnimationUpdateInterval(HTMLInputElement& element)
{
    if (RefPtr page = element.protectedDocument()->page())
        return page->preferredRenderingUpdateInterval();
    return 0_s;
}

static Seconds switchAnimationDuration(SwitchAnimationType type)
{
    if (type == SwitchAnimationType::VisuallyOn)
        return RenderTheme::singleton().switchAnimationVisuallyOnDuration();
    return RenderTheme::singleton().switchAnimationHeldDuration();
}

Seconds CheckboxInputType::switchAnimationStartTime(SwitchAnimationType type) const
{
    if (type == SwitchAnimationType::VisuallyOn)
        return m_switchAnimationVisuallyOnStartTime;
    return m_switchAnimationHeldStartTime;
}

void CheckboxInputType::setSwitchAnimationStartTime(SwitchAnimationType type, Seconds time)
{
    if (type == SwitchAnimationType::VisuallyOn)
        m_switchAnimationVisuallyOnStartTime = time;
    else
        m_switchAnimationHeldStartTime = time;
}

bool CheckboxInputType::isSwitchAnimating(SwitchAnimationType type) const
{
    return switchAnimationStartTime(type) != 0_s;
}

void CheckboxInputType::performSwitchAnimation(SwitchAnimationType type)
{
    ASSERT(isSwitch());
    ASSERT(element());
    Ref element = *this->element();
    if (!element->renderer() || !element->renderer()->style().hasUsedAppearance())
        return;

    auto updateInterval = switchAnimationUpdateInterval(element.get());
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

void CheckboxInputType::performSwitchVisuallyOnAnimation(SwitchTrigger trigger)
{
    performSwitchAnimation(SwitchAnimationType::VisuallyOn);

    if (!RenderTheme::singleton().hasSwitchHapticFeedback(trigger))
        return;

    if (trigger == SwitchTrigger::Click && !UserGestureIndicator::processingUserGesture())
        return;

    if (RefPtr page = element()->document().page())
        page->chrome().client().performSwitchHapticFeedback();
}

void CheckboxInputType::setIsSwitchHeld(bool isHeld)
{
    m_isSwitchHeld = isHeld;
    performSwitchAnimation(SwitchAnimationType::Held);
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

float CheckboxInputType::switchAnimationHeldProgress() const
{
    ASSERT(isSwitch());
    ASSERT(switchAnimationDuration(SwitchAnimationType::Held) > 0_s);

    return switchAnimationProgress(SwitchAnimationType::Held);
}

bool CheckboxInputType::isSwitchHeld() const
{
    ASSERT(element());
    ASSERT(isSwitch());
    return m_isSwitchHeld;
}

void CheckboxInputType::updateIsSwitchVisuallyOnFromAbsoluteLocation(LayoutPoint absoluteLocation)
{
    Ref element = *this->element();
    auto logicalLeftPosition = switchPointerTrackingLogicalLeftPosition(element.get(), absoluteLocation);
    auto isSwitchVisuallyOn = m_isSwitchVisuallyOn;
    auto isRTL = element->computedStyle()->writingMode().isBidiRTL();
    auto switchThumbIsLogicallyLeft = (!isRTL && !isSwitchVisuallyOn) || (isRTL && isSwitchVisuallyOn);
    auto switchTrackRect = element->checkedRenderer()->absoluteBoundingBoxRect();
    auto switchThumbLength = switchTrackRect.height();
    auto switchTrackWidth = switchTrackRect.width();

    auto changePosition = switchTrackWidth / 2;
    if (!m_hasSwitchVisuallyOnChanged) {
        auto switchTrackNoThumbWidth = switchTrackWidth - switchThumbLength;
        auto changeOffset = switchTrackWidth * RenderTheme::singleton().switchPointerTrackingMagnitudeProportion();
        if (switchThumbIsLogicallyLeft && *m_switchPointerTrackingLogicalLeftPositionStart > switchTrackNoThumbWidth)
            changePosition = *m_switchPointerTrackingLogicalLeftPositionStart + changeOffset;
        else if (!switchThumbIsLogicallyLeft && *m_switchPointerTrackingLogicalLeftPositionStart < switchTrackNoThumbWidth)
            changePosition = *m_switchPointerTrackingLogicalLeftPositionStart - changeOffset;
    }

    auto switchThumbIsLogicallyLeftNow = logicalLeftPosition < changePosition;
    if (switchThumbIsLogicallyLeftNow != switchThumbIsLogicallyLeft) {
        m_hasSwitchVisuallyOnChanged = true;
        m_isSwitchVisuallyOn = !m_isSwitchVisuallyOn;
        performSwitchVisuallyOnAnimation(SwitchTrigger::PointerTracking);
    }
}

void CheckboxInputType::switchAnimationTimerFired()
{
    ASSERT(m_switchAnimationTimer);
    if (!isSwitch())
        return;

    Ref element = *this->element();
    if (!element->renderer())
        return;

    auto updateInterval = switchAnimationUpdateInterval(element.get());
    if (!(updateInterval > 0_s))
        return;

    auto currentTime = MonotonicTime::now().secondsSinceEpoch();
    auto isVisuallyOnOngoing = currentTime - switchAnimationStartTime(SwitchAnimationType::VisuallyOn) < switchAnimationDuration(SwitchAnimationType::VisuallyOn);
    auto isHeldOngoing = currentTime - switchAnimationStartTime(SwitchAnimationType::Held) < switchAnimationDuration(SwitchAnimationType::Held);
    if (isVisuallyOnOngoing || isHeldOngoing)
        m_switchAnimationTimer->startOneShot(updateInterval);
    else {
        if (!isVisuallyOnOngoing)
            stopSwitchAnimation(SwitchAnimationType::VisuallyOn);
        if (!isHeldOngoing)
            stopSwitchAnimation(SwitchAnimationType::Held);
    }

    element->checkedRenderer()->repaint();
}

} // namespace WebCore
