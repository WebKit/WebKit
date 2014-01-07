/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009, 2013 Apple Inc. All rights reserved.
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
#include "XPathPath.h"

#include "Document.h"
#include "XPathPredicate.h"
#include "XPathStep.h"

namespace WebCore {
namespace XPath {
        
Filter::Filter(std::unique_ptr<Expression> expression, Vector<std::unique_ptr<Expression>> predicates)
    : m_expression(std::move(expression)), m_predicates(std::move(predicates))
{
    setIsContextNodeSensitive(m_expression->isContextNodeSensitive());
    setIsContextPositionSensitive(m_expression->isContextPositionSensitive());
    setIsContextSizeSensitive(m_expression->isContextSizeSensitive());
}

Value Filter::evaluate() const
{
    Value result = m_expression->evaluate();
    
    NodeSet& nodes = result.modifiableNodeSet();
    nodes.sort();

    EvaluationContext& evaluationContext = Expression::evaluationContext();
    for (unsigned i = 0; i < m_predicates.size(); i++) {
        NodeSet newNodes;
        evaluationContext.size = nodes.size();
        evaluationContext.position = 0;
        
        for (unsigned j = 0; j < nodes.size(); j++) {
            Node* node = nodes[j];
            
            evaluationContext.node = node;
            ++evaluationContext.position;
            
            if (evaluatePredicate(*m_predicates[i]))
                newNodes.append(node);
        }
        nodes = std::move(newNodes);
    }

    return result;
}

LocationPath::LocationPath()
    : m_isAbsolute(false)
{
    setIsContextNodeSensitive(true);
}

Value LocationPath::evaluate() const
{
    EvaluationContext& evaluationContext = Expression::evaluationContext();
    EvaluationContext backupContext = evaluationContext;
    // http://www.w3.org/TR/xpath/
    // Section 2, Location Paths:
    // "/ selects the document root (which is always the parent of the document element)"
    // "A / by itself selects the root node of the document containing the context node."
    // In the case of a tree that is detached from the document, we violate
    // the spec and treat / as the root node of the detached tree.
    // This is for compatibility with Firefox, and also seems like a more
    // logical treatment of where you would expect the "root" to be.
    Node* context = evaluationContext.node.get();
    if (m_isAbsolute && !context->isDocumentNode())  {
        if (context->inDocument())
            context = context->ownerDocument();
        else
            context = context->highestAncestor();
    }

    NodeSet nodes;
    nodes.append(context);
    evaluate(nodes);
    
    evaluationContext = backupContext;
    return Value(std::move(nodes));
}

void LocationPath::evaluate(NodeSet& nodes) const
{
    bool resultIsSorted = nodes.isSorted();

    for (unsigned i = 0; i < m_steps.size(); i++) {
        Step& step = *m_steps[i];
        NodeSet newNodes;
        HashSet<Node*> newNodesSet;

        bool needToCheckForDuplicateNodes = !nodes.subtreesAreDisjoint() || (step.axis() != Step::ChildAxis && step.axis() != Step::SelfAxis
            && step.axis() != Step::DescendantAxis && step.axis() != Step::DescendantOrSelfAxis && step.axis() != Step::AttributeAxis);

        if (needToCheckForDuplicateNodes)
            resultIsSorted = false;

        // This is a simplified check that can be improved to handle more cases.
        if (nodes.subtreesAreDisjoint() && (step.axis() == Step::ChildAxis || step.axis() == Step::SelfAxis))
            newNodes.markSubtreesDisjoint(true);

        for (unsigned j = 0; j < nodes.size(); j++) {
            NodeSet matches;
            step.evaluate(*nodes[j], matches);

            if (!matches.isSorted())
                resultIsSorted = false;

            for (size_t nodeIndex = 0; nodeIndex < matches.size(); ++nodeIndex) {
                Node* node = matches[nodeIndex];
                if (!needToCheckForDuplicateNodes || newNodesSet.add(node).isNewEntry)
                    newNodes.append(node);
            }
        }
        
        nodes = std::move(newNodes);
    }

    nodes.markSorted(resultIsSorted);
}

void LocationPath::appendStep(std::unique_ptr<Step> step)
{
    unsigned stepCount = m_steps.size();
    if (stepCount) {
        bool dropSecondStep;
        optimizeStepPair(*m_steps[stepCount - 1], *step, dropSecondStep);
        if (dropSecondStep)
            return;
    }
    step->optimize();
    m_steps.append(std::move(step));
}

void LocationPath::prependStep(std::unique_ptr<Step> step)
{
    if (m_steps.size()) {
        bool dropSecondStep;
        optimizeStepPair(*step, *m_steps[0], dropSecondStep);
        if (dropSecondStep) {
            m_steps[0] = std::move(step);
            return;
        }
    }
    step->optimize();
    m_steps.insert(0, std::move(step));
}

Path::Path(std::unique_ptr<Expression> filter, std::unique_ptr<LocationPath> path)
    : m_filter(std::move(filter))
    , m_path(std::move(path))
{
    setIsContextNodeSensitive(m_filter->isContextNodeSensitive());
    setIsContextPositionSensitive(m_filter->isContextPositionSensitive());
    setIsContextSizeSensitive(m_filter->isContextSizeSensitive());
}

Value Path::evaluate() const
{
    Value result = m_filter->evaluate();

    NodeSet& nodes = result.modifiableNodeSet();
    m_path->evaluate(nodes);
    
    return result;
}

}
}
