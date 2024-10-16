/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY CANON INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CANON INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "JSDOMGuardedObject.h"
#include <JavaScriptCore/JSObject.h>

namespace WebCore {
class ReadableStreamSink;

class InternalReadableStream final : public DOMGuarded<JSC::JSObject> {
public:
    static ExceptionOr<Ref<InternalReadableStream>> createFromUnderlyingSource(JSDOMGlobalObject&, JSC::JSValue underlyingSink, JSC::JSValue strategy);
    static Ref<InternalReadableStream> fromObject(JSDOMGlobalObject&, JSC::JSObject&);

    operator JSC::JSValue() const { return guarded(); }

    void lock();
    bool isLocked() const;
    WEBCORE_EXPORT bool isDisturbed() const;
    void cancel(Exception&&);
    void pipeTo(ReadableStreamSink&);
    ExceptionOr<std::pair<Ref<InternalReadableStream>, Ref<InternalReadableStream>>> tee(bool shouldClone);

    ExceptionOr<JSC::Strong<JSC::JSObject>> getByobReader();

    JSC::JSValue cancelForBindings(JSC::JSGlobalObject& globalObject, JSC::JSValue value) { return cancel(globalObject, value, Use::Bindings); }
    JSC::JSValue pipeTo(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue);
    JSC::JSValue pipeThrough(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue);

private:
    InternalReadableStream(JSDOMGlobalObject& globalObject, JSC::JSObject& jsObject)
        : DOMGuarded<JSC::JSObject>(globalObject, jsObject)
    {
    }

    enum class Use { Bindings, Private };
    JSC::JSValue cancel(JSC::JSGlobalObject&, JSC::JSValue, Use);
    JSC::JSValue tee(JSC::JSGlobalObject&, bool shouldClone);
};

}
