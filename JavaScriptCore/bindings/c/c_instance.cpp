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
#include <c_class.h>
#include <c_instance.h>
#include <c_runtime.h>
#include <c_utility.h>

#ifdef NDEBUG
#define C_LOG(formatAndArgs...) ((void)0)
#else
#define C_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:%d -- %s:  ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}
#endif

using namespace KJS::Bindings;
using namespace KJS;

CInstance::CInstance (NP_Object *o) 
{
    _object = NP_RetainObject (o);
};

CInstance::~CInstance () 
{
    NP_ReleaseObject (_object);
    delete _class;
}


CInstance::CInstance (const CInstance &other) : Instance() 
{
    _object = NP_RetainObject (other._object);
};

CInstance &CInstance::operator=(const CInstance &other){
    if (this == &other)
        return *this;
    
    NP_Object *_oldObject = _object;
    _object= NP_RetainObject (other._object);
    NP_ReleaseObject (_oldObject);
    
    return *this;
};

Class *CInstance::getClass() const
{
    if (!_class) {
        _class = new CClass (_object->_class);
    }
    return _class;
}


void CInstance::begin()
{
    // Do nothing.
}

void CInstance::end()
{
    // Do nothing.
}

Value CInstance::invokeMethod (KJS::ExecState *exec, const MethodList &methodList, const List &args)
{
    Value resultValue;

    // Overloading methods are not allowed by NP_Objects.  Should only be one
    // name match for a particular method.
    assert (methodList.length() == 1);

    CMethod *method = 0;
    method = static_cast<CMethod*>(methodList.methodAt(0));

    NP_Identifier ident = NP_IdentifierFromUTF8 (method->name());
    if (!_object->_class->hasMethod (_object->_class, ident))
        return Undefined();

    unsigned i, count = args.size();
    NP_Object **cArgs;
    NP_Object *localBuffer[128];
    if (count > 128)
        cArgs = (NP_Object **)malloc (sizeof(NP_Object *)*count);
    else
        cArgs = localBuffer;
    
    for (i = 0; i < count; i++) {
        cArgs[i] = convertValueToNPValueType (exec, args.at(i));
    }

    // Invoke the 'C' method.
    NP_Object *result = _object->_class->invoke (_object, ident, cArgs, count);
    
    resultValue = convertNPValueTypeToValue (exec, result);
    
    if (cArgs != localBuffer)
        free ((void *)cArgs);
        
    NP_ReleaseObject (result);
    
    return resultValue;
}


KJS::Value CInstance::defaultValue (KJS::Type hint) const
{
    if (hint == KJS::StringType) {
        return stringValue();
    }
    else if (hint == KJS::NumberType) {
        return numberValue();
    }
    else if (hint == KJS::BooleanType) {
        return booleanValue();
    }
    else if (hint == KJS::UnspecifiedType) {
        if (NP_IsKindOfClass (_object, NP_StringClass)) {
            return stringValue();
        }
        else if (NP_IsKindOfClass (_object, NP_NumberClass)) {
            return numberValue();
        }
        else if (NP_IsKindOfClass (_object, NP_BooleanClass)) {
            return booleanValue();
        }
        else if (NP_IsKindOfClass (_object, NP_NullClass)) {
            return Null();
        }
        else if (NP_IsKindOfClass (_object, NP_UndefinedClass)) {
            return Undefined();
        }
    }
    
    return valueOf();
}

KJS::Value CInstance::stringValue() const
{
    // FIXME:  Implement something sensible, like calling toString...
    KJS::String v("");
    return v;
}

KJS::Value CInstance::numberValue() const
{
    // FIXME:  Implement something sensible
    KJS::Number v(0);
    return v;
}

KJS::Value CInstance::booleanValue() const
{
    // FIXME:  Implement something sensible
    KJS::Boolean v((bool)0);
    return v;
}

KJS::Value CInstance::valueOf() const 
{
    return stringValue();
};
