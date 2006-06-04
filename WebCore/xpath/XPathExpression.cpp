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
#include "XPathExpression.h"

#if XPATH_SUPPORT

#include "PlatformString.h"
#include "Document.h"
#include "Node.h"
#include "XPathNSResolver.h"
#include "XPathParser.h"
#include "XPathResult.h"
#include "ExceptionCode.h"

#include "XPathExpressionNode.h"

namespace WebCore {

using namespace XPath;
    
PassRefPtr<XPathExpression> XPathExpression::createExpression(const String& expression, XPathNSResolver* resolver, ExceptionCode& ec)
{
    RefPtr<XPathExpression> expr = new XPathExpression;
    Parser parser;

    expr->m_topExpression = parser.parseStatement(expression, resolver, ec);
    if (!expr->m_topExpression)
        return 0;

    return expr.release();
}

XPathExpression::~XPathExpression()
{
    delete m_topExpression;
}

PassRefPtr<XPathResult> XPathExpression::evaluate(Node* contextNode, unsigned short type, XPathResult*, ExceptionCode& ec)
{
    if (!isValidContextNode(contextNode)) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    EventTargetNode* eventTarget = contextNode->ownerDocument()
        ? contextNode->ownerDocument()
        : static_cast<EventTargetNode*>(contextNode);

    Expression::evaluationContext().node = contextNode;    
    m_topExpression->optimize();
    RefPtr<XPathResult> result = new XPathResult(eventTarget, m_topExpression->evaluate());

    if (type != XPathResult::ANY_TYPE) {
        ec = 0;
        result->convertTo(type, ec);
        if (ec)
            return 0;
    }

    return result;
}

}

#endif // XPATH_SUPPORT
