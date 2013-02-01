/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebTestRunner.h"

#include "TestRunner.h"

using namespace WebKit;

namespace WebTestRunner {

WebTestRunner::WebTestRunner(TestRunner* testRunner)
    : m_private(testRunner)
{
}

bool WebTestRunner::shouldDumpEditingCallbacks() const
{
    return m_private->shouldDumpEditingCallbacks();
}

bool WebTestRunner::shouldDumpAsText() const
{
    return m_private->shouldDumpAsText();
}

void WebTestRunner::setShouldDumpAsText(bool value)
{
    m_private->setShouldDumpAsText(value);
}

bool WebTestRunner::shouldGeneratePixelResults() const
{
    return m_private->shouldGeneratePixelResults();
}

void WebTestRunner::setShouldGeneratePixelResults(bool value)
{
    m_private->setShouldGeneratePixelResults(value);
}

bool WebTestRunner::shouldDumpChildFrameScrollPositions() const
{
    return m_private->shouldDumpChildFrameScrollPositions();
}

bool WebTestRunner::shouldDumpChildFramesAsText() const
{
    return m_private->shouldDumpChildFramesAsText();
}

bool WebTestRunner::shouldDumpAsAudio() const
{
    return m_private->shouldDumpAsAudio();
}

const WebArrayBufferView* WebTestRunner::audioData() const
{
    return m_private->audioData();
}

bool WebTestRunner::shouldDumpFrameLoadCallbacks() const
{
    return m_private->shouldDumpFrameLoadCallbacks();
}

void WebTestRunner::setShouldDumpFrameLoadCallbacks(bool value)
{
    m_private->setShouldDumpFrameLoadCallbacks(value);
}

bool WebTestRunner::shouldDumpUserGestureInFrameLoadCallbacks() const
{
    return m_private->shouldDumpUserGestureInFrameLoadCallbacks();
}

bool WebTestRunner::stopProvisionalFrameLoads() const
{
    return m_private->stopProvisionalFrameLoads();
}

bool WebTestRunner::shouldDumpTitleChanges() const
{
    return m_private->shouldDumpTitleChanges();
}

bool WebTestRunner::shouldDumpCreateView() const
{
    return m_private->shouldDumpCreateView();
}

bool WebTestRunner::canOpenWindows() const
{
    return m_private->canOpenWindows();
}

bool WebTestRunner::shouldDumpResourceLoadCallbacks() const
{
    return m_private->shouldDumpResourceLoadCallbacks();
}

bool WebTestRunner::shouldDumpResourceRequestCallbacks() const
{
    return m_private->shouldDumpResourceRequestCallbacks();
}

bool WebTestRunner::shouldDumpResourceResponseMIMETypes() const
{
    return m_private->shouldDumpResourceResponseMIMETypes();
}

WebPermissionClient* WebTestRunner::webPermissions() const
{
    return m_private->webPermissions();
}

bool WebTestRunner::shouldDumpStatusCallbacks() const
{
    return m_private->shouldDumpStatusCallbacks();
}

bool WebTestRunner::shouldDumpProgressFinishedCallback() const
{
    return m_private->shouldDumpProgressFinishedCallback();
}

bool WebTestRunner::shouldDumpBackForwardList() const
{
    return m_private->shouldDumpBackForwardList();
}

bool WebTestRunner::deferMainResourceDataLoad() const
{
    return m_private->deferMainResourceDataLoad();
}

bool WebTestRunner::shouldDumpSelectionRect() const
{
    return m_private->shouldDumpSelectionRect();
}

bool WebTestRunner::testRepaint() const
{
    return m_private->testRepaint();
}

bool WebTestRunner::sweepHorizontally() const
{
    return m_private->sweepHorizontally();
}

bool WebTestRunner::isPrinting() const
{
    return m_private->isPrinting();
}

bool WebTestRunner::shouldStayOnPageAfterHandlingBeforeUnload() const
{
    return m_private->shouldStayOnPageAfterHandlingBeforeUnload();
}

void WebTestRunner::setTitleTextDirection(WebTextDirection direction)
{
    m_private->setTitleTextDirection(direction);
}

const std::set<std::string>* WebTestRunner::httpHeadersToClear() const
{
    return m_private->httpHeadersToClear();
}

bool WebTestRunner::shouldBlockRedirects() const
{
    return m_private->shouldBlockRedirects();
}

bool WebTestRunner::willSendRequestShouldReturnNull() const
{
    return m_private->willSendRequestShouldReturnNull();
}

void WebTestRunner::setTopLoadingFrame(WebFrame* frame, bool clear)
{
    m_private->setTopLoadingFrame(frame, clear);
}

WebFrame* WebTestRunner::topLoadingFrame() const
{
    return m_private->topLoadingFrame();
}

void WebTestRunner::policyDelegateDone()
{
    m_private->policyDelegateDone();
}

bool WebTestRunner::policyDelegateEnabled() const
{
    return m_private->policyDelegateEnabled();
}

bool WebTestRunner::policyDelegateIsPermissive() const
{
    return m_private->policyDelegateIsPermissive();
}

bool WebTestRunner::policyDelegateShouldNotifyDone() const
{
    return m_private->policyDelegateShouldNotifyDone();
}

bool WebTestRunner::shouldInterceptPostMessage() const
{
    return m_private->shouldInterceptPostMessage();
}

bool WebTestRunner::isSmartInsertDeleteEnabled() const
{
    return m_private->isSmartInsertDeleteEnabled();
}

bool WebTestRunner::isSelectTrailingWhitespaceEnabled() const
{
    return m_private->isSelectTrailingWhitespaceEnabled();
}

}
