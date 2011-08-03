/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "JSPeerConnection.h"

#include "CallbackFunction.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "JSDOMWindowCustom.h"
#include "JSSignalingCallback.h"
#include "MediaStreamFrameController.h"
#include "PeerConnection.h"

namespace WebCore {

using namespace JSC;

EncodedJSValue JSC_HOST_CALL JSPeerConnectionConstructor::constructJSPeerConnection(ExecState* exec)
{
    JSPeerConnectionConstructor* jsConstructor = static_cast<JSPeerConnectionConstructor*>(exec->callee());

    String configuration = ustringToString(exec->argument(0).toString(exec));
    if (exec->hadException())
        return JSValue::encode(JSValue());

    RefPtr<SignalingCallback> signalingCallback = createFunctionOnlyCallback<JSSignalingCallback>(exec, static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject()), exec->argument(1));
    if (exec->hadException())
        return JSValue::encode(JSValue());

    Frame* frame = asJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame();
    MediaStreamFrameController* frameController = frame ? frame->mediaStreamFrameController() : 0;

    RefPtr<PeerConnection> peerConnection;
    if (frameController)
        peerConnection = frameController->createPeerConnection(configuration, signalingCallback);

    if (!peerConnection)
        return JSValue::encode(JSValue());

    return JSValue::encode(asObject(toJS(exec, jsConstructor->globalObject(), peerConnection.get())));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
