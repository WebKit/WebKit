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

class WebTestRunner {
public:
    virtual bool shouldDumpEditingCallbacks() const = 0;
    virtual bool shouldDumpAsText() const = 0;
    virtual void setShouldDumpAsText(bool) = 0;
    virtual bool shouldGeneratePixelResults() const = 0;
    virtual void setShouldGeneratePixelResults(bool) = 0;
    virtual bool shouldDumpChildFrameScrollPositions() const = 0;
    virtual bool shouldDumpChildFramesAsText() const = 0;
    virtual bool shouldDumpAsAudio() const = 0;
    virtual const WebKit::WebArrayBufferView* audioData() const = 0;
    virtual bool shouldDumpFrameLoadCallbacks() const = 0;
    virtual void setShouldDumpFrameLoadCallbacks(bool) = 0;
    virtual bool shouldDumpUserGestureInFrameLoadCallbacks() const = 0;
    virtual bool stopProvisionalFrameLoads() const = 0;
    virtual bool shouldDumpTitleChanges() const = 0;
    virtual bool shouldDumpCreateView() const = 0;
    virtual bool canOpenWindows() const = 0;
    virtual bool shouldDumpResourceLoadCallbacks() const = 0;
    virtual bool shouldDumpResourceRequestCallbacks() const = 0;
    virtual bool shouldDumpResourceResponseMIMETypes() const = 0;
    virtual WebKit::WebPermissionClient* webPermissions() const = 0;
    virtual bool shouldDumpStatusCallbacks() const = 0;
    virtual bool shouldDumpProgressFinishedCallback() const = 0;
    virtual bool shouldDumpBackForwardList() const = 0;
    virtual bool deferMainResourceDataLoad() const = 0;
    virtual bool shouldDumpSelectionRect() const = 0;
    virtual bool testRepaint() const = 0;
    virtual bool sweepHorizontally() const = 0;
    virtual bool isPrinting() const = 0;
    virtual bool shouldStayOnPageAfterHandlingBeforeUnload() const = 0;
    virtual void setTitleTextDirection(WebKit::WebTextDirection) = 0;
    virtual const std::set<std::string>* httpHeadersToClear() const = 0;
    virtual bool shouldBlockRedirects() const = 0;
    virtual bool willSendRequestShouldReturnNull() const = 0;
    virtual void setTopLoadingFrame(WebKit::WebFrame*, bool) = 0;
    virtual WebKit::WebFrame* topLoadingFrame() const = 0;
    virtual void policyDelegateDone() = 0;
    virtual bool policyDelegateEnabled() const = 0;
    virtual bool policyDelegateIsPermissive() const = 0;
    virtual bool policyDelegateShouldNotifyDone() const = 0;
    virtual bool shouldInterceptPostMessage() const = 0;
    virtual bool isSmartInsertDeleteEnabled() const = 0;
    virtual bool isSelectTrailingWhitespaceEnabled() const = 0;
};

}

#endif // WebTestRunner_h
