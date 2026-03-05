/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "JSDOMGuardedObject.h"
#include "JSDOMPromise.h"
#include <JavaScriptCore/JSObject.h>

namespace WebCore {

class WritableStream;

class InternalWritableStreamWriter final : public DOMGuarded<JSC::JSObject> {
public:
    static Ref<InternalWritableStreamWriter> create(JSDOMGlobalObject& globalObject, JSC::JSObject& writer)
    {
        return adoptRef(*new InternalWritableStreamWriter(globalObject, writer));
    }

    void whenReady(Function<void(bool)>&&);
    void onClosedPromiseRejection(Function<void(JSDOMGlobalObject&, JSC::JSValue)>&&);
    void onClosedPromiseResolution(Function<void()>&&);

private:
    InternalWritableStreamWriter(JSDOMGlobalObject& globalObject, JSC::JSObject& writer)
        : DOMGuarded<JSC::JSObject>(globalObject, writer)
    {
    }
};

ExceptionOr<Ref<InternalWritableStreamWriter>> acquireWritableStreamDefaultWriter(JSDOMGlobalObject&, WritableStream&);
RefPtr<DOMPromise> writableStreamDefaultWriterCloseWithErrorPropagation(InternalWritableStreamWriter&);
void writableStreamDefaultWriterRelease(InternalWritableStreamWriter&);
RefPtr<DOMPromise> writableStreamDefaultWriterWrite(InternalWritableStreamWriter&, JSC::JSValue);
}
