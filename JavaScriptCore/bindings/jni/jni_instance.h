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

#include <jni_runtime.h>
#include <jni_utility.h>
#include <jni_class.h>
#include <jni_instance.h>

namespace Bindings {

class JObjectWrapper
{
friend class JavaInstance;

protected:
    JObjectWrapper(jobject instance) : _ref(1) {
        // Cache the JNIEnv used to get the global ref for this java instanace.
        // It'll be used to delete the reference.
        _env = getJNIEnv();
        
        _instance = _env->NewGlobalRef (instance);
        _env->DeleteLocalRef (instance);
        
        if  (_instance == NULL) {
            fprintf (stderr, "%s:  out of memory!\n", __PRETTY_FUNCTION__);
        }
    }
    
    ~JObjectWrapper() {
        _env->DeleteGlobalRef (_instance);
    }
    
    void ref() { _ref++; }
    void deref() { 
        _ref--;
        if (_ref == 0)
            delete this;
    }
    
    jobject _instance;

private:
    JNIEnv *_env;
    unsigned int _ref;
};

class JavaInstance : public Instance
{
public:
    JavaInstance (jobject instance);
        
    ~JavaInstance ();
    
    virtual Class *getClass() const {
        return JavaClass::classForInstance (_instance->_instance);
    }

    JavaInstance (const JavaInstance &other);

    JavaInstance &operator=(const JavaInstance &other){
        if (this == &other)
            return *this;
        
        JObjectWrapper *_oldInstance = _instance;
        _instance = other._instance;
        _instance->ref();
        _oldInstance->deref();
        
        return *this;
    };
    
private:
    JObjectWrapper *_instance;
};

}

#endif
