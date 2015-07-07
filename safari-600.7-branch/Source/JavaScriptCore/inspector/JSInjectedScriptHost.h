/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JSInjectedScriptHost_h
#define JSInjectedScriptHost_h

#if ENABLE(INSPECTOR)

#include "JSDestructibleObject.h"

namespace Inspector {

class InjectedScriptHost;

class JSInjectedScriptHost : public JSC::JSDestructibleObject {
public:
    typedef JSC::JSDestructibleObject Base;

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSInjectedScriptHost* create(JSC::VM& vm, JSC::Structure* structure, PassRefPtr<InjectedScriptHost> impl)
    {
        JSInjectedScriptHost* instance = new (NotNull, JSC::allocateCell<JSInjectedScriptHost>(vm.heap)) JSInjectedScriptHost(vm, structure, impl);
        instance->finishCreation(vm);
        return instance;
    }

    static JSC::JSObject* createPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static void destroy(JSC::JSCell*);

    InjectedScriptHost& impl() const { return *m_impl; }
    void releaseImpl();

    // Attributes.
    JSC::JSValue evaluate(JSC::ExecState*) const;

    // Functions.
    JSC::JSValue internalConstructorName(JSC::ExecState*);
    JSC::JSValue isHTMLAllCollection(JSC::ExecState*);
    JSC::JSValue type(JSC::ExecState*);
    JSC::JSValue functionDetails(JSC::ExecState*);
    JSC::JSValue getInternalProperties(JSC::ExecState*);

protected:
    static const unsigned StructureFlags = Base::StructureFlags;

    void finishCreation(JSC::VM&);

private:
    JSInjectedScriptHost(JSC::VM&, JSC::Structure*, PassRefPtr<InjectedScriptHost>);
    ~JSInjectedScriptHost();

    InjectedScriptHost* m_impl;
};

JSC::JSValue toJS(JSC::ExecState*, JSC::JSGlobalObject*, InjectedScriptHost*);
JSInjectedScriptHost* toJSInjectedScriptHost(JSC::JSValue);

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(JSInjectedScriptHost_h)
