/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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
#include "JSNode.h"

#include "Attr.h"
#include "CDATASection.h"
#include "Comment.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Entity.h"
#include "EntityReference.h"
#include "HTMLElement.h"
#include "JSAttr.h"
#include "JSCDATASection.h"
#include "JSComment.h"
#include "JSDocument.h"
#include "JSDocumentFragment.h"
#include "JSDocumentType.h"
#include "JSEntity.h"
#include "JSEntityReference.h"
#include "JSEventListener.h"
#include "JSHTMLElement.h"
#include "JSHTMLElementWrapperFactory.h"
#include "JSNotation.h"
#include "JSProcessingInstruction.h"
#include "JSText.h"
#include "Node.h"
#include "Notation.h"
#include "ProcessingInstruction.h"
#include "RegisteredEventListener.h"
#include "Text.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

#if ENABLE(SVG)
#include "JSSVGElementWrapperFactory.h"
#include "SVGElement.h"
#endif

using namespace JSC;

namespace WebCore {

typedef int ExpectionCode;

JSValue JSNode::insertBefore(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    bool ok = impl()->insertBefore(toNode(args.at(0)), toNode(args.at(1)), ec, true);
    setDOMException(exec, ec);
    if (ok)
        return args.at(0);
    return jsNull();
}

JSValue JSNode::replaceChild(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    bool ok = impl()->replaceChild(toNode(args.at(0)), toNode(args.at(1)), ec, true);
    setDOMException(exec, ec);
    if (ok)
        return args.at(1);
    return jsNull();
}

JSValue JSNode::removeChild(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    bool ok = impl()->removeChild(toNode(args.at(0)), ec);
    setDOMException(exec, ec);
    if (ok)
        return args.at(0);
    return jsNull();
}

JSValue JSNode::appendChild(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    bool ok = impl()->appendChild(toNode(args.at(0)), ec, true);
    setDOMException(exec, ec);
    if (ok)
        return args.at(0);
    return jsNull();
}

JSValue JSNode::addEventListener(ExecState* exec, const ArgList& args)
{
    JSValue listener = args.at(1);
    if (!listener.isObject())
        return jsUndefined();

    impl()->addEventListener(args.at(0).toString(exec), JSEventListener::create(asObject(listener), false, currentWorld(exec)), args.at(2).toBoolean(exec));
    return jsUndefined();
}

JSValue JSNode::removeEventListener(ExecState* exec, const ArgList& args)
{
    JSValue listener = args.at(1);
    if (!listener.isObject())
        return jsUndefined();

    impl()->removeEventListener(args.at(0).toString(exec), JSEventListener::create(asObject(listener), false, currentWorld(exec)).get(), args.at(2).toBoolean(exec));
    return jsUndefined();
}

void JSNode::pushEventHandlerScope(ExecState*, ScopeChain&) const
{
}

void JSNode::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);

    Node* node = m_impl.get();
    node->markEventListeners(markStack);

    // Nodes in the document are kept alive by JSDocument::mark, so, if we're in
    // the document, we need to mark the document, but we don't need to explicitly
    // mark any other nodes.
    if (node->inDocument()) {
        if (Document* doc = node->ownerDocument())
            markDOMObjectWrapper(markStack, *Heap::heap(this)->globalData(), doc);
        return;
    }

    // This is a node outside the document, so find the root of the tree it is in,
    // and start marking from there.
    Node* root = node;
    for (Node* current = m_impl.get(); current; current = current->parentNode())
        root = current;

    // Nodes in a subtree are marked by the tree's root, so, if the root is already
    // marking the tree, we don't need to explicitly mark any other nodes.
    if (root->inSubtreeMark())
        return;

    // Mark the whole tree subtree.
    root->setInSubtreeMark(true);
    for (Node* nodeToMark = root; nodeToMark; nodeToMark = nodeToMark->traverseNextNode())
        markDOMNodeWrapper(markStack, m_impl->document(), nodeToMark);
    root->setInSubtreeMark(false);
}

static ALWAYS_INLINE JSValue createWrapper(ExecState* exec, JSDOMGlobalObject* globalObject, Node* node)
{
    ASSERT(node);
    ASSERT(!getCachedDOMNodeWrapper(node->document(), node));
    
    JSNode* wrapper;    
    switch (node->nodeType()) {
        case Node::ELEMENT_NODE:
            if (node->isHTMLElement())
                wrapper = createJSHTMLWrapper(exec, globalObject, static_cast<HTMLElement*>(node));
#if ENABLE(SVG)
            else if (node->isSVGElement())
                wrapper = createJSSVGWrapper(exec, globalObject, static_cast<SVGElement*>(node));
#endif
            else
                wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, Element, node);
            break;
        case Node::ATTRIBUTE_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, Attr, node);
            break;
        case Node::TEXT_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, Text, node);
            break;
        case Node::CDATA_SECTION_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, CDATASection, node);
            break;
        case Node::ENTITY_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, Entity, node);
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, ProcessingInstruction, node);
            break;
        case Node::COMMENT_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, Comment, node);
            break;
        case Node::DOCUMENT_NODE:
            // we don't want to cache the document itself in the per-document dictionary
            return toJS(exec, globalObject, static_cast<Document*>(node));
        case Node::DOCUMENT_TYPE_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, DocumentType, node);
            break;
        case Node::NOTATION_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, Notation, node);
            break;
        case Node::DOCUMENT_FRAGMENT_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, DocumentFragment, node);
            break;
        case Node::ENTITY_REFERENCE_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, EntityReference, node);
            break;
        default:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, globalObject, Node, node);
    }

    return wrapper;    
}
    
JSValue toJSNewlyCreated(ExecState* exec, JSDOMGlobalObject* globalObject, Node* node)
{
    if (!node)
        return jsNull();
    
    return createWrapper(exec, globalObject, node);
}
    
JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, Node* node)
{
    if (!node)
        return jsNull();

    JSNode* wrapper = getCachedDOMNodeWrapper(node->document(), node);
    if (wrapper)
        return wrapper;

    return createWrapper(exec, globalObject, node);
}

} // namespace WebCore
