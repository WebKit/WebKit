/*
 * Copyright (C) 2011-2013 University of Washington.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_REPLAY)

#include "AllReplayInputs.h"
#include "MainFrame.h"
#include "NavigationScheduler.h"
#include "Page.h"
#include "ReplayController.h"
#include "URL.h"
#include "UserInputBridge.h"

namespace WebCore {

// Sentinel inputs.
void BeginSegmentSentinel::dispatch(ReplayController&)
{
}

void EndSegmentSentinel::dispatch(ReplayController&)
{
}

// Navigation inputs.
void InitialNavigation::dispatch(ReplayController& controller)
{
    controller.page().mainFrame().navigationScheduler().scheduleLocationChange(m_securityOrigin.get(), m_url, m_referrer, true, true);
}

void HandleKeyPress::dispatch(ReplayController& controller)
{
    controller.page().userInputBridge().handleKeyEvent(platformEvent(), InputSource::Synthetic);
}

// User interaction inputs.
void HandleMouseMove::dispatch(ReplayController& controller)
{
    if (m_scrollbarTargeted)
        controller.page().userInputBridge().handleMouseMoveOnScrollbarEvent(platformEvent(), InputSource::Synthetic);
    else
        controller.page().userInputBridge().handleMouseMoveEvent(platformEvent(), InputSource::Synthetic);
}

void HandleMousePress::dispatch(ReplayController& controller)
{
    controller.page().userInputBridge().handleMousePressEvent(platformEvent(), InputSource::Synthetic);
}

void HandleMouseRelease::dispatch(ReplayController& controller)
{
    controller.page().userInputBridge().handleMouseReleaseEvent(platformEvent(), InputSource::Synthetic);
}

void HandleWheelEvent::dispatch(ReplayController& controller)
{
    controller.page().userInputBridge().handleWheelEvent(platformEvent(), InputSource::Synthetic);
}

void LogicalScrollPage::dispatch(ReplayController& controller)
{
    controller.page().userInputBridge().logicalScrollRecursively(direction(), granularity(), InputSource::Synthetic);
}

void ScrollPage::dispatch(ReplayController& controller)
{
    controller.page().userInputBridge().scrollRecursively(direction(), granularity(), InputSource::Synthetic);
}

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
