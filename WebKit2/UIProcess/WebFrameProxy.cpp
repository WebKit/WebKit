/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebFrameProxy.h"

#include "WebFormSubmissionListenerProxy.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageProxy.h"
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

WebFrameProxy::WebFrameProxy(WebPageProxy* page, uint64_t frameID)
    : m_page(page)
    , m_loadState(LoadStateFinished)
    , m_frameID(frameID)
{
}

WebFrameProxy::~WebFrameProxy()
{
}

void WebFrameProxy::disconnect()
{
    m_page = 0;
    if (m_activeListener) {
        m_activeListener->invalidate();
        m_activeListener = 0;
    }
}

bool WebFrameProxy::isMainFrame() const
{
    if (!m_page)
        return false;

    return this == m_page->mainFrame();
}

void WebFrameProxy::didStartProvisionalLoad(const String& url)
{
    // FIXME: Add assertions.
    m_loadState = LoadStateProvisional;
    m_provisionalURL = url;
}

void WebFrameProxy::didReceiveServerRedirectForProvisionalLoad(const String& url)
{
    m_provisionalURL = url;
}

void WebFrameProxy::didCommitLoad()
{
    // FIXME: Add assertions.
    m_loadState = LoadStateCommitted;
    m_url = m_provisionalURL;
    m_provisionalURL = String();
}

void WebFrameProxy::didFinishLoad()
{
    // FIXME: Add assertions
    m_loadState = LoadStateFinished;
}

void WebFrameProxy::didReceiveTitle(const WTF::String&)
{
}

void WebFrameProxy::receivedPolicyDecision(WebCore::PolicyAction action, uint64_t listenerID)
{
    if (!m_page)
        return;

    ASSERT(m_activeListener);
    ASSERT(m_activeListener->listenerID() == listenerID);
    m_page->receivedPolicyDecision(action, this, listenerID);
}

WebFramePolicyListenerProxy* WebFrameProxy::setUpPolicyListenerProxy(uint64_t listenerID)
{
    if (m_activeListener)
        m_activeListener->invalidate();
    m_activeListener = WebFramePolicyListenerProxy::create(this, listenerID);
    return static_cast<WebFramePolicyListenerProxy*>(m_activeListener.get());
}

WebFormSubmissionListenerProxy* WebFrameProxy::setUpFormSubmissionListenerProxy(uint64_t listenerID)
{
    if (m_activeListener)
        m_activeListener->invalidate();
    m_activeListener = WebFormSubmissionListenerProxy::create(this, listenerID);
    return static_cast<WebFormSubmissionListenerProxy*>(m_activeListener.get());
}

} // namespace WebKit
