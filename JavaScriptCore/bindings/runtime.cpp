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
#include <value.h>
#include <interpreter.h>

#include <runtime_object.h>
#include <jni_instance.h>
#include <objc_instance.h>
#include <c_instance.h>

using namespace KJS;
using namespace KJS::Bindings;

    
void MethodList::addMethod (Method *aMethod)
{
    Method **_newMethods = new Method *[_length + 1];
    if (_length > 0) {
        memcpy (_newMethods, _methods, sizeof(Method *) * _length);
        delete [] _methods;
    }
    _methods = _newMethods;
    _methods[_length++] = aMethod;
}

unsigned int MethodList::length() const
{
    return _length;
}

Method *MethodList::methodAt (unsigned int index) const
{
    assert (index < _length);
    return _methods[index];
}
    
MethodList::~MethodList()
{
    delete [] _methods;
}

MethodList::MethodList (const MethodList &other) {
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
        memcpy (_methods, other._methods, sizeof(Method *) * _length);

    return *this;
}


static KJSDidExecuteFunctionPtr _DidExecuteFunction;

void Instance::setDidExecuteFunction (KJSDidExecuteFunctionPtr func) { _DidExecuteFunction = func; }
KJSDidExecuteFunctionPtr Instance::didExecuteFunction () { return _DidExecuteFunction; }

Value Instance::getValueOfField (KJS::ExecState *exec, const Field *aField) const {  
    return aField->valueFromInstance (exec, this);
}

void Instance::setValueOfField (KJS::ExecState *exec, const Field *aField, const Value &aValue) const {  
    aField->setValueToInstance (exec, this, aValue);
}

Instance *Instance::createBindingForLanguageInstance (BindingLanguage language, void *instance)
{
    if (language == Instance::JavaLanguage)
        return new Bindings::JavaInstance ((jobject)instance);
    else if (language == Instance::ObjectiveCLanguage)
        return new Bindings::ObjcInstance ((struct objc_object *)instance);

    else if (language == Instance::CLanguage)
        return new Bindings::CInstance ((NPObject *)instance);

    return 0;
}

Object Instance::createRuntimeObject (BindingLanguage language, void *myInterface)
{
    Instance *interfaceObject = Instance::createBindingForLanguageInstance (language, (void *)myInterface);
    
    Interpreter::lock();
    Object theObject(new RuntimeObjectImp(interfaceObject,true));
    Interpreter::unlock();
    
    return theObject;
}
