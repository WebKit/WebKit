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

namespace WebCore {

v8::Handle<v8::Value> V8Event::clipboardDataAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    Event* event = V8Event::toNative(info.Holder());

    if (event->isClipboardEvent())
        return toV8Fast(static_cast<ClipboardEvent*>(event)->clipboard(), info, event);

    return v8::Undefined();
}

#define TRY_TO_WRAP_WITH_INTERFACE(interfaceName) \
    if (eventNames().interfaceFor##interfaceName == desiredInterface) \
        return wrap(static_cast<interfaceName*>(event), creationContext, isolate);

v8::Handle<v8::Object> wrap(Event* event, v8::Handle<v8::Object> creationContext, v8::Isolate *isolate)
{
    ASSERT(event);

    String desiredInterface = event->interfaceName();

    // We need to check Event first to avoid infinite recursion.
    if (eventNames().interfaceForEvent == desiredInterface)
        return V8Event::createWrapper(event, creationContext, isolate);

    DOM_EVENT_INTERFACES_FOR_EACH(TRY_TO_WRAP_WITH_INTERFACE)

    return V8Event::createWrapper(event, creationContext, isolate);
}

} // namespace WebCore
