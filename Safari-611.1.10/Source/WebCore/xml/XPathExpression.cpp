/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "XPathNSResolver.h"
#include "XPathParser.h"
#include "XPathResult.h"
#include "XPathUtil.h"

namespace WebCore {

using namespace XPath;
    
inline XPathExpression::XPathExpression(std::unique_ptr<XPath::Expression> expression)
    : m_topExpression(WTFMove(expression))
{
}

ExceptionOr<Ref<XPathExpression>> XPathExpression::createExpression(const String& expression, RefPtr<XPathNSResolver>&& resolver)
{
    auto parseResult = Parser::parseStatement(expression, WTFMove(resolver));
    if (parseResult.hasException())
        return parseResult.releaseException();

    return adoptRef(*new XPathExpression(parseResult.releaseReturnValue()));
}

XPathExpression::~XPathExpression() = default;

// FIXME: Why does this take an XPathResult that it ignores?
ExceptionOr<Ref<XPathResult>> XPathExpression::evaluate(Node& contextNode, unsigned short type, XPathResult*)
{
    if (!isValidContextNode(contextNode))
        return Exception { NotSupportedError };

    EvaluationContext& evaluationContext = Expression::evaluationContext();
    evaluationContext.node = &contextNode;
    evaluationContext.size = 1;
    evaluationContext.position = 1;
    evaluationContext.hadTypeConversionError = false;
    auto result = XPathResult::create(contextNode.document(), m_topExpression->evaluate());
    evaluationContext.node = nullptr; // Do not hold a reference to the context node, as this may prevent the whole document from being destroyed in time.

    if (evaluationContext.hadTypeConversionError)
        return Exception { SyntaxError };

    if (type != XPathResult::ANY_TYPE) {
        auto convertToResult = result->convertTo(type);
        if (convertToResult.hasException())
            return convertToResult.releaseException();
    }

    return result;
}

}
