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

#ifndef WebDOMEventListenerPrivate_h
#define WebDOMEventListenerPrivate_h

#include "WebString.h"

#include <wtf/Vector.h>

namespace WebCore {
class Node;
}

namespace WebKit {

class EventListenerWrapper;
class WebDOMEventListener;

class WebDOMEventListenerPrivate {
public:
    WebDOMEventListenerPrivate(WebDOMEventListener* webDOMEventListener);
    ~WebDOMEventListenerPrivate();

    EventListenerWrapper* createEventListenerWrapper(
        const WebString& eventType, bool useCapture, WebCore::Node*);

    // Gets the ListenerEventWrapper for a specific node.
    // Used by WebNode::removeDOMEventListener().
    EventListenerWrapper* getEventListenerWrapper(
        const WebString& eventType, bool useCapture, WebCore::Node*);

    // Called by the WebDOMEventListener when it is about to be deleted.
    void webDOMEventListenerDeleted();

    // Called by the EventListenerWrapper when it is about to be deleted.
    void eventListenerDeleted(EventListenerWrapper* eventListener);

    struct ListenerInfo {
        ListenerInfo(const WebString& eventType, bool useCapture,
                     EventListenerWrapper* eventListenerWrapper,
                     WebCore::Node* node)
            : eventType(eventType)
            , useCapture(useCapture)
            , eventListenerWrapper(eventListenerWrapper)
            , node(node)
        {
        }

        WebString eventType;
        bool useCapture;
        EventListenerWrapper* eventListenerWrapper;
        WebCore::Node* node;
    };

private:
    WebDOMEventListener* m_webDOMEventListener;

    // We keep a list of the wrapper for the WebKit EventListener, it is needed
    // to implement WebNode::removeEventListener().
    Vector<ListenerInfo> m_listenerWrappers;
};

} // namespace WebKit

#endif
