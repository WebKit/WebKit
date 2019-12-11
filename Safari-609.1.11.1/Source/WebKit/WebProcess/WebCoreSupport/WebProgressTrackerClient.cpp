/*
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
#include "WebProgressTrackerClient.h"

#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/ProgressTracker.h>

namespace WebKit {
using namespace WebCore;

WebProgressTrackerClient::WebProgressTrackerClient(WebPage& webPage)
    : m_webPage(webPage)
{
}
    
void WebProgressTrackerClient::progressTrackerDestroyed()
{
    delete this;
}
    
void WebProgressTrackerClient::progressStarted(Frame& originatingProgressFrame)
{
    if (!originatingProgressFrame.isMainFrame())
        return;

    m_webPage.setMainFrameProgressCompleted(false);
    m_webPage.send(Messages::WebPageProxy::DidStartProgress());
}

void WebProgressTrackerClient::progressEstimateChanged(Frame& originatingProgressFrame)
{
    if (!originatingProgressFrame.isMainFrame())
        return;
    
    double progress = m_webPage.corePage()->progress().estimatedProgress();
    m_webPage.send(Messages::WebPageProxy::DidChangeProgress(progress));
}

void WebProgressTrackerClient::progressFinished(Frame& originatingProgressFrame)
{
    if (!originatingProgressFrame.isMainFrame())
        return;

    m_webPage.setMainFrameProgressCompleted(true);

    // Notify the bundle client.
    m_webPage.injectedBundleLoaderClient().didFinishProgress(m_webPage);

    m_webPage.send(Messages::WebPageProxy::DidFinishProgress());
}

} // namespace WebKit
