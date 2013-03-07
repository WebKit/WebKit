/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSAPIWrapperObject.h"

#include "JSCJSValueInlines.h"
#include "JSCallbackObject.h"
#include "JSCellInlines.h"
#include "SlotVisitorInlines.h"
#include "Structure.h"
#include "StructureInlines.h"

namespace JSC {
    
template <> const ClassInfo JSCallbackObject<JSAPIWrapperObject>::s_info = { "EnumerableCallbackObject", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSCallbackObject) };

template<> const bool JSCallbackObject<JSAPIWrapperObject>::needsDestruction = true;

template <>
Structure* JSCallbackObject<JSAPIWrapperObject>::createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto)
{
    return Structure::create(globalData, globalObject, proto, TypeInfo(ObjectType, StructureFlags), &s_info);
}

JSAPIWrapperObject::JSAPIWrapperObject(JSGlobalData& globalData, Structure* structure)
    : Base(globalData, structure)
    , m_wrappedObject(0)
{
}

void JSAPIWrapperObject::visitChildren(JSCell* cell, JSC::SlotVisitor& visitor)
{
    JSAPIWrapperObject* thisObject = JSC::jsCast<JSAPIWrapperObject*>(cell);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    Base::visitChildren(cell, visitor);

    if (thisObject->wrappedObject())
        visitor.addOpaqueRoot(thisObject->wrappedObject());
}

} // namespace JSC
