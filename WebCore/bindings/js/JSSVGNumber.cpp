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

#ifdef SVG_SUPPORT

#include "JSSVGNumber.h"

#include "JSSVGNumberTable.cpp"

using namespace KJS;

namespace WebCore {

const ClassInfo JSSVGNumber::info = { "SVGNumber", 0, &JSSVGNumberTable, 0 };

/*
@begin JSSVGNumberTable 4
  value         WebCore::JSSVGNumber::NumberValue       DontDelete
@end
*/

JSSVGNumber::~JSSVGNumber()
{
}

bool JSSVGNumber::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSSVGNumber, DOMObject>(exec,  &JSSVGNumberTable, this, propertyName, slot);
}

JSValue* JSSVGNumber::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
    case NumberValue:
        return jsNumber(m_value);
    default:
        return 0;
    }
}

void JSSVGNumber::put(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::JSValue* value, int attr)
{
    lookupPut<JSSVGNumber>(exec, propertyName, value, attr, &JSSVGNumberTable, this);
}

void JSSVGNumber::putValueProperty(KJS::ExecState* exec, int token, KJS::JSValue* value, int attr)
{
    switch (token) {
    case NumberValue:
        m_value = value->toNumber(exec);
        break;
    }
}

JSValue* getJSSVGNumber(ExecState* exec, double v)
{
    return new JSSVGNumber(exec, v);
}

} // namespace WebCore

#endif // SVG_SUPPORT
