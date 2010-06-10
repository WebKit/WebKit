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

#include "JSUint16ArrayConstructor.h"

#include "Document.h"
#include "Uint16Array.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferConstructor.h"
#include "JSUint16Array.h"
#include <runtime/Error.h>

namespace WebCore {

using namespace JSC;

const ClassInfo JSUint16ArrayConstructor::s_info = { "Uint16ArrayConstructor", &JSArrayBufferView::s_info, 0, 0 };

JSUint16ArrayConstructor::JSUint16ArrayConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMConstructorObject(JSUint16ArrayConstructor::createStructure(globalObject->objectPrototype()), globalObject)
{
    putDirect(exec->propertyNames().prototype, JSUint16ArrayPrototype::self(exec, globalObject), DontDelete | ReadOnly);
}

JSObject* JSUint16ArrayConstructor::createPrototype(ExecState* exec, JSGlobalObject* globalObject)
{
    return new (exec) JSUint16ArrayPrototype(globalObject, JSUint16ArrayPrototype::createStructure(globalObject->objectPrototype()));
}

bool JSUint16ArrayConstructor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSUint16ArrayConstructor, DOMObject>(exec, JSUint16ArrayPrototype::s_info.staticPropHashTable, this, propertyName, slot);
}

bool JSUint16ArrayConstructor::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticValueDescriptor<JSUint16ArrayConstructor, DOMObject>(exec, JSUint16ArrayPrototype::s_info.staticPropHashTable, this, propertyName, descriptor);
}

static EncodedJSValue JSC_HOST_CALL constructCanvasUnsignedShortArray(ExecState* exec)
{
    ArgList args(exec);
    JSUint16ArrayConstructor* jsConstructor = static_cast<JSUint16ArrayConstructor*>(exec->callee());
    RefPtr<Uint16Array> array = static_cast<Uint16Array*>(construct<Uint16Array, unsigned short>(exec, args).get());
    if (!array.get()) {
        setDOMException(exec, INDEX_SIZE_ERR);
        return JSValue::encode(JSValue());
    }
    return JSValue::encode(asObject(toJS(exec, jsConstructor->globalObject(), array.get())));
}

JSC::ConstructType JSUint16ArrayConstructor::getConstructData(JSC::ConstructData& constructData)
{
    constructData.native.function = constructCanvasUnsignedShortArray;
    return ConstructTypeHost;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
