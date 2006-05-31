/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "JSSVGNumber.h"

using namespace KJS;

namespace WebCore {

const ClassInfo JSSVGNumber::info = { "SVGNumber", 0, 0, 0 };

JSSVGNumber::~JSSVGNumber()
{
}

bool JSSVGNumber::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == "value") {
        slot.setCustom(this, getValue);
        return true;
    }
    return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue* JSSVGNumber::getValue(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSSVGNumber* thisObj = static_cast<JSSVGNumber*>(slot.slotBase());
    if (propertyName == "value")
        return jsNumber(thisObj->m_value);
    return 0;
}

JSValue* getJSSVGNumber(ExecState* exec, float v)
{
    return new JSSVGNumber(exec, v);
}

}
