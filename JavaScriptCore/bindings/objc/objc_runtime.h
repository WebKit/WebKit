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
#ifndef _OBJC_RUNTIME_H_
#define _OBJC_RUNTIME_H_

#include <CoreFoundation/CoreFoundation.h>

#include <runtime.h>
#include <ustring.h>

namespace KJS
{
class Value;

namespace Bindings
{

class ObjcParameter : public Parameter
{
public:
    ObjcParameter () {};
    
    ~ObjcParameter() {
    };

    ObjcParameter(const ObjcParameter &other) : Parameter() {
    };

    ObjcParameter &operator=(const ObjcParameter &other)
    {
        if (this == &other)
            return *this;
                    
        return *this;
    }
    
    virtual RuntimeType type() {}
    
private:
};


class ObjcConstructor : public Constructor
{
public:
    ObjcConstructor() : _parameters (0), _numParameters(0) {};
        
    ~ObjcConstructor() {
        delete [] _parameters;
    };

    void _commonCopy(const ObjcConstructor &other) {
        _numParameters = other._numParameters;
        _parameters = new ObjcParameter[_numParameters];
        long i;
        for (i = 0; i < _numParameters; i++) {
            _parameters[i] = other._parameters[i];
        }
    }
    
    ObjcConstructor(const ObjcConstructor &other) : Constructor() {
        _commonCopy (other);
    };

    ObjcConstructor &operator=(const ObjcConstructor &other)
    {
        if (this == &other)
            return *this;
            
        delete [] _parameters;
        
        _commonCopy (other);

        return *this;
    }

    virtual KJS::Value value() const { return KJS::Value(0); }
    virtual Parameter *parameterAt(long i) const { return &_parameters[i]; };
    virtual long numParameters() const { return _numParameters; };
    
private:
    ObjcParameter *_parameters;
    long _numParameters;
};


class ObjcField : public Field
{
public:
    ObjcField() {};
    ~ObjcField() {};

    ObjcField(const ObjcField &other);
    ObjcField &operator=(const ObjcField &other);
        
    virtual KJS::Value valueFromInstance(const Instance *instance) const;
    virtual void setValueToInstance(KJS::ExecState *exec, const Instance *instance, const KJS::Value &aValue) const;
    
    virtual const char *name() const;
    virtual RuntimeType type() const;
    
private:
};


class ObjcMethod : public Method
{
public:
    ObjcMethod() : Method();
    ~ObjcMethod ();

    ObjcMethod(const ObjcMethod &other) : Method();
    ObjcMethod &operator=(const ObjcMethod &other);

    virtual const char *name() const;
    virtual RuntimeType returnType() const;
    virtual Parameter *parameterAt(long i) const;
    virtual long numParameters() const;
    
private:
};

class ObjcArray : public Array
{
public:
    ObjcArray (jobject a, const char *type);
    virtual ~ObjcArray();

    ObjcArray (const ObjcArray &other);
    ObjcArray &operator=(const ObjcArray &other);

    virtual void setValueAt(KJS::ExecState *exec, unsigned int index, const KJS::Value &aValue) const;
    virtual KJS::Value valueAt(unsigned int index) const;
    virtual unsigned int getLength() const;
    

private:
};

} // namespace Bindings

} // namespace KJS

#endif
