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

#ifndef V8AbstractEventListener_h
#define V8AbstractEventListener_h

#include "EventListener.h"
#include "ScopedPersistent.h"
#include "V8Utilities.h"
#include "WorldContextHandle.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

    class Event;

    // There are two kinds of event listeners: HTML or non-HMTL. onload,
    // onfocus, etc (attributes) are always HTML event handler type; Event
    // listeners added by Window.addEventListener or
    // EventTargetNode::addEventListener are non-HTML type.
    //
    // Why does this matter?
    // WebKit does not allow duplicated HTML event handlers of the same type,
    // but ALLOWs duplicated non-HTML event handlers.
    class V8AbstractEventListener : public EventListener {
        friend class WeakHandleListener<V8AbstractEventListener>;
    public:
        virtual ~V8AbstractEventListener();

        static const V8AbstractEventListener* cast(const EventListener* listener)
        {
            return listener->type() == JSEventListenerType
                ? static_cast<const V8AbstractEventListener*>(listener)
                : 0;
        }

        static V8AbstractEventListener* cast(EventListener* listener)
        {
            return const_cast<V8AbstractEventListener*>(cast(const_cast<const EventListener*>(listener)));
        }

        // Implementation of EventListener interface.

        virtual bool operator==(const EventListener& other) { return this == &other; }

        virtual void handleEvent(ScriptExecutionContext*, Event*);

        virtual bool isLazy() const { return false; }

        // Returns the listener object, either a function or an object.
        v8::Local<v8::Object> getListenerObject(ScriptExecutionContext* context)
        {
            // prepareListenerObject can potentially deref this event listener
            // as it may attempt to compile a function (lazy event listener), get an error
            // and invoke onerror callback which can execute arbitrary JS code.
            // Protect this event listener to keep it alive.
            RefPtr<V8AbstractEventListener> guard(this);
            prepareListenerObject(context);
            return v8::Local<v8::Object>::New(m_listener.get());
        }

        v8::Local<v8::Object> getExistingListenerObject()
        {
            return v8::Local<v8::Object>::New(m_listener.get());
        }

        // Provides access to the underlying handle for GC. Returned
        // value is a weak handle and so not guaranteed to stay alive.
        v8::Persistent<v8::Object> existingListenerObjectPersistentHandle()
        {
            return m_listener.get();
        }

        bool hasExistingListenerObject()
        {
            return !m_listener.isEmpty();
        }

        const WorldContextHandle& worldContext() const { return m_worldContext; }

    protected:
        V8AbstractEventListener(bool isAttribute, const WorldContextHandle&, v8::Isolate*);

        virtual void prepareListenerObject(ScriptExecutionContext*) { }

        void setListenerObject(v8::Handle<v8::Object> listener);

        void invokeEventHandler(ScriptExecutionContext*, Event*, v8::Handle<v8::Value> jsEvent);

        // Get the receiver object to use for event listener call.
        v8::Local<v8::Object> getReceiverObject(ScriptExecutionContext*, Event*);

    private:
        static void weakEventListenerCallback(v8::Isolate*, v8::Persistent<v8::Value>, void* parameter);

        // Implementation of EventListener function.
        virtual bool virtualisAttribute() const { return m_isAttribute; }

        virtual v8::Local<v8::Value> callListenerFunction(ScriptExecutionContext*, v8::Handle<v8::Value> jsevent, Event*) = 0;

        virtual bool shouldPreventDefault(v8::Local<v8::Value> returnValue);

        ScopedPersistent<v8::Object> m_listener;

        // Indicates if this is an HTML type listener.
        bool m_isAttribute;

        WorldContextHandle m_worldContext;
        v8::Isolate* m_isolate;
    };

} // namespace WebCore

#endif // V8AbstractEventListener_h
