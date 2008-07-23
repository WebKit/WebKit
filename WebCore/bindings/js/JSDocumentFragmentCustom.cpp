/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSDocumentFragment.h"

#include "DocumentFragment.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "JSElement.h"
#include "JSNSResolver.h"
#include "JSNodeList.h"
#include "NodeList.h"

using namespace KJS;

namespace WebCore {

JSValue* JSDocumentFragment::querySelector(ExecState* exec, const ArgList& args)
{
    DocumentFragment* imp = static_cast<DocumentFragment*>(impl());
    ExceptionCode ec = 0;
    const UString& selectors = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0));
    RefPtr<NSResolver> resolver = args.at(exec, 1)->isUndefinedOrNull() ? 0 : toNSResolver(args.at(exec, 1));

    RefPtr<Element> element = imp->querySelector(selectors, resolver.get(), ec, exec);
    if (exec->hadException())
        return jsUndefined();
    JSValue* result = toJS(exec, element.get());
    setDOMException(exec, ec);
    return result;
}

JSValue* JSDocumentFragment::querySelectorAll(ExecState* exec, const ArgList& args)
{
    DocumentFragment* imp = static_cast<DocumentFragment*>(impl());
    ExceptionCode ec = 0;
    const UString& selectors = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0));
    RefPtr<NSResolver> resolver = args.at(exec, 1)->isUndefinedOrNull() ? 0 : toNSResolver(args.at(exec, 1));

    RefPtr<NodeList> nodeList = imp->querySelectorAll(selectors, resolver.get(), ec, exec);
    if (exec->hadException())
        return jsUndefined();
    JSValue* result = toJS(exec, nodeList.get());
    setDOMException(exec, ec);
    return result;
}

} // namespace WebCore
