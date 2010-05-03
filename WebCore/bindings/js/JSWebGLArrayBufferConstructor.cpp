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

#include "JSWebGLArrayBufferConstructor.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "JSWebGLArrayBuffer.h"

namespace WebCore {

using namespace JSC;

const ClassInfo JSWebGLArrayBufferConstructor::s_info = { "WebGLArrayBufferConstructor", 0, 0, 0 };

JSWebGLArrayBufferConstructor::JSWebGLArrayBufferConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMConstructorObject(JSWebGLArrayBufferConstructor::createStructure(globalObject->objectPrototype()), globalObject)
{
    putDirect(exec->propertyNames().prototype, JSWebGLArrayBufferPrototype::self(exec, globalObject), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 2), ReadOnly|DontDelete|DontEnum);
}

static JSObject* constructCanvasArrayBuffer(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    JSWebGLArrayBufferConstructor* jsConstructor = static_cast<JSWebGLArrayBufferConstructor*>(constructor);

    unsigned int size = 0;
    if (args.size() == 1) {
        size = (unsigned int)args.at(0).toInt32(exec);
        if (isnan(size))
            size = 0;
    }
    RefPtr<WebGLArrayBuffer> buffer = WebGLArrayBuffer::create(size, 1);
    if (!buffer.get()){
        setDOMException(exec, INDEX_SIZE_ERR);
        return 0;
    }
    return asObject(toJS(exec, jsConstructor->globalObject(), buffer.get()));
}

JSC::ConstructType JSWebGLArrayBufferConstructor::getConstructData(JSC::ConstructData& constructData)
{
    constructData.native.function = constructCanvasArrayBuffer;
    return ConstructTypeHost;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
