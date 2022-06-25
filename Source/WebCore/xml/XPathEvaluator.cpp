/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Inc.
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
#include "XPathEvaluator.h"

#include "NativeXPathNSResolver.h"
#include "XPathExpression.h"
#include "XPathResult.h"
#include "XPathUtil.h"

namespace WebCore {

ExceptionOr<Ref<XPathExpression>> XPathEvaluator::createExpression(const String& expression, RefPtr<XPathNSResolver>&& resolver)
{
    return XPathExpression::createExpression(expression, WTFMove(resolver));
}

Ref<XPathNSResolver> XPathEvaluator::createNSResolver(Node& nodeResolver)
{
    return NativeXPathNSResolver::create(nodeResolver);
}

ExceptionOr<Ref<XPathResult>> XPathEvaluator::evaluate(const String& expression, Node& contextNode, RefPtr<XPathNSResolver>&& resolver, unsigned short type, XPathResult* result)
{
    if (!XPath::isValidContextNode(contextNode))
        return Exception { NotSupportedError };

    auto createResult = createExpression(expression, WTFMove(resolver));
    if (createResult.hasException())
        return createResult.releaseException();

    return createResult.releaseReturnValue()->evaluate(contextNode, type, result);
}

}
