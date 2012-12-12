/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockWebRTCDataChannelHandler_h
#define MockWebRTCDataChannelHandler_h

#include "WebTask.h"
#include <public/WebRTCDataChannelHandler.h>
#include <public/WebString.h>

class MockWebRTCDataChannelHandler : public WebKit::WebRTCDataChannelHandler {
public:
    MockWebRTCDataChannelHandler(WebKit::WebString label, bool reliable);

    virtual void setClient(WebKit::WebRTCDataChannelHandlerClient*) OVERRIDE;
    virtual WebKit::WebString label() OVERRIDE { return m_label; }
    virtual bool isReliable() OVERRIDE { return m_reliable; }
    virtual unsigned long bufferedAmount() OVERRIDE;
    virtual bool sendStringData(const WebKit::WebString&) OVERRIDE;
    virtual bool sendRawData(const char*, size_t) OVERRIDE;
    virtual void close() OVERRIDE;

    // WebTask related methods
    WebTestRunner::WebTaskList* taskList() { return &m_taskList; }

private:
    MockWebRTCDataChannelHandler();

    WebKit::WebRTCDataChannelHandlerClient* m_client;
    WebKit::WebString m_label;
    bool m_reliable;
    WebTestRunner::WebTaskList m_taskList;
};

#endif // MockWebRTCDataChannelHandler_h
