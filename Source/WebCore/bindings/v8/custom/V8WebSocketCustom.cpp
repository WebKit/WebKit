/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#if ENABLE(WEB_SOCKETS)

#include "V8WebSocket.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "Settings.h"
#include "V8ArrayBuffer.h"
#include "V8ArrayBufferView.h"
#include "V8Binding.h"
#include "V8Blob.h"
#include "V8Utilities.h"
#include "WebSocket.h"
#include "WebSocketChannel.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

v8::Handle<v8::Value> V8WebSocket::sendCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebSocket.send()");

    if (!args.Length())
        return throwNotEnoughArgumentsError(args.GetIsolate());

    WebSocket* webSocket = V8WebSocket::toNative(args.Holder());
    v8::Handle<v8::Value> message = args[0];
    ExceptionCode ec = 0;
    bool result;
    if (V8ArrayBuffer::HasInstance(message)) {
        ArrayBuffer* arrayBuffer = V8ArrayBuffer::toNative(v8::Handle<v8::Object>::Cast(message));
        ASSERT(arrayBuffer);
        result = webSocket->send(arrayBuffer, ec);
    } else if (V8ArrayBufferView::HasInstance(message)) {
        ArrayBufferView* arrayBufferView = V8ArrayBufferView::toNative(v8::Handle<v8::Object>::Cast(message));
        ASSERT(arrayBufferView);
        result = webSocket->send(arrayBufferView, ec);
    } else if (V8Blob::HasInstance(message)) {
        Blob* blob = V8Blob::toNative(v8::Handle<v8::Object>::Cast(message));
        ASSERT(blob);
        result = webSocket->send(blob, ec);
    } else {
        v8::TryCatch tryCatch;
        v8::Handle<v8::String> stringMessage = message->ToString();
        if (tryCatch.HasCaught())
            return throwError(tryCatch.Exception(), args.GetIsolate());
        result = webSocket->send(toWebCoreString(stringMessage), ec);
    }
    if (ec)
        return setDOMException(ec, args.GetIsolate());

    return v8Boolean(result, args.GetIsolate());
}

}  // namespace WebCore

#endif  // ENABLE(WEB_SOCKETS)
