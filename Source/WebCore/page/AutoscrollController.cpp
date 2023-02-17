/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
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

#include "config.h"
#include "AutoscrollController.h"

#include "EventHandler.h"
#include "Frame.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderView.h"
#include "ScrollView.h"
#include "Settings.h"

namespace WebCore {

// Delay time in second for start autoscroll if pointer is in border edge of scrollable element.
static const Seconds autoscrollDelay { 200_ms };

// When the autoscroll or the panScroll is triggered when do the scroll every 50ms to make it smooth.
static const Seconds autoscrollInterval { 50_ms };

#if ENABLE(PAN_SCROLLING)
static Frame* getMainFrame(Frame* frame)
{
    Page* page = frame->page();
    return page ? &page->mainFrame() : 0;
}
#endif

AutoscrollController::AutoscrollController()
    : m_autoscrollTimer(*this, &AutoscrollController::autoscrollTimerFired)
{
}

RenderBox* AutoscrollController::autoscrollRenderer() const
{
    return m_autoscrollRenderer.get();
}

bool AutoscrollController::autoscrollInProgress() const
{
    return m_autoscrollType == AutoscrollForSelection;
}

void AutoscrollController::startAutoscrollForSelection(RenderObject* renderer)
{
    // We don't want to trigger the autoscroll or the panScroll if it's already active
    if (m_autoscrollTimer.isActive())
        return;
    auto* scrollable = RenderBox::findAutoscrollable(renderer);
    if (!scrollable)
        return;
    m_autoscrollType = AutoscrollForSelection;
    m_autoscrollRenderer = WeakPtr { *scrollable };
    startAutoscrollTimer();
}

void AutoscrollController::stopAutoscrollTimer(bool rendererIsBeingDestroyed)
{
    auto scrollable = m_autoscrollRenderer;

    m_autoscrollTimer.stop();
    m_autoscrollRenderer = nullptr;

    if (!scrollable)
        return;

    auto* frame = scrollable->document().frame();
    if (autoscrollInProgress() && frame && frame->eventHandler().mouseDownWasInSubframe()) {
        if (auto subframe = frame->eventHandler().subframeForTargetNode(frame->eventHandler().mousePressNode()))
            subframe->eventHandler().stopAutoscrollTimer(rendererIsBeingDestroyed);
        return;
    }

    if (!rendererIsBeingDestroyed)
        scrollable->stopAutoscroll();

#if ENABLE(PAN_SCROLLING)
    if (panScrollInProgress()) {
        FrameView& frameView = scrollable->view().frameView();
        frameView.removePanScrollIcon();
        frameView.setCursor(pointerCursor());
    }
#endif

    m_autoscrollType = NoAutoscroll;

#if ENABLE(PAN_SCROLLING)
    // If we're not in the top frame we notify it that we are not doing a panScroll any more.
    if (auto* localFrame = (frame && !frame->isMainFrame()) ? dynamicDowncast<LocalFrame>(frame->mainFrame()) : nullptr)
        localFrame->eventHandler().didPanScrollStop();
#endif
}

void AutoscrollController::updateAutoscrollRenderer()
{
    if (!m_autoscrollRenderer)
        return;

    RenderObject* renderer = m_autoscrollRenderer.get();

#if ENABLE(PAN_SCROLLING)
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult hitTest = m_autoscrollRenderer->frame().eventHandler().hitTestResultAtPoint(m_panScrollStartPos, hitType);

    if (auto* nodeAtPoint = hitTest.innerNode())
        renderer = nodeAtPoint->renderer();
#endif

    while (renderer && !(is<RenderBox>(*renderer) && downcast<RenderBox>(*renderer).canAutoscroll()))
        renderer = renderer->parent();

    if (!is<RenderBox>(renderer)) {
        m_autoscrollRenderer = nullptr;
        return;
    }

    m_autoscrollRenderer = WeakPtr { downcast<RenderBox>(*renderer) };
}

void AutoscrollController::updateDragAndDrop(Node* dropTargetNode, const IntPoint& eventPosition, WallTime eventTime)
{
    IntSize offset;
    auto findDragAndDropScroller = [&]() -> RenderBox* {
        if (!dropTargetNode)
            return nullptr;

        auto* scrollable = RenderBox::findAutoscrollable(dropTargetNode->renderer());
        if (!scrollable)
            return nullptr;

        auto& frame = scrollable->frame();
        auto* page = frame.page();
        if (!page || !page->settings().autoscrollForDragAndDropEnabled())
            return nullptr;

        offset = scrollable->calculateAutoscrollDirection(eventPosition);
        if (offset.isZero())
            return nullptr;

        return scrollable;
    };
    
    RenderBox* scrollable = findDragAndDropScroller();
    if (!scrollable) {
        stopAutoscrollTimer();
        return;
    }

    m_dragAndDropAutoscrollReferencePosition = eventPosition + offset;

    if (m_autoscrollType == NoAutoscroll) {
        m_autoscrollType = AutoscrollForDragAndDrop;
        m_autoscrollRenderer = WeakPtr { *scrollable };
        m_dragAndDropAutoscrollStartTime = eventTime;
        startAutoscrollTimer();
    } else if (m_autoscrollRenderer != scrollable) {
        m_dragAndDropAutoscrollStartTime = eventTime;
        m_autoscrollRenderer = WeakPtr { *scrollable };
    }
}

#if ENABLE(PAN_SCROLLING)
void AutoscrollController::didPanScrollStart()
{
    m_autoscrollType = AutoscrollForPan;
}

void AutoscrollController::didPanScrollStop()
{
    m_autoscrollType = NoAutoscroll;
}

void AutoscrollController::handleMouseReleaseEvent(const PlatformMouseEvent& mouseEvent)
{
    switch (m_autoscrollType) {
    case AutoscrollForPan:
        if (mouseEvent.button() == MiddleButton)
            m_autoscrollType = AutoscrollForPanCanStop;
        break;
    case AutoscrollForPanCanStop:
        stopAutoscrollTimer();
        break;
    default:
        break;
    }
}

bool AutoscrollController::panScrollInProgress() const
{
    return m_autoscrollType == AutoscrollForPan || m_autoscrollType == AutoscrollForPanCanStop;
}

void AutoscrollController::startPanScrolling(RenderBox& scrollable, const IntPoint& lastKnownMousePosition)
{
    // We don't want to trigger the autoscroll or the panScroll if it's already active
    if (m_autoscrollTimer.isActive())
        return;

    m_autoscrollType = AutoscrollForPan;
    m_autoscrollRenderer = WeakPtr { scrollable };
    m_panScrollStartPos = lastKnownMousePosition;

    if (auto* view = scrollable.frame().view())
        view->addPanScrollIcon(lastKnownMousePosition);

    scrollable.frame().eventHandler().didPanScrollStart();
    startAutoscrollTimer();
}
#else
bool AutoscrollController::panScrollInProgress() const
{
    return false;
}
#endif

void AutoscrollController::autoscrollTimerFired()
{
    if (!m_autoscrollRenderer) {
        stopAutoscrollTimer();
        return;
    }

    Frame& frame = m_autoscrollRenderer->frame();
    switch (m_autoscrollType) {
    case AutoscrollForDragAndDrop:
        if (WallTime::now() - m_dragAndDropAutoscrollStartTime > autoscrollDelay)
            m_autoscrollRenderer->autoscroll(m_dragAndDropAutoscrollReferencePosition);
        break;
    case AutoscrollForSelection: {
        if (!frame.eventHandler().shouldUpdateAutoscroll()) {
            stopAutoscrollTimer();
            return;
        }
#if ENABLE(DRAG_SUPPORT)
        frame.eventHandler().updateSelectionForMouseDrag();
#endif
        m_autoscrollRenderer->autoscroll(frame.eventHandler().targetPositionInWindowForSelectionAutoscroll());
        break;
    }
    case NoAutoscroll:
        break;
#if ENABLE(PAN_SCROLLING)
    case AutoscrollForPanCanStop:
    case AutoscrollForPan:
        // we verify that the main frame hasn't received the order to stop the panScroll
        if (Frame* mainFrame = getMainFrame(&frame)) {
            if (!mainFrame->eventHandler().panScrollInProgress()) {
                stopAutoscrollTimer();
                return;
            }
        }
        if (FrameView* view = frame.view())
            updatePanScrollState(view, frame.eventHandler().lastKnownMousePosition());
        m_autoscrollRenderer->panScroll(m_panScrollStartPos);
        break;
#endif
    }
}

void AutoscrollController::startAutoscrollTimer()
{
    m_autoscrollTimer.startRepeating(autoscrollInterval);
}

#if ENABLE(PAN_SCROLLING)
void AutoscrollController::updatePanScrollState(FrameView* view, const IntPoint& lastKnownMousePosition)
{
    // At the original click location we draw a 4 arrowed icon. Over this icon there won't be any scroll
    // So we don't want to change the cursor over this area
    bool east = m_panScrollStartPos.x() < (lastKnownMousePosition.x() - ScrollView::noPanScrollRadius);
    bool west = m_panScrollStartPos.x() > (lastKnownMousePosition.x() + ScrollView::noPanScrollRadius);
    bool north = m_panScrollStartPos.y() > (lastKnownMousePosition.y() + ScrollView::noPanScrollRadius);
    bool south = m_panScrollStartPos.y() < (lastKnownMousePosition.y() - ScrollView::noPanScrollRadius);

    if (m_autoscrollType == AutoscrollForPan && (east || west || north || south))
        m_autoscrollType = AutoscrollForPanCanStop;

    if (north) {
        if (east)
            view->setCursor(northEastPanningCursor());
        else if (west)
            view->setCursor(northWestPanningCursor());
        else
            view->setCursor(northPanningCursor());
    } else if (south) {
        if (east)
            view->setCursor(southEastPanningCursor());
        else if (west)
            view->setCursor(southWestPanningCursor());
        else
            view->setCursor(southPanningCursor());
    } else if (east)
        view->setCursor(eastPanningCursor());
    else if (west)
        view->setCursor(westPanningCursor());
    else
        view->setCursor(middlePanningCursor());
}
#endif

} // namespace WebCore
