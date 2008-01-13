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

#ifndef BINDINGS_OBJC_INSTANCE_H_
#define BINDINGS_OBJC_INSTANCE_H_

#include <JavaScriptCore/objc_class.h>
#include <JavaScriptCore/objc_utility.h>

namespace KJS {

namespace Bindings {

class ObjcClass;

class ObjcInstance : public Instance {
public:
    ObjcInstance(ObjectStructPtr instance, PassRefPtr<RootObject>);
        
    ~ObjcInstance();
    
    virtual Class *getClass() const;
    
    virtual void begin();
    virtual void end();
    
    virtual JSValue *valueOf() const;
    virtual JSValue *defaultValue(JSType hint) const;

    virtual bool implementsCall() const;
    
    virtual JSValue *invokeMethod(ExecState *exec, const MethodList &method, const List &args);
    virtual JSValue *invokeDefaultMethod(ExecState *exec, const List &args);

    virtual bool supportsSetValueOfUndefinedField();
    virtual void setValueOfUndefinedField(ExecState *exec, const Identifier &property, JSValue *aValue);
    
    virtual JSValue *getValueOfUndefinedField(ExecState *exec, const Identifier &property, JSType hint) const;

    ObjectStructPtr getObject() const { return _instance.get(); }
    
    JSValue *stringValue() const;
    JSValue *numberValue() const;
    JSValue *booleanValue() const;

    virtual BindingLanguage getBindingLanguage() const { return ObjectiveCLanguage; }

private:
    RetainPtr<ObjectStructPtr> _instance;
    mutable ObjcClass *_class;
    ObjectStructPtr _pool;
    int _beginCount;
};

} // namespace Bindings

} // namespace KJS

#endif // BINDINGS_OBJC_INSTANCE_H_
