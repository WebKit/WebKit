/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSRGBColor.h"

#include "CSSPrimitiveValue.h"
#include "JSCSSPrimitiveValue.h"

using namespace JSC;

static JSValue jsRGBColorRed(ExecState*, const Identifier&, const PropertySlot&);
static JSValue jsRGBColorGreen(ExecState*, const Identifier&, const PropertySlot&);
static JSValue jsRGBColorBlue(ExecState*, const Identifier&, const PropertySlot&);

/*
@begin JSRGBColorTable
  red   jsRGBColorRed        DontDelete|ReadOnly
  green jsRGBColorGreen      DontDelete|ReadOnly
  blue  jsRGBColorBlue       DontDelete|ReadOnly
@end
*/

#include "JSRGBColor.lut.h"

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSRGBColor);

const ClassInfo JSRGBColor::s_info = { "RGBColor", 0, &JSRGBColorTable, 0 };

JSRGBColor::JSRGBColor(ExecState* exec, unsigned color)
    : DOMObject(getDOMStructure<JSRGBColor>(exec))
    , m_color(color)
{
}

bool JSRGBColor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSRGBColor, DOMObject>(exec, &JSRGBColorTable, this, propertyName, slot);
}

JSValue getJSRGBColor(ExecState* exec, unsigned color)
{
    return new (exec) JSRGBColor(exec, color);
}

} // namespace WebCore

using namespace WebCore;

JSValue jsRGBColorRed(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return toJS(exec, CSSPrimitiveValue::create((static_cast<JSRGBColor*>(asObject(slot.slotBase()))->impl() >> 16) & 0xFF, CSSPrimitiveValue::CSS_NUMBER));
}

JSValue jsRGBColorGreen(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return toJS(exec, CSSPrimitiveValue::create((static_cast<JSRGBColor*>(asObject(slot.slotBase()))->impl() >> 8) & 0xFF, CSSPrimitiveValue::CSS_NUMBER));
}

JSValue jsRGBColorBlue(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return toJS(exec, CSSPrimitiveValue::create(static_cast<JSRGBColor*>(asObject(slot.slotBase()))->impl() & 0xFF, CSSPrimitiveValue::CSS_NUMBER));
}

