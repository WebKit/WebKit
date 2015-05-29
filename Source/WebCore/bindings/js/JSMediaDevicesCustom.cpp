/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "JSMediaDevices.h"

#include "Dictionary.h"
#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include "JSDOMPromise.h"
#include "JSMediaStream.h"
#include "JSNavigatorUserMediaError.h"

using namespace JSC;

namespace WebCore {

JSValue JSMediaDevices::getUserMedia(ExecState* exec)
{
    DeferredWrapper wrapper(exec, globalObject());

    Dictionary options(exec, exec->argument(0));
    if (exec->hadException()) {
        wrapper.reject(exec->exception());
        return wrapper.promise();
    }

    if (!options.isObject()) {
        JSValue error = createTypeError(exec, "First argument of getUserMedia must be a valid Dictionary");
        wrapper.reject(error);
        return wrapper.promise();
    }

    auto resolveCallback = [wrapper](MediaStream& stream) mutable {
        wrapper.resolve(&stream);
    };
    auto rejectCallback = [wrapper](NavigatorUserMediaError& error) mutable {
        wrapper.reject(&error);
    };

    ExceptionCode ec = 0;
    impl().getUserMedia(options, WTF::move(resolveCallback), WTF::move(rejectCallback), ec);
    if (ec)
        wrapper.reject(ec);

    return wrapper.promise();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
