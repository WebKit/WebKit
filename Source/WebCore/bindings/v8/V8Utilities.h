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

#ifndef V8Utilities_h
#define V8Utilities_h

#include <wtf/Forward.h>
#include <v8.h>

namespace WTF {
class ArrayBuffer;
}

namespace WebCore {

    class EventListener;
    class MessagePort;
    class ScriptExecutionContext;

    // Use an array to hold dependents. It works like a ref-counted scheme. A value can be added more than once to the DOM object.
    void createHiddenDependency(v8::Handle<v8::Object>, v8::Local<v8::Value>, int cacheIndex, v8::Isolate*);
    void removeHiddenDependency(v8::Handle<v8::Object>, v8::Local<v8::Value>, int cacheIndex, v8::Isolate*);

    // Combo create/remove, for generated event-handler-setter bindings:
    void transferHiddenDependency(v8::Handle<v8::Object>, EventListener* oldValue, v8::Local<v8::Value> newValue, int cacheIndex, v8::Isolate*);

    ScriptExecutionContext* getScriptExecutionContext();

    typedef WTF::Vector<RefPtr<MessagePort>, 1> MessagePortArray;
    typedef WTF::Vector<RefPtr<ArrayBuffer>, 1> ArrayBufferArray;

    // Helper function which pulls the values out of a JS sequence and into a MessagePortArray.
    // Also validates the elements per sections 4.1.13 and 4.1.15 of the WebIDL spec and section 8.3.3 
    // of the HTML5 spec and generates exceptions as appropriate.
    // Returns true if the array was filled, or false if the passed value was not of an appropriate type.
    bool extractTransferables(v8::Local<v8::Value>, MessagePortArray&, ArrayBufferArray&, v8::Isolate*); 
    bool getMessagePortArray(v8::Local<v8::Value>, MessagePortArray&, v8::Isolate*);

    // FIXME: We might move this to its own file.
    template<class OwnerType, class ArgumentType = OwnerType>
    class WeakHandleListener {
    public:
        template <class HandleType>
        static void makeWeak(v8::Isolate* isolate, HandleType handle, ArgumentType* parameter)
        {
            handle.MakeWeak(isolate, parameter, &invokeWeakCallback);
        }

        static void callback(v8::Isolate*, v8::Persistent<v8::Value>, ArgumentType*);

    private:
        static void invokeWeakCallback(v8::Isolate* isolate, v8::Persistent<v8::Value> handle, void* parameter)
        {
            WeakHandleListener<OwnerType, ArgumentType>::callback(isolate, handle, static_cast<ArgumentType*>(parameter));
        }
    };

} // namespace WebCore

#endif // V8Utilities_h
