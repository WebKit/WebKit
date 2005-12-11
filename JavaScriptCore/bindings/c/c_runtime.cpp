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

#include "config.h"
#include <JavaScriptCore/internal.h>

#include <c_instance.h>
#include <c_runtime.h>
#include <c_utility.h>

#include <runtime_array.h>
#include <runtime_object.h>

using namespace KJS;
using namespace KJS::Bindings;

// ---------------------- CMethod ----------------------



// ---------------------- CField ----------------------

JSValue *CField::valueFromInstance(ExecState *exec, const Instance *inst) const
{
    const CInstance *instance = static_cast<const CInstance*>(inst);
    NPObject *obj = instance->getObject();
    JSValue *aValue;
    NPVariant property;
    VOID_TO_NPVARIANT(property);
    if (obj->_class->getProperty) {
        obj->_class->getProperty (obj, _fieldIdentifier, &property);
        aValue = convertNPVariantToValue (exec, &property);
    }
    else {
        aValue = jsUndefined();
    }
    return aValue;
}

void CField::setValueToInstance(ExecState *exec, const Instance *inst, JSValue *aValue) const
{
    const CInstance *instance = static_cast<const CInstance*>(inst);
    NPObject *obj = instance->getObject();
    if (obj->_class->setProperty) {
        NPVariant variant;
        convertValueToNPVariant (exec, aValue, &variant);
        obj->_class->setProperty (obj, _fieldIdentifier, &variant);
    }
}

// ---------------------- CArray ----------------------
