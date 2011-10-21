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

#include "config.h"
#include "V8Event.h"

#include "Clipboard.h"
#include "ClipboardEvent.h"
#include "Event.h"
#include "EventHeaders.h"
#include "EventInterfaces.h"
#include "EventNames.h"
#include "V8Binding.h"
#include "V8Clipboard.h"
#include "V8Proxy.h"

namespace WebCore {

void V8Event::valueAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    Event* event = V8Event::toNative(info.Holder());
    event->setDefaultPrevented(!value->BooleanValue());
}

v8::Handle<v8::Value> V8Event::dataTransferAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    Event* event = V8Event::toNative(info.Holder());

    if (event->isDragEvent())
        return toV8(static_cast<MouseEvent*>(event)->clipboard());

    return v8::Undefined();
}

v8::Handle<v8::Value> V8Event::clipboardDataAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    Event* event = V8Event::toNative(info.Holder());

    if (event->isClipboardEvent())
        return toV8(static_cast<ClipboardEvent*>(event)->clipboard());

    return v8::Undefined();
}

#define DECLARE_EVENT_WRAPPER(interfaceName) \
    static v8::Handle<v8::Value> toV8##interfaceName(Event* event) \
    { \
        return toV8(static_cast<interfaceName*>(event)); \
    } \

DOM_EVENT_INTERFACES_FOR_EACH(DECLARE_EVENT_WRAPPER)

#define ADD_WRAPPER_TO_MAP(interfaceName) \
    map.add(eventNames().interfaceFor##interfaceName.impl(), toV8##interfaceName);

v8::Handle<v8::Value> toV8(Event* event)
{
    if (!event)
        return v8::Null();

    typedef v8::Handle<v8::Value> (*ToV8Function)(Event*);
    typedef HashMap<WTF::AtomicStringImpl*, ToV8Function> FunctionMap;

    DEFINE_STATIC_LOCAL(FunctionMap, map, ());
    if (map.isEmpty()) {
        DOM_EVENT_INTERFACES_FOR_EACH(ADD_WRAPPER_TO_MAP)
    }

    ToV8Function specializedToV8 = map.get(event->interfaceName().impl());
    if (specializedToV8)
        return specializedToV8(event);

    return V8Event::wrap(event);
}

} // namespace WebCore
