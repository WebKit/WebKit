/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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

#include "JSWebSocketConstructor.h"

#include "JSWebSocket.h"
#include "ScriptExecutionContext.h"
#include "WebSocket.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSWebSocketConstructor);

const ClassInfo JSWebSocketConstructor::s_info = { "WebSocketConstructor", 0, 0, 0 };

JSWebSocketConstructor::JSWebSocketConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMConstructorObject(JSWebSocketConstructor::createStructure(globalObject->objectPrototype()), globalObject)
{
    putDirect(exec->propertyNames().prototype, JSWebSocketPrototype::self(exec, globalObject), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontDelete | DontEnum);
}

static JSObject* constructWebSocket(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    JSWebSocketConstructor* jsConstructor = static_cast<JSWebSocketConstructor*>(constructor);
    ScriptExecutionContext* context = jsConstructor->scriptExecutionContext();
    if (!context)
        return throwError(exec, ReferenceError, "WebSocket constructor associated document is unavailable");

    if (args.size() == 0)
        return throwError(exec, SyntaxError, "Not enough arguments");

    const String& urlString = ustringToString(args.at(0).toString(exec));
    if (exec->hadException())
        return throwError(exec, SyntaxError, "wrong URL");
    const KURL& url = context->completeURL(urlString);
    RefPtr<WebSocket> webSocket = WebSocket::create(context);
    ExceptionCode ec = 0;
    if (args.size() < 2)
        webSocket->connect(url, ec);
    else {
        const String& protocol = ustringToString(args.at(1).toString(exec));
        if (exec->hadException())
            return 0;
        webSocket->connect(url, protocol, ec);
    }
    setDOMException(exec, ec);
    return CREATE_DOM_OBJECT_WRAPPER(exec, jsConstructor->globalObject(), WebSocket, webSocket.get());
}

ConstructType JSWebSocketConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWebSocket;
    return ConstructTypeHost;
}

}  // namespace WebCore

#endif
