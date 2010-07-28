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

#include "JSNPObject.h"

#include "NPRuntimeObjectMap.h"
#include "NPRuntimeUtilities.h"
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ObjectPrototype.h>
#include <WebCore/IdentifierRep.h>

using namespace WebCore;
using namespace JSC;

namespace WebKit {

static NPIdentifier npIdentifierFromIdentifier(const Identifier& identifier)
{
    return static_cast<NPIdentifier>(IdentifierRep::get(identifier.ascii()));
}
    
JSNPObject::JSNPObject(ExecState*, JSGlobalObject* globalObject, NPRuntimeObjectMap* objectMap, NPObject* npObject)
    : JSObjectWithGlobalObject(globalObject, createStructure(globalObject->objectPrototype()))
    , m_objectMap(objectMap)
    , m_npObject(npObject)
{
    retainNPObject(m_npObject);
}

JSNPObject::~JSNPObject()
{
    // FIXME: Implement.
}

bool JSNPObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (!m_npObject) {
        throwInvalidAccessError(exec);
        return false;
    }
    
    NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
    if (m_npObject->_class->hasProperty && m_npObject->_class->hasProperty(m_npObject, npIdentifier)) {
        slot.setCustom(this, propertyGetter);
        return true;
    }

    // FIXME: Check methods.
    return false;
}

JSValue JSNPObject::propertyGetter(ExecState* exec, JSValue slotBase, const Identifier& propertyName)
{
    JSNPObject* thisObj = static_cast<JSNPObject*>(asObject(slotBase));

    if (!thisObj->m_npObject)
        return throwInvalidAccessError(exec);

    if (!thisObj->m_npObject->_class->getProperty)
        return jsUndefined();

    NPVariant property;
    VOID_TO_NPVARIANT(property);

    bool result;
    {
        JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);
        NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
        result = thisObj->m_npObject->_class->getProperty(thisObj->m_npObject, npIdentifier, &property);
        
        // FIXME: Handle getProperty setting an exception.
        // FIXME: Find out what happens if calling getProperty causes the plug-in to go away.
    }

    if (!result)
        return jsUndefined();

    return thisObj->m_objectMap->convertNPVariantToValue(exec, property);
}

JSObject* JSNPObject::throwInvalidAccessError(ExecState* exec)
{
    return throwError(exec, createReferenceError(exec, "Trying to access object from destroyed plug-in."));
}

} // namespace WebKit
