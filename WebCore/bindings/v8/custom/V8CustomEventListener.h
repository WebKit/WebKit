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

#ifndef V8CustomEventListener_h
#define V8CustomEventListener_h

#include "V8AbstractEventListener.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    class Event;
    class Frame;

    // V8EventListener is a wrapper of a JS object implements EventListener interface (has handleEvent(event) method), or a JS function
    // that can handle the event.
    class V8EventListener : public V8AbstractEventListener {
    public:
        static PassRefPtr<V8EventListener> create(Frame* frame, v8::Local<v8::Object> listener, bool isAttribute)
        {
            return adoptRef(new V8EventListener(frame, listener, isAttribute));
        }

        // Detach the listener from its owner frame.
        void disconnectFrame() { m_frame = 0; }

    protected:
        V8EventListener(Frame*, v8::Local<v8::Object> listener, bool isAttribute);
        virtual ~V8EventListener();
        v8::Local<v8::Function> getListenerFunction();

    private:
        virtual v8::Local<v8::Value> callListenerFunction(v8::Handle<v8::Value> jsEvent, Event*);
        virtual bool virtualisAttribute() const { return m_isAttribute; }
    };

} // namespace WebCore

#endif // V8CustomEventListener_h
