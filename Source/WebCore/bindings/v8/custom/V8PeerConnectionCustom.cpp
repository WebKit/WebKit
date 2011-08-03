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

#include "V8PeerConnection.h"

#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "MediaStreamFrameController.h"
#include "PeerConnection.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include "V8SignalingCallback.h"

namespace WebCore {

v8::Handle<v8::Value> V8PeerConnection::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.PeerConnection.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::TypeError);

    int argLength = args.Length();
    if (argLength != 2)
        return throwError("The PeerConnection constructor takes two arguments.", V8Proxy::TypeError);

    v8::TryCatch exceptionCatcher;
    String configuration = toWebCoreString(args[0]->ToString());
    if (exceptionCatcher.HasCaught())
        return throwError(exceptionCatcher.Exception());

    bool succeeded = false;
    RefPtr<SignalingCallback> signalingCallback = createFunctionOnlyCallback<V8SignalingCallback>(args[1], succeeded);
    if (!succeeded || !signalingCallback)
        return throwError("The PeerConnection constructor must be given a SignalingCallback callback.", V8Proxy::TypeError);

    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    MediaStreamFrameController* frameController = frame ? frame->mediaStreamFrameController() : 0;

    RefPtr<PeerConnection> peerConnection;
    if (frameController)
        peerConnection = frameController->createPeerConnection(configuration, signalingCallback);

    if (!peerConnection)
        return v8::Undefined();

    V8DOMWrapper::setDOMWrapper(args.Holder(), &info, peerConnection.get());
    return toV8(peerConnection.release(), args.Holder());
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
