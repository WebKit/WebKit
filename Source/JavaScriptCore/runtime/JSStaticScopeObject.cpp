/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#include "JSStaticScopeObject.h"

#include "Error.h"

namespace JSC {
ASSERT_CLASS_FITS_IN_CELL(JSStaticScopeObject);

const ClassInfo JSStaticScopeObject::s_info = { "Object", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSStaticScopeObject) };

void JSStaticScopeObject::destroy(JSCell* cell)
{
    jsCast<JSStaticScopeObject*>(cell)->JSStaticScopeObject::~JSStaticScopeObject();
}

void JSStaticScopeObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSStaticScopeObject* thisObject = jsCast<JSStaticScopeObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    JSVariableObject::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_registerStore);
}

JSObject* JSStaticScopeObject::toThisObject(JSCell*, ExecState* exec)
{
    return exec->globalThisValue();
}

void JSStaticScopeObject::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    JSStaticScopeObject* thisObject = jsCast<JSStaticScopeObject*>(cell);
    if (slot.isStrictMode()) {
        // Double lookup in strict mode, but this only occurs when
        // a) indirectly writing to an exception slot
        // b) writing to a function expression name
        // (a) is unlikely, and (b) is an error.
        // Also with a single entry the symbol table lookup should simply be
        // a pointer compare.
        PropertySlot slot;
        bool isWritable = true;
        thisObject->symbolTableGet(propertyName, slot, isWritable);
        if (!isWritable) {
            throwError(exec, createTypeError(exec, StrictModeReadonlyPropertyWriteError));
            return;
        }
    }
    if (thisObject->symbolTablePut(exec, propertyName, value, slot.isStrictMode()))
        return;
    
    ASSERT_NOT_REACHED();
}

void JSStaticScopeObject::putDirectVirtual(JSObject* object, ExecState* exec, const Identifier& propertyName, JSValue value, unsigned attributes)
{
    JSStaticScopeObject* thisObject = jsCast<JSStaticScopeObject*>(object);
    if (thisObject->symbolTablePutWithAttributes(exec->globalData(), propertyName, value, attributes))
        return;
    
    ASSERT_NOT_REACHED();
}

bool JSStaticScopeObject::getOwnPropertySlot(JSCell* cell, ExecState*, const Identifier& propertyName, PropertySlot& slot)
{
    return jsCast<JSStaticScopeObject*>(cell)->symbolTableGet(propertyName, slot);
}

}
