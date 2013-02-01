/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebTestRunner_h
#define WebTestRunner_h

#include "WebKit/chromium/public/WebTextDirection.h"
#include <set>
#include <string>

namespace WebKit {
class WebArrayBufferView;
class WebPermissionClient;
class WebFrame;
}

namespace WebTestRunner {

class TestRunner;

class WebTestRunner {
public:
#if WEBTESTRUNNER_IMPLEMENTATION
    explicit WebTestRunner(TestRunner*);
#endif

    virtual bool shouldDumpEditingCallbacks() const;
    virtual bool shouldDumpAsText() const;
    virtual void setShouldDumpAsText(bool);
    virtual bool shouldGeneratePixelResults() const;
    virtual void setShouldGeneratePixelResults(bool);
    virtual bool shouldDumpChildFrameScrollPositions() const;
    virtual bool shouldDumpChildFramesAsText() const;
    virtual bool shouldDumpAsAudio() const;
    virtual const WebKit::WebArrayBufferView* audioData() const;
    virtual bool shouldDumpFrameLoadCallbacks() const;
    virtual void setShouldDumpFrameLoadCallbacks(bool);
    virtual bool shouldDumpUserGestureInFrameLoadCallbacks() const;
    virtual bool stopProvisionalFrameLoads() const;
    virtual bool shouldDumpTitleChanges() const;
    virtual bool shouldDumpCreateView() const;
    virtual bool canOpenWindows() const;
    virtual bool shouldDumpResourceLoadCallbacks() const;
    virtual bool shouldDumpResourceRequestCallbacks() const;
    virtual bool shouldDumpResourceResponseMIMETypes() const;
    virtual WebKit::WebPermissionClient* webPermissions() const;
    virtual bool shouldDumpStatusCallbacks() const;
    virtual bool shouldDumpProgressFinishedCallback() const;
    virtual bool shouldDumpBackForwardList() const;
    virtual bool deferMainResourceDataLoad() const;
    virtual bool shouldDumpSelectionRect() const;
    virtual bool testRepaint() const;
    virtual bool sweepHorizontally() const;
    virtual bool isPrinting() const;
    virtual bool shouldStayOnPageAfterHandlingBeforeUnload() const;
    virtual void setTitleTextDirection(WebKit::WebTextDirection);
    virtual const std::set<std::string>* httpHeadersToClear() const;
    virtual bool shouldBlockRedirects() const;
    virtual bool willSendRequestShouldReturnNull() const;
    virtual void setTopLoadingFrame(WebKit::WebFrame*, bool);
    virtual WebKit::WebFrame* topLoadingFrame() const;
    virtual void policyDelegateDone();
    virtual bool policyDelegateEnabled() const;
    virtual bool policyDelegateIsPermissive() const;
    virtual bool policyDelegateShouldNotifyDone() const;
    virtual bool shouldInterceptPostMessage() const;
    virtual bool isSmartInsertDeleteEnabled() const;
    virtual bool isSelectTrailingWhitespaceEnabled() const;

private:
    TestRunner* m_private;
};

}

#endif // WebTestRunner_h
