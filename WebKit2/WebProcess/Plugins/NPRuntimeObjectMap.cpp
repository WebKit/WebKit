/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "NPRuntimeObjectMap.h"

#include "NPRuntimeUtilities.h"
#include "PluginView.h"
#include <WebCore/Frame.h>
#include <WebCore/IdentifierRep.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/Protect.h>
#include <WebKit/npruntime.h>
#include <wtf/Noncopyable.h>

using namespace JSC;
using namespace WebCore;

namespace WebKit {

class NPJSObject : public NPObject, Noncopyable {
public:
    static NPJSObject* create(NPRuntimeObjectMap* objectMap, JSObject* jsObject);

private:
    NPJSObject()
        : m_objectMap(0)
    {
    }

    ~NPJSObject()
    {
        // Remove ourselves from the map.
        ASSERT(m_objectMap->m_objects.contains(m_jsObject.get()));
        m_objectMap->m_objects.remove(m_jsObject.get());
    }

    static bool isNPJSObject(NPObject*);

    static NPJSObject* toNPJSObject(NPObject* npObject)
    {
        ASSERT(isNPJSObject(npObject));
        return static_cast<NPJSObject*>(npObject);
    }

    void initialize(NPRuntimeObjectMap*, JSObject* jsObject);

    bool hasProperty(NPIdentifier);
    bool getProperty(NPIdentifier, NPVariant* result);

    static NPClass* npClass();
    static NPObject* NP_Allocate(NPP, NPClass*);
    static void NP_Deallocate(NPObject*);
    static bool NP_HasProperty(NPObject* npobj, NPIdentifier name);
    static bool NP_GetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result);
    
    NPRuntimeObjectMap* m_objectMap;
    ProtectedPtr<JSObject> m_jsObject;
};

NPJSObject* NPJSObject::create(NPRuntimeObjectMap* objectMap, JSObject* jsObject)
{
    NPJSObject* npJSObject = toNPJSObject(createNPObject(0, npClass()));
    npJSObject->initialize(objectMap, jsObject);

    return npJSObject;
}

bool NPJSObject::isNPJSObject(NPObject* npObject)
{
    return npObject->_class == npClass();
}

void NPJSObject::initialize(NPRuntimeObjectMap* objectMap, JSObject* jsObject)
{
    ASSERT(!m_objectMap);
    ASSERT(!m_jsObject);

    m_objectMap = objectMap;
    m_jsObject = jsObject;
}

static Identifier identifierFromIdentifierRep(ExecState* exec, IdentifierRep* identifierRep)
{
    ASSERT(identifierRep->isString());

    const char* string = identifierRep->string();
    int length = strlen(string);

    return Identifier(exec, String::fromUTF8WithLatin1Fallback(string, length).impl());
}

bool NPJSObject::hasProperty(NPIdentifier identifier)
{
    IdentifierRep* identifierRep = static_cast<IdentifierRep*>(identifier);
    
    Frame* frame = m_objectMap->m_pluginView->frame();
    if (!frame)
        return false;

    bool result;
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    if (identifierRep->isString())
        result = m_jsObject->hasProperty(exec, identifierFromIdentifierRep(exec, identifierRep));
    else
        result = m_jsObject->hasProperty(exec, identifierRep->number());

    exec->clearException();

    return result;
}

bool NPJSObject::getProperty(NPIdentifier identifier, NPVariant* result)
{
    // FIXME: Implement.
    return false;
}

NPClass* NPJSObject::npClass()
{
    static NPClass npClass = {
        NP_CLASS_STRUCT_VERSION,
        NP_Allocate,
        NP_Deallocate,
        0, 
        0,
        0,
        0,
        NP_HasProperty,
        NP_GetProperty,
        0,
        0,
        0,
        0
    };

    return &npClass;
}
    
NPObject* NPJSObject::NP_Allocate(NPP npp, NPClass* npClass)
{
    ASSERT_UNUSED(npp, !npp);

    return new NPJSObject;
}

void NPJSObject::NP_Deallocate(NPObject* npObject)
{
    NPJSObject* npJSObject = toNPJSObject(npObject);
    delete npJSObject;
}

bool NPJSObject::NP_HasProperty(NPObject* npObject, NPIdentifier propertyName)
{
    return toNPJSObject(npObject)->hasProperty(propertyName);
}
    
bool NPJSObject::NP_GetProperty(NPObject* npObject, NPIdentifier propertyName, NPVariant* result)
{
    return toNPJSObject(npObject)->getProperty(propertyName, result);
}

NPRuntimeObjectMap::NPRuntimeObjectMap(PluginView* pluginView)
    : m_pluginView(pluginView)
{
}

NPObject* NPRuntimeObjectMap::getOrCreateNPObject(JSObject* jsObject)
{
    // First, check if we already know about this object.
    if (NPJSObject* npJSObject = m_objects.get(jsObject)) {
        retainNPObject(npJSObject);
        return npJSObject;
    }

    NPJSObject* npJSObject = NPJSObject::create(this, jsObject);
    m_objects.set(jsObject, npJSObject);

    return npJSObject;
}

void NPRuntimeObjectMap::invalidate()
{
    Vector<NPJSObject*> npJSObjects;
    copyValuesToVector(m_objects, npJSObjects);

    // Deallocate all the object wrappers so we won't leak any JavaScript objects.
    for (size_t i = 0; i < npJSObjects.size(); ++i)
        deallocateNPObject(npJSObjects[i]);
    
    // We shouldn't have any objects left now.
    ASSERT(m_objects.isEmpty());
}

} // namespace WebKit
