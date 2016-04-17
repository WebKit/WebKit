/*
 * Copyright (C) 2016 Canon Inc.
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

#ifndef ReadableStreamController_h
#define ReadableStreamController_h

#if ENABLE(STREAMS_API)

#include "JSDOMBinding.h"
#include "JSReadableStreamController.h"
#include <runtime/JSCJSValue.h>
#include <runtime/JSCJSValueInlines.h>

namespace WebCore {

class ReadableStreamSource;

class ReadableStreamController {
public:
    explicit ReadableStreamController(JSReadableStreamController* controller) : m_jsController(controller) { }

    static JSC::JSValue invoke(JSC::ExecState&, JSC::JSObject&, const char*, JSC::JSValue);

    template<class ResolveResultType>
    void enqueue(const ResolveResultType&);

    template<class ResolveResultType>
    void error(const ResolveResultType&);

    void close() { invoke(*globalObject()->globalExec(), *m_jsController, "close", JSC::jsUndefined()); }

    bool isControlledReadableStreamLocked() const;

private:
    void error(JSC::ExecState& state, JSC::JSValue value) { invoke(state, *m_jsController, "error", value); }
    void enqueue(JSC::ExecState& state, JSC::JSValue value) { invoke(state, *m_jsController, "enqueue", value); }

    JSDOMGlobalObject* globalObject() const;

    // The owner of ReadableStreamController is responsible to keep uncollected the JSReadableStreamController.
    JSReadableStreamController* m_jsController { nullptr };
};

JSC::JSValue createReadableStream(JSC::ExecState&, JSDOMGlobalObject*, ReadableStreamSource*);

inline JSDOMGlobalObject* ReadableStreamController::globalObject() const
{
    ASSERT(m_jsController);
    return static_cast<JSDOMGlobalObject*>(m_jsController->globalObject());
}

template<>
inline void ReadableStreamController::enqueue<RefPtr<JSC::ArrayBuffer>>(const RefPtr<JSC::ArrayBuffer>& result)
{
    JSC::ExecState& state = *globalObject()->globalExec();
    JSC::JSLockHolder locker(&state);

    if (result)
        enqueue(state, toJS(&state, globalObject(), result.get()));
    else
        error(state, createOutOfMemoryError(&state));
}

template<>
inline void ReadableStreamController::error<String>(const String& result)
{
    JSC::ExecState* state = globalObject()->globalExec();
    JSC::JSLockHolder locker(state);
    error(*state, jsString(state, result));
}

} // namespace WebCore

#endif // ENABLE(STREAMS_API)

#endif // ReadableStreamController_h
