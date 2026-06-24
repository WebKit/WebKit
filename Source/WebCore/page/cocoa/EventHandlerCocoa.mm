/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "EventHandler.h"

#import "AutoscrollController.h"
#import "DictionaryLookup.h"
#import "DocumentPage.h"
#import "DocumentView.h"
#import "Editor.h"
#import "EventNames.h"
#import "HandleUserInputEventResult.h"
#import "LocalFrameInlines.h"
#import "LocalFrameView.h"
#import "Page.h"
#import "RenderObject.h"

#if PLATFORM(COCOA)

namespace WebCore {

#if ENABLE(REVEAL)

VisibleSelection EventHandler::selectClosestWordFromHitTestResultBasedOnLookup(const HitTestResult& result)
{
    if (!protect(m_frame)->editor().behavior().shouldSelectBasedOnDictionaryLookup())
        return { };

    auto range = DictionaryLookup::rangeAtHitTestResult(result);
    if (!range)
        return { };

    return *range;
}

#else

VisibleSelection EventHandler::selectClosestWordFromHitTestResultBasedOnLookup(const HitTestResult&)
{
    return VisibleSelection();
}

#endif

#if ENABLE(TWO_PHASE_CLICKS)

void EventHandler::dispatchSyntheticMouseOut(const PlatformMouseEvent& platformMouseEvent)
{
    updateMouseEventTargetNode(eventNames().mouseoutEvent, nullptr, platformMouseEvent, FireMouseOverOut::Yes);
}

void EventHandler::dispatchSyntheticMouseMove(const PlatformMouseEvent& platformMouseEvent)
{
    mouseMoved(platformMouseEvent);
}

#endif // ENABLE(TWO_PHASE_CLICKS)

void EventHandler::startSelectionAutoscroll(RenderObject* renderer, const FloatPoint& positionInWindow)
{
    Ref frame = m_frame.get();
    RefPtr frameView = frame->view();
    if (!frameView)
        return;

    m_targetAutoscrollPositionInRootView = roundedIntPoint(positionInWindow);

#if PLATFORM(IOS_FAMILY)
    m_targetAutoscrollPositionInUnscrolledRootView = m_targetAutoscrollPositionInRootView - toIntSize(frameView->documentScrollPositionRelativeToViewOrigin());

    if (!m_isAutoscrolling)
        m_initialAutoscrollPositionInUnscrolledRootView = m_targetAutoscrollPositionInUnscrolledRootView;
#endif // PLATFORM(IOS_FAMILY)

    m_isAutoscrolling = true;
    m_autoscrollController->startAutoscrollForSelection(renderer);
}

void EventHandler::cancelSelectionAutoscroll()
{
    m_isAutoscrolling = false;

#if PLATFORM(IOS_FAMILY)
    m_initialAutoscrollPositionInUnscrolledRootView = std::nullopt;
    m_targetAutoscrollPositionInUnscrolledRootView = { };
#endif

    m_targetAutoscrollPositionInRootView = { };
    m_autoscrollController->stopAutoscrollTimer();
}

bool EventHandler::shouldUpdateAutoscroll()
{
    if (m_isAutoscrolling)
        return true;

#if PLATFORM(MAC)
    return mousePressed();
#else
    return false;
#endif
}

}

#endif // PLATFORM(COCOA)
