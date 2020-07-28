/*
 * Copyright (C) 2007, 2013 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "JSCustomXPathNSResolver.h"

#include "CommonVM.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWindowCustom.h"
#include "JSExecState.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include <JavaScriptCore/JSLock.h>
#include <wtf/Ref.h>

namespace WebCore {

using namespace JSC;

ExceptionOr<Ref<JSCustomXPathNSResolver>> JSCustomXPathNSResolver::create(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    if (value.isUndefinedOrNull())
        return Exception { TypeError };

    auto* resolverObject = value.getObject();
    if (!resolverObject)
        return Exception { TypeMismatchError };

    VM& vm = lexicalGlobalObject.vm();
    return adoptRef(*new JSCustomXPathNSResolver(vm, resolverObject, asJSDOMWindow(&lexicalGlobalObject)));
}

JSCustomXPathNSResolver::JSCustomXPathNSResolver(VM& vm, JSObject* customResolver, JSDOMWindow* globalObject)
    : m_customResolver(vm, customResolver)
    , m_globalObject(vm, globalObject)
{
}

JSCustomXPathNSResolver::~JSCustomXPathNSResolver() = default;

String JSCustomXPathNSResolver::lookupNamespaceURI(const String& prefix)
{
    ASSERT(m_customResolver);

    JSLockHolder lock(commonVM());

    JSGlobalObject* lexicalGlobalObject = m_globalObject.get();
    VM& vm = lexicalGlobalObject->vm();
        
    JSValue function = m_customResolver->get(lexicalGlobalObject, Identifier::fromString(vm, "lookupNamespaceURI"));
    auto callData = getCallData(vm, function);
    if (callData.type == CallData::Type::None) {
        callData = getCallData(vm, m_customResolver.get());
        if (callData.type == CallData::Type::None) {
            if (PageConsoleClient* console = m_globalObject->wrapped().console())
                console->addMessage(MessageSource::JS, MessageLevel::Error, "XPathNSResolver does not have a lookupNamespaceURI method."_s);
            return String();
        }
        function = m_customResolver.get();
    }

    Ref<JSCustomXPathNSResolver> protectedThis(*this);

    MarkedArgumentBuffer args;
    args.append(jsStringWithCache(vm, prefix));
    ASSERT(!args.hasOverflowed());

    NakedPtr<JSC::Exception> exception;
    JSValue retval = JSExecState::call(lexicalGlobalObject, function, callData, m_customResolver.get(), args, exception);

    String result;
    if (exception)
        reportException(lexicalGlobalObject, exception);
    else {
        if (!retval.isUndefinedOrNull())
            result = retval.toWTFString(lexicalGlobalObject);
    }

    return result;
}

} // namespace WebCore
