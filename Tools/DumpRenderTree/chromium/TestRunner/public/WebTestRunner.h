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

namespace WebKit {
class WebArrayBufferView;
class WebPermissionClient;
}

namespace WebTestRunner {

// FIXME: Once the TestRunner class is complete, this class should take a
// TestRunner* as ctor argument, and not have default implementations.
class WebTestRunner {
public:
    virtual void setTestIsRunning(bool) { }
    virtual bool shouldDumpEditingCallbacks() const { return false; }
    virtual bool shouldDumpAsText() const { return false; }
    virtual void setShouldDumpAsText(bool) { }
    virtual bool shouldGeneratePixelResults() const { return false; }
    virtual void setShouldGeneratePixelResults(bool) { }
    virtual bool shouldDumpChildFrameScrollPositions() const { return false; }
    virtual bool shouldDumpChildFramesAsText() const { return false; }
    virtual bool shouldDumpAsAudio() const { return false; }
    virtual const WebKit::WebArrayBufferView* audioData() const { return 0; }
    virtual bool shouldDumpFrameLoadCallbacks() const { return false; }
    virtual void setShouldDumpFrameLoadCallbacks(bool) { }
    virtual bool shouldDumpUserGestureInFrameLoadCallbacks() const { return false; }
    virtual bool stopProvisionalFrameLoads() const { return false; }
    virtual bool shouldDumpTitleChanges() const { return false; }
    virtual bool shouldDumpCreateView() const { return false; }
    virtual bool canOpenWindows() const { return false; }
    virtual bool shouldDumpResourceLoadCallbacks() const { return false; }
    virtual bool shouldDumpResourceRequestCallbacks() const { return false; }
    virtual bool shouldDumpResourceResponseMIMETypes() const { return false; }
    virtual WebKit::WebPermissionClient* webPermissions() const { return 0; }
    virtual bool shouldDumpStatusCallbacks() const { return false; }
    virtual bool shouldDumpProgressFinishedCallback() const { return false; }
    virtual bool shouldDumpBackForwardList() const { return false; }
    virtual bool deferMainResourceDataLoad() const { return false; }
    virtual bool shouldDumpSelectionRect() const { return false; }
    virtual bool testRepaint() const { return false; }
    virtual bool sweepHorizontally() const { return false; }
    virtual bool isPrinting() const { return false; }
    virtual bool shouldStayOnPageAfterHandlingBeforeUnload() const { return false; } 
};

}

#endif // WebTestRunner_h
