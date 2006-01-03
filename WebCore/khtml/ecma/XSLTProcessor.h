/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#ifdef KHTML_XSLT

#ifndef XSLTProcessor_H
#define XSLTProcessor_H

#include <kxmlcore/RefPtr.h>
#include "kjs_binding.h"

namespace DOM {
    class XSLTProcessorImpl;
};

// Eventually we should implement XSLTException:
// http://lxr.mozilla.org/seamonkey/source/content/xsl/public/nsIXSLTException.idl
// http://bugzilla.opendarwin.org/show_bug.cgi?id=5446

namespace KJS {

class XSLTProcessor : public DOMObject {
public:
    XSLTProcessor(ExecState *exec);
    ~XSLTProcessor();
    
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    
    enum { ImportStylesheet, TransformToFragment, TransformToDocument, SetParameter,
            GetParameter, RemoveParameter, ClearParameters, Reset };
    
    DOM::XSLTProcessorImpl *impl() const { return m_impl.get(); }
private:
    RefPtr<DOM::XSLTProcessorImpl> m_impl;
};

class XSLTProcessorConstructorImp : public JSObject {
public:
    XSLTProcessorConstructorImp(ExecState *);
    virtual bool implementsConstruct() const { return true; }
    virtual JSObject *construct(ExecState *exec, const List &args) { return new XSLTProcessor(exec); }
};

};

#endif

#endif // KHTML_XSLT
