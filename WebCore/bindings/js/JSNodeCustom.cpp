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
#include "JSHTMLElement.h"
#include "JSHTMLElementWrapperFactory.h"
#include "JSNotation.h"
#include "JSProcessingInstruction.h"
#include "JSText.h"
#include "Node.h"
#include "Notation.h"
#include "ProcessingInstruction.h"
#include "Text.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

#if ENABLE(SVG)
#include "JSSVGElementWrapperFactory.h"
#include "SVGElement.h"
#endif

using namespace KJS;

namespace WebCore {

typedef int ExpectionCode;

JSValue* JSNode::insertBefore(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    bool ok = impl()->insertBefore(toNode(args[0]), toNode(args[1]), ec);
    setDOMException(exec, ec);
    if (ok)
        return args[0];
    return jsNull();
}

JSValue* JSNode::replaceChild(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    bool ok = impl()->replaceChild(toNode(args[0]), toNode(args[1]), ec);
    setDOMException(exec, ec);
    if (ok)
        return args[1];
    return jsNull();
}

JSValue* JSNode::removeChild(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    bool ok = impl()->removeChild(toNode(args[0]), ec);
    setDOMException(exec, ec);
    if (ok)
        return args[0];
    return jsNull();
}

JSValue* JSNode::appendChild(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    bool ok = impl()->appendChild(toNode(args[0]), ec);
    setDOMException(exec, ec);
    if (ok)
        return args[0];
    return jsNull();
}

void JSNode::mark()
{
    ASSERT(!marked());

    Node* node = m_impl.get();

    // Nodes in the document are kept alive by ScriptInterpreter::mark,
    // so we have no special responsibilities and can just call the base class here.
    if (node->inDocument()) {
        DOMObject::mark();
        return;
    }

    // This is a node outside the document, so find the root of the tree it is in,
    // and start marking from there.
    Node* root = node;
    for (Node* current = m_impl.get(); current; current = current->parentNode())
        root = current;

    // If we're already marking this tree, then we can simply mark this wrapper
    // by calling the base class; our caller is iterating the tree.
    if (root->m_inSubtreeMark) {
        DOMObject::mark();
        return;
    }

    // Mark the whole tree; use the global set of roots to avoid reentering.
    root->m_inSubtreeMark = true;
    for (Node* nodeToMark = root; nodeToMark; nodeToMark = nodeToMark->traverseNextNode()) {
        JSNode* wrapper = ScriptInterpreter::getDOMNodeForDocument(m_impl->document(), nodeToMark);
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
    root->m_inSubtreeMark = false;

    // Double check that we actually ended up marked. This assert caught problems in the past.
    ASSERT(marked());
}

JSValue* toJS(ExecState* exec, PassRefPtr<Node> n)
{
    Node* node = n.get(); 
    if (!node)
        return jsNull();

    Document* doc = node->document();
    JSNode* ret = ScriptInterpreter::getDOMNodeForDocument(doc, node);
    if (ret)
        return ret;

    switch (node->nodeType()) {
        case Node::ELEMENT_NODE:
            if (node->isHTMLElement())
                ret = createJSHTMLWrapper(exec, static_pointer_cast<HTMLElement>(n));
#if ENABLE(SVG)
            else if (node->isSVGElement())
                ret = createJSSVGWrapper(exec, static_pointer_cast<SVGElement>(n));
#endif
            else
                ret = new JSElement(JSElementPrototype::self(exec), static_cast<Element*>(node));
            break;
        case Node::ATTRIBUTE_NODE:
            ret = new JSAttr(JSAttrPrototype::self(exec), static_cast<Attr*>(node));
            break;
        case Node::TEXT_NODE:
            ret = new JSText(JSTextPrototype::self(exec), static_cast<Text*>(node));
            break;
        case Node::CDATA_SECTION_NODE:
            ret = new JSCDATASection(JSCDATASectionPrototype::self(exec), static_cast<CDATASection*>(node));
            break;
        case Node::ENTITY_NODE:
            ret = new JSEntity(JSEntityPrototype::self(exec), static_cast<Entity*>(node));
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
            ret = new JSProcessingInstruction(JSProcessingInstructionPrototype::self(exec), static_cast<ProcessingInstruction*>(node));
            break;
        case Node::COMMENT_NODE:
            ret = new JSComment(JSCommentPrototype::self(exec), static_cast<Comment*>(node));
            break;
        case Node::DOCUMENT_NODE:
            // we don't want to cache the document itself in the per-document dictionary
            return toJS(exec, static_cast<Document*>(node));
        case Node::DOCUMENT_TYPE_NODE:
            ret = new JSDocumentType(JSDocumentTypePrototype::self(exec), static_cast<DocumentType*>(node));
            break;
        case Node::NOTATION_NODE:
            ret = new JSNotation(JSNotationPrototype::self(exec), static_cast<Notation*>(node));
            break;
        case Node::DOCUMENT_FRAGMENT_NODE:
            ret = new JSDocumentFragment(JSDocumentFragmentPrototype::self(exec), static_cast<DocumentFragment*>(node));
            break;
        case Node::ENTITY_REFERENCE_NODE:
            ret = new JSEntityReference(JSEntityReferencePrototype::self(exec), static_cast<EntityReference*>(node));
            break;
        default:
            ret = new JSNode(JSNodePrototype::self(exec), node);
    }

    ScriptInterpreter::putDOMNodeForDocument(doc, node, ret);

    return ret;
}

} // namespace WebCore
