/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebDevToolsAgentPrivate_h
#define WebDevToolsAgentPrivate_h

#include "WebDevToolsAgent.h"

namespace WebKit {
class WebFrameImpl;
struct WebSize;

class WebDevToolsAgentPrivate : public WebDevToolsAgent {
public:

    // Notification from FrameLoaderClientImpl:
    // New context has been created for a given world in given frame. Any
    // processing hat needs to happen before the first script is evaluated
    // in this context should be done here.
    virtual void didCreateScriptContext(WebFrameImpl*, int worldId) = 0;

    // A new FrameView has been created for the specified WebFrame using
    // the Frame::createView() call.
    virtual void mainFrameViewCreated(WebFrameImpl*) = 0;

    // Returns true if the device metrics override mode is enabled.
    virtual bool metricsOverridden() = 0;

    // WebViewImpl has been resized.
    virtual void webViewResized() = 0;
};

} // namespace WebKit

#endif
