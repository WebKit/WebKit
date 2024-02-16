/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
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

#include "Attr.h"
#include "CDATASection.h"
#include "Comment.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLCanvasElement.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "JSAttr.h"
#include "JSCDATASection.h"
#include "JSComment.h"
#include "JSDOMBinding.h"
#include "JSDocument.h"
#include "JSDocumentFragment.h"
#include "JSDocumentType.h"
#include "JSEventListener.h"
#include "JSHTMLElement.h"
#include "JSHTMLElementWrapperFactory.h"
#include "JSLocalDOMWindowCustom.h"
#include "JSMathMLElementWrapperFactory.h"
#include "JSProcessingInstruction.h"
#include "JSSVGElementWrapperFactory.h"
#include "JSShadowRoot.h"
#include "JSText.h"
#include "MathMLElement.h"
#include "Node.h"
#include "ProcessingInstruction.h"
#include "RegisteredEventListener.h"
#include "SVGElement.h"
#include "ShadowRoot.h"
#include "GCReachableRef.h"
#include "Text.h"
#include "WebCoreOpaqueRootInlines.h"

namespace WebCore {

using namespace JSC;
using namespace HTMLNames;

bool JSNodeOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, const char** reason)
{
    auto& node = jsCast<JSNode*>(handle.slot()->asCell())->wrapped();
    if (!node.isConnected()) {
        if (GCReachableRefMap::contains(node)) {
            if (UNLIKELY(reason))
                *reason = "Node is scheduled to be used in an async script invocation)";
            return true;
        }
    }

    if (UNLIKELY(reason))
        *reason = "Connected node";

    return containsWebCoreOpaqueRoot(visitor, node);
}

JSScope* JSNode::pushEventHandlerScope(JSGlobalObject* lexicalGlobalObject, JSScope* node) const
{
    if (inherits<JSHTMLElement>())
        return jsCast<const JSHTMLElement*>(this)->pushEventHandlerScope(lexicalGlobalObject, node);
    return node;
}

template<typename Visitor>
void JSNode::visitAdditionalChildren(Visitor& visitor)
{
    addWebCoreOpaqueRoot(visitor, wrapped());
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSNode);

static ALWAYS_INLINE JSValue createWrapperInline(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, Ref<Node>&& node)
{
    ASSERT(!getCachedWrapper(globalObject->world(), node));
    
    JSDOMObject* wrapper;    
    switch (node->nodeType()) {
        case Node::ELEMENT_NODE:
            if (auto* htmlElement = dynamicDowncast<HTMLElement>(node.get()))
                wrapper = createJSHTMLWrapper(globalObject, *htmlElement);
            else if (auto* svgElement = dynamicDowncast<SVGElement>(node.get()))
                wrapper = createJSSVGWrapper(globalObject, *svgElement);
#if ENABLE(MATHML)
            else if (auto* mathmlElement = dynamicDowncast<MathMLElement>(node.get()))
                wrapper = createJSMathMLWrapper(globalObject, *mathmlElement);
#endif
            else
                wrapper = createWrapper<Element>(globalObject, WTFMove(node));
            break;
        case Node::ATTRIBUTE_NODE:
            wrapper = createWrapper<Attr>(globalObject, WTFMove(node));
            break;
        case Node::TEXT_NODE:
            wrapper = createWrapper<Text>(globalObject, WTFMove(node));
            break;
        case Node::CDATA_SECTION_NODE:
            wrapper = createWrapper<CDATASection>(globalObject, WTFMove(node));
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
            wrapper = createWrapper<ProcessingInstruction>(globalObject, WTFMove(node));
            break;
        case Node::COMMENT_NODE:
            wrapper = createWrapper<Comment>(globalObject, WTFMove(node));
            break;
        case Node::DOCUMENT_NODE:
            // we don't want to cache the document itself in the per-document dictionary
            return toJS(lexicalGlobalObject, globalObject, uncheckedDowncast<Document>(node.get()));
        case Node::DOCUMENT_TYPE_NODE:
            wrapper = createWrapper<DocumentType>(globalObject, WTFMove(node));
            break;
        case Node::DOCUMENT_FRAGMENT_NODE:
            if (node->isShadowRoot())
                wrapper = createWrapper<ShadowRoot>(globalObject, WTFMove(node));
            else
                wrapper = createWrapper<DocumentFragment>(globalObject, WTFMove(node));
            break;
        default:
            wrapper = createWrapper<Node>(globalObject, WTFMove(node));
    }

    return wrapper;
}

JSValue createWrapper(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, Ref<Node>&& node)
{
    return createWrapperInline(lexicalGlobalObject, globalObject, WTFMove(node));
}
    
JSValue toJSNewlyCreated(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, Ref<Node>&& node)
{
    return createWrapperInline(lexicalGlobalObject, globalObject, WTFMove(node));
}

JSC::JSObject* getOutOfLineCachedWrapper(JSDOMGlobalObject* globalObject, Node& node)
{
    ASSERT(!globalObject->world().isNormal());
    return globalObject->world().wrappers().get(&node);
}

void willCreatePossiblyOrphanedTreeByRemovalSlowCase(Node& root)
{
    auto frame = root.document().frame();
    if (!frame)
        return;

    auto& globalObject = mainWorldGlobalObject(*frame);
    JSLockHolder lock(&globalObject);
    ASSERT(!root.wrapper());
    createWrapper(&globalObject, &globalObject, root);
}

} // namespace WebCore
