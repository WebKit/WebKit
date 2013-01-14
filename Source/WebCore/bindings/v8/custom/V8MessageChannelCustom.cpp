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

#include "config.h"
#include "V8MessageChannel.h"

#include "Document.h"
#include "Frame.h"
#include "MessageChannel.h"
#include "V8Binding.h"
#include "V8MessagePort.h"
#include "V8Utilities.h"
#include "WorkerContext.h"

#include <wtf/RefPtr.h>

namespace WebCore {

v8::Handle<v8::Value> V8MessageChannel::constructorCallbackCustom(const v8::Arguments& args)
{
    ScriptExecutionContext* context = getScriptExecutionContext();

    RefPtr<MessageChannel> obj = MessageChannel::create(context);

    v8::Local<v8::Object> wrapper = args.Holder();

    // Create references from the MessageChannel wrapper to the two
    // MessagePort wrappers to make sure that the MessagePort wrappers
    // stay alive as long as the MessageChannel wrapper is around.
    V8DOMWrapper::setNamedHiddenReference(wrapper, "port1", toV8(obj->port1(), args.Holder(), args.GetIsolate()));
    V8DOMWrapper::setNamedHiddenReference(wrapper, "port2", toV8(obj->port2(), args.Holder(), args.GetIsolate()));

    V8DOMWrapper::associateObjectWithWrapper(obj.release(), &info, wrapper, args.GetIsolate());
    return wrapper;
}

} // namespace WebCore
