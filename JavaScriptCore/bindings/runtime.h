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
#ifndef _RUNTIME_H_
#define _RUNTIME_H_

#include "list.h"
#include "object.h"
#include "value.h"

namespace KJS 
{

namespace Bindings
{

class Instance;
class Method;
class RootObject;

// For now just use Java style type descriptors.
typedef const char * RuntimeType;

// FIXME:  Parameter should be removed from abstract runtime classes.
class Parameter
{
public:
    virtual RuntimeType type() const = 0;
    virtual ~Parameter() {};
};

// FIXME:  Constructor should be removed from abstract runtime classes
// unless we want to support instantiation of runtime objects from
// JavaScript.
class Constructor
{
public:
    virtual Parameter *parameterAt(long i) const = 0;
    virtual long numParameters() const = 0;

    virtual ~Constructor() {}
};

class Field
{
public:
    virtual const char *name() const = 0;
    virtual RuntimeType type() const = 0;

    virtual ValueImp *valueFromInstance(ExecState *, const Instance *) const = 0;
    virtual void setValueToInstance(ExecState *, const Instance *, ValueImp *) const = 0;

    virtual ~Field() {}
};


class MethodList
{
public:
    MethodList() : _methods(0), _length(0) {}
    
    void addMethod(Method *aMethod);
    unsigned int length() const;
    Method *methodAt(unsigned int index) const;
    
    ~MethodList();
    
    MethodList(const MethodList &other);
    MethodList &operator=(const MethodList &other);

private:
    Method **_methods;
    unsigned int _length;
};


class Method
{
public:
    virtual const char *name() const = 0;

    virtual long numParameters() const = 0;
        
    virtual ~Method() {};
};

class Class
{
public:
    virtual const char *name() const = 0;
    
    virtual MethodList methodsNamed(const char *name, Instance *instance) const = 0;
    
    virtual Constructor *constructorAt(long i) const = 0;
    virtual long numConstructors() const = 0;
    
    virtual Field *fieldNamed(const char *name, Instance *instance) const = 0;

    virtual ValueImp *fallbackObject(ExecState *, Instance *, const Identifier &) { return Undefined(); }
    
    virtual ~Class() {}
};

typedef void (*KJSDidExecuteFunctionPtr)(ExecState *exec, ObjectImp *rootObject);

class Instance
{
public:
    typedef enum {
        JavaLanguage,
        ObjectiveCLanguage,
        CLanguage
    } BindingLanguage;

    static void setDidExecuteFunction(KJSDidExecuteFunctionPtr func);
    static KJSDidExecuteFunctionPtr didExecuteFunction();
    
    static Instance *createBindingForLanguageInstance(BindingLanguage language, void *nativeInstance, const RootObject *r = 0);
    static void *createLanguageInstanceForValue(ExecState *exec, BindingLanguage language, ObjectImp *value, const RootObject *origin, const RootObject *current);
    static ObjectImp *createRuntimeObject(BindingLanguage language, void *nativeInstance, const RootObject *r = 0);

    Instance() : _executionContext(0) {};
    
    Instance(const Instance &other);

    Instance &operator=(const Instance &other);

    // These functions are called before and after the main entry points into
    // the native implementations.  They can be used to establish and cleanup
    // any needed state.
    virtual void begin() {};
    virtual void end() {};
    
    virtual Class *getClass() const = 0;
    
    virtual ValueImp *getValueOfField(ExecState *exec, const Field *aField) const;
    virtual ValueImp *getValueOfUndefinedField(ExecState *exec, const Identifier &property, Type hint) const { return Undefined(); };
    virtual void setValueOfField(ExecState *exec, const Field *aField, ValueImp *aValue) const;
    virtual bool supportsSetValueOfUndefinedField() { return false; };
    virtual void setValueOfUndefinedField(ExecState *exec, const Identifier &property, ValueImp *aValue) {};
    
    virtual ValueImp *invokeMethod(ExecState *exec, const MethodList &method, const List &args) = 0;
    virtual ValueImp *invokeDefaultMethod(ExecState *exec, const List &args) = 0;
    
    virtual ValueImp *defaultValue(Type hint) const = 0;
    
    virtual ValueImp *valueOf() const { return String(getClass()->name()); };
    
    void setExecutionContext(const RootObject *r) { _executionContext = r; }
    const RootObject *executionContext() const { return _executionContext; }
    
    virtual ~Instance() {}

protected:
    const RootObject *_executionContext;
};

class Array
{
public:
    virtual void setValueAt(ExecState *, unsigned index, ValueImp *) const = 0;
    virtual ValueImp *valueAt(ExecState *, unsigned index) const = 0;
    virtual unsigned int getLength() const = 0;
    virtual ~Array() {}
};

const char *signatureForParameters(const List &aList);

} // namespace Bindings

} // namespace KJS

#endif
