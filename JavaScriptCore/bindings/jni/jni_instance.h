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

#include <CoreFoundation/CoreFoundation.h>

#include <JavaVM/jni.h>

#include <JavaScriptCore/runtime.h>

namespace KJS {

namespace Bindings {

class JavaClass;

class JObjectWrapper
{
friend class JavaArray;
friend class JavaInstance;
friend class JavaMethod;

protected:
    JObjectWrapper(jobject instance);    
    void ref() { _ref++; }
    void deref() { 
        _ref--;
        if (_ref == 0)
            delete this;
    }
    
    ~JObjectWrapper();
	
    jobject _instance;

private:
    JNIEnv *_env;
    unsigned int _ref;
};

class JavaInstance : public Instance
{
public:
    JavaInstance (jobject instance, const RootObject *r);
        
    ~JavaInstance ();
    
    virtual Class *getClass() const;
    
    JavaInstance (const JavaInstance &other);

    JavaInstance &operator=(const JavaInstance &other){
        if (this == &other)
            return *this;
        
        JObjectWrapper *_oldInstance = _instance;
        _instance = other._instance;
        _instance->ref();
        _oldInstance->deref();
		
        // Classes are kept around forever.
        _class = other._class;
        
        return *this;
    };

    virtual void begin();
    virtual void end();
    
    virtual ValueImp *valueOf() const;
    virtual ValueImp *defaultValue (Type hint) const;

    virtual ValueImp *invokeMethod (ExecState *exec, const MethodList &method, const List &args);
    virtual ValueImp *invokeDefaultMethod (ExecState *exec, const List &args);

    jobject javaInstance() const { return _instance->_instance; }
    
    ValueImp *stringValue() const;
    ValueImp *numberValue() const;
    ValueImp *booleanValue() const;
        
private:
    JObjectWrapper *_instance;
    mutable JavaClass *_class;
};

} // namespace Bindings

} // namespace KJS

#endif
