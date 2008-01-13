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
#ifndef _JNI_INSTANCE_H_
#define _JNI_INSTANCE_H_

#include "runtime.h"

#include <JavaVM/jni.h>


namespace KJS {

namespace Bindings {

class JavaClass;

class JObjectWrapper
{
friend class RefPtr<JObjectWrapper>;
friend class JavaArray;
friend class JavaField;
friend class JavaInstance;
friend class JavaMethod;

protected:
    JObjectWrapper(jobject instance);    
    ~JObjectWrapper();
    
    void ref() { _refCount++; }
    void deref() 
    { 
        if (--_refCount == 0) 
            delete this; 
    }

    jobject _instance;

private:
    JNIEnv *_env;
    unsigned int _refCount;
};

class JavaInstance : public Instance
{
public:
    JavaInstance(jobject instance, PassRefPtr<RootObject>);
    ~JavaInstance();
    
    virtual Class *getClass() const;
    
    virtual void begin();
    virtual void end();
    
    virtual JSValue *valueOf() const;
    virtual JSValue *defaultValue (JSType hint) const;

    virtual JSValue *invokeMethod (ExecState *exec, const MethodList &method, const List &args);

    jobject javaInstance() const { return _instance->_instance; }
    
    JSValue *stringValue() const;
    JSValue *numberValue() const;
    JSValue *booleanValue() const;

    virtual BindingLanguage getBindingLanguage() const { return JavaLanguage; }

private:
    RefPtr<JObjectWrapper> _instance;
    mutable JavaClass *_class;
};

} // namespace Bindings

} // namespace KJS

#endif
