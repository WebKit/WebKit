/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#include "Attr.h"
#include "Document.h"
#include "HTMLElement.h"
#include "NodeTraversal.h"
#include "XMLNSNames.h"
#include "XPathParser.h"
#include "XPathUtil.h"

namespace WebCore {
namespace XPath {

Step::Step(Axis axis, NodeTest nodeTest)
    : m_axis(axis)
    , m_nodeTest(WTF::move(nodeTest))
{
}

Step::Step(Axis axis, NodeTest nodeTest, Vector<std::unique_ptr<Expression>> predicates)
    : m_axis(axis)
    , m_nodeTest(WTF::move(nodeTest))
    , m_predicates(WTF::move(predicates))
{
}

Step::~Step()
{
}

void Step::optimize()
{
    // Evaluate predicates as part of node test if possible to avoid building unnecessary NodeSets.
    // E.g., there is no need to build a set of all "foo" nodes to evaluate "foo[@bar]", we can check the predicate while enumerating.
    // This optimization can be applied to predicates that are not context node list sensitive, or to first predicate that is only context position sensitive, e.g. foo[position() mod 2 = 0].
    Vector<std::unique_ptr<Expression>> remainingPredicates;
    for (size_t i = 0; i < m_predicates.size(); ++i) {
        auto& predicate = m_predicates[i];
        if ((!predicateIsContextPositionSensitive(*predicate) || m_nodeTest.m_mergedPredicates.isEmpty()) && !predicate->isContextSizeSensitive() && remainingPredicates.isEmpty())
            m_nodeTest.m_mergedPredicates.append(WTF::move(predicate));
        else
            remainingPredicates.append(WTF::move(predicate));
    }
    m_predicates = WTF::move(remainingPredicates);
}

void optimizeStepPair(Step& first, Step& second, bool& dropSecondStep)
{
    dropSecondStep = false;

    if (first.m_axis != Step::DescendantOrSelfAxis)
        return;

    if (first.m_nodeTest.m_kind != Step::NodeTest::AnyNodeTest)
        return;

    if (!first.m_predicates.isEmpty())
        return;

    if (!first.m_nodeTest.m_mergedPredicates.isEmpty())
        return;

    ASSERT(first.m_nodeTest.m_data.isEmpty());
    ASSERT(first.m_nodeTest.m_namespaceURI.isEmpty());

    // Optimize the common case of "//" AKA /descendant-or-self::node()/child::NodeTest to /descendant::NodeTest.
    if (second.m_axis != Step::ChildAxis)
        return;

    if (!second.predicatesAreContextListInsensitive())
        return;

    first.m_axis = Step::DescendantAxis;
    first.m_nodeTest = WTF::move(second.m_nodeTest);
    first.m_predicates = WTF::move(second.m_predicates);
    first.optimize();
    dropSecondStep = true;
}

bool Step::predicatesAreContextListInsensitive() const
{
    for (size_t i = 0; i < m_predicates.size(); ++i) {
        auto& predicate = *m_predicates[i];
        if (predicateIsContextPositionSensitive(predicate) || predicate.isContextSizeSensitive())
            return false;
    }

    for (size_t i = 0; i < m_nodeTest.m_mergedPredicates.size(); ++i) {
        auto& predicate = *m_nodeTest.m_mergedPredicates[i];
        if (predicateIsContextPositionSensitive(predicate) || predicate.isContextSizeSensitive())
            return false;
    }

    return true;
}

void Step::evaluate(Node& context, NodeSet& nodes) const
{
    EvaluationContext& evaluationContext = Expression::evaluationContext();
    evaluationContext.position = 0;

    nodesInAxis(context, nodes);

    // Check predicates that couldn't be merged into node test.
    for (unsigned i = 0; i < m_predicates.size(); i++) {
        auto& predicate = *m_predicates[i];

        NodeSet newNodes;
        if (!nodes.isSorted())
            newNodes.markSorted(false);

        for (unsigned j = 0; j < nodes.size(); j++) {
            Node* node = nodes[j];

            evaluationContext.node = node;
            evaluationContext.size = nodes.size();
            evaluationContext.position = j + 1;
            if (evaluatePredicate(predicate))
                newNodes.append(node);
        }

        nodes = WTF::move(newNodes);
    }
}

#if !ASSERT_DISABLED
static inline Node::NodeType primaryNodeType(Step::Axis axis)
{
    switch (axis) {
        case Step::AttributeAxis:
            return Node::ATTRIBUTE_NODE;
        case Step::NamespaceAxis:
            return Node::XPATH_NAMESPACE_NODE;
        default:
            return Node::ELEMENT_NODE;
    }
}
#endif

// Evaluate NodeTest without considering merged predicates.
inline bool nodeMatchesBasicTest(Node& node, Step::Axis axis, const Step::NodeTest& nodeTest)
{
    switch (nodeTest.m_kind) {
        case Step::NodeTest::TextNodeTest:
            return node.nodeType() == Node::TEXT_NODE || node.nodeType() == Node::CDATA_SECTION_NODE;
        case Step::NodeTest::CommentNodeTest:
            return node.nodeType() == Node::COMMENT_NODE;
        case Step::NodeTest::ProcessingInstructionNodeTest: {
            const AtomicString& name = nodeTest.m_data;
            return node.nodeType() == Node::PROCESSING_INSTRUCTION_NODE && (name.isEmpty() || node.nodeName() == name);
        }
        case Step::NodeTest::AnyNodeTest:
            return true;
        case Step::NodeTest::NameTest: {
            const AtomicString& name = nodeTest.m_data;
            const AtomicString& namespaceURI = nodeTest.m_namespaceURI;

            if (axis == Step::AttributeAxis) {
                ASSERT(node.isAttributeNode());

                // In XPath land, namespace nodes are not accessible on the attribute axis.
                if (node.namespaceURI() == XMLNSNames::xmlnsNamespaceURI)
                    return false;

                if (name == starAtom)
                    return namespaceURI.isEmpty() || node.namespaceURI() == namespaceURI;

                return node.localName() == name && node.namespaceURI() == namespaceURI;
            }

            // Node test on the namespace axis is not implemented yet, the caller has a check for it.
            ASSERT(axis != Step::NamespaceAxis);

            // For other axes, the principal node type is element.
            ASSERT(primaryNodeType(axis) == Node::ELEMENT_NODE);
            if (!node.isElementNode())
                return false;

            if (name == starAtom)
                return namespaceURI.isEmpty() || namespaceURI == node.namespaceURI();

            if (node.document().isHTMLDocument()) {
                if (node.isHTMLElement()) {
                    // Paths without namespaces should match HTML elements in HTML documents despite those having an XHTML namespace. Names are compared case-insensitively.
                    return equalIgnoringCase(toHTMLElement(node).localName(), name) && (namespaceURI.isNull() || namespaceURI == node.namespaceURI());
                }
                // An expression without any prefix shouldn't match no-namespace nodes (because HTML5 says so).
                return toElement(node).hasLocalName(name) && namespaceURI == node.namespaceURI() && !namespaceURI.isNull();
            }
            return toElement(node).hasLocalName(name) && namespaceURI == node.namespaceURI();
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

inline bool nodeMatches(Node& node, Step::Axis axis, const Step::NodeTest& nodeTest)
{
    if (!nodeMatchesBasicTest(node, axis, nodeTest))
        return false;

    EvaluationContext& evaluationContext = Expression::evaluationContext();

    // Only the first merged predicate may depend on position.
    ++evaluationContext.position;

    auto& mergedPredicates = nodeTest.m_mergedPredicates;
    for (unsigned i = 0; i < mergedPredicates.size(); i++) {
        // No need to set context size - we only get here when evaluating predicates that do not depend on it.
        evaluationContext.node = &node;
        if (!evaluatePredicate(*mergedPredicates[i]))
            return false;
    }

    return true;
}

// Result nodes are ordered in axis order. Node test (including merged predicates) is applied.
void Step::nodesInAxis(Node& context, NodeSet& nodes) const
{
    ASSERT(nodes.isEmpty());
    switch (m_axis) {
        case ChildAxis:
            if (context.isAttributeNode()) // In XPath model, attribute nodes do not have children.
                return;
            for (Node* node = context.firstChild(); node; node = node->nextSibling()) {
                if (nodeMatches(*node, ChildAxis, m_nodeTest))
                    nodes.append(node);
            }
            return;
        case DescendantAxis:
            if (context.isAttributeNode()) // In XPath model, attribute nodes do not have children.
                return;
            for (Node* node = context.firstChild(); node; node = NodeTraversal::next(node, &context)) {
                if (nodeMatches(*node, DescendantAxis, m_nodeTest))
                    nodes.append(node);
            }
            return;
        case ParentAxis:
            if (context.isAttributeNode()) {
                Element* node = static_cast<Attr&>(context).ownerElement();
                if (nodeMatches(*node, ParentAxis, m_nodeTest))
                    nodes.append(node);
            } else {
                ContainerNode* node = context.parentNode();
                if (node && nodeMatches(*node, ParentAxis, m_nodeTest))
                    nodes.append(node);
            }
            return;
        case AncestorAxis: {
            Node* node = &context;
            if (context.isAttributeNode()) {
                node = static_cast<Attr&>(context).ownerElement();
                if (nodeMatches(*node, AncestorAxis, m_nodeTest))
                    nodes.append(node);
            }
            for (node = node->parentNode(); node; node = node->parentNode()) {
                if (nodeMatches(*node, AncestorAxis, m_nodeTest))
                    nodes.append(node);
            }
            nodes.markSorted(false);
            return;
        }
        case FollowingSiblingAxis:
            if (context.nodeType() == Node::ATTRIBUTE_NODE || context.nodeType() == Node::XPATH_NAMESPACE_NODE)
                return;
            for (Node* node = context.nextSibling(); node; node = node->nextSibling()) {
                if (nodeMatches(*node, FollowingSiblingAxis, m_nodeTest))
                    nodes.append(node);
            }
            return;
        case PrecedingSiblingAxis:
            if (context.nodeType() == Node::ATTRIBUTE_NODE || context.nodeType() == Node::XPATH_NAMESPACE_NODE)
                return;
            for (Node* node = context.previousSibling(); node; node = node->previousSibling()) {
                if (nodeMatches(*node, PrecedingSiblingAxis, m_nodeTest))
                    nodes.append(node);
            }
            nodes.markSorted(false);
            return;
        case FollowingAxis:
            if (context.isAttributeNode()) {
                Node* node = static_cast<Attr&>(context).ownerElement();
                while ((node = NodeTraversal::next(node))) {
                    if (nodeMatches(*node, FollowingAxis, m_nodeTest))
                        nodes.append(node);
                }
            } else {
                for (Node* parent = &context; !isRootDomNode(parent); parent = parent->parentNode()) {
                    for (Node* node = parent->nextSibling(); node; node = node->nextSibling()) {
                        if (nodeMatches(*node, FollowingAxis, m_nodeTest))
                            nodes.append(node);
                        for (Node* child = node->firstChild(); child; child = NodeTraversal::next(child, node)) {
                            if (nodeMatches(*child, FollowingAxis, m_nodeTest))
                                nodes.append(child);
                        }
                    }
                }
            }
            return;
        case PrecedingAxis: {
            Node* node;
            if (context.isAttributeNode())
                node = static_cast<Attr&>(context).ownerElement();
            else
                node = &context;
            while (ContainerNode* parent = node->parentNode()) {
                for (node = NodeTraversal::previous(node); node != parent; node = NodeTraversal::previous(node)) {
                    if (nodeMatches(*node, PrecedingAxis, m_nodeTest))
                        nodes.append(node);
                }
                node = parent;
            }
            nodes.markSorted(false);
            return;
        }
        case AttributeAxis: {
            if (!context.isElementNode())
                return;

            Element& contextElement = toElement(context);

            // Avoid lazily creating attribute nodes for attributes that we do not need anyway.
            if (m_nodeTest.m_kind == NodeTest::NameTest && m_nodeTest.m_data != starAtom) {
                RefPtr<Attr> attr = contextElement.getAttributeNodeNS(m_nodeTest.m_namespaceURI, m_nodeTest.m_data);
                if (attr && attr->namespaceURI() != XMLNSNames::xmlnsNamespaceURI) { // In XPath land, namespace nodes are not accessible on the attribute axis.
                    if (nodeMatches(*attr, AttributeAxis, m_nodeTest)) // Still need to check merged predicates.
                        nodes.append(attr.release());
                }
                return;
            }
            
            if (!contextElement.hasAttributes())
                return;

            for (const Attribute& attribute : contextElement.attributesIterator()) {
                RefPtr<Attr> attr = contextElement.ensureAttr(attribute.name());
                if (nodeMatches(*attr, AttributeAxis, m_nodeTest))
                    nodes.append(attr.release());
            }
            return;
        }
        case NamespaceAxis:
            // XPath namespace nodes are not implemented yet.
            return;
        case SelfAxis:
            if (nodeMatches(context, SelfAxis, m_nodeTest))
                nodes.append(&context);
            return;
        case DescendantOrSelfAxis:
            if (nodeMatches(context, DescendantOrSelfAxis, m_nodeTest))
                nodes.append(&context);
            if (context.isAttributeNode()) // In XPath model, attribute nodes do not have children.
                return;
            for (Node* node = context.firstChild(); node; node = NodeTraversal::next(node, &context)) {
                if (nodeMatches(*node, DescendantOrSelfAxis, m_nodeTest))
                    nodes.append(node);
            }
            return;
        case AncestorOrSelfAxis: {
            if (nodeMatches(context, AncestorOrSelfAxis, m_nodeTest))
                nodes.append(&context);
            Node* node = &context;
            if (context.isAttributeNode()) {
                node = static_cast<Attr&>(context).ownerElement();
                if (nodeMatches(*node, AncestorOrSelfAxis, m_nodeTest))
                    nodes.append(node);
            }
            for (node = node->parentNode(); node; node = node->parentNode()) {
                if (nodeMatches(*node, AncestorOrSelfAxis, m_nodeTest))
                    nodes.append(node);
            }
            nodes.markSorted(false);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

}
}
