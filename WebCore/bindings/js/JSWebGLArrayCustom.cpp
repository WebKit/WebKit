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
#include "JSWebGLArray.h"
#include "JSWebGLByteArray.h"
#include "JSWebGLUnsignedByteArray.h"
#include "JSWebGLShortArray.h"
#include "JSWebGLUnsignedShortArray.h"
#include "JSWebGLIntArray.h"
#include "JSWebGLUnsignedIntArray.h"
#include "JSWebGLFloatArray.h"

#include "WebGLArray.h"

using namespace JSC;

namespace WebCore {

JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, WebGLArray* object)
{
    if (object->isFloatArray())
        return getDOMObjectWrapper<JSWebGLFloatArray>(exec, globalObject, static_cast<WebGLFloatArray*>(object));
    if (object->isUnsignedByteArray())
        return getDOMObjectWrapper<JSWebGLUnsignedByteArray>(exec, globalObject, static_cast<WebGLUnsignedByteArray*>(object));
    if (object->isByteArray())
        return getDOMObjectWrapper<JSWebGLByteArray>(exec, globalObject, static_cast<WebGLByteArray*>(object));
    if (object->isIntArray())
        return getDOMObjectWrapper<JSWebGLIntArray>(exec, globalObject, static_cast<WebGLIntArray*>(object));
    if (object->isUnsignedIntArray())
        return getDOMObjectWrapper<JSWebGLUnsignedIntArray>(exec, globalObject, static_cast<WebGLUnsignedIntArray*>(object));
    if (object->isShortArray())
        return getDOMObjectWrapper<JSWebGLShortArray>(exec, globalObject, static_cast<WebGLShortArray*>(object));
    if (object->isUnsignedShortArray())
        return getDOMObjectWrapper<JSWebGLUnsignedShortArray>(exec, globalObject, static_cast<WebGLUnsignedShortArray*>(object));
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
