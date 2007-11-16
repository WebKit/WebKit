/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XPathStep.h"

#if ENABLE(XPATH)

#include "Document.h"
#include "Element.h"
#include "NamedAttrMap.h"
#include "XPathNSResolver.h"
#include "XPathParser.h"
#include "XPathUtil.h"

namespace WebCore {
namespace XPath {

Step::Step(Axis axis, const NodeTest& nodeTest, const Vector<Predicate*>& predicates)
    : m_axis(axis)
    , m_nodeTest(nodeTest)
    , m_predicates(predicates)
{
}

Step::~Step()
{
    deleteAllValues(m_predicates);
}

void Step::evaluate(Node* context, NodeSet& nodes) const
{
    nodesInAxis(context, nodes);
    
    EvaluationContext& evaluationContext = Expression::evaluationContext();
    
    for (unsigned i = 0; i < m_predicates.size(); i++) {
        Predicate* predicate = m_predicates[i];

        NodeSet newNodes;
        if (!nodes.isSorted())
            newNodes.markSorted(false);

        for (unsigned j = 0; j < nodes.size(); j++) {
            Node* node = nodes[j];

            Expression::evaluationContext().node = node;
            evaluationContext.size = nodes.size();
            evaluationContext.position = j + 1;
            if (predicate->evaluate())
                newNodes.append(node);
        }

        nodes.swap(newNodes);
    }
}

void Step::nodesInAxis(Node* context, NodeSet& nodes) const
{
    ASSERT(nodes.isEmpty());
    switch (m_axis) {
        case ChildAxis:
            if (context->isAttributeNode()) // In XPath model, attribute nodes do not have children.
                return;

            for (Node* n = context->firstChild(); n; n = n->nextSibling())
                if (nodeMatches(n))
                    nodes.append(n);
            return;
        case DescendantAxis:
            if (context->isAttributeNode()) // In XPath model, attribute nodes do not have children.
                return;

            for (Node* n = context->firstChild(); n; n = n->traverseNextNode(context))
                if (nodeMatches(n))
                    nodes.append(n);
            return;
        case ParentAxis:
            if (context->isAttributeNode()) {
                Node* n = static_cast<Attr*>(context)->ownerElement();
                if (nodeMatches(n))
                    nodes.append(n);
            } else {
                Node* n = context->parentNode();
                if (n && nodeMatches(n))
                    nodes.append(n);
            }
            return;
        case AncestorAxis: {
            Node* n = context;
            if (context->isAttributeNode()) {
                n = static_cast<Attr*>(context)->ownerElement();
                if (nodeMatches(n))
                    nodes.append(n);
            }
            for (n = n->parentNode(); n; n = n->parentNode())
                if (nodeMatches(n))
                    nodes.append(n);
            nodes.markSorted(false);
            return;
        }
        case FollowingSiblingAxis:
            if (context->nodeType() == Node::ATTRIBUTE_NODE ||
                 context->nodeType() == Node::XPATH_NAMESPACE_NODE) 
                return;
            
            for (Node* n = context->nextSibling(); n; n = n->nextSibling())
                if (nodeMatches(n))
                    nodes.append(n);
            return;
        case PrecedingSiblingAxis:
            if (context->nodeType() == Node::ATTRIBUTE_NODE ||
                 context->nodeType() == Node::XPATH_NAMESPACE_NODE)
                return;
            
            for (Node* n = context->previousSibling(); n; n = n->previousSibling())
                if (nodeMatches(n))
                    nodes.append(n);

            nodes.markSorted(false);
            return;
        case FollowingAxis:
            if (context->isAttributeNode()) {
                Node* p = static_cast<Attr*>(context)->ownerElement();
                while ((p = p->traverseNextNode()))
                    if (nodeMatches(p))
                        nodes.append(p);
            } else {
                for (Node* p = context; !isRootDomNode(p); p = p->parentNode()) {
                    for (Node* n = p->nextSibling(); n; n = n->nextSibling()) {
                        if (nodeMatches(n))
                            nodes.append(n);
                        for (Node* c = n->firstChild(); c; c = c->traverseNextNode(n))
                            if (nodeMatches(c))
                                nodes.append(c);
                    }
                }
            }
            return;
        case PrecedingAxis: {
            if (context->isAttributeNode())
                context = static_cast<Attr*>(context)->ownerElement();

            Node* n = context;
            while (Node* parent = n->parent()) {
                for (n = n->traversePreviousNode(); n != parent; n = n->traversePreviousNode())
                    if (nodeMatches(n))
                        nodes.append(n);
                n = parent;
            }
            nodes.markSorted(false);
            return;
        }
        case AttributeAxis: {
            if (context->nodeType() != Node::ELEMENT_NODE)
                return;

            // Avoid lazily creating attribute nodes for attributes that we do not need anyway.
            if (m_nodeTest.kind() == NodeTest::NameTest && m_nodeTest.data() != "*") {
                RefPtr<Node> n = static_cast<Element*>(context)->getAttributeNodeNS(m_nodeTest.namespaceURI(), m_nodeTest.data());
                if (n && n->namespaceURI() != "http://www.w3.org/2000/xmlns/") // In XPath land, namespace nodes are not accessible on the attribute axis.
                    nodes.append(n.release());
                return;
            }
            
            NamedAttrMap* attrs = context->attributes();
            if (!attrs)
                return;

            for (unsigned long i = 0; i < attrs->length(); ++i) {
                RefPtr<Node> n = attrs->item(i);
                if (nodeMatches(n.get()))
                    nodes.append(n.release());
            }
            return;
        }
        case NamespaceAxis:
            // XPath namespace nodes are not implemented yet.
            return;
        case SelfAxis:
            if (nodeMatches(context))
                nodes.append(context);
            return;
        case DescendantOrSelfAxis:
            if (nodeMatches(context))
                nodes.append(context);
            if (context->isAttributeNode()) // In XPath model, attribute nodes do not have children.
                return;

            for (Node* n = context->firstChild(); n; n = n->traverseNextNode(context))
            if (nodeMatches(n))
                nodes.append(n);
            return;
        case AncestorOrSelfAxis: {
            if (nodeMatches(context))
                nodes.append(context);
            Node* n = context;
            if (context->isAttributeNode()) {
                n = static_cast<Attr*>(context)->ownerElement();
                if (nodeMatches(n))
                    nodes.append(n);
            }
            for (n = n->parentNode(); n; n = n->parentNode())
                if (nodeMatches(n))
                    nodes.append(n);

            nodes.markSorted(false);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}


bool Step::nodeMatches(Node* node) const
{
    switch (m_nodeTest.kind()) {
        case NodeTest::TextNodeTest:
            return node->nodeType() == Node::TEXT_NODE || node->nodeType() == Node::CDATA_SECTION_NODE;
        case NodeTest::CommentNodeTest:
            return node->nodeType() == Node::COMMENT_NODE;
        case NodeTest::ProcessingInstructionNodeTest: {
            const String& name = m_nodeTest.data();
            return node->nodeType() == Node::PROCESSING_INSTRUCTION_NODE && (name.isEmpty() || node->nodeName() == name);
        }
        case NodeTest::ElementNodeTest:
            return node->isElementNode();
        case NodeTest::AnyNodeTest:
            return true;
        case NodeTest::NameTest: {
            const String& name = m_nodeTest.data();
            const String& namespaceURI = m_nodeTest.namespaceURI();

            if (m_axis == AttributeAxis) {
                ASSERT(node->isAttributeNode());

                // In XPath land, namespace nodes are not accessible on the attribute axis.
                if (node->namespaceURI() == "http://www.w3.org/2000/xmlns/")
                    return false;

                if (name == "*")
                    return namespaceURI.isEmpty() || node->namespaceURI() == namespaceURI;

                return node->localName() == name && node->namespaceURI() == namespaceURI;
            }

            if (m_axis == NamespaceAxis) {
                // Node test on the namespace axis is not implemented yet
                return false;
            }
            
            if (name == "*")
                return node->nodeType() == primaryNodeType(m_axis) && (namespaceURI.isEmpty() || namespaceURI == node->namespaceURI());

            // We use tagQName here because we don't want the element name in uppercase 
            // like we get with HTML elements.
            // Paths without namespaces should match HTML elements in HTML documents despite those having an XHTML namespace.
            return node->nodeType() == Node::ELEMENT_NODE
                    && static_cast<Element*>(node)->tagQName().localName() == name
                    && ((node->isHTMLElement() && node->document()->isHTMLDocument() && namespaceURI.isNull()) || namespaceURI == node->namespaceURI());
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

Node::NodeType Step::primaryNodeType(Axis axis) const
{
    switch (axis) {
        case AttributeAxis:
            return Node::ATTRIBUTE_NODE;
        case NamespaceAxis:
            return Node::XPATH_NAMESPACE_NODE;
        default:
            return Node::ELEMENT_NODE;
    }
}

}
}

#endif // ENABLE(XPATH)
