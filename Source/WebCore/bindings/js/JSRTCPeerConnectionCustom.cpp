/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#if ENABLE(MEDIA_STREAM)

#include "JSRTCPeerConnection.h"

#include "ExceptionCode.h"

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL JSRTCPeerConnectionConstructor::constructJSRTCPeerConnection(ExecState* exec)
{
    // Spec says that we must have at least one arument, the RTCConfiguration.
    if (exec->argumentCount() < 1)
        return throwVMError(exec, createNotEnoughArgumentsError(exec));

    ExceptionCode ec = 0;
    Dictionary rtcConfiguration(exec, exec->argument(0));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    if (!rtcConfiguration.isObject())
        return throwVMError(exec, createTypeError(exec, "First argument of RTCPeerConnection must be a valid Dictionary"));

    Dictionary mediaConstraints;
    if (exec->argumentCount() > 1) {
        mediaConstraints = Dictionary(exec, exec->argument(1));
        if (!mediaConstraints.isObject())
            return throwVMError(exec, createTypeError(exec, "Optional constraints argument of RTCPeerConnection must be a valid Dictionary"));

        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    JSRTCPeerConnectionConstructor* jsConstructor = jsCast<JSRTCPeerConnectionConstructor*>(exec->callee());
    ScriptExecutionContext* scriptExecutionContext = jsConstructor->scriptExecutionContext();
    if (!scriptExecutionContext)
        return throwVMError(exec, createReferenceError(exec, "RTCPeerConnection constructor associated document is unavailable"));

    RefPtr<RTCPeerConnection> peerConnection = RTCPeerConnection::create(*scriptExecutionContext, rtcConfiguration, mediaConstraints, ec);
    if (ec == TYPE_MISMATCH_ERR) {
        setDOMException(exec, ec);
        return throwVMError(exec, createTypeError(exec, "Invalid RTCPeerConnection constructor arguments"));
    }

    if (ec) {
        setDOMException(exec, ec);
        return throwVMError(exec, createTypeError(exec, "Error creating RTCPeerConnection"));
    }

    return JSValue::encode(CREATE_DOM_WRAPPER(exec, jsConstructor->globalObject(), RTCPeerConnection, peerConnection.get()));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
