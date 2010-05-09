/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
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
#include "ExceptionCode.h"
#include "HTMLElement.h"
#include "JSAttr.h"
#include "JSCDATASection.h"
#include "JSComment.h"
#include "JSDOMBinding.h"
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

static inline bool isAttrFrameSrc(Element *element, const String& name)
{
    return element && (element->hasTagName(HTMLNames::iframeTag) || element->hasTagName(HTMLNames::frameTag)) && equalIgnoringCase(name, "src");
}

void JSNode::setNodeValue(JSC::ExecState* exec, JSC::JSValue value)
{
    Node* imp = static_cast<Node*>(impl());
    String nodeValue = valueToStringWithNullCheck(exec, value);

    if (imp->nodeType() == Node::ATTRIBUTE_NODE) {
        Element* ownerElement = static_cast<Attr*>(impl())->ownerElement();
        if (ownerElement && !allowSettingSrcToJavascriptURL(exec, ownerElement, imp->nodeName(), nodeValue))
            return;
    }

    ExceptionCode ec = 0;
    imp->setNodeValue(nodeValue, ec);
    setDOMException(exec, ec);
}

void JSNode::setTextContent(JSC::ExecState* exec, JSC::JSValue value)
{
    Node* imp = static_cast<Node*>(impl());
    String nodeValue = valueToStringWithNullCheck(exec, value);

    if (imp->nodeType() == Node::ATTRIBUTE_NODE) {
        Element* ownerElement = static_cast<Attr*>(impl())->ownerElement();
        if (ownerElement && !allowSettingSrcToJavascriptURL(exec, ownerElement, imp->nodeName(), nodeValue))
            return;
    }

    ExceptionCode ec = 0;
    imp->setTextContent(nodeValue, ec);
    setDOMException(exec, ec);
}

JSValue JSNode::insertBefore(ExecState* exec, const ArgList& args)
{
    Node* imp = static_cast<Node*>(impl());
    if (imp->nodeType() == Node::ATTRIBUTE_NODE && isAttrFrameSrc(static_cast<Attr*>(impl())->ownerElement(), imp->nodeName())) {
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsNull();
    }

    ExceptionCode ec = 0;
    bool ok = imp->insertBefore(toNode(args.at(0)), toNode(args.at(1)), ec, true);
    setDOMException(exec, ec);
    if (ok)
        return args.at(0);
    return jsNull();
}

JSValue JSNode::replaceChild(ExecState* exec, const ArgList& args)
{
    Node* imp = static_cast<Node*>(impl());
    if (imp->nodeType() == Node::ATTRIBUTE_NODE && isAttrFrameSrc(static_cast<Attr*>(impl())->ownerElement(), imp->nodeName())) {
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsNull();
    }

    ExceptionCode ec = 0;
    bool ok = imp->replaceChild(toNode(args.at(0)), toNode(args.at(1)), ec, true);
    setDOMException(exec, ec);
    if (ok)
        return args.at(1);
    return jsNull();
}

JSValue JSNode::removeChild(ExecState* exec, const ArgList& args)
{
    Node* imp = static_cast<Node*>(impl());
    if (imp->nodeType() == Node::ATTRIBUTE_NODE && isAttrFrameSrc(static_cast<Attr*>(impl())->ownerElement(), imp->nodeName())) {
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsNull();
    }

    ExceptionCode ec = 0;
    bool ok = imp->removeChild(toNode(args.at(0)), ec);
    setDOMException(exec, ec);
    if (ok)
        return args.at(0);
    return jsNull();
}

JSValue JSNode::appendChild(ExecState* exec, const ArgList& args)
{
    Node* imp = static_cast<Node*>(impl());
    if (imp->nodeType() == Node::ATTRIBUTE_NODE && isAttrFrameSrc(static_cast<Attr*>(impl())->ownerElement(), imp->nodeName())) {
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsNull();
    }

    ExceptionCode ec = 0;
    bool ok = imp->appendChild(toNode(args.at(0)), ec, true);
    setDOMException(exec, ec);
    if (ok)
        return args.at(0);
    return jsNull();
}

void JSNode::pushEventHandlerScope(ExecState*, ScopeChain&) const
{
}

void JSNode::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);

    Node* node = m_impl.get();
    node->markJSEventListeners(markStack);

    // Nodes in the document are kept alive by JSDocument::mark, so, if we're in
    // the document, we need to mark the document, but we don't need to explicitly
    // mark any other nodes.
    if (node->inDocument()) {
        if (Document* doc = node->ownerDocument())
            markDOMNodeWrapper(markStack, doc, doc);
        return;
    }

    // This is a node outside the document.
    // Find the the root, and the highest ancestor with a wrapper.
    Node* root = node;
    Node* outermostNodeWithWrapper = node;
    for (Node* current = m_impl.get(); current; current = current->parentNode()) {
        root = current;
        if (hasCachedDOMNodeWrapperUnchecked(current->document(), current))
            outermostNodeWithWrapper = current;
    }

    // Only nodes that have no ancestors with wrappers mark the subtree. In the common
    // case, the root of the detached subtree has a wrapper, so the tree will only
    // get marked once. Nodes that aren't outermost need to mark the outermost
    // in case it is otherwise unreachable.
    // FIXME: In the non-common case of root not having a wrapper, this is still an O(n^2) algorithm,
    // as we will traverse the whole tree as many times as there are nodes with wrappers in it.
    if (node != outermostNodeWithWrapper) {
        markDOMNodeWrapper(markStack, m_impl->document(), outermostNodeWithWrapper);
        return;
    }

    // Mark the whole tree subtree.
    for (Node* nodeToMark = root; nodeToMark; nodeToMark = nodeToMark->traverseNextNode())
        markDOMNodeWrapper(markStack, m_impl->document(), nodeToMark);
}

static ALWAYS_INLINE JSValue createWrapperInline(ExecState* exec, JSDOMGlobalObject* globalObject, Node* node)
{
    ASSERT(node);
    ASSERT(!getCachedDOMNodeWrapper(exec, node->document(), node));
    
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

JSValue createWrapper(ExecState* exec, JSDOMGlobalObject* globalObject, Node* node)
{
    return createWrapperInline(exec, globalObject, node);
}
    
JSValue toJSNewlyCreated(ExecState* exec, JSDOMGlobalObject* globalObject, Node* node)
{
    if (!node)
        return jsNull();
    
    return createWrapperInline(exec, globalObject, node);
}

} // namespace WebCore
