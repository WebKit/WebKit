/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSXSLTProcessor_h
#define JSXSLTProcessor_h

#if ENABLE(XSLT)

#include "kjs_binding.h"

namespace WebCore {

class XSLTProcessor;

// Eventually we should implement XSLTException:
// http://lxr.mozilla.org/seamonkey/source/content/xsl/public/nsIXSLTException.idl
// http://bugs.webkit.org/show_bug.cgi?id=5446

class JSXSLTProcessor : public DOMObject {
public:
    JSXSLTProcessor(KJS::JSObject* prototype);
    ~JSXSLTProcessor();
    
    virtual const KJS::ClassInfo* classInfo() const { return &info; }
    static const KJS::ClassInfo info;

    XSLTProcessor* impl() const { return m_impl.get(); }

private:
    RefPtr<XSLTProcessor> m_impl;
};

class XSLTProcessorConstructorImp : public DOMObject {
public:
    XSLTProcessorConstructorImp(KJS::ExecState*);

    virtual bool implementsConstruct() const;
    virtual KJS::JSObject* construct(KJS::ExecState*, const KJS::List&);
};

KJS::JSValue* jsXSLTProcessorPrototypeFunctionImportStylesheet(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXSLTProcessorPrototypeFunctionTransformToFragment(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXSLTProcessorPrototypeFunctionTransformToDocument(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXSLTProcessorPrototypeFunctionSetParameter(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXSLTProcessorPrototypeFunctionGetParameter(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXSLTProcessorPrototypeFunctionRemoveParameter(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXSLTProcessorPrototypeFunctionClearParameters(KJS::ExecState*, KJS::JSObject*, const KJS::List&);
KJS::JSValue* jsXSLTProcessorPrototypeFunctionReset(KJS::ExecState*, KJS::JSObject*, const KJS::List&);

} // namespace WebCore

#endif // ENABLE(XSLT)

#endif // JSXSLTProcessor_h
