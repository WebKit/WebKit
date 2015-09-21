/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Canon INC.
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
#include "JSNavigator.h"

#include "Dictionary.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "JSNavigatorUserMediaErrorCallback.h"
#include "JSNavigatorUserMediaSuccessCallback.h"
#include "MediaStream.h"
#include "UserMediaRequest.h"

using namespace JSC;

namespace WebCore {

JSValue JSNavigator::webkitGetUserMedia(ExecState& state)
{
    if (state.argumentCount() < 3) {
        throwVMError(&state, createNotEnoughArgumentsError(&state));
        return jsUndefined();
    }

    Dictionary options(&state, state.argument(0));
    if (state.hadException())
        return jsUndefined();

    if (!options.isObject()) {
        throwVMError(&state, createTypeError(&state, "First argument of webkitGetUserMedia must be a valid Dictionary"));
        return jsUndefined();
    }

    if (!state.argument(1).isFunction()) {
        throwVMTypeError(&state, "Argument 2 ('successCallback') to Navigator.webkitGetUserMedia must be a function");
        return jsUndefined();
    }

    if (!state.argument(2).isFunction()) {
        throwVMTypeError(&state, "Argument 3 ('errorCallback') to Navigator.webkitGetUserMedia must be a function");
        return jsUndefined();
    }

    if (!impl().frame()) {
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    // We do not need to protect the context (i.e. document) here as UserMediaRequest is observing context destruction and will check validity before resolving/rejecting promise.
    Document* document = impl().frame()->document();

    RefPtr<NavigatorUserMediaSuccessCallback> successCallback = JSNavigatorUserMediaSuccessCallback::create(asObject(state.uncheckedArgument(1)), globalObject());
    auto resolveCallback = [successCallback, document](const RefPtr<MediaStream>& stream) mutable {
        RefPtr<MediaStream> protectedStream = stream;
        document->postTask([successCallback, protectedStream](ScriptExecutionContext&) {
            successCallback->handleEvent(protectedStream.get());
        });
    };

    RefPtr<NavigatorUserMediaErrorCallback> errorCallback = JSNavigatorUserMediaErrorCallback::create(asObject(state.uncheckedArgument(2)), globalObject());
    auto rejectCallback = [errorCallback, document](const RefPtr<NavigatorUserMediaError>& error) mutable {
        RefPtr<NavigatorUserMediaError> protectedError = error;
        document->postTask([errorCallback, protectedError](ScriptExecutionContext&) {
            errorCallback->handleEvent(protectedError.get());
        });
    };

    ExceptionCode ec = 0;
    UserMediaRequest::start(document, options, MediaDevices::Promise(WTF::move(resolveCallback), WTF::move(rejectCallback)), ec);
    setDOMException(&state, ec);
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
