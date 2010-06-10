/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "JSInt8ArrayConstructor.h"

#include "Document.h"
#include "Int8Array.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferConstructor.h"
#include "JSInt8Array.h"
#include <runtime/Error.h>

namespace WebCore {

using namespace JSC;

const ClassInfo JSInt8ArrayConstructor::s_info = { "Int8ArrayConstructor", &JSArrayBufferView::s_info, 0, 0 };

JSInt8ArrayConstructor::JSInt8ArrayConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMConstructorObject(JSInt8ArrayConstructor::createStructure(globalObject->objectPrototype()), globalObject)
{
    putDirect(exec->propertyNames().prototype, JSInt8ArrayPrototype::self(exec, globalObject), DontDelete | ReadOnly);
}

JSObject* JSInt8ArrayConstructor::createPrototype(ExecState* exec, JSGlobalObject* globalObject)
{
    return new (exec) JSInt8ArrayPrototype(globalObject, JSInt8ArrayPrototype::createStructure(globalObject->objectPrototype()));
}

bool JSInt8ArrayConstructor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSInt8ArrayConstructor, DOMObject>(exec, JSInt8ArrayPrototype::s_info.staticPropHashTable, this, propertyName, slot);
}

bool JSInt8ArrayConstructor::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticValueDescriptor<JSInt8ArrayConstructor, DOMObject>(exec, JSInt8ArrayPrototype::s_info.staticPropHashTable, this, propertyName, descriptor);
}

static EncodedJSValue JSC_HOST_CALL constructCanvasByteArray(ExecState* exec)
{
    ArgList args(exec);
    JSInt8ArrayConstructor* jsConstructor = static_cast<JSInt8ArrayConstructor*>(exec->callee());
    RefPtr<Int8Array> array = static_cast<Int8Array*>(construct<Int8Array, signed char>(exec, args).get());
    if (!array.get()) {
        setDOMException(exec, INDEX_SIZE_ERR);
        return JSValue::encode(JSValue());
    }
    return JSValue::encode(asObject(toJS(exec, jsConstructor->globalObject(), array.get())));
}

JSC::ConstructType JSInt8ArrayConstructor::getConstructData(JSC::ConstructData& constructData)
{
    constructData.native.function = constructCanvasByteArray;
    return ConstructTypeHost;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
