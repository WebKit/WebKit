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

#include "config.h"
#include "JSCustomXPathNSResolver.h"

#if ENABLE(XPATH)

#include "CString.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "Page.h"

#include "kjs_binding.h"
#include "kjs_proxy.h"
#include "kjs_window.h"

namespace WebCore {

using namespace KJS;

PassRefPtr<JSCustomXPathNSResolver> JSCustomXPathNSResolver::create(KJS::ExecState* exec, KJS::JSValue* value)
{
    if (value->isUndefinedOrNull())
        return 0;

    JSObject* resolverObject = value->getObject();
    if (!resolverObject) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return 0;
    }
    
    return new JSCustomXPathNSResolver(resolverObject, KJS::Window::retrieveActive(exec)->impl()->frame());
}

JSCustomXPathNSResolver::JSCustomXPathNSResolver(JSObject* customResolver, Frame* frame)
    : m_customResolver(customResolver)
    , m_frame(frame)
{
}

JSCustomXPathNSResolver::~JSCustomXPathNSResolver()
{
}

String JSCustomXPathNSResolver::lookupNamespaceURI(const String& prefix)
{
    ASSERT(m_customResolver);

    if (!m_frame)
        return String();
    if (!m_frame->scriptProxy()->isEnabled())
        return String();

    JSLock lock;

    JSGlobalObject* globalObject = m_frame->scriptProxy()->globalObject();
    ExecState* exec = globalObject->globalExec();
        
    JSValue* lookupNamespaceURIFuncValue = m_customResolver->get(exec, "lookupNamespaceURI");
    JSObject* lookupNamespaceURIFunc = 0;
    if (lookupNamespaceURIFuncValue->isObject()) {      
        lookupNamespaceURIFunc = static_cast<JSObject*>(lookupNamespaceURIFuncValue);
        if (!lookupNamespaceURIFunc->implementsCall())
            lookupNamespaceURIFunc = 0;
    }

    if (!lookupNamespaceURIFunc && !m_customResolver->implementsCall()) {
        // FIXME: pass actual line number and source URL.
        if (Page* page = m_frame->page())
            page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, "XPathNSResolver does not have a lookupNamespaceURI method.", 0, String());
        return String();
    }

    RefPtr<JSCustomXPathNSResolver> selfProtector(this);

    List args;
    args.append(jsString(prefix));

    String result;
    JSValue* retval;
    globalObject->startTimeoutCheck();
    if (lookupNamespaceURIFunc)
        retval = lookupNamespaceURIFunc->call(exec, m_customResolver, args);
    else
        retval = m_customResolver->call(exec, m_customResolver, args);
    globalObject->stopTimeoutCheck();

    if (exec->hadException()) {
        JSObject* exception = exec->exception()->toObject(exec);
        String message = exception->get(exec, exec->propertyNames().message)->toString(exec);
        int lineNumber = exception->get(exec, "line")->toInt32(exec);
        String sourceURL = exception->get(exec, "sourceURL")->toString(exec);
        if (Interpreter::shouldPrintExceptions())
            printf("XPathNSResolver: %s\n", message.utf8().data());
        if (Page* page = m_frame->page())
            page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, lineNumber, sourceURL);
        exec->clearException();
    } else {
        if (!retval->isUndefinedOrNull())
            result = retval->toString(exec);
    }

    Document::updateDocumentsRendering();

    return result;
}

} // namespace WebCore

#endif // ENABLE(XPATH)
