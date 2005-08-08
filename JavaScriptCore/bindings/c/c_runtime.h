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

#include <npruntime.h>
#include <npruntime_impl.h>

#include <runtime.h>
#include <ustring.h>

namespace KJS
{

namespace Bindings
{

class CInstance;

class CField : public Field
{
public:
    CField(NPIdentifier ident) : Field() {
        _fieldIdentifier = ident;
    };
    
    virtual ValueImp *valueFromInstance(ExecState *exec, const Instance *instance) const;
    virtual void setValueToInstance(ExecState *exec, const Instance *instance, ValueImp *aValue) const;
    
    virtual const char *name() const { return _NPN_UTF8FromIdentifier(_fieldIdentifier); }
    virtual RuntimeType type() const { return ""; }
    
private:
    NPIdentifier _fieldIdentifier;
};


class CMethod : public Method
{
public:
    CMethod() : Method(), _methodIdentifier(0) {};

    CMethod(NPIdentifier ident) : Method(), _methodIdentifier(ident) {};
    
    virtual const char *name() const { return _NPN_UTF8FromIdentifier(_methodIdentifier); };

    virtual long numParameters() const { return 0; };

private:
    NPIdentifier _methodIdentifier;
};

#if 0
class CArray : public Array
{
public:
    CArray (ObjectStructPtr a);

    CArray (const CArray &other);

    CArray &operator=(const CArray &other);
    
    virtual void setValueAt(ExecState *exec, unsigned int index, ValueImp *aValue) const;
    virtual ValueImp *valueAt(ExecState *exec, unsigned int index) const;
    virtual unsigned int getLength() const;
    
    virtual ~CArray();

private:
};
#endif


} // namespace Bindings

} // namespace KJS

#endif
