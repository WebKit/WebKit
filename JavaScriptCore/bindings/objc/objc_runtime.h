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
#ifndef _BINDINGS_OBJC_RUNTIME_H_
#define _BINDINGS_OBJC_RUNTIME_H_

#include <CoreFoundation/CoreFoundation.h>

#include <runtime.h>
#include <ustring.h>

#include <objc_header.h>

namespace KJS
{
class Value;

namespace Bindings
{

class ObjcField : public Field
{
public:
    ObjcField(Ivar ivar);
    
    ~ObjcField() {};

    ObjcField(const ObjcField &other) : Field() {
        _ivar = other._ivar;
    };
    
    ObjcField &operator=(const ObjcField &other) {
        if (this == &other)
            return *this;

        _ivar = other._ivar;
        
        return *this;
    };
        
    virtual KJS::Value valueFromInstance(KJS::ExecState *exec, const Instance *instance) const;
    virtual void setValueToInstance(KJS::ExecState *exec, const Instance *instance, const KJS::Value &aValue) const;
    
    virtual const char *name() const;
    virtual RuntimeType type() const;
    
private:
    Ivar _ivar;
};


class ObjcMethod : public Method
{
public:
    ObjcMethod() : Method(), _objcClass(0), _selector(0) {};

    ObjcMethod(struct objc_class *aClass, const char *_selector);
    ~ObjcMethod () {
    };

    ObjcMethod(const ObjcMethod &other) : Method() {
        _objcClass = other._objcClass;
        _selector = other._selector;
    };
    
    ObjcMethod &operator=(const ObjcMethod &other) {
        if (this == &other)
            return *this;

        _objcClass = other._objcClass;
        _selector = other._selector;
        
        return *this;
    }

    virtual const char *name() const;

    virtual long numParameters() const;
    
    NSMethodSignature *getMethodSignature() const;
    
private:
    struct objc_class *_objcClass;
    const char *_selector;
};

class ObjcArray : public Array
{
public:
    ObjcArray (ObjectStructPtr a);

    ObjcArray (const ObjcArray &other);

    ObjcArray &operator=(const ObjcArray &other);
    
    virtual void setValueAt(KJS::ExecState *exec, unsigned int index, const KJS::Value &aValue) const;
    virtual KJS::Value valueAt(KJS::ExecState *exec, unsigned int index) const;
    virtual unsigned int getLength() const;
    
    virtual ~ObjcArray();

    ObjectStructPtr getObjcArray() const { return _array; }

    static KJS::Value convertObjcArrayToArray (KJS::ExecState *exec, ObjectStructPtr anObject);

private:
    ObjectStructPtr _array;
};

} // namespace Bindings

} // namespace KJS

#endif
