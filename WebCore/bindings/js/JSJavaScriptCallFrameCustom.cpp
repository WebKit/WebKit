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
#include "JSJavaScriptCallFrame.h"

#include "JavaScriptCallFrame.h"
#include <kjs/array_object.h>

using namespace KJS;

namespace WebCore {

JSValue* JSJavaScriptCallFrame::evaluate(ExecState* exec, const List& args)
{
    if (!impl()->isValid())
        return jsUndefined();

    JSValue* exception = 0;
    JSValue* result = impl()->evaluate(args[0]->toString(exec), exception);

    if (exception)
        exec->setException(exception);

    return result;
}

JSValue* JSJavaScriptCallFrame::thisObject(ExecState* exec) const
{
    if (!impl()->isValid() || !impl()->thisObject())
        return jsNull();
    return impl()->thisObject();
}

JSValue* JSJavaScriptCallFrame::scopeChain(ExecState* exec) const
{
    if (!impl()->isValid())
        return jsNull();

    const ScopeChain& chain = impl()->scopeChain();
    ScopeChainIterator iter = chain.begin();
    ScopeChainIterator end = chain.end();

    // we must always have something in the scope chain
    ASSERT(iter != end);

    List list;
    do {
        list.append(*iter);
        ++iter;
    } while (iter != end);

    return exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, list);
}

} // namespace WebCore
