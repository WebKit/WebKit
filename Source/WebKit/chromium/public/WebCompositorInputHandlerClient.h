/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCompositorInputHandlerClient_h
#define WebCompositorInputHandlerClient_h

namespace WebKit {

class WebCompositorInputHandlerClient {
public:
    // Callbacks invoked from the compositor thread.
    virtual void willShutdown() = 0;

    // Exactly one of the following two callbacks will be invoked after every call to WebCompositor::handleInputEvent():

    // Called when the WebCompositor handled the input event and no further processing is required.
    virtual void didHandleInputEvent() = 0;

    // Called when the WebCompositor did not handle the input event. If sendToWidget is true, the input event
    // should be forwarded to the WebWidget associated with this compositor for further processing.
    virtual void didNotHandleInputEvent(bool sendToWidget) = 0;

protected:
    virtual ~WebCompositorInputHandlerClient() { }
};

} // namespace WebKit

#endif
