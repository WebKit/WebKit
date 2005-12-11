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
#include "config.h"
#include <c_class.h>
#include <c_instance.h>
#include <c_runtime.h>
#include <c_utility.h>

#include <JavaScriptCore/npruntime_impl.h>
#include <JavaScriptCore/npruntime_priv.h>

#ifdef NDEBUG
#define C_LOG(formatAndArgs...) ((void)0)
#else
#define C_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:%d -- %s:  ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}
#endif

namespace KJS {
namespace Bindings {

CInstance::CInstance (NPObject *o) 
{
    _object = _NPN_RetainObject (o);
    _class = 0;
    setExecutionContext (0);
}

CInstance::~CInstance () 
{
    _NPN_ReleaseObject (_object);
    delete _class;
}


CInstance::CInstance (const CInstance &other) : Instance() 
{
    _object = _NPN_RetainObject (other._object);
    _class = 0;
    setExecutionContext (other.executionContext());
};

CInstance &CInstance::operator=(const CInstance &other){
    if (this == &other)
        return *this;
    
    NPObject *_oldObject = _object;
    _object= _NPN_RetainObject (other._object);
    _NPN_ReleaseObject (_oldObject);
    _class = 0;
    
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

ValueImp *CInstance::invokeMethod (ExecState *exec, const MethodList &methodList, const List &args)
{
    ValueImp *resultValue;

    // Overloading methods are not allowed by NPObjects.  Should only be one
    // name match for a particular method.
    assert (methodList.length() == 1);

    CMethod *method = 0;
    method = static_cast<CMethod*>(methodList.methodAt(0));

    NPIdentifier ident = _NPN_GetStringIdentifier (method->name());
    if (!_object->_class->hasMethod (_object, ident)) {
        return jsUndefined();
    }

    unsigned i, count = args.size();
    NPVariant *cArgs;
    NPVariant localBuffer[128];
    if (count > 128)
        cArgs = (NPVariant *)malloc (sizeof(NPVariant)*count);
    else
        cArgs = localBuffer;
    
    for (i = 0; i < count; i++) {
        convertValueToNPVariant (exec, args.at(i), &cArgs[i]);
    }

    // Invoke the 'C' method.
    NPVariant resultVariant;
    VOID_TO_NPVARIANT(resultVariant);
    _object->_class->invoke (_object, ident, cArgs, count, &resultVariant);

    for (i = 0; i < count; i++) {
        _NPN_ReleaseVariantValue (&cArgs[i]);
    }

    if (cArgs != localBuffer)
        free ((void *)cArgs);
            
    if (!NPVARIANT_IS_VOID(resultVariant)) {
        resultValue = convertNPVariantToValue (exec, &resultVariant);
        
        _NPN_ReleaseVariantValue (&resultVariant);
        
        return resultValue;
    }
    
    return jsUndefined();
}


ValueImp *CInstance::invokeDefaultMethod (ExecState *exec, const List &args)
{
    ValueImp *resultValue;

    if (_object->_class->invokeDefault) {     
        unsigned i, count = args.size();
        NPVariant *cArgs;
        NPVariant localBuffer[128];
        if (count > 128)
            cArgs = (NPVariant *)malloc (sizeof(NPVariant)*count);
        else
            cArgs = localBuffer;
        
        for (i = 0; i < count; i++) {
            convertValueToNPVariant (exec, args.at(i), &cArgs[i]);
        }

        // Invoke the 'C' method.
        NPVariant resultVariant;
        VOID_TO_NPVARIANT(resultVariant);
        _object->_class->invokeDefault (_object, cArgs, count, &resultVariant);

        for (i = 0; i < count; i++) {
            _NPN_ReleaseVariantValue (&cArgs[i]);
        }

        if (cArgs != localBuffer)
            free ((void *)cArgs);
                
        if (!NPVARIANT_IS_VOID(resultVariant)) {
            resultValue = convertNPVariantToValue (exec, &resultVariant);
            
            _NPN_ReleaseVariantValue (&resultVariant);
            
            return resultValue;
        }
    }
    
    return jsUndefined();
}


ValueImp *CInstance::defaultValue (Type hint) const
{
    if (hint == StringType) {
        return stringValue();
    }
    else if (hint == NumberType) {
        return numberValue();
    }
    else if (hint == BooleanType) {
        return booleanValue();
    }
    
    return valueOf();
}

ValueImp *CInstance::stringValue() const
{
    char buf[1024];
    snprintf(buf, 1024, "NPObject %p, NPClass %p", _object, _object->_class);
    return jsString(buf);
}

ValueImp *CInstance::numberValue() const
{
    // FIXME:  Implement something sensible
    return jsNumber(0);
}

ValueImp *CInstance::booleanValue() const
{
    // FIXME:  Implement something sensible
    return jsBoolean(false);
}

ValueImp *CInstance::valueOf() const 
{
    return stringValue();
}

}
}
