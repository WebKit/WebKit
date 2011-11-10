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
#include "EventConstructors.h"

#include "BeforeLoadEvent.h"
#include "CloseEvent.h"
#include "CustomEvent.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "ErrorEvent.h"
#include "HashChangeEvent.h"
#include "MessageEvent.h"
#include "Node.h"
#include "OverflowEvent.h"
#include "PageTransitionEvent.h"
#include "PopStateEvent.h"
#include "ProgressEvent.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"

#include "OptionsObject.h"
#include "V8BeforeLoadEvent.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8CloseEvent.h"
#include "V8CustomEvent.h"
#include "V8Document.h"
#include "V8ErrorEvent.h"
#include "V8Event.h"
#include "V8HashChangeEvent.h"
#include "V8MessageEvent.h"
#include "V8Node.h"
#include "V8OverflowEvent.h"
#include "V8PageTransitionEvent.h"
#include "V8PopStateEvent.h"
#include "V8ProgressEvent.h"
#include "V8Proxy.h"
#include "V8WebKitAnimationEvent.h"
#include "V8WebKitTransitionEvent.h"

#if ENABLE(VIDEO_TRACK)
#include "TrackEvent.h"
#include "V8TrackEvent.h"
#endif

#include <wtf/RefPtr.h>

namespace WebCore {

template<typename EventType, typename EventInitType>
static v8::Handle<v8::Value> constructV8Event(const v8::Arguments& args, bool (*filler)(EventInitType&, const OptionsObject&), WrapperTypeInfo* info)
{
    INC_STATS("DOM.Event.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::TypeError);

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::TypeError);

    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<>, type, args[0]);
    EventInitType eventInit;
    if (args.Length() >= 2) {
        EXCEPTION_BLOCK(OptionsObject, options, args[1]);
        if (!filler(eventInit, options))
            return v8::Undefined();
    }

    RefPtr<EventType> event = EventType::create(type, eventInit);

    V8DOMWrapper::setDOMWrapper(args.Holder(), info, event.get());
    return toV8(event.release(), args.Holder());
}

#define DICTIONARY_START(EventType) \
    static bool fill##EventType##Init(EventType##Init& eventInit, const OptionsObject& options) \
    {

#define DICTIONARY_END(EventType) \
        return true; \
    } \
    \
    v8::Handle<v8::Value> V8##EventType::constructorCallback(const v8::Arguments& args) \
    { \
        return constructV8Event<EventType, EventType##Init>(args, fill##EventType##Init, &info); \
    }

#define FILL_PARENT_PROPERTIES(parentEventType) \
    if (!fill##parentEventType##Init(eventInit, options)) \
        return false;

#define FILL_PROPERTY(propertyName) \
    options.get(#propertyName, eventInit.propertyName); // This can fail but it is OK.

INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_CUSTOM_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_PROGRESS_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_WEBKIT_ANIMATION_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_HASH_CHANGE_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_CLOSE_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_PAGE_TRANSITION_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_ERROR_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_POP_STATE_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_WEBKIT_TRANSITION_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_BEFORE_LOAD_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_OVERFLOW_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_MESSAGE_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)
INSTANTIATE_INITIALIZING_CONSTRUCTOR_FOR_TRACK_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)

} // namespace WebCore
