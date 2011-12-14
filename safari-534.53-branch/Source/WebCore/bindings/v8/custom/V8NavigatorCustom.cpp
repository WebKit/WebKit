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
#include "V8Navigator.h"

#if ENABLE(MEDIA_STREAM)

#include "ExceptionCode.h"
#include "Navigator.h"
#include "V8Binding.h"
#include "V8NavigatorUserMediaErrorCallback.h"
#include "V8NavigatorUserMediaSuccessCallback.h"
#include "V8Utilities.h"

using namespace WTF;

namespace WebCore {

v8::Handle<v8::Value> V8Navigator::webkitGetUserMediaCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Navigator.webkitGetUserMedia()");

    v8::TryCatch exceptionCatcher;
    v8::Handle<v8::String> options = args[0]->ToString();
    if (exceptionCatcher.HasCaught())
        return throwError(exceptionCatcher.Exception());

    bool succeeded = false;
    RefPtr<NavigatorUserMediaSuccessCallback> successCallback = createFunctionOnlyCallback<V8NavigatorUserMediaSuccessCallback>(args[1], succeeded);
    if (!succeeded)
        return v8::Undefined();

    // Argument is optional, hence undefined is allowed.
    RefPtr<NavigatorUserMediaErrorCallback> errorCallback = createFunctionOnlyCallback<V8NavigatorUserMediaErrorCallback>(args[2], succeeded, CallbackAllowUndefined);
    if (!succeeded)
        return v8::Undefined();

    ExceptionCode ec = 0;
    Navigator* navigator = V8Navigator::toNative(args.Holder());
    navigator->webkitGetUserMedia(toWebCoreString(options), successCallback.release(), errorCallback.release(), ec);
    return throwError(ec);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
