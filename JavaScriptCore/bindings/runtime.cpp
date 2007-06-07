/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "runtime.h"

#include "JSLock.h"
#include "NP_jsobject.h"
#include "c_instance.h"
#include "runtime_object.h"
#include "runtime_root.h"

#if HAVE(JNI)
#include "jni_instance.h"
#endif
#if PLATFORM(MAC)
#include "objc_instance.h"
#endif
#if PLATFORM(QT)
#include "qt_instance.h"
#endif

namespace KJS { namespace Bindings {

void MethodList::addMethod(Method *aMethod)
{
    Method **_newMethods = new Method *[_length + 1];
    if (_length > 0) {
        memcpy(_newMethods, _methods, sizeof(Method *) * _length);
        delete [] _methods;
    }
    _methods = _newMethods;
    _methods[_length++] = aMethod;
}

unsigned int MethodList::length() const
{
    return _length;
}

Method *MethodList::methodAt(unsigned int index) const
{
    assert(index < _length);
    return _methods[index];
}
    
MethodList::~MethodList()
{
    delete [] _methods;
}

MethodList::MethodList(const MethodList &other)
{
    _length = other._length;
    _methods = new Method *[_length];
    if (_length > 0)
        memcpy (_methods, other._methods, sizeof(Method *) * _length);
}

MethodList &MethodList::operator=(const MethodList &other)
{
    if (this == &other)
        return *this;
            
    delete [] _methods;
    
    _length = other._length;
    _methods = new Method *[_length];
    if (_length > 0)
        memcpy(_methods, other._methods, sizeof(Method *) * _length);

    return *this;
}


Instance::Instance(PassRefPtr<RootObject> rootObject)
    : _rootObject(rootObject)
    , _refCount(0)
{
}

Instance::~Instance()
{
}

static KJSDidExecuteFunctionPtr _DidExecuteFunction;

void Instance::setDidExecuteFunction(KJSDidExecuteFunctionPtr func) { _DidExecuteFunction = func; }
KJSDidExecuteFunctionPtr Instance::didExecuteFunction() { return _DidExecuteFunction; }

JSValue *Instance::getValueOfField(ExecState *exec, const Field *aField) const
{
    return aField->valueFromInstance(exec, this);
}

void Instance::setValueOfField(ExecState *exec, const Field *aField, JSValue *aValue) const
{
    aField->setValueToInstance(exec, this, aValue);
}

Instance* Instance::createBindingForLanguageInstance(BindingLanguage language, void* nativeInstance, PassRefPtr<RootObject> rootObject)
{
    Instance *newInstance = 0;
    
    switch (language) {
#if HAVE(JNI)
        case Instance::JavaLanguage: {
            newInstance = new Bindings::JavaInstance((jobject)nativeInstance, rootObject);
            break;
        }
#endif
#if PLATFORM(MAC)
        case Instance::ObjectiveCLanguage: {
            newInstance = new Bindings::ObjcInstance((ObjectStructPtr)nativeInstance, rootObject);
            break;
        }
#endif
        case Instance::CLanguage: {
            newInstance = new Bindings::CInstance((NPObject *)nativeInstance, rootObject);
            break;
        }
#if PLATFORM(QT)
        case Instance::QtLanguage: {
            newInstance = new Bindings::QtInstance((QObject *)nativeInstance, rootObject);
            break;
        }
#endif
        default:
            break;
    }

    return newInstance;
}

JSObject* Instance::createRuntimeObject(BindingLanguage language, void* nativeInstance, PassRefPtr<RootObject> rootObject)
{
    Instance* instance = Instance::createBindingForLanguageInstance(language, nativeInstance, rootObject);
    
    JSLock lock;
    return new RuntimeObjectImp(instance);
}

RootObject* Instance::rootObject() const 
{ 
    return _rootObject && _rootObject->isValid() ? _rootObject.get() : 0;
}

} } // namespace KJS::Bindings
