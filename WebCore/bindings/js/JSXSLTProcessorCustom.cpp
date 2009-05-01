/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(XSLT)

#include "JSXSLTProcessor.h"

#include "Document.h"
#include "DocumentFragment.h"
#include "JSDocument.h"
#include "JSDocumentFragment.h"
#include "JSNode.h"
#include "Node.h"
#include "PlatformString.h"
#include "XSLTProcessor.h"
#include "JSDOMBinding.h"

using namespace JSC;

namespace WebCore {

JSValue JSXSLTProcessor::importStylesheet(ExecState*, const ArgList& args)
{
    JSValue nodeVal = args.at(0);
    if (nodeVal.isObject(&JSNode::s_info)) {
        JSNode* node = static_cast<JSNode*>(asObject(nodeVal));
        impl()->importStylesheet(node->impl());
        return jsUndefined();
    }
    // Throw exception?
    return jsUndefined();
}

JSValue JSXSLTProcessor::transformToFragment(ExecState* exec, const ArgList& args)
{
    JSValue nodeVal = args.at(0);
    JSValue docVal = args.at(1);
    if (nodeVal.isObject(&JSNode::s_info) && docVal.isObject(&JSDocument::s_info)) {
        WebCore::Node* node = static_cast<JSNode*>(asObject(nodeVal))->impl();
        Document* doc = static_cast<Document*>(static_cast<JSDocument*>(asObject(docVal))->impl());
        return toJS(exec, impl()->transformToFragment(node, doc).get());
    }
    // Throw exception?
    return jsUndefined();
}

JSValue JSXSLTProcessor::transformToDocument(ExecState* exec, const ArgList& args)
{
    JSValue nodeVal = args.at(0);
    if (nodeVal.isObject(&JSNode::s_info)) {
        JSNode* node = static_cast<JSNode*>(asObject(nodeVal));
        RefPtr<Document> resultDocument = impl()->transformToDocument(node->impl());
        if (resultDocument)
            return toJS(exec, resultDocument.get());
        return jsUndefined();
    }
    // Throw exception?
    return jsUndefined();
}

JSValue JSXSLTProcessor::setParameter(ExecState* exec, const ArgList& args)
{
    if (args.at(1).isUndefinedOrNull() || args.at(2).isUndefinedOrNull())
        return jsUndefined(); // Throw exception?
    String namespaceURI = args.at(0).toString(exec);
    String localName = args.at(1).toString(exec);
    String value = args.at(2).toString(exec);
    impl()->setParameter(namespaceURI, localName, value);
    return jsUndefined();
}

JSValue JSXSLTProcessor::getParameter(ExecState* exec, const ArgList& args)
{
    if (args.at(1).isUndefinedOrNull())
        return jsUndefined();
    String namespaceURI = args.at(0).toString(exec);
    String localName = args.at(1).toString(exec);
    String value = impl()->getParameter(namespaceURI, localName);
    return jsStringOrUndefined(exec, value);
}

JSValue JSXSLTProcessor::removeParameter(ExecState* exec, const ArgList& args)
{
    if (args.at(1).isUndefinedOrNull())
        return jsUndefined();
    String namespaceURI = args.at(0).toString(exec);
    String localName = args.at(1).toString(exec);
    impl()->removeParameter(namespaceURI, localName);
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(XSLT)
