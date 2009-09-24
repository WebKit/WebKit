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

#ifndef V8LazyEventListener_h
#define V8LazyEventListener_h

#include "PlatformString.h"
#include "V8AbstractEventListener.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    class Event;
    class Frame;

    // V8LazyEventListener is a wrapper for a JavaScript code string that is compiled and evaluated when an event is fired.
    // A V8LazyEventListener is always a HTML event handler.
    class V8LazyEventListener : public V8AbstractEventListener {
    public:
        static PassRefPtr<V8LazyEventListener> create(Frame* frame, const String& code, const String& functionName, bool isSVGEvent)
        {
            return adoptRef(new V8LazyEventListener(frame, code, functionName, isSVGEvent));
        }

        // For lazy event listener, the listener object is the same as its listener
        // function without additional scope chains.
        virtual v8::Local<v8::Object> getListenerObject() { return getWrappedListenerFunction(); }

    private:
        V8LazyEventListener(Frame*, const String& code, const String& functionName, bool isSVGEvent);
        virtual ~V8LazyEventListener();

        virtual bool virtualisAttribute() const { return true; }

        String m_code;
        String m_functionName;
        bool m_isSVGEvent;
        bool m_compiled;

        // If the event listener is on a non-document dom node, we compile the function with some implicit scope chains before it.
        bool m_wrappedFunctionCompiled;
        v8::Persistent<v8::Function> m_wrappedFunction;

        v8::Local<v8::Function> getWrappedListenerFunction();

        virtual v8::Local<v8::Value> callListenerFunction(v8::Handle<v8::Value> jsEvent, Event*);

        v8::Local<v8::Function> getListenerFunction();
    };

} // namespace WebCore

#endif // V8LazyEventListener_h
