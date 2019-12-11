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
#include "DOMJITCheckDOM.h"
#include "DOMJITHelpers.h"
#include "JSDOMWrapper.h"
#include "Node.h"
#include <JavaScriptCore/FrameTracers.h>
#include <JavaScriptCore/Snippet.h>
#include <JavaScriptCore/SnippetParams.h>

namespace WebCore {
using namespace JSC;

Ref<JSC::Snippet> checkSubClassSnippetForJSNode()
{
    return DOMJIT::checkDOM<Node>();
}

enum class IsContainerGuardRequirement { Required, NotRequired };

template<typename WrappedNode>
static Ref<JSC::DOMJIT::CallDOMGetterSnippet> createCallDOMGetterForOffsetAccess(ptrdiff_t offset, IsContainerGuardRequirement isContainerGuardRequirement)
{
    Ref<JSC::DOMJIT::CallDOMGetterSnippet> snippet = JSC::DOMJIT::CallDOMGetterSnippet::create();
    snippet->numGPScratchRegisters = 1;
    snippet->setGenerator([=](CCallHelpers& jit, JSC::SnippetParams& params) {
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

        DOMJIT::toWrapper<WrappedNode>(jit, params, scratch, globalObject, result, DOMJIT::toWrapperSlow<WrappedNode>, globalObjectValue);
        CCallHelpers::Jump done = jit.jump();

        nullCases.link(&jit);
        jit.moveValue(jsNull(), result);
        done.link(&jit);
        return CCallHelpers::JumpList();
    });
    return snippet;
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileNodeFirstChildAttribute()
{
    auto snippet = createCallDOMGetterForOffsetAccess<Node>(CAST_OFFSET(Node*, ContainerNode*) + ContainerNode::firstChildMemoryOffset(), IsContainerGuardRequirement::Required);
    snippet->effect = JSC::DOMJIT::Effect::forDef(DOMJIT::AbstractHeapRepository::Node_firstChild);
    return snippet;
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileNodeLastChildAttribute()
{
    auto snippet = createCallDOMGetterForOffsetAccess<Node>(CAST_OFFSET(Node*, ContainerNode*) + ContainerNode::lastChildMemoryOffset(), IsContainerGuardRequirement::Required);
    snippet->effect = JSC::DOMJIT::Effect::forDef(DOMJIT::AbstractHeapRepository::Node_lastChild);
    return snippet;
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileNodeNextSiblingAttribute()
{
    auto snippet = createCallDOMGetterForOffsetAccess<Node>(Node::nextSiblingMemoryOffset(), IsContainerGuardRequirement::NotRequired);
    snippet->effect = JSC::DOMJIT::Effect::forDef(DOMJIT::AbstractHeapRepository::Node_nextSibling);
    return snippet;
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileNodePreviousSiblingAttribute()
{
    auto snippet = createCallDOMGetterForOffsetAccess<Node>(Node::previousSiblingMemoryOffset(), IsContainerGuardRequirement::NotRequired);
    snippet->effect = JSC::DOMJIT::Effect::forDef(DOMJIT::AbstractHeapRepository::Node_previousSibling);
    return snippet;
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileNodeParentNodeAttribute()
{
    auto snippet = createCallDOMGetterForOffsetAccess<ContainerNode>(Node::parentNodeMemoryOffset(), IsContainerGuardRequirement::NotRequired);
    snippet->effect = JSC::DOMJIT::Effect::forDef(DOMJIT::AbstractHeapRepository::Node_parentNode);
    return snippet;
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileNodeNodeTypeAttribute()
{
    Ref<JSC::DOMJIT::CallDOMGetterSnippet> snippet = JSC::DOMJIT::CallDOMGetterSnippet::create();
    snippet->effect = JSC::DOMJIT::Effect::forPure();
    snippet->requireGlobalObject = false;
    snippet->setGenerator([=](CCallHelpers& jit, JSC::SnippetParams& params) {
        JSValueRegs result = params[0].jsValueRegs();
        GPRReg node = params[1].gpr();
        jit.load8(CCallHelpers::Address(node, JSC::JSCell::typeInfoTypeOffset()), result.payloadGPR());
        jit.and32(CCallHelpers::TrustedImm32(JSNodeTypeMask), result.payloadGPR());
        jit.boxInt32(result.payloadGPR(), result);
        return CCallHelpers::JumpList();
    });
    return snippet;
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileNodeOwnerDocumentAttribute()
{
    Ref<JSC::DOMJIT::CallDOMGetterSnippet> snippet = JSC::DOMJIT::CallDOMGetterSnippet::create();
    snippet->numGPScratchRegisters = 2;
    snippet->setGenerator([=](CCallHelpers& jit, JSC::SnippetParams& params) {
        JSValueRegs result = params[0].jsValueRegs();
        GPRReg node = params[1].gpr();
        GPRReg globalObject = params[2].gpr();
        JSValue globalObjectValue = params[2].value();
        GPRReg wrapped = params.gpScratch(0);
        GPRReg document = params.gpScratch(1);

        jit.loadPtr(CCallHelpers::Address(node, JSNode::offsetOfWrapped()), wrapped);
        DOMJIT::loadDocument(jit, wrapped, document);
        RELEASE_ASSERT(!CAST_OFFSET(EventTarget*, Node*));
        RELEASE_ASSERT(!CAST_OFFSET(Node*, Document*));

        CCallHelpers::JumpList nullCases;
        // If the |this| is the document itself, ownerDocument will return null.
        nullCases.append(jit.branchPtr(CCallHelpers::Equal, wrapped, document));
        DOMJIT::toWrapper<Document>(jit, params, document, globalObject, result, DOMJIT::toWrapperSlow<Document>, globalObjectValue);
        auto done = jit.jump();

        nullCases.link(&jit);
        jit.moveValue(jsNull(), result);
        done.link(&jit);
        return CCallHelpers::JumpList();
    });
    snippet->effect = JSC::DOMJIT::Effect::forDef(DOMJIT::AbstractHeapRepository::Node_ownerDocument);
    return snippet;
}

}

#endif
