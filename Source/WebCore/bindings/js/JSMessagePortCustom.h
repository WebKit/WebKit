/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef JSMessagePortCustom_h
#define JSMessagePortCustom_h

#include "MessagePort.h"
#include <runtime/Error.h>
#include <runtime/JSCInlines.h>
#include <runtime/JSCJSValue.h>
#include <wtf/Forward.h>

namespace WebCore {

    typedef int ExceptionCode;

    // Helper function which pulls the values out of a JS sequence and into a MessagePortArray.
    // Also validates the elements per sections 4.1.13 and 4.1.15 of the WebIDL spec and section 8.3.3 of the HTML5 spec.
    // May generate an exception via the passed ExecState.
    void fillMessagePortArray(JSC::ExecState&, JSC::JSValue, MessagePortArray&, ArrayBufferArray&);

    // Helper function to convert from JS postMessage arguments to WebCore postMessage arguments.
    template <typename T>
    inline JSC::JSValue handlePostMessage(JSC::ExecState& state, T* impl)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (UNLIKELY(state.argumentCount() < 1))
            return throwException(&state, scope, createNotEnoughArgumentsError(&state));

        MessagePortArray portArray;
        ArrayBufferArray arrayBufferArray;
        fillMessagePortArray(state, state.argument(1), portArray, arrayBufferArray);
        auto message = SerializedScriptValue::create(&state, state.uncheckedArgument(0), &portArray, &arrayBufferArray);
        if (state.hadException())
            return JSC::jsUndefined();

        ExceptionCode ec = 0;
        impl->postMessage(WTFMove(message), &portArray, ec);
        setDOMException(&state, ec);
        return JSC::jsUndefined();
    }

}
#endif // JSMessagePortCustom_h
