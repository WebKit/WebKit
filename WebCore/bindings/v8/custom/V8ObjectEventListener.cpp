/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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
#include "V8ObjectEventListener.h"

#include "Frame.h"
#include "V8Proxy.h"

namespace WebCore {

static void weakObjectEventListenerCallback(v8::Persistent<v8::Value>, void* parameter)
{
    V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(parameter);

    // Remove the wrapper
    Frame* frame = listener->frame();
    if (frame) {
        V8Proxy* proxy = V8Proxy::retrieve(frame);
        if (proxy)
            proxy->RemoveObjectEventListener(listener);

        // Because the listener is no longer in the list, it must be disconnected from the frame to avoid dangling frame pointer
        // in the destructor.
        listener->disconnectFrame();
    }
    listener->disposeListenerObject();
}

V8ObjectEventListener::V8ObjectEventListener(Frame* frame, v8::Local<v8::Object> listener, bool isInline)
    : V8EventListener(frame, listener, isInline)
{
    m_listener.MakeWeak(this, weakObjectEventListenerCallback);
}

V8ObjectEventListener::~V8ObjectEventListener()
{
    if (m_frame) {
        ASSERT(!m_listener.IsEmpty());
        V8Proxy* proxy = V8Proxy::retrieve(m_frame);
        if (proxy)
            proxy->RemoveObjectEventListener(this);
    }

    disposeListenerObject();
}

} // namespace WebCore
