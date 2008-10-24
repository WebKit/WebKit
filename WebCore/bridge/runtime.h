/*
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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

#ifndef JAVASCRIPTCORE_BINDINGS_RUNTIME_H
#define JAVASCRIPTCORE_BINDINGS_RUNTIME_H

#include <kjs/JSString.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace JSC  {

class ArgList;
class Identifier;
class JSGlobalObject;
class PropertyNameArray;
class RuntimeObjectImp;

namespace Bindings {

class Instance;
class Method;
class RootObject;

typedef Vector<Method*> MethodList;

class Field {
public:
    virtual const char* name() const = 0;
    virtual JSValue* valueFromInstance(ExecState*, const Instance*) const = 0;
    virtual void setValueToInstance(ExecState*, const Instance*, JSValue*) const = 0;

    virtual ~Field() { }
};

class Method : Noncopyable {
public:
    virtual const char *name() const = 0;
    virtual int numParameters() const = 0;
        
    virtual ~Method() { }
};

class Class : Noncopyable {
public:
    virtual const char* name() const = 0;
    virtual MethodList methodsNamed(const Identifier&, Instance*) const = 0;
    virtual Field* fieldNamed(const Identifier&, Instance*) const = 0;
    virtual JSValue* fallbackObject(ExecState*, Instance*, const Identifier&) { return jsUndefined(); }

    virtual ~Class() { }
};

typedef void (*KJSDidExecuteFunctionPtr)(ExecState*, JSObject* rootObject);

class Instance : public RefCounted<Instance> {
public:
    typedef enum {
#if ENABLE(MAC_JAVA_BRIDGE)
        JavaLanguage,
#endif
#if PLATFORM(MAC)
        ObjectiveCLanguage,
#endif
        CLanguage
#if PLATFORM(QT)
        , QtLanguage
#endif
    } BindingLanguage;

    Instance(PassRefPtr<RootObject>);

    static void setDidExecuteFunction(KJSDidExecuteFunctionPtr func);
    static KJSDidExecuteFunctionPtr didExecuteFunction();

    static RuntimeObjectImp* createRuntimeObject(ExecState*, PassRefPtr<Instance>);
    static Instance* getInstance(JSObject*, BindingLanguage);

    // These functions are called before and after the main entry points into
    // the native implementations.  They can be used to establish and cleanup
    // any needed state.
    void begin();
    void end();
    
    virtual Class *getClass() const = 0;
    
    virtual JSValue* getValueOfField(ExecState*, const Field*) const;
    virtual JSValue* getValueOfUndefinedField(ExecState*, const Identifier&) const { return jsUndefined(); }
    virtual void setValueOfField(ExecState*, const Field*, JSValue*) const;
    virtual bool supportsSetValueOfUndefinedField() { return false; }
    virtual void setValueOfUndefinedField(ExecState*, const Identifier&, JSValue*) {}

    virtual JSValue* invokeMethod(ExecState*, const MethodList&, const ArgList& args) = 0;

    virtual bool supportsInvokeDefaultMethod() const { return false; }
    virtual JSValue* invokeDefaultMethod(ExecState*, const ArgList&) { return jsUndefined(); }
    
    virtual void getPropertyNames(ExecState*, PropertyNameArray&) { }

    virtual JSValue* defaultValue(ExecState*, PreferredPrimitiveType) const = 0;
    
    virtual JSValue* valueOf(ExecState* exec) const { return jsString(exec, getClass()->name()); }
    
    RootObject* rootObject() const;
    
    virtual ~Instance();

    virtual BindingLanguage getBindingLanguage() const = 0;

protected:
    virtual void virtualBegin() { }
    virtual void virtualEnd() { }

    RefPtr<RootObject> _rootObject;
};

class Array : Noncopyable {
public:
    Array(PassRefPtr<RootObject>);
    virtual ~Array();
    
    virtual void setValueAt(ExecState *, unsigned index, JSValue*) const = 0;
    virtual JSValue* valueAt(ExecState *, unsigned index) const = 0;
    virtual unsigned int getLength() const = 0;

protected:
    RefPtr<RootObject> _rootObject;
};

const char *signatureForParameters(const ArgList&);

typedef HashMap<RefPtr<UString::Rep>, MethodList*> MethodListMap;
typedef HashMap<RefPtr<UString::Rep>, Method*> MethodMap; 
typedef HashMap<RefPtr<UString::Rep>, Field*> FieldMap; 
    
} // namespace Bindings

} // namespace JSC

#endif
