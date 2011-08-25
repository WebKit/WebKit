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

#include "Document.h"
#include "DocumentFragment.h"
#include "Node.h"

#include "OptionsObject.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8Document.h"
#include "V8Event.h"
#include "V8Node.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {


template<typename EventType, typename EventConfigurationType>
static v8::Handle<v8::Value> constructV8Event(const v8::Arguments& args, bool (*filler)(EventConfigurationType&, const OptionsObject&), WrapperTypeInfo* info)
{
    INC_STATS("DOM.Event.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::TypeError);

    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::TypeError);

    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<>, type, args[0]);
    EventConfigurationType eventConfiguration;
    if (args.Length() >= 2) {
        EXCEPTION_BLOCK(OptionsObject, options, args[1]);
        if (!filler(eventConfiguration, options))
            return v8::Undefined();
    }

    RefPtr<EventType> event = EventType::create(type, eventConfiguration);

    V8DOMWrapper::setDOMWrapper(args.Holder(), info, event.get());
    return toV8(event.release(), args.Holder());
}

#define DICTIONARY_START(EventType) \
    static bool fill##EventType##Configuration(Event##Configuration& eventConfiguration, const OptionsObject& options) \
    {

#define DICTIONARY_END(EventType) \
        return true; \
    } \
     \
    v8::Handle<v8::Value> V8##EventType::constructorCallback(const v8::Arguments& args) \
    { \
      return constructV8Event<EventType, EventType##Configuration>(args, fill##EventType##Configuration, &info); \
    }

#define FILL_PARENT_PROPERTIES(parentEventType) \
    if (!fill##parentEventType##Configuration(eventConfiguration)) \
        return false;

#define FILL_PROPERTY(propertyName) \
    options.getKeyValue(#propertyName, eventConfiguration.propertyName); // This can fail but it is OK.

SETUP_EVENT_CONFIGURATION_FOR_ALL_EVENTS(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)

} // namespace WebCore
