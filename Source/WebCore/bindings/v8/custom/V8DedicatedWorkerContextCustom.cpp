/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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

#if ENABLE(WORKERS)
#include "V8DedicatedWorkerContext.h"

#include <wtf/ArrayBuffer.h>
#include "DedicatedWorkerContext.h"
#include "V8Binding.h"
#include "V8Utilities.h"
#include "V8WorkerContextEventListener.h"

namespace WebCore {

v8::Handle<v8::Value> V8DedicatedWorkerContext::postMessageCallback(const v8::Arguments& args)
{

    DedicatedWorkerContext* workerContext = V8DedicatedWorkerContext::toNative(args.Holder());
    MessagePortArray ports;
    ArrayBufferArray arrayBuffers;
    if (args.Length() > 1) {
        if (!extractTransferables(args[1], ports, arrayBuffers, args.GetIsolate()))
            return v8::Undefined();
    }
    bool didThrow = false;
    RefPtr<SerializedScriptValue> message =
        SerializedScriptValue::create(args[0],
                                      &ports,
                                      &arrayBuffers,
                                      didThrow,
                                      args.GetIsolate());
    if (didThrow)
        return v8::Undefined();
    ExceptionCode ec = 0;
    workerContext->postMessage(message.release(), &ports, ec);
    return setDOMException(ec, args.GetIsolate());
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
