/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(STREAMS_API)

#include "JSDOMConvertBufferSource.h"
#include "JSReadableStreamDefaultController.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/TypedArrays.h>

namespace WebCore {

class ReadableStreamSource;

class ReadableStreamDefaultController {
public:
    explicit ReadableStreamDefaultController(JSReadableStreamDefaultController* controller) : m_jsController(controller) { }

    static JSC::JSValue invoke(JSC::ExecState&, JSC::JSObject&, const char*, JSC::JSValue);

    bool enqueue(RefPtr<JSC::ArrayBuffer>&&);

    template<class ResolveResultType>
    void error(const ResolveResultType&);

    void close() { invoke(*globalObject().globalExec(), jsController(), "close", JSC::jsUndefined()); }

private:
    void error(JSC::ExecState& state, JSC::JSValue value) { invoke(state, jsController(), "error", value); }
    void enqueue(JSC::ExecState& state, JSC::JSValue value) { invoke(state, jsController(), "enqueue", value); }
    JSReadableStreamDefaultController& jsController() const;

    JSDOMGlobalObject& globalObject() const;
    JSC::ExecState& globalExec() const;

    // The owner of ReadableStreamDefaultController is responsible to keep uncollected the JSReadableStreamDefaultController.
    JSReadableStreamDefaultController* m_jsController { nullptr };
};

inline JSReadableStreamDefaultController& ReadableStreamDefaultController::jsController() const
{
    ASSERT(m_jsController);
    return *m_jsController;
}

inline JSDOMGlobalObject& ReadableStreamDefaultController::globalObject() const
{
    ASSERT(m_jsController);
    ASSERT(m_jsController->globalObject());
    return *static_cast<JSDOMGlobalObject*>(m_jsController->globalObject());
}

inline JSC::ExecState& ReadableStreamDefaultController::globalExec() const
{
    ASSERT(globalObject().globalExec());
    return *globalObject().globalExec();
}

inline bool ReadableStreamDefaultController::enqueue(RefPtr<JSC::ArrayBuffer>&& buffer)
{
    auto& globalObject = this->globalObject();
    JSC::VM& vm = globalObject.vm();
    JSC::JSLockHolder locker(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSC::ExecState& state = globalExec();

    if (!buffer) {
        error(state, createOutOfMemoryError(&state));
        return false;
    }
    auto length = buffer->byteLength();
    auto chunk = JSC::Uint8Array::create(WTFMove(buffer), 0, length);
    enqueue(state, toJS(&state, &globalObject, chunk.ptr()));
    scope.assertNoException();
    return true;
}

template<>
inline void ReadableStreamDefaultController::error<String>(const String& errorMessage)
{
    JSC::ExecState& state = globalExec();
    JSC::JSLockHolder locker(&state);
    error(state, JSC::createTypeError(&state, errorMessage));
}

} // namespace WebCore

#endif // ENABLE(STREAMS_API)
