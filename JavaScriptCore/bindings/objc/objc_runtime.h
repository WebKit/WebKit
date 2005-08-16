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

#include "runtime.h"
#include "ustring.h"

#include "objc_header.h"

namespace KJS {
namespace Bindings {

class ObjcInstance;

class ObjcField : public Field
{
public:
    ObjcField(Ivar ivar);
    ObjcField(CFStringRef name);
    
    ~ObjcField()
    {
        if (_name)
            CFRelease(_name);
    }

    ObjcField(const ObjcField &other) : Field()
    {
        _ivar = other._ivar;

        if (other._name)
            _name = (CFStringRef)CFRetain(other._name);
        else 
            _name = 0;
    }
    
    ObjcField &operator=(const ObjcField &other)
    {
        if (this == &other)
            return *this;

        _ivar = other._ivar;
        
        if (other._name != _name) {
            if (_name)
                CFRelease(_name);
            if (other._name)
                _name = (CFStringRef)CFRetain(other._name);
            else 
                _name = 0;
        }
        
        return *this;
    }

    virtual ValueImp *valueFromInstance(ExecState *exec, const Instance *instance) const;
    virtual void setValueToInstance(ExecState *exec, const Instance *instance, ValueImp *aValue) const;
    
    virtual const char *name() const;
    virtual RuntimeType type() const;
        
private:
    Ivar _ivar;
    CFStringRef _name;
};

class ObjcMethod : public Method
{
public:
    ObjcMethod() : _objcClass(0), _selector(0) {}
    ObjcMethod(struct objc_class *aClass, const char *_selector);
    ~ObjcMethod ()
    {
        if (_javaScriptName);
            CFRelease(_javaScriptName);
    }

    ObjcMethod(const ObjcMethod &other) : Method()
    {
        _objcClass = other._objcClass;
        _selector = other._selector;
    }
    
    ObjcMethod &operator=(const ObjcMethod &other)
    {
        if (this == &other)
            return *this;
        _objcClass = other._objcClass;
        _selector = other._selector;
        return *this;
    }

    virtual const char *name() const;

    virtual long numParameters() const;

    NSMethodSignature *getMethodSignature() const;
    
    bool isFallbackMethod() const { return strcmp(_selector, "invokeUndefinedMethodFromWebScript:withArguments:") == 0; }
    void setJavaScriptName(CFStringRef n);
    CFStringRef javaScriptName() const { return _javaScriptName; }
    
private:
    struct objc_class *_objcClass;
    const char *_selector;
    CFStringRef _javaScriptName;
};

class ObjcArray : public Array
{
public:
    ObjcArray(ObjectStructPtr);

    ObjcArray(const ObjcArray &);
    ObjcArray &operator=(const ObjcArray &);
    
    virtual ~ObjcArray();

    virtual void setValueAt(ExecState *exec, unsigned int index, ValueImp *aValue) const;
    virtual ValueImp *valueAt(ExecState *exec, unsigned int index) const;
    virtual unsigned int getLength() const;
    
    ObjectStructPtr getObjcArray() const { return _array; }

    static ValueImp *convertObjcArrayToArray(ExecState *exec, ObjectStructPtr anObject);

private:
    ObjectStructPtr _array;
};

class ObjcFallbackObjectImp : public ObjectImp {
public:
    ObjcFallbackObjectImp(ObjectImp *proto);
    ObjcFallbackObjectImp(ObjcInstance *i, const Identifier propertyName);

    const ClassInfo *classInfo() const { return &info; }

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual bool canPut(ExecState *exec, const Identifier &propertyName) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);
    virtual ValueImp *defaultValue(ExecState *exec, Type hint) const;

    virtual Type type() const;
    virtual bool toBoolean(ExecState *exec) const;

private:
    static const ClassInfo info;

    ObjcInstance *_instance;
    Identifier _item;
};

} // namespace Bindings
} // namespace KJS

#endif
