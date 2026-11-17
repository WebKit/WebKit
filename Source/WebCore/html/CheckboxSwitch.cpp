/*
 * Copyright (C) 2023-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CheckboxSwitch.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "ContainerNodeInlines.h"
#include "DocumentPage.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLInputElement.h"
#include "LayoutPoint.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "MouseEvent.h"
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "PlatformRenderTheme.h"
#include "RenderBoxInlines.h"
#include "RenderElement.h"
#include "RenderTheme.h"
#include "Timer.h"
#include "UserGestureIndicator.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(IOS_TOUCH_EVENTS)
#include "TouchEvent.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CheckboxSwitch);

CheckboxSwitch::CheckboxSwitch(HTMLInputElement& element)
    : m_element(element)
{
}

CheckboxSwitch::~CheckboxSwitch()
{
    if (element())
        stopSwitchPointerTracking();
}

bool CheckboxSwitch::isSwitch() const
{
    return m_element && m_element->isSwitch();
}

void CheckboxSwitch::handleMouseDownEvent(MouseEvent& event)
{
    ASSERT(element());
    Ref element = *this->element();
    if (element->isDisabledFormControl() || !element->renderer())
        return;
    startSwitchPointerTracking(LayoutPoint(event.absoluteLocation()));
}

void CheckboxSwitch::handleMouseMoveEvent(MouseEvent& event)
{
    if (!isSwitchPointerTracking())
        return;

    ASSERT(element());
    ASSERT(!element()->isDisabledFormControl());

    if (!event.isTrusted() || !isSwitch() || !element()->renderer()) {
        stopSwitchPointerTracking();
        return;
    }

    updateIsSwitchVisuallyOnFromAbsoluteLocation(LayoutPoint(event.absoluteLocation()));
}

static bool switchContainsAbsoluteLocation(Element& element, LayoutPoint absoluteLocation)
{
    CheckedPtr renderer = element.renderBox();
    if (!renderer)
        return false;
    auto localLocation = renderer->absoluteToLocal(absoluteLocation, MapCoordinatesMode::UseTransforms);
    return renderer->borderBoxRect().contains(LayoutPoint { localLocation });
}

void CheckboxSwitch::handleMouseUpEvent(MouseEvent& event)
{
    if (!event.isTrusted() || !isSwitchPointerTracking())
        return;

    ASSERT(element());
    Ref element = *this->element();
    bool releasedOutside = !m_hasSwitchVisuallyOnChanged && !switchContainsAbsoluteLocation(element, LayoutPoint(event.absoluteLocation()));
    if (!hasSwitchThumbReturned() && !releasedOutside)
        return;

    if (RefPtr frame = element->document().frame())
        frame->eventHandler().invalidateClick();
    stopSwitchPointerTracking();
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

Touch* CheckboxSwitch::subsequentTouchEventTouch(const TouchEvent& event) const
{
    if (!m_switchPointerTrackingTouchIdentifier)
        return nullptr;

    RefPtr targetTouches = event.targetTouches();
    if (!targetTouches)
        return nullptr;

    return findTouchWithIdentifier(*targetTouches, *m_switchPointerTrackingTouchIdentifier);
}

bool CheckboxSwitch::handleTouchEvent(TouchEvent& event)
{
    ASSERT(element());
    Ref element = *this->element();

    if (!event.isTrusted() || !isSwitch() || element->isDisabledFormControl() || !element->renderer()) {
        stopSwitchPointerTracking();
        return false;
    }

    const AtomString& eventType = event.type();
    auto& eventNames = WebCore::eventNames();
    if (eventType == eventNames.touchstartEvent) {
        RefPtr targetTouches = event.targetTouches();
        if (!targetTouches || targetTouches->length() != 1)
            return false;
        RefPtr touch = targetTouches->item(0);

        m_switchPointerTrackingTouchIdentifier = touch->identifier();
        if (!m_switchHeldTimer) {
            lazyInitialize(m_switchHeldTimer, makeUnique<Timer>([checkedThis = CheckedRef { *this }, touch] {
                if (!checkedThis->isSwitch() || !checkedThis->element() || !checkedThis->element()->renderer())
                    return;
                checkedThis->startSwitchPointerTracking({ static_cast<float>(touch->pageX()), static_cast<float>(touch->pageY()) });
                checkedThis->setIsSwitchHeld(true);
            }));
        }
        constexpr Seconds switchHeldDelay = 200_ms;
        m_switchHeldTimer->startOneShot(switchHeldDelay);
        event.setDefaultHandled();
    } else if (eventType == eventNames.touchmoveEvent) {
        if (!isSwitchPointerTracking())
            return false;
        RefPtr touch = subsequentTouchEventTouch(event);
        if (!touch)
            return false;

        updateIsSwitchVisuallyOnFromAbsoluteLocation({ touch->pageX(), touch->pageY() });
        event.setDefaultHandled();
    } else if (eventType == eventNames.touchendEvent || eventType == eventNames.touchcancelEvent) {
        // If our touch still exists, this is not our touchend/touchcancel.
        RefPtr touch = subsequentTouchEventTouch(event);
        if (touch)
            return false;

        m_switchPointerTrackingTouchIdentifier = { };
        if (m_switchHeldTimer)
            m_switchHeldTimer->stop();
        if (std::exchange(m_isSwitchHeld, false))
            performSwitchAnimation(SwitchAnimationType::Held);
        if (hasSwitchThumbReturned()) {
            stopSwitchPointerTracking();
            return false;
        }
        return true;
    }
    return false;
}
#endif

bool CheckboxSwitch::willSkipToggleForClick(bool checked)
{
    if (isSwitchPointerTracking() && m_hasSwitchVisuallyOnChanged && m_isSwitchVisuallyOn == checked) {
        stopSwitchPointerTracking();
        m_didSkipSwitchToggle = true;
        return true;
    }
    return false;
}

void CheckboxSwitch::didToggleForClick(bool wasChecked)
{
    if (!(isSwitchPointerTracking() && m_hasSwitchVisuallyOnChanged && m_isSwitchVisuallyOn == !wasChecked))
        performSwitchVisuallyOnAnimation(SwitchTrigger::Click);
    stopSwitchPointerTracking();
}

bool CheckboxSwitch::takeDidSkipSwitchToggle()
{
    return std::exchange(m_didSkipSwitchToggle, false);
}

void CheckboxSwitch::stopAnimationsAndPointerTracking()
{
    stopSwitchAnimation(SwitchAnimationType::VisuallyOn);
    stopSwitchAnimation(SwitchAnimationType::Held);
    stopSwitchPointerTracking();
}

static int switchPointerTrackingLogicalLeftPosition(Element& element, LayoutPoint absoluteLocation)
{
    CheckedRef renderer = *element.renderer();
    auto isVertical = !renderer->writingMode().isHorizontal();
    auto localLocation = renderer->absoluteToLocal(absoluteLocation, MapCoordinatesMode::UseTransforms);
    return isVertical ? localLocation.y() : localLocation.x();
}

void CheckboxSwitch::startSwitchPointerTracking(LayoutPoint absoluteLocation)
{
    ASSERT(element());
    Ref element = *this->element();
    ASSERT(element->renderer());
    if (RefPtr frame = element->document().frame()) {
        frame->eventHandler().setCapturingMouseEventsElement(element.ptr());
        m_isSwitchVisuallyOn = element->checked();
        m_switchPointerTrackingLogicalLeftPositionStart = switchPointerTrackingLogicalLeftPosition(element.get(), absoluteLocation);
    }
}

void CheckboxSwitch::stopSwitchPointerTracking()
{
    ASSERT(element());
    if (!isSwitchPointerTracking())
        return;

    if (RefPtr frame = element()->document().frame())
        frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    m_hasSwitchVisuallyOnChanged = false;
    m_switchPointerTrackingLogicalLeftPositionStart = { };
}

bool CheckboxSwitch::isSwitchPointerTracking() const
{
    return !!m_switchPointerTrackingLogicalLeftPositionStart;
}

bool CheckboxSwitch::hasSwitchThumbReturned() const
{
    ASSERT(element());
    return isSwitchPointerTracking() && m_hasSwitchVisuallyOnChanged && m_isSwitchVisuallyOn == element()->checked();
}

// FIXME: ideally CheckboxSwitch would not be responsible for the timer specifics and instead
// ask a more knowledgable system for a refresh callback (perhaps passing a desired FPS).
static Seconds switchAnimationUpdateInterval(HTMLInputElement& element)
{
    if (RefPtr page = element.document().page())
        return page->preferredRenderingUpdateInterval();
    return 0_s;
}

static Seconds switchAnimationDuration(SwitchAnimationType type)
{
    if (type == SwitchAnimationType::VisuallyOn)
        return RenderTheme::singleton().switchAnimationVisuallyOnDuration();
    return RenderTheme::singleton().switchAnimationHeldDuration();
}

Seconds CheckboxSwitch::switchAnimationStartTime(SwitchAnimationType type) const
{
    if (type == SwitchAnimationType::VisuallyOn)
        return m_switchAnimationVisuallyOnStartTime;
    return m_switchAnimationHeldStartTime;
}

void CheckboxSwitch::setSwitchAnimationStartTime(SwitchAnimationType type, Seconds time)
{
    if (type == SwitchAnimationType::VisuallyOn)
        m_switchAnimationVisuallyOnStartTime = time;
    else
        m_switchAnimationHeldStartTime = time;
}

bool CheckboxSwitch::isSwitchAnimating(SwitchAnimationType type) const
{
    return switchAnimationStartTime(type) != 0_s;
}

void CheckboxSwitch::performSwitchAnimation(SwitchAnimationType type)
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
        lazyInitialize(m_switchAnimationTimer, makeUnique<Timer>(*this, &CheckboxSwitch::switchAnimationTimerFired));
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

void CheckboxSwitch::performSwitchVisuallyOnAnimation(SwitchTrigger trigger)
{
    performSwitchAnimation(SwitchAnimationType::VisuallyOn);

    if (!RenderTheme::singleton().hasSwitchHapticFeedback(trigger))
        return;

    if (trigger == SwitchTrigger::Click && !UserGestureIndicator::processingUserGesture())
        return;

    if (RefPtr page = element()->document().page())
        page->chrome().client().performSwitchHapticFeedback();
}

void CheckboxSwitch::setIsSwitchHeld(bool isHeld)
{
    m_isSwitchHeld = isHeld;
    performSwitchAnimation(SwitchAnimationType::Held);
}

void CheckboxSwitch::stopSwitchAnimation(SwitchAnimationType type)
{
    setSwitchAnimationStartTime(type, 0_s);
}

float CheckboxSwitch::switchAnimationProgress(SwitchAnimationType type) const
{
    if (!isSwitchAnimating(type))
        return 1.0f;
    auto duration = switchAnimationDuration(type);
    return std::min((float)((MonotonicTime::now().secondsSinceEpoch() - switchAnimationStartTime(type)) / duration), 1.0f);
}

float CheckboxSwitch::switchAnimationVisuallyOnProgress() const
{
    ASSERT(isSwitch());
    ASSERT(switchAnimationDuration(SwitchAnimationType::VisuallyOn) > 0_s);

    return switchAnimationProgress(SwitchAnimationType::VisuallyOn);
}

bool CheckboxSwitch::isSwitchVisuallyOn() const
{
    ASSERT(element());
    ASSERT(isSwitch());
    return isSwitchPointerTracking() ? m_isSwitchVisuallyOn : element()->checked();
}

float CheckboxSwitch::switchAnimationHeldProgress() const
{
    ASSERT(isSwitch());
    ASSERT(switchAnimationDuration(SwitchAnimationType::Held) > 0_s);

    return switchAnimationProgress(SwitchAnimationType::Held);
}

bool CheckboxSwitch::isSwitchHeld() const
{
    ASSERT(element());
    ASSERT(isSwitch());
    return m_isSwitchHeld;
}

void CheckboxSwitch::updateIsSwitchVisuallyOnFromAbsoluteLocation(LayoutPoint absoluteLocation)
{
    Ref element = *this->element();
    CheckedPtr renderer = element->renderBox();
    if (!renderer)
        return;
    auto logicalLeftPosition = switchPointerTrackingLogicalLeftPosition(element.get(), absoluteLocation);
    auto isSwitchVisuallyOn = m_isSwitchVisuallyOn;
    auto isRTL = renderer->writingMode().isBidiRTL();
    auto switchThumbIsLogicallyLeft = (!isRTL && !isSwitchVisuallyOn) || (isRTL && isSwitchVisuallyOn);
    auto switchThumbLogicalLength = renderer->logicalHeight();
    auto switchTrackLogicalWidth = renderer->logicalWidth();

    auto changePosition = switchTrackLogicalWidth / 2;
    if (!m_hasSwitchVisuallyOnChanged) {
        auto switchTrackNoThumbLogicalWidth = switchTrackLogicalWidth - switchThumbLogicalLength;
        auto changeOffset = switchTrackLogicalWidth * RenderTheme::singleton().switchPointerTrackingMagnitudeProportion();
        if (switchThumbIsLogicallyLeft && *m_switchPointerTrackingLogicalLeftPositionStart > switchTrackNoThumbLogicalWidth)
            changePosition = *m_switchPointerTrackingLogicalLeftPositionStart + changeOffset;
        else if (!switchThumbIsLogicallyLeft && *m_switchPointerTrackingLogicalLeftPositionStart < switchTrackNoThumbLogicalWidth)
            changePosition = *m_switchPointerTrackingLogicalLeftPositionStart - changeOffset;
    }

    auto switchThumbIsLogicallyLeftNow = logicalLeftPosition < changePosition;
    if (switchThumbIsLogicallyLeftNow != switchThumbIsLogicallyLeft) {
        m_hasSwitchVisuallyOnChanged = true;
        m_isSwitchVisuallyOn = !m_isSwitchVisuallyOn;
        performSwitchVisuallyOnAnimation(SwitchTrigger::PointerTracking);
    }
}

void CheckboxSwitch::switchAnimationTimerFired()
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

    protect(element->renderer())->repaint();
}

} // namespace WebCore
