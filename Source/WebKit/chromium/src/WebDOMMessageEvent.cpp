/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "WebDOMMessageEvent.h"

#include "DOMWindow.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "PlatformMessagePortChannel.h"
#include "SerializedScriptValue.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "platform/WebSerializedScriptValue.h"
#include "platform/WebString.h"

#if USE(V8)
#include "V8Proxy.h"
#endif

using namespace WebCore;

namespace WebKit {

void WebDOMMessageEvent::initMessageEvent(const WebString& type, bool canBubble, bool cancelable, const WebSerializedScriptValue& messageData, const WebString& origin, const WebFrame* sourceFrame, const WebString& lastEventId)
{
    ASSERT(m_private);
    ASSERT(isMessageEvent());
    DOMWindow* window = 0;
    if (sourceFrame)
        window = static_cast<const WebFrameImpl*>(sourceFrame)->frame()->domWindow();
    OwnPtr<MessagePortArray> ports;
    unwrap<MessageEvent>()->initMessageEvent(type, canBubble, cancelable, messageData, origin, lastEventId, window, ports.release());
}

WebSerializedScriptValue WebDOMMessageEvent::data() const
{
    return WebSerializedScriptValue(constUnwrap<MessageEvent>()->data());
}

WebString WebDOMMessageEvent::origin() const
{
    return WebString(constUnwrap<MessageEvent>()->origin());
}

} // namespace WebKit
