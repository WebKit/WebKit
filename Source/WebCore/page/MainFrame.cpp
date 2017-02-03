/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "MainFrame.h"

#include "Element.h"
#include "PageConfiguration.h"
#include "PageOverlayController.h"
#include "PaymentCoordinator.h"
#include "PerformanceLogging.h"
#include "ScrollLatchingState.h"
#include "WheelEventDeltaFilter.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(MAC)
#include "ServicesOverlayController.h"
#endif

namespace WebCore {

inline MainFrame::MainFrame(Page& page, PageConfiguration& configuration)
    : Frame(page, nullptr, *configuration.loaderClientForMainFrame)
    , m_selfOnlyRefCount(0)
#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    , m_servicesOverlayController(std::make_unique<ServicesOverlayController>(*this))
#endif
    , m_recentWheelEventDeltaFilter(WheelEventDeltaFilter::create())
    , m_pageOverlayController(std::make_unique<PageOverlayController>(*this))
#if ENABLE(APPLE_PAY)
    , m_paymentCoordinator(std::make_unique<PaymentCoordinator>(*configuration.paymentCoordinatorClient))
#endif
    , m_performanceLogging(std::make_unique<PerformanceLogging>(*this))
{
}

MainFrame::~MainFrame()
{
    m_recentWheelEventDeltaFilter = nullptr;
    m_eventHandler = nullptr;

    setMainFrameWasDestroyed();
}

Ref<MainFrame> MainFrame::create(Page& page, PageConfiguration& configuration)
{
    return adoptRef(*new MainFrame(page, configuration));
}

void MainFrame::selfOnlyRef()
{
    if (m_selfOnlyRefCount++)
        return;

    ref();
}

void MainFrame::selfOnlyDeref()
{
    ASSERT(m_selfOnlyRefCount);
    if (--m_selfOnlyRefCount)
        return;

    if (hasOneRef())
        dropChildren();

    deref();
}

void MainFrame::dropChildren()
{
    while (Frame* child = tree().firstChild())
        tree().removeChild(*child);
}

void MainFrame::didCompleteLoad()
{
    m_timeOfLastCompletedLoad = MonotonicTime::now();
    performanceLogging().didReachPointOfInterest(PerformanceLogging::MainFrameLoadCompleted);
}

#if PLATFORM(MAC)
ScrollLatchingState* MainFrame::latchingState()
{
    if (m_latchingState.isEmpty())
        return nullptr;

    return &m_latchingState.last();
}

void MainFrame::pushNewLatchingState()
{
    m_latchingState.append(ScrollLatchingState());
}

void MainFrame::resetLatchingState()
{
    m_latchingState.clear();
}
    
void MainFrame::popLatchingState()
{
    m_latchingState.removeLast();
}

void MainFrame::removeLatchingStateForTarget(Element& targetNode)
{
    if (m_latchingState.isEmpty())
        return;

    m_latchingState.removeAllMatching([&targetNode] (ScrollLatchingState& state) {
        auto* wheelElement = state.wheelEventElement();
        if (!wheelElement)
            return false;

        return targetNode.isEqualNode(wheelElement);
    });
}
#endif

}
