/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "MainFrame.h"
#include "NavigationScheduler.h"
#include "Page.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/MainThread.h>

namespace WebCore {

History::History(Frame& frame)
    : DOMWindowProperty(&frame)
{
}

unsigned History::length() const
{
    if (!m_frame)
        return 0;
    auto* page = m_frame->page();
    if (!page)
        return 0;
    return page->backForward().count();
}

ExceptionOr<History::ScrollRestoration> History::scrollRestoration() const
{
    if (!m_frame)
        return Exception { SecurityError };

    auto* historyItem = m_frame->loader().history().currentItem();
    if (!historyItem)
        return ScrollRestoration::Auto;
    
    return historyItem->shouldRestoreScrollPosition() ? ScrollRestoration::Auto : ScrollRestoration::Manual;
}

ExceptionOr<void> History::setScrollRestoration(ScrollRestoration scrollRestoration)
{
    if (!m_frame)
        return Exception { SecurityError };

    auto* historyItem = m_frame->loader().history().currentItem();
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
    if (!m_frame)
        return nullptr;
    auto* historyItem = m_frame->loader().history().currentItem();
    if (!historyItem)
        return nullptr;
    return historyItem->stateObject();
}

bool History::stateChanged() const
{
    return m_lastStateObjectRequested != stateInternal();
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
    LOG(History, "History %p go(%d) frame %p (main frame %d)", this, distance, m_frame, m_frame ? m_frame->isMainFrame() : false);

    if (!m_frame)
        return;

    m_frame->navigationScheduler().scheduleHistoryNavigation(distance);
}

void History::go(Document& document, int distance)
{
    LOG(History, "History %p go(%d) in document %p frame %p (main frame %d)", this, distance, &document, m_frame, m_frame ? m_frame->isMainFrame() : false);

    if (!m_frame)
        return;

    ASSERT(isMainThread());

    if (!document.canNavigate(m_frame))
        return;

    m_frame->navigationScheduler().scheduleHistoryNavigation(distance);
}

URL History::urlForState(const String& urlString)
{
    URL baseURL = m_frame->document()->baseURL();
    if (urlString.isEmpty())
        return baseURL;

    return URL(baseURL, urlString);
}

ExceptionOr<void> History::stateObjectAdded(RefPtr<SerializedScriptValue>&& data, const String& title, const String& urlString, StateObjectType stateObjectType)
{
    // Each unique main-frame document is only allowed to send 64MB of state object payload to the UI client/process.
    static uint32_t totalStateObjectPayloadLimit = 0x4000000;
    static double stateObjectTimeSpan = 30.0;
    static unsigned perStateObjectTimeSpanLimit = 100;

    if (!m_frame || !m_frame->page())
        return { };

    URL fullURL = urlForState(urlString);
    if (!fullURL.isValid())
        return Exception { SecurityError };

    const URL& documentURL = m_frame->document()->url();

    auto createBlockedURLSecurityErrorWithMessageSuffix = [&] (const char* suffix) {
        const char* functionName = stateObjectType == StateObjectType::Replace ? "history.replaceState()" : "history.pushState()";
        return Exception { SecurityError, makeString("Blocked attempt to use ", functionName, " to change session history URL from ", documentURL.stringCenterEllipsizedToLength(), " to ", fullURL.stringCenterEllipsizedToLength(), ". ", suffix) };
    };
    if (!protocolHostAndPortAreEqual(fullURL, documentURL) || fullURL.user() != documentURL.user() || fullURL.pass() != documentURL.pass())
        return createBlockedURLSecurityErrorWithMessageSuffix("Protocols, domains, ports, usernames, and passwords must match.");
    if (!m_frame->document()->securityOrigin().canRequest(fullURL) && (fullURL.path() != documentURL.path() || fullURL.query() != documentURL.query()))
        return createBlockedURLSecurityErrorWithMessageSuffix("Paths and fragments must match for a sandboxed document.");

    Document* mainDocument = m_frame->page()->mainFrame().document();
    History* mainHistory = nullptr;
    if (mainDocument) {
        if (auto* mainDOMWindow = mainDocument->domWindow())
            mainHistory = mainDOMWindow->history();
    }

    if (!mainHistory)
        return { };

    double currentTimestamp = currentTime();
    if (currentTimestamp - mainHistory->m_currentStateObjectTimeSpanStart > stateObjectTimeSpan) {
        mainHistory->m_currentStateObjectTimeSpanStart = currentTimestamp;
        mainHistory->m_currentStateObjectTimeSpanObjectsAdded = 0;
    }
    
    if (mainHistory->m_currentStateObjectTimeSpanObjectsAdded >= perStateObjectTimeSpanLimit) {
        if (stateObjectType == StateObjectType::Replace)
            return Exception { SecurityError, String::format("Attempt to use history.replaceState() more than %u times per %f seconds", perStateObjectTimeSpanLimit, stateObjectTimeSpan) };
        return Exception { SecurityError, String::format("Attempt to use history.pushState() more than %u times per %f seconds", perStateObjectTimeSpanLimit, stateObjectTimeSpan) };
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
            return Exception { QuotaExceededError, ASCIILiteral("Attempt to store more data than allowed using history.replaceState()") };
        return Exception { QuotaExceededError, ASCIILiteral("Attempt to store more data than allowed using history.pushState()") };
    }

    m_mostRecentStateObjectUsage = payloadSize.unsafeGet();

    mainHistory->m_totalStateObjectUsage = newTotalUsage.unsafeGet();
    ++mainHistory->m_currentStateObjectTimeSpanObjectsAdded;

    if (!urlString.isEmpty())
        m_frame->document()->updateURLForPushOrReplaceState(fullURL);

    if (stateObjectType == StateObjectType::Push) {
        m_frame->loader().history().pushState(WTFMove(data), title, fullURL.string());
        m_frame->loader().client().dispatchDidPushStateWithinPage();
    } else if (stateObjectType == StateObjectType::Replace) {
        m_frame->loader().history().replaceState(WTFMove(data), title, fullURL.string());
        m_frame->loader().client().dispatchDidReplaceStateWithinPage();
    }

    return { };
}

} // namespace WebCore
