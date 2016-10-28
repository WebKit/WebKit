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
 */

#include "config.h"
#include "JSDocument.h"

#if ENABLE(JIT)

#include "DOMJITAbstractHeapRepository.h"
#include "DOMJITCheckDOM.h"
#include "DOMJITHelpers.h"
#include "Document.h"
#include "JSDOMWrapper.h"
#include "JSElement.h"
#include <domjit/DOMJITPatchpoint.h>
#include <domjit/DOMJITPatchpointParams.h>

using namespace JSC;

namespace WebCore {

Ref<JSC::DOMJIT::Patchpoint> DocumentDocumentElementDOMJIT::checkDOM()
{
    return DOMJIT::checkDOM<Document>();
}

Ref<JSC::DOMJIT::CallDOMGetterPatchpoint> DocumentDocumentElementDOMJIT::callDOMGetter()
{
    const auto& heap = DOMJIT::AbstractHeapRepository::shared();
    Ref<JSC::DOMJIT::CallDOMGetterPatchpoint> patchpoint = JSC::DOMJIT::CallDOMGetterPatchpoint::create();
    patchpoint->numGPScratchRegisters = 1;
    patchpoint->setGenerator([=](CCallHelpers& jit, JSC::DOMJIT::PatchpointParams& params) {
        JSValueRegs result = params[0].jsValueRegs();
        GPRReg document = params[1].gpr();
        GPRReg globalObject = params[2].gpr();
        JSValue globalObjectValue = params[2].value();
        GPRReg scratch = params.gpScratch(0);

        jit.loadPtr(CCallHelpers::Address(document, JSDocument::offsetOfWrapped()), scratch);
        DOMJIT::loadDocumentElement(jit, scratch, scratch);
        auto nullCase = jit.branchTestPtr(CCallHelpers::Zero, scratch);
        DOMJIT::toWrapper<Element>(jit, params, scratch, globalObject, result, DOMJIT::toWrapperSlow<Element>, globalObjectValue);
        auto done = jit.jump();

        nullCase.link(&jit);
        jit.moveValue(jsNull(), result);
        done.link(&jit);

        return CCallHelpers::JumpList();
    });
    patchpoint->effect = JSC::DOMJIT::Effect::forDef(heap.Document_documentElement);
    return patchpoint;
}

}

#endif
