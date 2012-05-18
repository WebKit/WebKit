/*
    Copyright (C) 2012 Intel Corporation

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "Intent.h"

#include "Dictionary.h"
#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include "JSIntent.h"
#include "JSMessagePortCustom.h"
#include "JSValue.h"
#include <runtime/Error.h>
#include <wtf/ArrayBuffer.h>

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL JSIntentConstructor::constructJSIntent(ExecState* exec)
{
    JSIntentConstructor* castedThis = jsCast<JSIntentConstructor*>(exec->callee());

    if (exec->argumentCount() < 1)
        return throwVMError(exec, createNotEnoughArgumentsError(exec));

    if (exec->argumentCount() == 1) {
        // Use the dictionary constructor. This block will return if the
        // argument isn't a valid Dictionary.
        JSValue optionsValue = exec->argument(0);
        if (!optionsValue.isObject()) {
            setDOMException(exec, SYNTAX_ERR);
            return JSValue::encode(jsUndefined());
        }
        Dictionary options(exec, optionsValue);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        ExceptionCode ec = 0;
        RefPtr<Intent> impl = Intent::create(exec, options, ec);
        if (ec) {
            setDOMException(exec, ec);
            return JSValue::encode(jsUndefined());
        }
        return JSValue::encode(asObject(toJS(exec, castedThis->globalObject(), impl.get())));
    }

    const String& action(ustringToString(MAYBE_MISSING_PARAMETER(exec, 0, DefaultIsUndefined).isEmpty() ? UString() : MAYBE_MISSING_PARAMETER(exec, 0, DefaultIsUndefined).toString(exec)->value(exec)));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    const String& type(ustringToString(MAYBE_MISSING_PARAMETER(exec, 1, DefaultIsUndefined).isEmpty() ? UString() : MAYBE_MISSING_PARAMETER(exec, 1, DefaultIsUndefined).toString(exec)->value(exec)));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    MessagePortArray messagePortArrayTransferList;
    ArrayBufferArray arrayBufferArrayTransferList;
    if (exec->argumentCount() > 3) {
        fillMessagePortArray(exec, exec->argument(3), messagePortArrayTransferList, arrayBufferArrayTransferList);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
    RefPtr<SerializedScriptValue> data = SerializedScriptValue::create(exec, exec->argument(2), &messagePortArrayTransferList, &arrayBufferArrayTransferList);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    ExceptionCode ec = 0;
    RefPtr<Intent> impl = Intent::create(action, type, data, messagePortArrayTransferList, ec);
    if (ec) {
        setDOMException(exec, ec);
        return JSValue::encode(jsUndefined());
    }
    return JSValue::encode(asObject(toJS(exec, castedThis->globalObject(), impl.get())));
}

} // namespace WebCore
