/*
 * Copyright (C) 2007-2018 Apple Inc.  All rights reserved.
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
#include "History.h"

#include "BackForwardController.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "Logging.h"
#include "NavigationScheduler.h"
#include "Page.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/MainThread.h>

namespace WebCore {

History::History(DOMWindow& window)
    : DOMWindowProperty(&window)
{
}

unsigned History::length() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    auto* page = frame->page();
    if (!page)
        return 0;
    return page->backForward().count();
}

ExceptionOr<History::ScrollRestoration> History::scrollRestoration() const
{
    auto* frame = this->frame();
    if (!frame)
        return Exception { SecurityError };

    auto* historyItem = frame->loader().history().currentItem();
    if (!historyItem)
        return ScrollRestoration::Auto;
    
    return historyItem->shouldRestoreScrollPosition() ? ScrollRestoration::Auto : ScrollRestoration::Manual;
}

ExceptionOr<void> History::setScrollRestoration(ScrollRestoration scrollRestoration)
{
    auto* frame = this->frame();
    if (!frame)
        return Exception { SecurityError };

    auto* historyItem = frame->loader().history().currentItem();
    if (historyItem)
        historyItem->setShouldRestoreScrollPosition(scrollRestoration == ScrollRestoration::Auto);

    return { };
}

SerializedScriptValue* History::state()
{
    m_lastStateObjectRequested = stateInternal();
    return m_lastStateObjectRequested.get();
}

SerializedScriptValue* History::stateInternal() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;
    auto* historyItem = frame->loader().history().currentItem();
    if (!historyItem)
        return nullptr;
    return historyItem->stateObject();
}

bool History::stateChanged() const
{
    return m_lastStateObjectRequested != stateInternal();
}

JSValueInWrappedObject& History::cachedState()
{
    if (m_cachedState && stateChanged())
        m_cachedState = { };
    return m_cachedState;
}

bool History::isSameAsCurrentState(SerializedScriptValue* state) const
{
    return state == stateInternal();
}

void History::back()
{
    go(-1);
}

void History::back(Document& document)
{
    go(document, -1);
}

void History::forward()
{
    go(1);
}

void History::forward(Document& document)
{
    go(document, 1);
}

void History::go(int distance)
{
    auto* frame = this->frame();
    LOG(History, "History %p go(%d) frame %p (main frame %d)", this, distance, frame, frame ? frame->isMainFrame() : false);

    if (!frame)
        return;

    frame->navigationScheduler().scheduleHistoryNavigation(distance);
}

void History::go(Document& document, int distance)
{
    auto* frame = this->frame();
    LOG(History, "History %p go(%d) in document %p frame %p (main frame %d)", this, distance, &document, frame, frame ? frame->isMainFrame() : false);

    if (!frame)
        return;

    ASSERT(isMainThread());

    if (!document.canNavigate(frame))
        return;

    frame->navigationScheduler().scheduleHistoryNavigation(distance);
}

URL History::urlForState(const String& urlString)
{
    auto* frame = this->frame();
    if (urlString.isNull())
        return frame->document()->url();
    return frame->document()->completeURL(urlString);
}

ExceptionOr<void> History::stateObjectAdded(RefPtr<SerializedScriptValue>&& data, const String& title, const String& urlString, StateObjectType stateObjectType)
{
    m_cachedState = { };

    // Each unique main-frame document is only allowed to send 64MB of state object payload to the UI client/process.
    static uint32_t totalStateObjectPayloadLimit = 0x4000000;
    static Seconds stateObjectTimeSpan { 30_s };
    static unsigned perStateObjectTimeSpanLimit = 100;

    auto* frame = this->frame();
    if (!frame || !frame->page())
        return { };

    URL fullURL = urlForState(urlString);
    if (!fullURL.isValid())
        return Exception { SecurityError };

    const URL& documentURL = frame->document()->url();

    auto createBlockedURLSecurityErrorWithMessageSuffix = [&] (const char* suffix) {
        const char* functionName = stateObjectType == StateObjectType::Replace ? "history.replaceState()" : "history.pushState()";
        return Exception { SecurityError, makeString("Blocked attempt to use ", functionName, " to change session history URL from ", documentURL.stringCenterEllipsizedToLength(), " to ", fullURL.stringCenterEllipsizedToLength(), ". ", suffix) };
    };
    if (!protocolHostAndPortAreEqual(fullURL, documentURL) || fullURL.user() != documentURL.user() || fullURL.pass() != documentURL.pass())
        return createBlockedURLSecurityErrorWithMessageSuffix("Protocols, domains, ports, usernames, and passwords must match.");

    const auto& documentSecurityOrigin = frame->document()->securityOrigin();
    // We allow sandboxed documents, 'data:'/'file:' URLs, etc. to use 'pushState'/'replaceState' to modify the URL query and fragments.
    // See https://bugs.webkit.org/show_bug.cgi?id=183028 for the compatibility concerns.
    bool allowSandboxException = (documentSecurityOrigin.isLocal() || documentSecurityOrigin.isUnique()) && equalIgnoringQueryAndFragment(documentURL, fullURL);

    if (!allowSandboxException && !documentSecurityOrigin.canRequest(fullURL) && (fullURL.path() != documentURL.path() || fullURL.query() != documentURL.query()))
        return createBlockedURLSecurityErrorWithMessageSuffix("Paths and fragments must match for a sandboxed document.");

    Document* mainDocument = frame->page()->mainFrame().document();
    History* mainHistory = nullptr;
    if (mainDocument) {
        if (auto* mainDOMWindow = mainDocument->domWindow())
            mainHistory = mainDOMWindow->history();
    }

    if (!mainHistory)
        return { };

    WallTime currentTimestamp = WallTime::now();
    if (currentTimestamp - mainHistory->m_currentStateObjectTimeSpanStart > stateObjectTimeSpan) {
        mainHistory->m_currentStateObjectTimeSpanStart = currentTimestamp;
        mainHistory->m_currentStateObjectTimeSpanObjectsAdded = 0;
    }
    
    if (mainHistory->m_currentStateObjectTimeSpanObjectsAdded >= perStateObjectTimeSpanLimit) {
        if (stateObjectType == StateObjectType::Replace)
            return Exception { SecurityError, String::format("Attempt to use history.replaceState() more than %u times per %f seconds", perStateObjectTimeSpanLimit, stateObjectTimeSpan.seconds()) };
        return Exception { SecurityError, String::format("Attempt to use history.pushState() more than %u times per %f seconds", perStateObjectTimeSpanLimit, stateObjectTimeSpan.seconds()) };
    }

    Checked<unsigned> titleSize = title.length();
    titleSize *= 2;

    Checked<unsigned> urlSize = fullURL.string().length();
    urlSize *= 2;

    Checked<uint64_t> payloadSize = titleSize;
    payloadSize += urlSize;
    payloadSize += data ? data->data().size() : 0;

    Checked<uint64_t> newTotalUsage = mainHistory->m_totalStateObjectUsage;

    if (stateObjectType == StateObjectType::Replace)
        newTotalUsage -= m_mostRecentStateObjectUsage;
    newTotalUsage += payloadSize;

    if (newTotalUsage > totalStateObjectPayloadLimit) {
        if (stateObjectType == StateObjectType::Replace)
            return Exception { QuotaExceededError, "Attempt to store more data than allowed using history.replaceState()"_s };
        return Exception { QuotaExceededError, "Attempt to store more data than allowed using history.pushState()"_s };
    }

    m_mostRecentStateObjectUsage = payloadSize.unsafeGet();

    mainHistory->m_totalStateObjectUsage = newTotalUsage.unsafeGet();
    ++mainHistory->m_currentStateObjectTimeSpanObjectsAdded;

    if (!urlString.isEmpty())
        frame->document()->updateURLForPushOrReplaceState(fullURL);

    if (stateObjectType == StateObjectType::Push) {
        frame->loader().history().pushState(WTFMove(data), title, fullURL.string());
        frame->loader().client().dispatchDidPushStateWithinPage();
    } else if (stateObjectType == StateObjectType::Replace) {
        frame->loader().history().replaceState(WTFMove(data), title, fullURL.string());
        frame->loader().client().dispatchDidReplaceStateWithinPage();
    }

    return { };
}

} // namespace WebCore
