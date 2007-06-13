/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef KJS_BINDINGS_OBJC_RUNTIME_H
#define KJS_BINDINGS_OBJC_RUNTIME_H

#include <CoreFoundation/CoreFoundation.h>
#include <JavaScriptCore/objc_header.h>
#include <JavaScriptCore/object.h>
#include <JavaScriptCore/runtime.h>

#include <WTF/RetainPtr.h>

namespace KJS {
namespace Bindings {

extern ClassStructPtr webScriptObjectClass();
extern ClassStructPtr webUndefinedClass();

class ObjcInstance;

class ObjcField : public Field
{
public:
    ObjcField(IvarStructPtr ivar);
    ObjcField(CFStringRef name);
    
    virtual JSValue *valueFromInstance(ExecState *exec, const Instance *instance) const;
    virtual void setValueToInstance(ExecState *exec, const Instance *instance, JSValue *aValue) const;
    
    virtual const char *name() const;
    virtual RuntimeType type() const;
        
private:
    IvarStructPtr _ivar;
    RetainPtr<CFStringRef> _name;
};

class ObjcMethod : public Method
{
public:
    ObjcMethod() : _objcClass(0), _selector(0), _javaScriptName(0) {}
    ObjcMethod(ClassStructPtr aClass, const char *_selector);

    virtual const char *name() const;

    virtual int numParameters() const;

    NSMethodSignature *getMethodSignature() const;
    
    bool isFallbackMethod() const { return strcmp(_selector, "invokeUndefinedMethodFromWebScript:withArguments:") == 0; }
    void setJavaScriptName(CFStringRef n) { _javaScriptName = n; }
    CFStringRef javaScriptName() const { return _javaScriptName.get(); }
    
private:
    ClassStructPtr _objcClass;
    const char *_selector;
    RetainPtr<CFStringRef> _javaScriptName;
};

class ObjcArray : public Array
{
public:
    ObjcArray(ObjectStructPtr, PassRefPtr<RootObject>);

    virtual void setValueAt(ExecState *exec, unsigned int index, JSValue *aValue) const;
    virtual JSValue *valueAt(ExecState *exec, unsigned int index) const;
    virtual unsigned int getLength() const;
    
    ObjectStructPtr getObjcArray() const { return _array.get(); }

    static JSValue *convertObjcArrayToArray(ExecState *exec, ObjectStructPtr anObject);

private:
    RetainPtr<ObjectStructPtr> _array;
};

class ObjcFallbackObjectImp : public JSObject {
public:
    ObjcFallbackObjectImp(ObjcInstance *i, const Identifier propertyName);

    const ClassInfo *classInfo() const { return &info; }

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual bool canPut(ExecState *exec, const Identifier &propertyName) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);
    virtual JSValue *defaultValue(ExecState *exec, JSType hint) const;

    virtual JSType type() const;
    virtual bool toBoolean(ExecState *exec) const;

private:
    ObjcFallbackObjectImp(); // prevent default construction
    
    static const ClassInfo info;

    RefPtr<ObjcInstance> _instance;
    Identifier _item;
};

} // namespace Bindings
} // namespace KJS

#endif
