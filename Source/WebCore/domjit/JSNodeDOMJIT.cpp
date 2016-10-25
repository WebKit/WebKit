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
#include "JSNode.h"

#if ENABLE(JIT)

#include "DOMJITAbstractHeapRepository.h"
#include "DOMJITHelpers.h"
#include "JSDOMWrapper.h"
#include "Node.h"
#include <domjit/DOMJITPatchpoint.h>
#include <domjit/DOMJITPatchpointParams.h>
#include <interpreter/FrameTracers.h>

using namespace JSC;

namespace WebCore {

enum class IsContainerGuardRequirement { Required, NotRequired };

template<typename WrappedNode>
EncodedJSValue JIT_OPERATION toWrapperSlow(JSC::ExecState* exec, JSC::JSGlobalObject* globalObject, void* result)
{
    ASSERT(exec);
    ASSERT(result);
    ASSERT(globalObject);
    JSC::NativeCallFrameTracer tracer(&exec->vm(), exec);
    return JSValue::encode(toJS(exec, static_cast<JSDOMGlobalObject*>(globalObject), *static_cast<WrappedNode*>(result)));
}

template<typename WrappedNode>
static Ref<JSC::DOMJIT::CallDOMPatchpoint> createCallDOMForOffsetAccess(ptrdiff_t offset, IsContainerGuardRequirement isContainerGuardRequirement)
{
    Ref<JSC::DOMJIT::CallDOMPatchpoint> patchpoint = JSC::DOMJIT::CallDOMPatchpoint::create();
    patchpoint->numGPScratchRegisters = 1;
    patchpoint->setGenerator([=](CCallHelpers& jit, JSC::DOMJIT::PatchpointParams& params) {
        JSValueRegs result = params[0].jsValueRegs();
        GPRReg node = params[1].gpr();
        GPRReg globalObject = params[2].gpr();
        GPRReg scratch = params.gpScratch(0);
        JSValue globalObjectValue = params[2].value();

        CCallHelpers::JumpList nullCases;
        // Load a wrapped object. "node" should be already type checked by CheckDOM.
        jit.loadPtr(CCallHelpers::Address(node, JSNode::offsetOfWrapped()), scratch);

        if (isContainerGuardRequirement == IsContainerGuardRequirement::Required)
            nullCases.append(jit.branchTest32(CCallHelpers::Zero, CCallHelpers::Address(scratch, Node::nodeFlagsMemoryOffset()), CCallHelpers::TrustedImm32(Node::flagIsContainer())));

        jit.loadPtr(CCallHelpers::Address(scratch, offset), scratch);
        nullCases.append(jit.branchTestPtr(CCallHelpers::Zero, scratch));

        DOMJIT::toWrapper<WrappedNode>(jit, params, scratch, globalObject, result, toWrapperSlow<WrappedNode>, globalObjectValue);
        CCallHelpers::Jump done = jit.jump();

        nullCases.link(&jit);
        jit.moveValue(jsNull(), result);
        done.link(&jit);
        return CCallHelpers::JumpList();
    });
    return patchpoint;
}

static Ref<JSC::DOMJIT::Patchpoint> checkNode()
{
    Ref<JSC::DOMJIT::Patchpoint> patchpoint = JSC::DOMJIT::Patchpoint::create();
    patchpoint->setGenerator([=](CCallHelpers& jit, JSC::DOMJIT::PatchpointParams& params) {
        CCallHelpers::JumpList failureCases;
        failureCases.append(DOMJIT::branchIfNotNode(jit, params[0].gpr()));
        return failureCases;
    });
    return patchpoint;
}

// Node#firstChild.
Ref<JSC::DOMJIT::Patchpoint> NodeFirstChildDOMJIT::checkDOM()
{
    return checkNode();
}

Ref<JSC::DOMJIT::CallDOMPatchpoint> NodeFirstChildDOMJIT::callDOM()
{
    const auto& heap = DOMJIT::AbstractHeapRepository::shared();
    auto patchpoint = createCallDOMForOffsetAccess<Node>(CAST_OFFSET(Node*, ContainerNode*) + ContainerNode::firstChildMemoryOffset(), IsContainerGuardRequirement::Required);
    patchpoint->effect = JSC::DOMJIT::Effect::forDef(heap.Node_firstChild);
    return patchpoint;
}

// Node#lastChild.
Ref<JSC::DOMJIT::Patchpoint> NodeLastChildDOMJIT::checkDOM()
{
    return checkNode();
}

Ref<JSC::DOMJIT::CallDOMPatchpoint> NodeLastChildDOMJIT::callDOM()
{
    const auto& heap = DOMJIT::AbstractHeapRepository::shared();
    auto patchpoint = createCallDOMForOffsetAccess<Node>(CAST_OFFSET(Node*, ContainerNode*) + ContainerNode::lastChildMemoryOffset(), IsContainerGuardRequirement::Required);
    patchpoint->effect = JSC::DOMJIT::Effect::forDef(heap.Node_lastChild);
    return patchpoint;
}

// Node#nextSibling.
Ref<JSC::DOMJIT::Patchpoint> NodeNextSiblingDOMJIT::checkDOM()
{
    return checkNode();
}

Ref<JSC::DOMJIT::CallDOMPatchpoint> NodeNextSiblingDOMJIT::callDOM()
{
    const auto& heap = DOMJIT::AbstractHeapRepository::shared();
    auto patchpoint = createCallDOMForOffsetAccess<Node>(Node::nextSiblingMemoryOffset(), IsContainerGuardRequirement::NotRequired);
    patchpoint->effect = JSC::DOMJIT::Effect::forDef(heap.Node_nextSibling);
    return patchpoint;
}

// Node#previousSibling.
Ref<JSC::DOMJIT::Patchpoint> NodePreviousSiblingDOMJIT::checkDOM()
{
    return checkNode();
}

Ref<JSC::DOMJIT::CallDOMPatchpoint> NodePreviousSiblingDOMJIT::callDOM()
{
    const auto& heap = DOMJIT::AbstractHeapRepository::shared();
    auto patchpoint = createCallDOMForOffsetAccess<Node>(Node::previousSiblingMemoryOffset(), IsContainerGuardRequirement::NotRequired);
    patchpoint->effect = JSC::DOMJIT::Effect::forDef(heap.Node_previousSibling);
    return patchpoint;
}

// Node#parentNode.
Ref<JSC::DOMJIT::Patchpoint> NodeParentNodeDOMJIT::checkDOM()
{
    return checkNode();
}

Ref<JSC::DOMJIT::CallDOMPatchpoint> NodeParentNodeDOMJIT::callDOM()
{
    const auto& heap = DOMJIT::AbstractHeapRepository::shared();
    auto patchpoint = createCallDOMForOffsetAccess<ContainerNode>(Node::parentNodeMemoryOffset(), IsContainerGuardRequirement::NotRequired);
    patchpoint->effect = JSC::DOMJIT::Effect::forDef(heap.Node_parentNode);
    return patchpoint;
}

// Node#nodeType.
Ref<JSC::DOMJIT::Patchpoint> NodeNodeTypeDOMJIT::checkDOM()
{
    return checkNode();
}

Ref<JSC::DOMJIT::CallDOMPatchpoint> NodeNodeTypeDOMJIT::callDOM()
{
    Ref<JSC::DOMJIT::CallDOMPatchpoint> patchpoint = JSC::DOMJIT::CallDOMPatchpoint::create();
    patchpoint->effect = JSC::DOMJIT::Effect::forPure();
    patchpoint->requireGlobalObject = false;
    patchpoint->setGenerator([=](CCallHelpers& jit, JSC::DOMJIT::PatchpointParams& params) {
        JSValueRegs result = params[0].jsValueRegs();
        GPRReg node = params[1].gpr();
        jit.load8(CCallHelpers::Address(node, JSC::JSCell::typeInfoTypeOffset()), result.payloadGPR());
        jit.and32(CCallHelpers::TrustedImm32(JSNodeTypeMask), result.payloadGPR());
        jit.boxInt32(result.payloadGPR(), result);
        return CCallHelpers::JumpList();
    });
    return patchpoint;
}

}

#endif
