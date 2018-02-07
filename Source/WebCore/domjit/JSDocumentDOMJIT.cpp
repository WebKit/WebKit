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
#include "JSHTMLElement.h"
#include <JavaScriptCore/Snippet.h>
#include <JavaScriptCore/SnippetParams.h>

namespace WebCore {
using namespace JSC;

Ref<JSC::Snippet> checkSubClassSnippetForJSDocument()
{
    return DOMJIT::checkDOM<Document>();
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileDocumentDocumentElementAttribute()
{
    Ref<JSC::DOMJIT::CallDOMGetterSnippet> snippet = JSC::DOMJIT::CallDOMGetterSnippet::create();
    snippet->numGPScratchRegisters = 1;
    snippet->setGenerator([=](CCallHelpers& jit, JSC::SnippetParams& params) {
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
    snippet->effect = JSC::DOMJIT::Effect::forDef(DOMJIT::AbstractHeapRepository::Document_documentElement);
    return snippet;
}

static void loadLocalName(CCallHelpers& jit, GPRReg htmlElement, GPRReg localNameImpl)
{
    jit.loadPtr(CCallHelpers::Address(htmlElement, Element::tagQNameMemoryOffset() + QualifiedName::implMemoryOffset()), localNameImpl);
    jit.loadPtr(CCallHelpers::Address(localNameImpl, QualifiedName::QualifiedNameImpl::localNameMemoryOffset()), localNameImpl);
}

Ref<JSC::DOMJIT::CallDOMGetterSnippet> compileDocumentBodyAttribute()
{
    Ref<JSC::DOMJIT::CallDOMGetterSnippet> snippet = JSC::DOMJIT::CallDOMGetterSnippet::create();
    snippet->numGPScratchRegisters = 2;
    snippet->setGenerator([=](CCallHelpers& jit, JSC::SnippetParams& params) {
        JSValueRegs result = params[0].jsValueRegs();
        GPRReg document = params[1].gpr();
        GPRReg globalObject = params[2].gpr();
        JSValue globalObjectValue = params[2].value();
        GPRReg scratch1 = params.gpScratch(0);
        GPRReg scratch2 = params.gpScratch(1);

        jit.loadPtr(CCallHelpers::Address(document, JSDocument::offsetOfWrapped()), scratch1);
        DOMJIT::loadDocumentElement(jit, scratch1, scratch1);

        CCallHelpers::JumpList nullCases;
        CCallHelpers::JumpList successCases;
        nullCases.append(jit.branchTestPtr(CCallHelpers::Zero, scratch1));
        nullCases.append(DOMJIT::branchTestIsHTMLFlagOnNode(jit, CCallHelpers::Zero, scratch1));
        // We ensured that the name of the given element is HTML qualified.
        // It allows us to perform local name comparison!
        loadLocalName(jit, scratch1, scratch2);
        nullCases.append(jit.branchPtr(CCallHelpers::NotEqual, scratch2, CCallHelpers::TrustedImmPtr(HTMLNames::htmlTag->localName().impl())));

        RELEASE_ASSERT(!CAST_OFFSET(Node*, ContainerNode*));
        RELEASE_ASSERT(!CAST_OFFSET(Node*, Element*));
        RELEASE_ASSERT(!CAST_OFFSET(Node*, HTMLElement*));

        // Node* node = current.firstChild();
        // while (node && !is<HTMLElement>(*node))
        //     node = node->nextSibling();
        // return downcast<HTMLElement>(node);
        jit.loadPtr(CCallHelpers::Address(scratch1, ContainerNode::firstChildMemoryOffset()), scratch1);

        CCallHelpers::Label loopStart = jit.label();
        nullCases.append(jit.branchTestPtr(CCallHelpers::Zero, scratch1));
        auto notHTMLElementCase = DOMJIT::branchTestIsHTMLFlagOnNode(jit, CCallHelpers::Zero, scratch1);
        // We ensured that the name of the given element is HTML qualified.
        // It allows us to perform local name comparison!
        loadLocalName(jit, scratch1, scratch2);
        successCases.append(jit.branchPtr(CCallHelpers::Equal, scratch2, CCallHelpers::TrustedImmPtr(HTMLNames::bodyTag->localName().impl())));
        successCases.append(jit.branchPtr(CCallHelpers::Equal, scratch2, CCallHelpers::TrustedImmPtr(HTMLNames::framesetTag->localName().impl())));

        notHTMLElementCase.link(&jit);
        jit.loadPtr(CCallHelpers::Address(scratch1, Node::nextSiblingMemoryOffset()), scratch1);
        jit.jump().linkTo(loopStart, &jit);

        successCases.link(&jit);
        DOMJIT::toWrapper<HTMLElement>(jit, params, scratch1, globalObject, result, DOMJIT::toWrapperSlow<HTMLElement>, globalObjectValue);
        auto done = jit.jump();

        nullCases.link(&jit);
        jit.moveValue(jsNull(), result);
        done.link(&jit);

        return CCallHelpers::JumpList();
    });
    snippet->effect = JSC::DOMJIT::Effect::forDef(DOMJIT::AbstractHeapRepository::Document_body);
    return snippet;
}

}

#endif
