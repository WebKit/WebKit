/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "c_class.h"

#include "c_instance.h"
#include "c_runtime.h"
#include "npruntime_impl.h"
#include <runtime/Identifier.h>
#include <runtime/JSGlobalObject.h>
#include <runtime/JSObject.h>
#include <wtf/text/StringHash.h>

namespace JSC { namespace Bindings {

CClass::CClass(NPClass* aClass)
{
    m_isa = aClass;
}

CClass::~CClass()
{
    m_methods.clear();
    m_fields.clear();
}

typedef HashMap<NPClass*, CClass*> ClassesByIsAMap;
static ClassesByIsAMap* classesByIsA = 0;

CClass* CClass::classForIsA(NPClass* isa)
{
    if (!classesByIsA)
        classesByIsA = new ClassesByIsAMap;

    CClass* aClass = classesByIsA->get(isa);
    if (!aClass) {
        aClass = new CClass(isa);
        classesByIsA->set(isa, aClass);
    }

    return aClass;
}

Method* CClass::methodNamed(PropertyName propertyName, Instance* instance) const
{
    String name(propertyName.publicName());
    
    if (Method* method = m_methods.get(name.impl()))
        return method;

    NPIdentifier ident = _NPN_GetStringIdentifier(name.ascii().data());
    const CInstance* inst = static_cast<const CInstance*>(instance);
    NPObject* obj = inst->getObject();
    if (m_isa->hasMethod && m_isa->hasMethod(obj, ident)) {
        Method* method = new CMethod(ident);
        m_methods.set(name.impl(), adoptPtr(method));
        return method;
    }
    
    return 0;
}

Field* CClass::fieldNamed(PropertyName propertyName, Instance* instance) const
{
    String name(propertyName.publicName());
    
    if (Field* field = m_fields.get(name.impl()))
        return field;

    NPIdentifier ident = _NPN_GetStringIdentifier(name.ascii().data());
    const CInstance* inst = static_cast<const CInstance*>(instance);
    NPObject* obj = inst->getObject();
    if (m_isa->hasProperty && m_isa->hasProperty(obj, ident)) {
        Field* field = new CField(ident);
        m_fields.set(name.impl(), adoptPtr(field));
        return field;
    }

    return 0;
}

} } // namespace JSC::Bindings

#endif // ENABLE(NETSCAPE_PLUGIN_API)
