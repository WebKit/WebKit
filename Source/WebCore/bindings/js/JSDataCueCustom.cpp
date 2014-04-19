/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

using namespace JSC;

namespace WebCore {

#if ENABLE(DATACUE_VALUE)
JSValue JSDataCue::value(ExecState* exec) const
{
    return impl().value(exec);
}

void JSDataCue::setValue(ExecState* exec, JSValue value)
{
    impl().setValue(exec, value);
}
#endif

EncodedJSValue JSC_HOST_CALL JSDataCueConstructor::constructJSDataCue(ExecState* exec)
{
    JSDataCueConstructor* castedThis = jsCast<JSDataCueConstructor*>(exec->callee());
    if (exec->argumentCount() < 3)
        return throwVMError(exec, createNotEnoughArgumentsError(exec));

    ExceptionCode ec = 0;
    double startTime(exec->argument(0).toNumber(exec));
    if (UNLIKELY(exec->hadException()))
        return JSValue::encode(jsUndefined());

    double endTime(exec->argument(1).toNumber(exec));
    if (UNLIKELY(exec->hadException()))
        return JSValue::encode(jsUndefined());

    ScriptExecutionContext* context = castedThis->scriptExecutionContext();
    if (!context)
        return throwConstructorDocumentUnavailableError(*exec, "DataCue");

    String type;
#if ENABLE(DATACUE_VALUE)
    if (exec->argumentCount() > 3) {
        if (!exec->argument(3).isString())
            return throwVMError(exec, createTypeError(exec, "Second argument of the constructor is not of type String"));
        type = exec->argument(3).getString(exec);
    }
#endif

    JSValue valueArgument = exec->argument(2);
    if (valueArgument.isUndefinedOrNull()) {
        setDOMException(exec, TypeError);
        return JSValue::encode(JSValue());
    }

    RefPtr<DataCue> object;
    if (valueArgument.isCell() && valueArgument.asCell()->inherits(std::remove_pointer<JSArrayBuffer*>::type::info())) {

        ArrayBuffer* data(toArrayBuffer(valueArgument));
        if (UNLIKELY(exec->hadException()))
            return JSValue::encode(jsUndefined());

        object = DataCue::create(*context, startTime, endTime, data, type, ec);
        if (ec) {
            setDOMException(exec, ec);
            return JSValue::encode(JSValue());
        }

        return JSValue::encode(asObject(toJS(exec, castedThis->globalObject(), object.get())));
    }

#if !ENABLE(DATACUE_VALUE)
    return JSValue::encode(jsUndefined());
#else
    object = DataCue::create(*context, startTime, endTime, valueArgument, type);
    return JSValue::encode(asObject(toJS(exec, castedThis->globalObject(), object.get())));
#endif
}

} // namespace WebCore

#endif
