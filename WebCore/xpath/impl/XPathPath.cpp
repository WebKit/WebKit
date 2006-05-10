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

#include "XPathPath.h"

#include "Document.h"
#include "Logging.h"
#include "Node.h"
#include "XPathValue.h"

namespace WebCore {
namespace XPath {
        
Filter::Filter(Expression* expr, const Vector<Predicate*>& predicates)
    : m_expr(expr), m_predicates(predicates)
{
}

Filter::~Filter()
{
    delete m_expr;
    deleteAllValues(m_predicates);
}

Value Filter::doEvaluate() const
{
    Value v = m_expr->evaluate();
    
    if (!v.isNodeVector()) {
        if (!m_predicates.isEmpty())
            LOG(XPath, "Ignoring predicates for filter since expression does not evaluate to a nodevector!");

        return v;
    }

    NodeVector inNodes = v.toNodeVector(), outNodes;
    for (unsigned i = 0; i < m_predicates.size(); i++) {
        outNodes.clear();
        Expression::evaluationContext().size = inNodes.size();
        Expression::evaluationContext().position = 0;
        
        for (unsigned j = 0; j < inNodes.size(); j++) {
            Node* node = inNodes[j].get();
            
            Expression::evaluationContext().node = node;
            ++Expression::evaluationContext().position;
            
            if (m_predicates[i]->evaluate())
                outNodes.append(node);
        }
        inNodes = outNodes;
    }

    return outNodes;
}

LocationPath::LocationPath()
    : m_absolute(false)
{
}

LocationPath::~LocationPath()
{
    deleteAllValues(m_steps);
}

void LocationPath::optimize()
{
    for (unsigned i = 0; i < m_steps.size(); i++)
        m_steps[i]->optimize();
}

Value LocationPath::doEvaluate() const
{
    if (m_absolute) {
        LOG(XPath, "Evaluating absolute path expression with %i location steps.", m_steps.size());
    } else {
        LOG(XPath, "Evaluating relative path expression with %i location steps.", m_steps.size());
    }

    NodeVector inDomNodes, outDomNodes;

    /* For absolute location paths, the context node is ignored - the
     * document's root node is used instead.
     */
    Node* context = Expression::evaluationContext().node.get();
    if (m_absolute && context->nodeType() != Node::DOCUMENT_NODE) 
        context = context->ownerDocument();

    inDomNodes.append(context);
    
    for (unsigned i = 0; i < m_steps.size(); i++) {
        Step* step = m_steps[i];

        for (unsigned j = 0; j < inDomNodes.size(); j++) {
            NodeVector matches = step->evaluate(inDomNodes[j].get());
            
            outDomNodes.append(matches);
        }
        
        inDomNodes = outDomNodes;
        outDomNodes.clear();
    }

    return inDomNodes;
}

Path::Path(Filter* filter, LocationPath* path)
    : m_filter(filter),
    m_path(path)
{
}

Path::~Path()
{
    delete m_filter;
    delete m_path;
}

Value Path::doEvaluate() const
{
    return Value();
}

}
}

#endif // XPATH_SUPPORT
