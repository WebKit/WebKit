/*
 * Copyright (C) 2007 Alexey Proskuryakov (ap@nypop.com)
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

#pragma once

#include "ExceptionOr.h"
#include "XPathNSResolver.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/Strong.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {

class JSDOMWindow;

class JSCustomXPathNSResolver final : public XPathNSResolver {
public:
    static ExceptionOr<Ref<JSCustomXPathNSResolver>> create(JSC::JSGlobalObject&, JSC::JSValue);
    virtual ~JSCustomXPathNSResolver();

private:
    JSCustomXPathNSResolver(JSC::VM&, JSC::JSObject*, JSDOMWindow*);

    String lookupNamespaceURI(const String& prefix) final;

    // JSCustomXPathNSResolvers are always temporary so using a Strong reference is safe here.
    JSC::Strong<JSC::JSObject> m_customResolver;
    JSC::Strong<JSDOMWindow> m_globalObject;
};

} // namespace WebCore
