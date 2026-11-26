/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSImageData.h"

#if ENABLE(JIT)

#include "JSDOMConvertBufferSource.h"
#include "JSDOMConvertUnion.h"
#include "JSDOMGlobalObject.h"
#include <JavaScriptCore/DOMJITCallDOMGetterSnippet.h>
#include <JavaScriptCore/FrameTracers.h>
#include <JavaScriptCore/SnippetParams.h>

IGNORE_WARNINGS_BEGIN("frame-address")

namespace WebCore {
using namespace JSC;

extern "C" { static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(operationImageDataData, EncodedJSValue, (JSGlobalObject*, void*)); }

JSC_DEFINE_JIT_OPERATION(operationImageDataData, EncodedJSValue, (JSGlobalObject* lexicalGlobalObject, void* thisObjectPointer))
{
    VM& vm = lexicalGlobalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto& thisObject = *static_cast<JSImageData*>(thisObjectPointer);
    if (JSValue cachedValue = thisObject.m_data.get())
        OPERATION_RETURN(scope, JSValue::encode(cachedValue));
    JSValue result = toJS<IDLUnion<IDLUint8ClampedArray, IDLFloat16Array>>(*lexicalGlobalObject, *thisObject.realm(), scope, thisObject.wrapped().data());
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    thisObject.m_data.set(vm, &thisObject, result);
    OPERATION_RETURN(scope, JSValue::encode(result));
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileImageDataDataAttribute()
{
    Ref<JSC::DOMJIT::CallDOMGetterSnippet> snippet = JSC::DOMJIT::CallDOMGetterSnippet::create();
    // ImageData's data never changes, so the getter can be CSE'd and hoisted.
    snippet->effect = JSC::DOMJIT::Effect::forPure();
    snippet->setGenerator([=](CCallHelpers& jit, JSC::SnippetParams& params) {
        GPRReg result = params[0].gpr();
        GPRReg imageData = params[1].gpr();
        GPRReg globalObject = params[2].gpr();

        CCallHelpers::JumpList slowCases;
        jit.loadPtr(CCallHelpers::Address(imageData, OBJECT_OFFSETOF(JSImageData, m_data)), result);
        slowCases.append(jit.branchTestPtr(CCallHelpers::Zero, result));
        params.addSlowPathCall(slowCases, jit, operationImageDataData, result, globalObject, imageData);
        return CCallHelpers::JumpList();
    });
    return snippet;
}

}

IGNORE_WARNINGS_END

#endif
