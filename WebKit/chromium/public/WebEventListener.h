/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebEventListener_h
#define WebEventListener_h

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class Node; }
#endif

namespace WebKit {

class EventListenerWrapper;
class WebEvent;
class WebEventListenerPrivate;
class WebNode;
class WebString;

class WebEventListener {
public:
    WebEventListener();
    virtual ~WebEventListener();

    // Called when an event is received.
    virtual void handleEvent(const WebEvent&) = 0;

#if WEBKIT_IMPLEMENTATION
    void notifyEventListenerDeleted(EventListenerWrapper*);
    EventListenerWrapper* createEventListenerWrapper(const WebString& eventType, bool useCapture, WebCore::Node* node);
    EventListenerWrapper* getEventListenerWrapper(const WebString& eventType, bool useCapture, WebCore::Node* node);
#endif

private:
    WebEventListenerPrivate* m_private;
};

} // namespace WebKit

#endif
