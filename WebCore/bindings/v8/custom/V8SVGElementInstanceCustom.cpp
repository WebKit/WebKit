/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include <config.h>

#if ENABLE(SVG)
#include "V8SVGElementInstance.h"

#include "EventListener.h"
#include "SVGElementInstance.h"

#include "V8Binding.h"
#include "V8CustomEventListener.h"
#include "V8SVGPODTypeWrapper.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8SVGElementInstance::addEventListenerCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.SVGElementInstance.AddEventListener()");
    SVGElementInstance* instance = V8SVGElementInstance::toNative(args.Holder());

    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(instance, args[1], false, ListenerFindOrCreate);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        instance->addEventListener(type, listener, useCapture);
        createHiddenDependency(args.Holder(), args[1], cacheIndex);
    }

    return v8::Undefined();
}

v8::Handle<v8::Value> V8SVGElementInstance::removeEventListenerCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.SVGElementInstance.RemoveEventListener()");
    SVGElementInstance* instance = V8SVGElementInstance::toNative(args.Holder());

    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(instance, args[1], false, ListenerFindOnly);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        instance->removeEventListener(type, listener.get(), useCapture);
        removeHiddenDependency(args.Holder(), args[1], cacheIndex);
    }

    return v8::Undefined();
}

} // namespace WebCore

#endif
