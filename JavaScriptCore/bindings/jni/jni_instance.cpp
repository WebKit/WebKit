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
#include <jni_class.h>
#include <jni_instance.h>
#include <jni_runtime.h>
#include <jni_utility.h>

using namespace Bindings;
using namespace KJS;

JavaInstance::JavaInstance (jobject instance) 
{
    _instance = new JObjectWrapper (instance);
};

JavaInstance::~JavaInstance () 
{
    _instance->deref();
}


JavaInstance::JavaInstance (const JavaInstance &other) : Instance() 
{
    _instance = other._instance;
    _instance->ref();
};

Class *JavaInstance::getClass() const 
{
    return JavaClass::classForInstance (_instance->_instance);
}

Value JavaInstance::invokeMethod (const Method *method, const List &args)
{
    const JavaMethod *jMethod = static_cast<const JavaMethod*>(method);
    int i, count = args.size();
    jvalue *jArgs;
    
    fprintf(stderr,"%s: this=%p, invoking %s which returns %s and takes %d args\n", __PRETTY_FUNCTION__, this, method->name(), method->returnType(), count);
    
    if (count > 0) {
        jArgs = (jvalue *)malloc (count * sizeof(jvalue));
    }
    else
        jArgs = 0;
        
    for (i = 0; i < count; i++) {
        fprintf (stderr, "%s:  %d, type %d\n", __PRETTY_FUNCTION__, i, args.at(i).type());
    }
    
    jvalue result;
    switch (jMethod->JNIReturnType()){
        case void_type: {
            callJNIVoidMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case object_type: {
            result.l = callJNIObjectMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case boolean_type: {
            result.z = callJNIBooleanMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case byte_type: {
            result.b = callJNIByteMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case char_type: {
            result.c = callJNICharMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case short_type: {
            result.s = callJNIShortMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case int_type: {
            result.i = callJNIIntMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case long_type: {
            result.j = callJNILongMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case float_type: {
            result.f = callJNIFloatMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;
        
        case double_type: {
            result.d = callJNIDoubleMethod (_instance->_instance, method->name(), jMethod->signature(), jArgs);
        }
        break;

        case invalid_type:
        default:
        break;
    }
    
    // FIXME:  create a KJS::Value from the jvalue result.
    
    free (jArgs);
    
    return Undefined();
}


JObjectWrapper::JObjectWrapper(jobject instance)
{
    _ref = 1;
    // Cache the JNIEnv used to get the global ref for this java instanace.
    // It'll be used to delete the reference.
    _env = getJNIEnv();
    
    _instance = _env->NewGlobalRef (instance);
    _env->DeleteLocalRef (instance);
    
    if  (_instance == NULL) {
        fprintf (stderr, "%s:  could not get GlobalRef for %p\n", __PRETTY_FUNCTION__, instance);
    }
}
