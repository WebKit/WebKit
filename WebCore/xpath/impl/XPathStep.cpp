/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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

#if XPATH_SUPPORT

#include "XPathStep.h"

#include "Logging.h"
#include "Document.h"
#include "NamedAttrMap.h"
#include "Node.h"
#include "Text.h"
#include "XPathNSResolver.h"

namespace WebCore {
namespace XPath {

String Step::axisAsString(AxisType axis)
{
    switch (axis) {
        case AncestorAxis: return "ancestor";
        case AncestorOrSelfAxis: return "ancestor-or-self";
        case AttributeAxis: return "attribute";
        case ChildAxis: return "child";
        case DescendantAxis: return "descendant";
        case DescendantOrSelfAxis: return "descendant-or-self";
        case FollowingAxis: return "following";
        case FollowingSiblingAxis: return "following-sibling";
        case NamespaceAxis: return "namespace";
        case ParentAxis: return "parent";
        case PrecedingAxis: return "preceding";
        case PrecedingSiblingAxis: return "preceding-sibling";
        case SelfAxis: return "self";
    }
    return String();
}

Step::Step()
{
}

Step::Step(AxisType axis, const String& nodeTest, const Vector<Predicate*>& predicates)
    : m_axis(axis),
    m_nodeTest(nodeTest),
    m_predicates(predicates)
{
}

Step::~Step()
{
    deleteAllValues(m_predicates);
}

NodeVector Step::evaluate(Node* context) const
{
    LOG(XPath, "Evaluating step, axis='%s', nodetest='%s', %u predicates",
            axisAsString(m_axis).ascii(), m_nodeTest.ascii(), m_predicates.size());
    if (context->nodeType() == Node::ELEMENT_NODE)
        LOG(XPath, "Context node is an element called %s", context->nodeName().ascii());
    else if (context->nodeType() == Node::ATTRIBUTE_NODE)
        LOG(XPath, "Context node is an attribute called %s with value %s", 
               context->nodeName().ascii(), 
               context->nodeValue().ascii());
    else
        LOG(XPath, "Context node is of unknown type %d", context->nodeType());

    NodeVector inNodes = nodesInAxis(context), outNodes;
    LOG(XPath, "Axis %s matches %d nodes.", axisAsString(m_axis).ascii(), inNodes.size());

    inNodes = nodeTestMatches(inNodes);
    LOG(XPath, "Nodetest %s trims this number to %d", m_nodeTest.ascii(), inNodes.size());
    
    outNodes = inNodes;
    for (unsigned i = 0; i < m_predicates.size(); i++) {
        Predicate* predicate = m_predicates[i];

        outNodes.clear();
        Expression::evaluationContext().size = inNodes.size();
        Expression::evaluationContext().position = 1;
        for (unsigned j = 0; j < inNodes.size(); j++) {
            Node* node = inNodes[j].get();

            Expression::evaluationContext().node = node;
            EvaluationContext backupCtx = Expression::evaluationContext();
            if (predicate->evaluate())
                outNodes.append(node);

            Expression::evaluationContext() = backupCtx;
            ++Expression::evaluationContext().position;
        }

        LOG(XPath, "\tPredicate trims this number to %d", outNodes.size());

        inNodes = outNodes;
    }
    return outNodes;
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
        case ParentAxis:
            nodes.append(context->parentNode());
            return nodes;
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
            if (!attrs) {
                LOG(XPath, "Node::attributes() returned NULL!");
                return nodes;
            }

            for (unsigned long i = 0; i < attrs->length(); ++i) 
                nodes.append (attrs->item(i));
            return nodes;
        }
        case NamespaceAxis: {
            if (context->nodeType() != Node::ELEMENT_NODE)
                return NodeVector();

            bool foundXmlNsNode = false;
            for (Node* node = context; node; node = node->parentNode()) {
                NamedAttrMap* attrs = node->attributes();
                if (!attrs) {
                    LOG(XPath, "Node::attributes() returned NULL!");

                    continue;
                }

                for (unsigned long i = 0; i < attrs->length(); ++i) {
                    Node* n = attrs->item(i).get();
                    if (n->nodeName().startsWith("xmlns:")) {
                        nodes.append(n);
                    } else if (n->nodeName() == "xmlns" &&
                               !foundXmlNsNode) {
                        foundXmlNsNode = true;
                        if (!n->nodeValue().isEmpty())
                            nodes.append(n);
                    }
                }
            }
            return nodes;
        }
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

    return NodeVector();
}


NodeVector Step::nodeTestMatches(const NodeVector& nodes) const
{
    String ns = namespaceFromNodetest(m_nodeTest);
    NodeVector matches;
    if (m_nodeTest == "*") {
        for (unsigned i = 0; i < nodes.size(); i++) {
            Node* node = nodes[i].get();
            if (node->nodeType() == primaryNodeType(m_axis)) {
                if (ns.isEmpty() ||
                     node->namespaceURI() == ns) {
                    matches.append(node);
                }
            }
        }
        return nodes;
    } else if (m_nodeTest == "text()") {
        for (unsigned i = 0; i < nodes.size(); i++) {
            Node* node = nodes[i].get();
            if (node->nodeType() == Node::TEXT_NODE ||
                node->nodeType() == Node::CDATA_SECTION_NODE) 
                matches.append(node);
        }
        return matches;
    } else if (m_nodeTest == "comment()") {
        for (unsigned i = 0; i < nodes.size(); i++) {
            Node* node = nodes[i].get();
            if (node->nodeType() == Node::COMMENT_NODE)
                matches.append(node);
        }
        return matches;
    } else if (m_nodeTest.startsWith("processing-instruction")) {
        String param;

        const int space = m_nodeTest.find(' ');
        if (space > -1)
            param = m_nodeTest.deprecatedString().mid(space + 1);

        for (unsigned i = 0; i < nodes.size(); i++) {
            Node* node = nodes[i].get();

            if (node->nodeType() == Node::PROCESSING_INSTRUCTION_NODE &&
                (param.isEmpty() || node->nodeName() == param))
                    matches.append(node);
        }    
        return matches;
    } else if (m_nodeTest == "node()")
        return nodes;
    else {
        String prefix, localName;

        const int colon = m_nodeTest.find(':');
        if (colon > -1) {
            prefix = m_nodeTest.left(colon);
            localName = m_nodeTest.deprecatedString().mid(colon + 1);
        } else {
            localName = m_nodeTest;
        }

        if (!prefix.isEmpty() &&
            Expression::evaluationContext().resolver->lookupNamespaceURI(prefix).isEmpty()) {
                /* FIXME: Throw NAMESPACE_ERR exception */
        }

        if (m_axis == AttributeAxis) {
            // In XPath land, namespace nodes are not accessible
            // on the attribute axis.
            if (localName == "xmlns")
                return matches;

            for (unsigned i = 0; i < nodes.size(); i++) {
                Node* node = nodes[i].get();
                
                if (node->nodeName() == localName) {
                    matches.append(node);
                    break; // There can only be one.
                }
            }

            return matches;
        } else if (m_axis == NamespaceAxis) {
            LOG(XPath, "Node test %s on axis %s is not implemented yet.",
                m_nodeTest.ascii(), axisAsString(m_axis).ascii());
        } else {
            for (unsigned i = 0; i < nodes.size(); i++) {
                Node* node = nodes[i].get();
 
                // We use tagQName here because we don't want the element name in uppercase 
                // like we get with HTML elements.
                if (node->nodeType() == Node::ELEMENT_NODE &&
                    static_cast<Element*>(node)->tagQName().toString() == localName) {
                    matches.append(node);
                }
            }

            return matches;
        }
    }
    
    LOG(XPath, "Node test %s on axis %s is not implemented yet.",
        m_nodeTest.ascii(), axisAsString(m_axis).ascii());

    return matches;
}

void Step::optimize()
{
    for (unsigned i = 0; i < m_predicates.size(); i++)
        m_predicates[i]->optimize();
}

String Step::namespaceFromNodetest(const String& nodeTest) const
{
    int i = nodeTest.find(':');
    if (i == -1)
        return String();

    String prefix(nodeTest.left(i));
    
    Node* ctxNode = Expression::evaluationContext().node.get();
    return ctxNode->lookupNamespaceURI(prefix);
}

Node::NodeType Step::primaryNodeType(AxisType axis) const
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

#endif // XPATH_SUPPORT
