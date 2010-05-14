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

#include "config.h"
#include "JSArrayBufferView.h"
#include "JSInt8Array.h"
#include "JSUint8Array.h"
#include "JSInt16Array.h"
#include "JSUint16Array.h"
#include "JSInt32Array.h"
#include "JSUint32Array.h"
#include "JSFloatArray.h"

#include "ArrayBufferView.h"

using namespace JSC;

namespace WebCore {

JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, ArrayBufferView* object)
{
    if (!object)
        return jsUndefined();
        
    if (object) {
        if (object->isFloatArray())
            return getDOMObjectWrapper<JSFloatArray>(exec, globalObject, static_cast<FloatArray*>(object));
        if (object->isUnsignedByteArray())
            return getDOMObjectWrapper<JSUint8Array>(exec, globalObject, static_cast<Uint8Array*>(object));
        if (object->isByteArray())
            return getDOMObjectWrapper<JSInt8Array>(exec, globalObject, static_cast<Int8Array*>(object));
        if (object->isIntArray())
            return getDOMObjectWrapper<JSInt32Array>(exec, globalObject, static_cast<Int32Array*>(object));
        if (object->isUnsignedIntArray())
            return getDOMObjectWrapper<JSUint32Array>(exec, globalObject, static_cast<Uint32Array*>(object));
        if (object->isShortArray())
            return getDOMObjectWrapper<JSInt16Array>(exec, globalObject, static_cast<Int16Array*>(object));
        if (object->isUnsignedShortArray())
            return getDOMObjectWrapper<JSUint16Array>(exec, globalObject, static_cast<Uint16Array*>(object));
    }
    return jsUndefined();
}

JSValue JSArrayBufferView::slice(ExecState* exec, const ArgList& args)
{
    ArrayBufferView* array = reinterpret_cast<ArrayBufferView*>(impl());

    int start, end;
    switch (args.size()) {
    case 0:
        start = 0;
        end = array->length();
        break;
    case 1:
        start = args.at(0).toInt32(exec);
        end = array->length();
        break;
    default:
        start = args.at(0).toInt32(exec);
        end = args.at(1).toInt32(exec);
    }
    return toJS(exec, globalObject(), array->slice(start, end));
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
