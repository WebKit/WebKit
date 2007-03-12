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
#include "NamedAttrMap.h"
#include "XPathNSResolver.h"
#include "XPathParser.h"

namespace WebCore {
namespace XPath {

Step::Step(Axis axis, const NodeTest& nodeTest, const Vector<Predicate*>& predicates)
    : m_axis(axis)
    , m_nodeTest(nodeTest)
    , m_predicates(predicates)
{
}

Step::Step(Axis axis, const NodeTest& nodeTest, const String& namespaceURI, const Vector<Predicate*>& predicates)
    : m_axis(axis)
    , m_nodeTest(nodeTest)
    , m_namespaceURI(namespaceURI)
    , m_predicates(predicates)
{
}

Step::~Step()
{
    deleteAllValues(m_predicates);
}

NodeVector Step::evaluate(Node* context) const
{
    NodeVector nodes = nodesInAxis(context);
    nodes = nodeTestMatches(nodes);
    
    EvaluationContext& evaluationContext = Expression::evaluationContext();
    
    for (unsigned i = 0; i < m_predicates.size(); i++) {
        Predicate* predicate = m_predicates[i];

        NodeVector newNodes;
        evaluationContext.size = nodes.size();
        evaluationContext.position = 1;
        for (unsigned j = 0; j < nodes.size(); j++) {
            Node* node = nodes[j].get();

            Expression::evaluationContext().node = node;
            EvaluationContext backupCtx = evaluationContext;
            if (predicate->evaluate())
                newNodes.append(node);

            evaluationContext = backupCtx;
            ++evaluationContext.position;
        }

        nodes.swap(newNodes);
    }
    return nodes;
}

NodeVector Step::nodesInAxis(Node* context) const
{
    NodeVector nodes;
    switch (m_axis) {
        case ChildAxis:
            for (Node* n = context->firstChild(); n; n = n->nextSibling())
                nodes.append(n);
            return nodes;
        case DescendantAxis: 
            for (Node* n = context->firstChild(); n; n = n->traverseNextNode(context))
                nodes.append(n);
            return nodes;
        case ParentAxis: {
            Node* parent = context->parentNode();
            if (parent)
                nodes.append(parent);
            return nodes;
        }
        case AncestorAxis:
            for (Node* n = context->parentNode(); n; n = n->parentNode())
                nodes.append(n);
            return nodes;
        case FollowingSiblingAxis:
            if (context->nodeType() == Node::ATTRIBUTE_NODE ||
                 context->nodeType() == Node::XPATH_NAMESPACE_NODE) 
                return NodeVector();
            
            for (Node* n = context->nextSibling(); n; n = n->nextSibling())
                nodes.append(n);
            return nodes;
        case PrecedingSiblingAxis:
            if (context->nodeType() == Node::ATTRIBUTE_NODE ||
                 context->nodeType() == Node::XPATH_NAMESPACE_NODE)
                return NodeVector();
            
            for (Node* n = context->previousSibling(); n; n = n->previousSibling())
                nodes.append(n);
            return nodes;
        case FollowingAxis: 
            for (Node *p = context; !isRootDomNode(p); p = p->parentNode()) {
                for (Node* n = p->nextSibling(); n; n = n->nextSibling()) {
                    nodes.append(n);
                    for (Node* c = n->firstChild(); c; c = c->traverseNextNode(n))
                        nodes.append(c);
                }
            }
            return nodes;
        case PrecedingAxis:
            for (Node* p = context; !isRootDomNode(p); p = p->parentNode()) {
                for (Node* n = p->previousSibling(); n ; n = n->previousSibling()) {
                    nodes.append(n);
                    for (Node* c = n->firstChild(); c; c = c->traverseNextNode(n))
                        nodes.append(c);
                }
            }
            return nodes;
        case AttributeAxis: {
            if (context->nodeType() != Node::ELEMENT_NODE)
                return NodeVector();

            NamedAttrMap* attrs = context->attributes();
            if (!attrs)
                return nodes;

            for (unsigned long i = 0; i < attrs->length(); ++i) 
                nodes.append (attrs->item(i));
            return nodes;
        }
        case NamespaceAxis:
            // XPath namespace nodes are not implemented yet.
            return NodeVector();
        case SelfAxis:
            nodes.append(context);
            return nodes;
        case DescendantOrSelfAxis:
            nodes.append(context);
            for (Node* n = context->firstChild(); n; n = n->traverseNextNode(context))
                nodes.append(n);
            return nodes;
        case AncestorOrSelfAxis:
            nodes.append(context);
            for (Node* n = context->parentNode(); n; n = n->parentNode())
                nodes.append(n);
            return nodes;
    }
    ASSERT_NOT_REACHED();
    return NodeVector();
}


NodeVector Step::nodeTestMatches(const NodeVector& nodes) const
{
    NodeVector matches;

    switch (m_nodeTest.kind()) {
        case NodeTest::TextNodeTest:
            for (unsigned i = 0; i < nodes.size(); i++) {
                Node* node = nodes[i].get();
                if ((node->nodeType() == Node::TEXT_NODE || node->nodeType() == Node::CDATA_SECTION_NODE))
                    matches.append(node);
            }
            return matches;
        case NodeTest::CommentNodeTest:
            for (unsigned i = 0; i < nodes.size(); i++) {
                Node* node = nodes[i].get();
                if (node->nodeType() == Node::COMMENT_NODE)
                    matches.append(node);
            }
            return matches;
        case NodeTest::ProcessingInstructionNodeTest:
            for (unsigned i = 0; i < nodes.size(); i++) {
                Node* node = nodes[i].get();
                const String& name = m_nodeTest.data();
                if (node->nodeType() == Node::PROCESSING_INSTRUCTION_NODE && (name.isEmpty() || node->nodeName() == name))
                        matches.append(node);
            }    
            return matches;
        case NodeTest::ElementNodeTest:
            for (unsigned i = 0; i < nodes.size(); i++) {
                Node* node = nodes[i].get();
                if (node->isElementNode())
                    matches.append(node);
            }
            return matches;
        case NodeTest::AnyNodeTest:
            return nodes;
        case NodeTest::NameTest: {
            const String& name = m_nodeTest.data();
            if (name == "*") {
                for (unsigned i = 0; i < nodes.size(); i++) {
                    Node* node = nodes[i].get();
                    if (node->nodeType() == primaryNodeType(m_axis) &&
                        (m_namespaceURI.isEmpty() || m_namespaceURI == node->namespaceURI()))
                        matches.append(node);
                }
                return matches;
            }
            if (m_axis == AttributeAxis) {
                // In XPath land, namespace nodes are not accessible
                // on the attribute axis.
                if (name == "xmlns")
                    return matches;

                for (unsigned i = 0; i < nodes.size(); i++) {
                    Node* node = nodes[i].get();
                    
                    if (node->nodeName() == name) {
                        matches.append(node);
                        break; // There can only be one.
                    }
                }

                return matches;
            } else if (m_axis == NamespaceAxis) {
                // Node test on the namespace axis is not implemented yet
            } else {
                for (unsigned i = 0; i < nodes.size(); i++) {
                    Node* node = nodes[i].get();

                    // We use tagQName here because we don't want the element name in uppercase 
                    // like we get with HTML elements.
                    // Paths without namespaces should match HTML elements in HTML documents despite those having an XHTML namespace.
                    if (node->nodeType() == Node::ELEMENT_NODE
                        && static_cast<Element*>(node)->tagQName().localName() == name
                        && ((node->isHTMLElement() && node->document()->isHTMLDocument() && m_namespaceURI.isNull()) || m_namespaceURI == node->namespaceURI()))
                        matches.append(node);
                }

                return matches;
            }
        }
    }
    ASSERT_NOT_REACHED();
    return NodeVector();
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
