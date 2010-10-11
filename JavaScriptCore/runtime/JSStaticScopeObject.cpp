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

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSStaticScopeObject);

void JSStaticScopeObject::markChildren(MarkStack& markStack)
{
    JSVariableObject::markChildren(markStack);
    markStack.append(d()->registerStore.jsValue());
}

JSObject* JSStaticScopeObject::toThisObject(ExecState* exec) const
{
    return exec->globalThisValue();
}

JSValue JSStaticScopeObject::toStrictThisObject(ExecState*) const
{
    return jsNull();
}

void JSStaticScopeObject::put(ExecState*, const Identifier& propertyName, JSValue value, PutPropertySlot&)
{
    if (symbolTablePut(propertyName, value))
        return;
    
    ASSERT_NOT_REACHED();
}

void JSStaticScopeObject::putWithAttributes(ExecState*, const Identifier& propertyName, JSValue value, unsigned attributes)
{
    if (symbolTablePutWithAttributes(propertyName, value, attributes))
        return;
    
    ASSERT_NOT_REACHED();
}

bool JSStaticScopeObject::isDynamicScope(bool&) const
{
    return false;
}

JSStaticScopeObject::~JSStaticScopeObject()
{
    ASSERT(d());
    delete d();
}

inline bool JSStaticScopeObject::getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot& slot)
{
    return symbolTableGet(propertyName, slot);
}

}
