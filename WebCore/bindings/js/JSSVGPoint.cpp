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

#include "JSSVGPoint.h"

#include "JSSVGMatrix.h"
#include "SVGMatrix.h"
#include "JSSVGPointTable.cpp"

using namespace KJS;

namespace WebCore {

const ClassInfo JSSVGPoint::info = { "SVGPoint", 0, &JSSVGPointTable, 0 };
/*
@begin JSSVGPointTable 4
  x         WebCore::JSSVGPoint::PointX       DontDelete
  y         WebCore::JSSVGPoint::PointY       DontDelete
@end
@begin JSSVGPointProtoTable 2
 matrixTransform    WebCore::JSSVGPoint::MatrixTransform DontDelete|Function 1
@end
*/

KJS_IMPLEMENT_PROTOFUNC(JSSVGPointProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("JSSVGPoint", JSSVGPointProto, JSSVGPointProtoFunc)

JSSVGPoint::JSSVGPoint(KJS::ExecState* exec, const FloatPoint& p) : m_point(p)
{
    setPrototype(JSSVGPointProto::self(exec));
}

JSSVGPoint::~JSSVGPoint()
{
}

bool JSSVGPoint::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSSVGPoint, DOMObject>(exec,  &JSSVGPointTable, this, propertyName, slot);
}

JSValue* JSSVGPoint::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
    case PointX:
        return jsNumber(m_point.x());
    case PointY:
        return jsNumber(m_point.y());
    default:
        return 0;
    }
}

void JSSVGPoint::put(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::JSValue* value, int attr)
{
    lookupPut<JSSVGPoint>(exec, propertyName, value, attr, &JSSVGPointTable, this);
}

void JSSVGPoint::putValueProperty(KJS::ExecState* exec, int token, KJS::JSValue* value, int attr)
{
    switch (token) {
    case PointX:
        m_point.setX(value->toNumber(exec));
        break;
    case PointY:
        m_point.setY(value->toNumber(exec));
        break;
    }
}

JSValue* JSSVGPointProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSSVGPoint::info))
        return throwError(exec, TypeError);

#ifdef SVG_SUPPORT
    FloatPoint point = static_cast<JSSVGPoint*>(thisObj)->impl();
    switch (id) {
        case JSSVGPoint::MatrixTransform: {
            SVGMatrix* mat = toSVGMatrix(args[0]);
            if (!mat)
                return jsUndefined();
            FloatPoint p = point.matrixTransform(mat->matrix());
            return getJSSVGPoint(exec, p);
        }
    }
#endif

    return jsUndefined();
}

JSValue* getJSSVGPoint(ExecState* exec, const FloatPoint& p)
{
    return new JSSVGPoint(exec, p);
}

FloatPoint toFloatPoint(JSValue* val)
{
    return val->isObject(&JSSVGPoint::info) ? static_cast<JSSVGPoint*>(val)->impl() : FloatPoint();
}

} // namespace WebCore

#endif // SVG_SUPPORT
