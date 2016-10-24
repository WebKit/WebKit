/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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
 *
 */

#pragma once

#include "JSDOMWrapper.h"
#include <domjit/DOMJITPatchpoint.h>
#include <domjit/DOMJITPatchpointParams.h>

#if ENABLE(JIT)

namespace WebCore { namespace DOMJIT {

using JSC::CCallHelpers;
using JSC::GPRReg;
using JSC::JSValueRegs;

inline CCallHelpers::Jump branchIfNotWorldIsNormal(CCallHelpers& jit, GPRReg globalObject)
{
    return jit.branchTest8(CCallHelpers::Zero, CCallHelpers::Address(globalObject, JSDOMGlobalObject::offsetOfWorldIsNormal()));
}

inline CCallHelpers::Jump branchIfNotWeakIsLive(CCallHelpers& jit, GPRReg weakImpl)
{
    return jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::Address(weakImpl, JSC::WeakImpl::offsetOfWeakHandleOwner()), CCallHelpers::TrustedImm32(JSC::WeakImpl::StateMask));
}

template<typename WrappedType>
void tryLookUpWrapperCache(CCallHelpers& jit, CCallHelpers::JumpList& failureCases, GPRReg wrapped, GPRReg resultGPR)
{
    jit.loadPtr(CCallHelpers::Address(wrapped, ScriptWrappable::offsetOfWrapper<WrappedType>()), resultGPR);
    failureCases.append(jit.branchTestPtr(CCallHelpers::Zero, resultGPR));
    failureCases.append(branchIfNotWeakIsLive(jit, resultGPR));
    jit.loadPtr(CCallHelpers::Address(resultGPR, JSC::WeakImpl::offsetOfJSValue() + JSC::JSValue::offsetOfPayload()), resultGPR);
}

template<typename WrappedType, typename ToJSFunction>
void toWrapper(CCallHelpers& jit, JSC::DOMJIT::PatchpointParams& params, GPRReg wrapped, GPRReg globalObject, JSValueRegs result, ToJSFunction function, JSC::JSValue globalObjectConstant)
{
    ASSERT(wrapped != result.payloadGPR());
    ASSERT(globalObject != result.payloadGPR());
    GPRReg payloadGPR = result.payloadGPR();
    CCallHelpers::JumpList slowCases;

    if (globalObjectConstant) {
        if (!JSC::jsCast<JSDOMGlobalObject*>(globalObjectConstant)->worldIsNormal()) {
            slowCases.append(jit.jump());
            params.addSlowPathCall(slowCases, jit, function, result, globalObject, wrapped);
            return;
        }
    } else
        slowCases.append(branchIfNotWorldIsNormal(jit, globalObject));

    tryLookUpWrapperCache<WrappedType>(jit, slowCases, wrapped, payloadGPR);
    jit.boxCell(payloadGPR, result);
    params.addSlowPathCall(slowCases, jit, function, result, globalObject, wrapped);
}

inline CCallHelpers::Jump branchIfDOMWrapper(CCallHelpers& jit, GPRReg target)
{
    return jit.branch8(
        CCallHelpers::AboveOrEqual,
        CCallHelpers::Address(target, JSC::JSCell::typeInfoTypeOffset()),
        CCallHelpers::TrustedImm32(JSC::JSType(JSDOMWrapperType)));
}

inline CCallHelpers::Jump branchIfNotDOMWrapper(CCallHelpers& jit, GPRReg target)
{
    return jit.branch8(
        CCallHelpers::Below,
        CCallHelpers::Address(target, JSC::JSCell::typeInfoTypeOffset()),
        CCallHelpers::TrustedImm32(JSC::JSType(JSDOMWrapperType)));
}

inline CCallHelpers::Jump branchIfNode(CCallHelpers& jit, GPRReg target)
{
    return jit.branch8(
        CCallHelpers::AboveOrEqual,
        CCallHelpers::Address(target, JSC::JSCell::typeInfoTypeOffset()),
        CCallHelpers::TrustedImm32(JSC::JSType(JSNodeType)));
}

inline CCallHelpers::Jump branchIfNotNode(CCallHelpers& jit, GPRReg target)
{
    return jit.branch8(
        CCallHelpers::Below,
        CCallHelpers::Address(target, JSC::JSCell::typeInfoTypeOffset()),
        CCallHelpers::TrustedImm32(JSC::JSType(JSNodeType)));
}

inline CCallHelpers::Jump branchIfElement(CCallHelpers& jit, GPRReg target)
{
    return jit.branch8(
        CCallHelpers::AboveOrEqual,
        CCallHelpers::Address(target, JSC::JSCell::typeInfoTypeOffset()),
        CCallHelpers::TrustedImm32(JSC::JSType(JSElementType)));
}

inline CCallHelpers::Jump branchIfNotElement(CCallHelpers& jit, GPRReg target)
{
    return jit.branch8(
        CCallHelpers::Below,
        CCallHelpers::Address(target, JSC::JSCell::typeInfoTypeOffset()),
        CCallHelpers::TrustedImm32(JSC::JSType(JSElementType)));
}

inline CCallHelpers::Jump branchIfDocumentWrapper(CCallHelpers& jit, GPRReg target)
{
    return jit.branchIfType(target, JSC::JSType(JSDocumentWrapperType));
}

inline CCallHelpers::Jump branchIfNotDocumentWrapper(CCallHelpers& jit, GPRReg target)
{
    return jit.branchIfNotType(target, JSC::JSType(JSDocumentWrapperType));
}

} }

#endif
