/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "ScriptedAnimationController.h"

#include "InspectorInstrumentation.h"
#include "Page.h"
#include "RequestAnimationFrameCallback.h"
#include "Settings.h"
#include <wtf/SystemTracing.h>

namespace WebCore {

ScriptedAnimationController::ScriptedAnimationController(Document& document)
    : m_document(makeWeakPtr(document))
{
}

ScriptedAnimationController::~ScriptedAnimationController() = default;

bool ScriptedAnimationController::requestAnimationFrameEnabled() const
{
    return m_document && m_document->settings().requestAnimationFrameEnabled();
}

Page* ScriptedAnimationController::page() const
{
    return m_document ? m_document->page() : nullptr;
}

Seconds ScriptedAnimationController::preferredScriptedAnimationInterval() const
{
    if (auto* page = this->page())
        return page->renderingUpdateThrottlingEnabled() ? preferredFrameInterval(m_throttlingReasons) : FullSpeedAnimationInterval;
    return FullSpeedAnimationInterval;
}

Seconds ScriptedAnimationController::interval() const
{
    if (auto* page = this->page())
        return std::max(preferredScriptedAnimationInterval(), page->preferredRenderingUpdateInterval());
    return FullSpeedAnimationInterval;
}

void ScriptedAnimationController::suspend()
{
    ++m_suspendCount;
}

void ScriptedAnimationController::resume()
{
    // It would be nice to put an ASSERT(m_suspendCount > 0) here, but in WK1 resume() can be called
    // even when suspend hasn't (if a tab was created in the background).
    if (m_suspendCount > 0)
        --m_suspendCount;

    if (!m_suspendCount && m_callbacks.size())
        scheduleAnimation();
}

bool ScriptedAnimationController::isThrottled() const
{
    if (auto* page = this->page())
        return page->renderingUpdateThrottlingEnabled() && (page->isRenderingUpdateThrottled() || !m_throttlingReasons.isEmpty());
    return false;
}

bool ScriptedAnimationController::isThrottledRelativeToPage() const
{
    if (auto* page = this->page())
        return page->preferredRenderingUpdateInterval() < preferredScriptedAnimationInterval();
    return false;
}

bool ScriptedAnimationController::shouldRescheduleRequestAnimationFrame(DOMHighResTimeStamp timestamp) const
{
    return isThrottledRelativeToPage() && Seconds(timestamp - m_lastAnimationFrameTimestamp) < preferredScriptedAnimationInterval();
}

ScriptedAnimationController::CallbackId ScriptedAnimationController::registerCallback(Ref<RequestAnimationFrameCallback>&& callback)
{
    CallbackId callbackId = ++m_nextCallbackId;
    callback->m_firedOrCancelled = false;
    callback->m_id = callbackId;
    m_callbacks.append(WTFMove(callback));

    if (m_document)
        InspectorInstrumentation::didRequestAnimationFrame(*m_document, callbackId);

    if (!m_suspendCount)
        scheduleAnimation();
    return callbackId;
}

void ScriptedAnimationController::cancelCallback(CallbackId callbackId)
{
    bool cancelled = m_callbacks.removeFirstMatching([&](auto& callback) {
        if (callback->m_id != callbackId)
            return false;
        callback->m_firedOrCancelled = true;
        return true;
    });

    if (cancelled && m_document)
        InspectorInstrumentation::didCancelAnimationFrame(*m_document, callbackId);
}

void ScriptedAnimationController::serviceRequestAnimationFrameCallbacks(DOMHighResTimeStamp timestamp)
{
    if (!m_callbacks.size() || m_suspendCount || !requestAnimationFrameEnabled())
        return;

    if (shouldRescheduleRequestAnimationFrame(timestamp)) {
        scheduleAnimation();
        return;
    }
    
    TraceScope tracingScope(RAFCallbackStart, RAFCallbackEnd);

    // We round this to the nearest microsecond so that we can return a time that matches what is returned by document.timeline.currentTime.
    DOMHighResTimeStamp highResNowMs = std::round(1000 * timestamp);

    // First, generate a list of callbacks to consider.  Callbacks registered from this point
    // on are considered only for the "next" frame, not this one.
    CallbackList callbacks(m_callbacks);

    // Invoking callbacks may detach elements from our document, which clears the document's
    // reference to us, so take a defensive reference.
    Ref<ScriptedAnimationController> protectedThis(*this);
    Ref<Document> protectedDocument(*m_document);

    for (auto& callback : callbacks) {
        if (callback->m_firedOrCancelled)
            continue;
        callback->m_firedOrCancelled = true;

        InspectorInstrumentation::willFireAnimationFrame(protectedDocument, callback->m_id);
        callback->handleEvent(highResNowMs);
        InspectorInstrumentation::didFireAnimationFrame(protectedDocument);
    }

    // Remove any callbacks we fired from the list of pending callbacks.
    m_callbacks.removeAllMatching([](auto& callback) {
        return callback->m_firedOrCancelled;
    });

    m_lastAnimationFrameTimestamp = timestamp;

    if (m_callbacks.size())
        scheduleAnimation();
}

void ScriptedAnimationController::scheduleAnimation()
{
    if (!requestAnimationFrameEnabled())
        return;

    if (auto* page = this->page())
        page->renderingUpdateScheduler().scheduleTimedRenderingUpdate();
}

}
