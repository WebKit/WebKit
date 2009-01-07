/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef BINDINGS_C_INSTANCE_H_
#define BINDINGS_C_INSTANCE_H_

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "runtime.h"
#include <wtf/PassRefPtr.h>
#include "runtime_root.h"

typedef struct NPObject NPObject;

namespace JSC {

namespace Bindings {

class CClass;

class CInstance : public Instance {
public:
    static PassRefPtr<CInstance> create(NPObject* object, PassRefPtr<RootObject> rootObject)
    {
        return adoptRef(new CInstance(object, rootObject));
    }
    ~CInstance ();
    
    virtual Class *getClass() const;

    virtual JSValuePtr valueOf(ExecState*) const;
    virtual JSValuePtr defaultValue(ExecState*, PreferredPrimitiveType) const;
    
    virtual JSValuePtr invokeMethod(ExecState*, const MethodList&, const ArgList&);
    virtual bool supportsInvokeDefaultMethod() const;
    virtual JSValuePtr invokeDefaultMethod(ExecState*, const ArgList&);
    
    virtual bool supportsConstruct() const;
    virtual JSValuePtr invokeConstruct(ExecState*, const ArgList&);

    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    JSValuePtr stringValue(ExecState*) const;
    JSValuePtr numberValue(ExecState*) const;
    JSValuePtr booleanValue() const;
    
    NPObject *getObject() const { return _object; }

    virtual BindingLanguage getBindingLanguage() const { return CLanguage; }

private:
    CInstance(NPObject*, PassRefPtr<RootObject>);
    
    mutable CClass *_class;
    NPObject *_object;
};

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif
