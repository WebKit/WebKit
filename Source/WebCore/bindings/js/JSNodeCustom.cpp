/*
 * Copyright (C) 2007, 2009-2010, 2016 Apple Inc. All rights reserved.
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
#include "ExceptionCode.h"
#include "HTMLAudioElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLImageElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLStyleElement.h"
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
#include "JSProcessingInstruction.h"
#include "JSSVGElementWrapperFactory.h"
#include "JSShadowRoot.h"
#include "JSText.h"
#include "Node.h"
#include "ProcessingInstruction.h"
#include "RegisteredEventListener.h"
#include "SVGElement.h"
#include "ScriptState.h"
#include "ShadowRoot.h"
#include "StyleSheet.h"
#include "StyledElement.h"
#include "Text.h"

using namespace JSC;

namespace WebCore {

using namespace HTMLNames;

static inline bool isReachableFromDOM(Node* node, SlotVisitor& visitor)
{
    if (!node->isConnected()) {
        if (is<Element>(*node)) {
            auto& element = downcast<Element>(*node);

            // If a wrapper is the last reference to an image element
            // that is loading but not in the document, the wrapper is observable
            // because it is the only thing keeping the image element alive, and if
            // the element is destroyed, its load event will not fire.
            // FIXME: The DOM should manage this issue without the help of JavaScript wrappers.
            if (is<HTMLImageElement>(element)) {
                if (downcast<HTMLImageElement>(element).hasPendingActivity())
                    return true;
            }
#if ENABLE(VIDEO)
            else if (is<HTMLAudioElement>(element)) {
                if (!downcast<HTMLAudioElement>(element).paused())
                    return true;
            }
#endif
        }

        // If a node is firing event listeners, its wrapper is observable because
        // its wrapper is responsible for marking those event listeners.
        if (node->isFiringEventListeners())
            return true;
    }

    return visitor.containsOpaqueRoot(root(node));
}

bool JSNodeOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    JSNode* jsNode = jsCast<JSNode*>(handle.slot()->asCell());
    return isReachableFromDOM(&jsNode->wrapped(), visitor);
}

JSValue JSNode::insertBefore(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    JSValue newChildValue = state.uncheckedArgument(0);
    auto* newChild = JSNode::toWrapped(vm, newChildValue);
    if (UNLIKELY(!newChild))
        return JSValue::decode(throwArgumentTypeError(state, scope, 0, "node", "Node", "insertBefore", "Node"));

    propagateException(state, scope, wrapped().insertBefore(*newChild, JSNode::toWrapped(vm, state.uncheckedArgument(1))));
    return newChildValue;
}

JSValue JSNode::replaceChild(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto* newChild = JSNode::toWrapped(vm, state.uncheckedArgument(0));
    JSValue oldChildValue = state.uncheckedArgument(1);
    auto* oldChild = JSNode::toWrapped(vm, oldChildValue);
    if (UNLIKELY(!newChild || !oldChild)) {
        if (!newChild)
            return JSValue::decode(throwArgumentTypeError(state, scope, 0, "node", "Node", "replaceChild", "Node"));
        return JSValue::decode(throwArgumentTypeError(state, scope, 1, "child", "Node", "replaceChild", "Node"));
    }

    propagateException(state, scope, wrapped().replaceChild(*newChild, *oldChild));
    return oldChildValue;
}

JSValue JSNode::removeChild(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue childValue = state.argument(0);
    auto* child = JSNode::toWrapped(vm, childValue);
    if (UNLIKELY(!child))
        return JSValue::decode(throwArgumentTypeError(state, scope, 0, "child", "Node", "removeChild", "Node"));

    propagateException(state, scope, wrapped().removeChild(*child));
    return childValue;
}

JSValue JSNode::appendChild(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue newChildValue = state.argument(0);
    auto newChild = JSNode::toWrapped(vm, newChildValue);
    if (UNLIKELY(!newChild))
        return JSValue::decode(throwArgumentTypeError(state, scope, 0, "node", "Node", "appendChild", "Node"));

    propagateException(state, scope, wrapped().appendChild(*newChild));
    return newChildValue;
}

JSScope* JSNode::pushEventHandlerScope(ExecState* exec, JSScope* node) const
{
    if (inherits(exec->vm(), JSHTMLElement::info()))
        return jsCast<const JSHTMLElement*>(this)->pushEventHandlerScope(exec, node);
    return node;
}

void JSNode::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(root(wrapped()));
}

static ALWAYS_INLINE JSValue createWrapperInline(ExecState* exec, JSDOMGlobalObject* globalObject, Ref<Node>&& node)
{
    ASSERT(!getCachedWrapper(globalObject->world(), node));
    
    JSDOMObject* wrapper;    
    switch (node->nodeType()) {
        case Node::ELEMENT_NODE:
            if (is<HTMLElement>(node.get()))
                wrapper = createJSHTMLWrapper(globalObject, static_reference_cast<HTMLElement>(WTFMove(node)));
            else if (is<SVGElement>(node.get()))
                wrapper = createJSSVGWrapper(globalObject, static_reference_cast<SVGElement>(WTFMove(node)));
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
            return toJS(exec, globalObject, downcast<Document>(node.get()));
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

JSValue createWrapper(ExecState* exec, JSDOMGlobalObject* globalObject, Ref<Node>&& node)
{
    return createWrapperInline(exec, globalObject, WTFMove(node));
}
    
JSValue toJSNewlyCreated(ExecState* exec, JSDOMGlobalObject* globalObject, Ref<Node>&& node)
{
    return createWrapperInline(exec, globalObject, WTFMove(node));
}

JSC::JSObject* getOutOfLineCachedWrapper(JSDOMGlobalObject* globalObject, Node& node)
{
    ASSERT(!globalObject->world().isNormal());
    return globalObject->world().m_wrappers.get(&node);
}

void willCreatePossiblyOrphanedTreeByRemovalSlowCase(Node* root)
{
    JSC::ExecState* scriptState = mainWorldExecState(root->document().frame());
    if (!scriptState)
        return;

    JSLockHolder lock(scriptState);
    toJS(scriptState, static_cast<JSDOMGlobalObject*>(scriptState->lexicalGlobalObject()), *root);
}

} // namespace WebCore
