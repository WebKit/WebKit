// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 James G. Speth (speth@end.com)
 *  Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "kjs_css.h"

#include "CSSPrimitiveValue.h"
#include "JSCSSPrimitiveValue.h"
#include "kjs_dom.h"

#include "kjs_css.lut.h"

namespace WebCore {

using namespace KJS;

const ClassInfo JSRGBColor::info = { "RGBColor", 0, &JSRGBColorTable };

/*
@begin JSRGBColorTable 3
  red   WebCore::JSRGBColor::Red        DontDelete|ReadOnly
  green WebCore::JSRGBColor::Green      DontDelete|ReadOnly
  blue  WebCore::JSRGBColor::Blue       DontDelete|ReadOnly
@end
*/

JSRGBColor::JSRGBColor(JSObject* prototype, unsigned color)
    : DOMObject(prototype)
    , m_color(color)
{
}

JSRGBColor::~JSRGBColor()
{
}

bool JSRGBColor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSRGBColor, DOMObject>(exec, &JSRGBColorTable, this, propertyName, slot);
}

JSValue* JSRGBColor::getValueProperty(ExecState* exec, int token) const
{
    int color = m_color;
    switch (token) {
        case Red:
            color >>= 8;
            // fall through
        case Green:
            color >>= 8;
            // fall through
        case Blue:
            return toJS(exec, new CSSPrimitiveValue(color & 0xFF, CSSPrimitiveValue::CSS_NUMBER));
        default:
            return 0;
    }
}

JSValue* getJSRGBColor(ExecState* exec, unsigned color)
{
    // FIXME: implement equals for RGBColor since they're not refcounted objects
    return new JSRGBColor(exec->lexicalGlobalObject()->objectPrototype(), color);
}

} // namespace WebCore
