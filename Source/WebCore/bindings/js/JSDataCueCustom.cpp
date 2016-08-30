/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "JSDataCue.h"

#include "JSDOMBinding.h"

using namespace JSC;

namespace WebCore {

#if ENABLE(DATACUE_VALUE)
JSValue JSDataCue::value(ExecState& state) const
{
    return wrapped().value(&state);
}

void JSDataCue::setValue(ExecState& state, JSValue value)
{
    wrapped().setValue(&state, value);
}
#endif

EncodedJSValue JSC_HOST_CALL constructJSDataCue(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    DOMConstructorObject* castedThis = jsCast<DOMConstructorObject*>(exec.callee());
    ASSERT(castedThis);
    if (exec.argumentCount() < 3)
        return throwVMError(&exec, scope, createNotEnoughArgumentsError(&exec));

    double startTime(exec.uncheckedArgument(0).toNumber(&exec));
    if (UNLIKELY(exec.hadException()))
        return JSValue::encode(jsUndefined());

    double endTime(exec.uncheckedArgument(1).toNumber(&exec));
    if (UNLIKELY(exec.hadException()))
        return JSValue::encode(jsUndefined());

    ScriptExecutionContext* context = castedThis->scriptExecutionContext();
    if (!context)
        return throwConstructorScriptExecutionContextUnavailableError(exec, scope, "DataCue");

    String type;
#if ENABLE(DATACUE_VALUE)
    if (exec.argumentCount() > 3) {
        if (!exec.uncheckedArgument(3).isString())
            return throwArgumentTypeError(exec, scope, 3, "type", "DataCue", nullptr, "DOMString");
        type = exec.uncheckedArgument(3).getString(&exec);
    }
#endif

    JSValue valueArgument = exec.uncheckedArgument(2);
    if (valueArgument.isUndefinedOrNull()) {
        setDOMException(&exec, TypeError);
        return JSValue::encode(JSValue());
    }

    if (valueArgument.isCell() && valueArgument.asCell()->inherits(std::remove_pointer<JSArrayBuffer*>::type::info())) {

        ArrayBuffer* data = toArrayBuffer(valueArgument);
        if (UNLIKELY(exec.hadException()))
            return JSValue::encode(jsUndefined());

        if (UNLIKELY(!data)) {
            setDOMException(&exec, TypeError);
            return JSValue::encode(jsUndefined());
        }
        return JSValue::encode(CREATE_DOM_WRAPPER(castedThis->globalObject(), DataCue, DataCue::create(*context, MediaTime::createWithDouble(startTime), MediaTime::createWithDouble(endTime), *data, type)));
    }

#if !ENABLE(DATACUE_VALUE)
    return JSValue::encode(jsUndefined());
#else
    return JSValue::encode(CREATE_DOM_WRAPPER(castedThis->globalObject(), DataCue,DataCue::create(*context, MediaTime::createWithDouble(startTime), MediaTime::createWithDouble(endTime), valueArgument, type)));
#endif
}

} // namespace WebCore

#endif
