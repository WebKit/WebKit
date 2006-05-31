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
#include "JSSVGRect.h"

#include "JSSVGRectTable.cpp"

using namespace KJS;

namespace WebCore {

const ClassInfo JSSVGRect::info = { "SVGRect", 0, &JSSVGRectTable, 0 };
/*
@begin JSSVGRectTable 4
  x         WebCore::JSSVGRect::RectX           DontDelete|ReadOnly
  y         WebCore::JSSVGRect::RectY           DontDelete|ReadOnly
  width     WebCore::JSSVGRect::RectWidth       DontDelete|ReadOnly
  height    WebCore::JSSVGRect::RectHeight      DontDelete|ReadOnly
@end
*/

JSSVGRect::~JSSVGRect()
{
}

bool JSSVGRect::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSSVGRect, DOMObject>(exec,  &JSSVGRectTable, this, propertyName, slot);
}

JSValue* JSSVGRect::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
    case RectX:
        return jsNumber(m_rect.x());
    case RectY:
        return jsNumber(m_rect.y());
    case RectWidth:
        return jsNumber(m_rect.width());
    case RectHeight:
        return jsNumber(m_rect.height());
    default:
        return 0;
    }
}

JSValue* getJSSVGRect(ExecState* exec, const FloatRect& r)
{
    return new JSSVGRect(exec, r);
}

FloatRect toFloatRect(JSValue* val)
{
    return val->isObject(&JSSVGRect::info) ? static_cast<JSSVGRect*>(val)->impl() : FloatRect();
}

}
