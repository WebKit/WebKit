/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(impl()->scriptExecutionContext());
    if (!globalObject)
        return jsUndefined();

    if (RefPtr<JSEventListener> listener = globalObject->findOrCreateJSEventListener(args.at(1)))
        impl()->addEventListener(args.at(0).toString(exec), listener.release(), args.at(2).toBoolean(exec));

    return jsUndefined();
}

JSValue JSNode::removeEventListener(ExecState* exec, const ArgList& args)
{
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(impl()->scriptExecutionContext());
    if (!globalObject)
        return jsUndefined();

    if (JSEventListener* listener = globalObject->findJSEventListener(args.at(1)))
        impl()->removeEventListener(args.at(0).toString(exec), listener, args.at(2).toBoolean(exec));

    return jsUndefined();
}

void JSNode::pushEventHandlerScope(ExecState*, ScopeChain&) const
{
}

void JSNode::mark()
{
    ASSERT(!marked());

    Node* node = m_impl.get();

    // Nodes in the document are kept alive by JSDocument::mark, so, if we're in
    // the document, we need to mark the document, but we don't need to explicitly
    // mark any other nodes.
    if (node->inDocument()) {
        DOMObject::mark();
        markEventListeners(node->eventListeners());
        if (Document* doc = node->ownerDocument())
            if (DOMObject* docWrapper = getCachedDOMObjectWrapper(*Heap::heap(this)->globalData(), doc))
                if (!docWrapper->marked())
                    docWrapper->mark();
        return;
    }

    // This is a node outside the document, so find the root of the tree it is in,
    // and start marking from there.
    Node* root = node;
    for (Node* current = m_impl.get(); current; current = current->parentNode())
        root = current;

    // Nodes in a subtree are marked by the tree's root, so, if the root is already
    // marking the tree, we don't need to explicitly mark any other nodes.
    if (root->inSubtreeMark()) {
        DOMObject::mark();
        markEventListeners(node->eventListeners());
        return;
    }

    // Mark the whole tree subtree.
    root->setInSubtreeMark(true);
    for (Node* nodeToMark = root; nodeToMark; nodeToMark = nodeToMark->traverseNextNode()) {
        JSNode* wrapper = getCachedDOMNodeWrapper(m_impl->document(), nodeToMark);
        if (wrapper) {
            if (!wrapper->marked())
                wrapper->mark();
        } else if (nodeToMark == node) {
            // This is the case where the map from the document to wrappers has
            // been cleared out, but a wrapper is being marked. For now, we'll
            // let the rest of the tree of wrappers get collected, because we have
            // no good way of finding them. Later we should test behavior of other
            // browsers and see if we need to preserve other wrappers in this case.
            if (!marked())
                mark();
        }
    }
    root->setInSubtreeMark(false);

    // Double check that we actually ended up marked. This assert caught problems in the past.
    ASSERT(marked());
}

static ALWAYS_INLINE JSValue createWrapper(ExecState* exec, Node* node)
{
    ASSERT(node);
    ASSERT(!getCachedDOMNodeWrapper(node->document(), node));
    
    JSNode* wrapper;    
    switch (node->nodeType()) {
        case Node::ELEMENT_NODE:
            if (node->isHTMLElement())
                wrapper = createJSHTMLWrapper(exec, static_cast<HTMLElement*>(node));
#if ENABLE(SVG)
            else if (node->isSVGElement())
                wrapper = createJSSVGWrapper(exec, static_cast<SVGElement*>(node));
#endif
            else
                wrapper = CREATE_DOM_NODE_WRAPPER(exec, Element, node);
            break;
        case Node::ATTRIBUTE_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, Attr, node);
            break;
        case Node::TEXT_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, Text, node);
            break;
        case Node::CDATA_SECTION_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, CDATASection, node);
            break;
        case Node::ENTITY_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, Entity, node);
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, ProcessingInstruction, node);
            break;
        case Node::COMMENT_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, Comment, node);
            break;
        case Node::DOCUMENT_NODE:
            // we don't want to cache the document itself in the per-document dictionary
            return toJS(exec, static_cast<Document*>(node));
        case Node::DOCUMENT_TYPE_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, DocumentType, node);
            break;
        case Node::NOTATION_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, Notation, node);
            break;
        case Node::DOCUMENT_FRAGMENT_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, DocumentFragment, node);
            break;
        case Node::ENTITY_REFERENCE_NODE:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, EntityReference, node);
            break;
        default:
            wrapper = CREATE_DOM_NODE_WRAPPER(exec, Node, node);
    }

    return wrapper;    
}
    
JSValue toJSNewlyCreated(ExecState* exec, Node* node)
{
    if (!node)
        return jsNull();
    
    return createWrapper(exec, node);
}
    
JSValue toJS(ExecState* exec, Node* node)
{
    if (!node)
        return jsNull();

    JSNode* wrapper = getCachedDOMNodeWrapper(node->document(), node);
    if (wrapper)
        return wrapper;

    return createWrapper(exec, node);
}

} // namespace WebCore
