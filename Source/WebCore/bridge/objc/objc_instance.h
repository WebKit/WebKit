/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#import "objc_class.h"
#import "objc_utility.h"

namespace JSC {

namespace Bindings {

class ObjcClass;

class ObjcInstance : public Instance {
public:
    static Ref<ObjcInstance> create(ObjectStructPtr, RefPtr<RootObject>&&);
    virtual ~ObjcInstance();
    
    static void setGlobalException(NSString*, JSGlobalObject* exceptionEnvironment = 0); // A null exceptionEnvironment means the exception should propogate to any execution environment.

    virtual Class* getClass() const;
        
    virtual JSValue valueOf(JSGlobalObject*) const;
    virtual JSValue defaultValue(JSGlobalObject*, PreferredPrimitiveType) const;

    virtual JSValue getMethod(JSGlobalObject*, PropertyName);
    JSValue invokeObjcMethod(JSGlobalObject*, CallFrame*, ObjcMethod* method);
    virtual JSValue invokeMethod(JSGlobalObject*, CallFrame*, RuntimeMethod* method);
    virtual bool supportsInvokeDefaultMethod() const;
    virtual JSValue invokeDefaultMethod(JSGlobalObject*, CallFrame*);

    JSValue getValueOfUndefinedField(JSGlobalObject*, PropertyName) const;
    virtual bool setValueOfUndefinedField(JSGlobalObject*, PropertyName, JSValue);

    ObjectStructPtr getObject() const { return _instance.get(); }
    
    JSValue stringValue(JSGlobalObject*) const;
    JSValue numberValue(JSGlobalObject*) const;
    JSValue booleanValue() const;

    static bool isInStringValue();

protected:
    virtual void virtualBegin();
    virtual void virtualEnd();

private:
    friend class ObjcField;
    static void moveGlobalExceptionToExecState(JSGlobalObject*);

    ObjcInstance(ObjectStructPtr, RefPtr<RootObject>&&);

    virtual RuntimeObject* newRuntimeObject(JSGlobalObject*);

    RetainPtr<ObjectStructPtr> _instance;
    mutable ObjcClass* _class { nullptr };
    void* m_autoreleasePool { nullptr };
    int _beginCount { 0 };
};

} // namespace Bindings

} // namespace JSC
