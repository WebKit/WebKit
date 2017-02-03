/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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

#pragma once

#include "EventHandler.h"
#include "Frame.h"
#include <wtf/Vector.h>

namespace WebCore {

class PageConfiguration;
class PageOverlayController;
class PaymentCoordinator;
class PerformanceLogging;
class ScrollLatchingState;
class ServicesOverlayController;
class WheelEventDeltaFilter;

class MainFrame final : public Frame {
public:
    static Ref<MainFrame> create(Page&, PageConfiguration&);

    virtual ~MainFrame();

    void selfOnlyRef();
    void selfOnlyDeref();

    WheelEventDeltaFilter* wheelEventDeltaFilter() { return m_recentWheelEventDeltaFilter.get(); }
    PageOverlayController& pageOverlayController() { return *m_pageOverlayController; }

#if PLATFORM(MAC)
#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)
    ServicesOverlayController& servicesOverlayController() { return *m_servicesOverlayController; }
#endif // ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)

    ScrollLatchingState* latchingState();
    void pushNewLatchingState();
    void popLatchingState();
    void resetLatchingState();
    void removeLatchingStateForTarget(Element&);
#endif // PLATFORM(MAC)

#if ENABLE(APPLE_PAY)
    PaymentCoordinator& paymentCoordinator() const { return *m_paymentCoordinator; }
#endif

    PerformanceLogging& performanceLogging() const { return *m_performanceLogging; }

    void didCompleteLoad();
    MonotonicTime timeOfLastCompletedLoad() const { return m_timeOfLastCompletedLoad; }

private:
    MainFrame(Page&, PageConfiguration&);

    void dropChildren();

    unsigned m_selfOnlyRefCount;

#if PLATFORM(MAC)
    Vector<ScrollLatchingState> m_latchingState;
#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)
    std::unique_ptr<ServicesOverlayController> m_servicesOverlayController;
#endif
#endif

    std::unique_ptr<WheelEventDeltaFilter> m_recentWheelEventDeltaFilter;
    std::unique_ptr<PageOverlayController> m_pageOverlayController;

#if ENABLE(APPLE_PAY)
    std::unique_ptr<PaymentCoordinator> m_paymentCoordinator;
#endif

    std::unique_ptr<PerformanceLogging> m_performanceLogging;

    MonotonicTime m_timeOfLastCompletedLoad;
};

} // namespace WebCore
