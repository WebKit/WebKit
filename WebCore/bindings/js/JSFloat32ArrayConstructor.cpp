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

#include "JSFloat32ArrayConstructor.h"

#include "Document.h"
#include "Float32Array.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferConstructor.h"
#include "JSFloat32Array.h"
#include <runtime/Error.h>

namespace WebCore {

using namespace JSC;

const ClassInfo JSFloat32ArrayConstructor::s_info = { "Float32ArrayConstructor", &JSArrayBufferView::s_info, 0, 0 };

JSFloat32ArrayConstructor::JSFloat32ArrayConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMConstructorObject(JSFloat32ArrayConstructor::createStructure(globalObject->objectPrototype()), globalObject)
{
    putDirect(exec->propertyNames().prototype, JSFloat32ArrayPrototype::self(exec, globalObject), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 2), ReadOnly|DontDelete|DontEnum);
}

static EncodedJSValue JSC_HOST_CALL constructCanvasFloatArray(ExecState* exec)
{
    ArgList args(exec);
    JSFloat32ArrayConstructor* jsConstructor = static_cast<JSFloat32ArrayConstructor*>(exec->callee());
    RefPtr<Float32Array> array = static_cast<Float32Array*>(construct<Float32Array, float>(exec, args).get());
    if (!array.get()) {
        setDOMException(exec, INDEX_SIZE_ERR);
        return JSValue::encode(JSValue());
    }
    return JSValue::encode(asObject(toJS(exec, jsConstructor->globalObject(), array.get())));
}

JSC::ConstructType JSFloat32ArrayConstructor::getConstructData(JSC::ConstructData& constructData)
{
    constructData.native.function = constructCanvasFloatArray;
    return ConstructTypeHost;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
