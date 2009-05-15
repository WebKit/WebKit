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

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "JavaScriptCallFrame.h"
#include <runtime/ArrayPrototype.h>

using namespace JSC;

namespace WebCore {

JSValue JSJavaScriptCallFrame::evaluate(ExecState* exec, const ArgList& args)
{
    JSValue exception;
    JSValue result = impl()->evaluate(args.at(0).toString(exec), exception);

    if (exception)
        exec->setException(exception);

    return result;
}

JSValue JSJavaScriptCallFrame::thisObject(ExecState*) const
{
    return impl()->thisObject() ? impl()->thisObject() : jsNull();
}

JSValue JSJavaScriptCallFrame::type(ExecState* exec) const
{
    switch (impl()->type()) {
        case DebuggerCallFrame::FunctionType:
            return jsString(exec, "function");
        case DebuggerCallFrame::ProgramType:
            return jsString(exec, "program");
    }

    ASSERT_NOT_REACHED();
    return jsNull();
}

JSValue JSJavaScriptCallFrame::scopeChain(ExecState* exec) const
{
    if (!impl()->scopeChain())
        return jsNull();

    const ScopeChainNode* scopeChain = impl()->scopeChain();
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // we must always have something in the scope chain
    ASSERT(iter != end);

    MarkedArgumentBuffer list;
    do {
        list.append(*iter);
        ++iter;
    } while (iter != end);

    return constructArray(exec, list);
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
