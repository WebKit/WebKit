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
#include "JSCanvasArray.h"
#include "JSCanvasByteArray.h"
#include "JSCanvasUnsignedByteArray.h"
#include "JSCanvasShortArray.h"
#include "JSCanvasUnsignedShortArray.h"
#include "JSCanvasIntArray.h"
#include "JSCanvasUnsignedIntArray.h"
#include "JSCanvasFloatArray.h"

#include "CanvasArray.h"

using namespace JSC;

namespace WebCore {

JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, CanvasArray* object)
{
    if (object->isFloatArray())
        return getDOMObjectWrapper<JSCanvasFloatArray>(exec, globalObject, static_cast<CanvasFloatArray*>(object));
    if (object->isUnsignedByteArray())
        return getDOMObjectWrapper<JSCanvasUnsignedByteArray>(exec, globalObject, static_cast<CanvasUnsignedByteArray*>(object));
    if (object->isByteArray())
        return getDOMObjectWrapper<JSCanvasByteArray>(exec, globalObject, static_cast<CanvasByteArray*>(object));
    if (object->isIntArray())
        return getDOMObjectWrapper<JSCanvasIntArray>(exec, globalObject, static_cast<CanvasIntArray*>(object));
    if (object->isUnsignedIntArray())
        return getDOMObjectWrapper<JSCanvasUnsignedIntArray>(exec, globalObject, static_cast<CanvasUnsignedIntArray*>(object));
    if (object->isShortArray())
        return getDOMObjectWrapper<JSCanvasShortArray>(exec, globalObject, static_cast<CanvasShortArray*>(object));
    if (object->isUnsignedShortArray())
        return getDOMObjectWrapper<JSCanvasUnsignedShortArray>(exec, globalObject, static_cast<CanvasUnsignedShortArray*>(object));
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
