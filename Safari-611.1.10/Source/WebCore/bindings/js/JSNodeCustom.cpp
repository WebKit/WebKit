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
#include "ScriptState.h"
#include "ShadowRoot.h"
#include "GCReachableRef.h"
#include "StyleSheet.h"
#include "StyledElement.h"
#include "Text.h"


namespace WebCore {
using namespace JSC;

using namespace HTMLNames;

static inline bool isReachableFromDOM(Node* node, SlotVisitor& visitor, const char** reason)
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
                if (downcast<HTMLImageElement>(element).hasPendingActivity()) {
                    if (UNLIKELY(reason))
                        *reason = "Image element with pending activity";
                    return true;
                }
            }
#if ENABLE(VIDEO)
            else if (is<HTMLAudioElement>(element)) {
                if (!downcast<HTMLAudioElement>(element).paused()) {
                    if (UNLIKELY(reason))
                        *reason = "Audio element which is not paused";
                    return true;
                }
            }
#endif
        }

        // If a node is firing event listeners, its wrapper is observable because
        // its wrapper is responsible for marking those event listeners.
        if (node->isFiringEventListeners()) {
            if (UNLIKELY(reason))
                *reason = "Node which is firing event listeners";
            return true;
        }
        if (GCReachableRefMap::contains(*node)) {
            if (UNLIKELY(reason))
                *reason = "Node is scheduled to be used in an async script invocation)";
            return true;
        }
    }

    if (UNLIKELY(reason))
        *reason = "Connected node";

    return visitor.containsOpaqueRoot(root(node));
}

bool JSNodeOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor, const char** reason)
{
    JSNode* jsNode = jsCast<JSNode*>(handle.slot()->asCell());
    return isReachableFromDOM(&jsNode->wrapped(), visitor, reason);
}

JSScope* JSNode::pushEventHandlerScope(JSGlobalObject* lexicalGlobalObject, JSScope* node) const
{
    if (inherits<JSHTMLElement>(lexicalGlobalObject->vm()))
        return jsCast<const JSHTMLElement*>(this)->pushEventHandlerScope(lexicalGlobalObject, node);
    return node;
}

void JSNode::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(root(wrapped()));
}

static ALWAYS_INLINE JSValue createWrapperInline(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, Ref<Node>&& node)
{
    ASSERT(!getCachedWrapper(globalObject->world(), node));
    
    JSDOMObject* wrapper;    
    switch (node->nodeType()) {
        case Node::ELEMENT_NODE:
            if (is<HTMLElement>(node))
                wrapper = createJSHTMLWrapper(globalObject, static_reference_cast<HTMLElement>(WTFMove(node)));
            else if (is<SVGElement>(node))
                wrapper = createJSSVGWrapper(globalObject, static_reference_cast<SVGElement>(WTFMove(node)));
#if ENABLE(MATHML)
            else if (is<MathMLElement>(node))
                wrapper = createJSMathMLWrapper(globalObject, static_reference_cast<MathMLElement>(WTFMove(node)));
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
            return toJS(lexicalGlobalObject, globalObject, downcast<Document>(node.get()));
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

void willCreatePossiblyOrphanedTreeByRemovalSlowCase(Node* root)
{
    JSC::JSGlobalObject* lexicalGlobalObject = mainWorldExecState(root->document().frame());
    if (!lexicalGlobalObject)
        return;

    JSLockHolder lock(lexicalGlobalObject);
    toJS(lexicalGlobalObject, static_cast<JSDOMGlobalObject*>(lexicalGlobalObject), *root);
}

} // namespace WebCore
