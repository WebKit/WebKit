/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebWorkerAnimationController.h"

#if PLATFORM(COCOA)

#include "EventDispatcher.h"
#include "WebProcess.h"

namespace WebKit {
using namespace WebCore;

class WebWorkerAsyncRenderingRefreshObserver : public EventDispatcher::AsyncRenderingRefreshObserver {
public:
    WebWorkerAsyncRenderingRefreshObserver(WebWorkerAnimationController* controller)
        : m_controller(controller)
    { }

    void displayDidRefresh() final
    {
        if (m_controller)
            m_controller->displayDidRefresh();
    }

    void disconnect()
    {
        m_controller = nullptr;
    }

private:
    WebWorkerAnimationController* m_controller;
};

Ref<WebWorkerAnimationController> WebWorkerAnimationController::create(WorkerGlobalScope& workerGlobalScope, WebCore::PageIdentifier identifier, FunctionDispatcher& dispatcher)
{
    auto controller = adoptRef(*new WebWorkerAnimationController(workerGlobalScope, identifier, dispatcher));
    controller->suspendIfNeeded();
    return controller;
}

WebWorkerAnimationController::WebWorkerAnimationController(WorkerGlobalScope& workerGlobalScope, WebCore::PageIdentifier identifier, FunctionDispatcher& dispatcher)
    : WorkerAnimationController(workerGlobalScope)
    , m_identifier(identifier)
    , m_dispatcher(dispatcher)
{ }

WebWorkerAnimationController::~WebWorkerAnimationController() = default;

void WebWorkerAnimationController::displayDidRefresh()
{
    animationTimerFired();
}

void WebWorkerAnimationController::scheduleAnimation()
{
    if (!m_observer) {
        m_observer = adoptRef(new WebWorkerAsyncRenderingRefreshObserver(this));
        WebProcess::singleton().eventDispatcher().addAsyncRenderingRefreshObserver(m_identifier, m_dispatcher, *m_observer);
    }
}
void WebWorkerAnimationController::stopAnimation()
{
    if (m_observer) {
        WebProcess::singleton().eventDispatcher().removeAsyncRenderingRefreshObserver(m_identifier, *m_observer);
        m_observer->disconnect();
        m_observer = nullptr;
    }
}

} // namespace WebKit

#endif // AVE(CVDISPLAYLINK)
