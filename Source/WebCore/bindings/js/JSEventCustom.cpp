/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSEvent.h"

#include "Clipboard.h"
#include "Event.h"
#include "EventHeaders.h"
#include "EventInterfaces.h"
#include "EventNames.h"
#include "JSClipboard.h"
#include <runtime/JSLock.h>
#include <wtf/HashMap.h>
#include <wtf/text/AtomicString.h>

using namespace JSC;

namespace WebCore {

JSValue JSEvent::clipboardData(ExecState* exec) const
{
    return impl()->isClipboardEvent() ? toJS(exec, globalObject(), impl()->clipboardData()) : jsUndefined();
}

#define DECLARE_EVENT_WRAPPER(interfaceName) \
    static JSDOMWrapper* create##interfaceName##Wrapper(ExecState* exec, JSDOMGlobalObject* globalObject, Event* event) \
    { \
        return CREATE_DOM_WRAPPER(exec, globalObject, interfaceName, event); \
    } \

DOM_EVENT_INTERFACES_FOR_EACH(DECLARE_EVENT_WRAPPER)

#define ADD_WRAPPER_TO_MAP(interfaceName) \
    map.add(eventNames().interfaceFor##interfaceName.impl(), create##interfaceName##Wrapper);

JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, Event* event)
{
    JSLock lock(SilenceAssertionsOnly);

    if (!event)
        return jsNull();

    JSDOMWrapper* wrapper = getCachedWrapper(currentWorld(exec), event);
    if (wrapper)
        return wrapper;

    typedef JSDOMWrapper* (*CreateEventWrapperFunction)(ExecState*, JSDOMGlobalObject*, Event*);
    typedef HashMap<WTF::AtomicStringImpl*, CreateEventWrapperFunction> FunctionMap;

    DEFINE_STATIC_LOCAL(FunctionMap, map, ());
    if (map.isEmpty()) {
        DOM_EVENT_INTERFACES_FOR_EACH(ADD_WRAPPER_TO_MAP)
    }

    CreateEventWrapperFunction createWrapperFunction = map.get(event->interfaceName().impl());
    if (createWrapperFunction)
        return createWrapperFunction(exec, globalObject, event);

    return CREATE_DOM_WRAPPER(exec, globalObject, Event, event);
}

} // namespace WebCore
