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
#include "JSNSResolver.h"

#include "JSDOMBinding.h"
#include "PlatformString.h"
#include <kjs/JSLock.h>
#include <kjs/JSValue.h>
#include <kjs/JSString.h>

using namespace KJS;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSNSResolver)

JSNSResolver::JSNSResolver(JSValue* resolver)
    : m_resolver(resolver)
{
}

void JSNSResolver::mark()
{
    if (!m_resolver->marked())
        m_resolver->mark();
}

String JSNSResolver::lookupNamespaceURI(ExecState* exec, const String& prefix)
{
    JSLock lock(false);

    JSValue* function = m_resolver->get(exec, Identifier(exec, "lookupNamespaceURI"));
    if (exec->hadException())
        return String();

    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone) {
        callType = m_resolver->getCallData(callData);
        if (callType == CallTypeNone)
            return String();
        function = m_resolver;
    }

    ArgList args;
    args.append(jsStringOrNull(exec, prefix));

    RefPtr<JSNSResolver> selfProtector(this);

    JSValue* result = call(exec, function, callType, callData, m_resolver, args);
    if (exec->hadException())
        return String();

    String stringResult = valueToStringWithUndefinedOrNullCheck(exec, result);
    if (exec->hadException())
        return String();

    return stringResult;
}

PassRefPtr<NSResolver> toNSResolver(JSValue* value)
{
    return JSNSResolver::create(value);
}

} // namespace WebCore
