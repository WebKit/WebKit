// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
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

#include "CSSRule.h"
#include "CSSRuleList.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "JSCSSPrimitiveValue.h"
#include "JSCSSRule.h"
#include "JSCSSRuleList.h"
#include "JSStyleSheet.h"
#include "StyleSheetList.h"
#include "kjs_dom.h"

#include "kjs_css.lut.h"

namespace WebCore {

using namespace KJS;
using namespace HTMLNames;

const ClassInfo JSStyleSheetList::info = { "StyleSheetList", 0, &JSStyleSheetListTable, 0 };

/*
@begin JSStyleSheetListTable 2
  length        WebCore::JSStyleSheetList::Length       DontDelete|ReadOnly
  item          WebCore::JSStyleSheetList::Item         DontDelete|Function 1
@end
*/

KJS_IMPLEMENT_PROTOTYPE_FUNCTION(JSStyleSheetListFunc) // not really a prototype, but doesn't matter

JSStyleSheetList::JSStyleSheetList(ExecState* exec, StyleSheetList* styleSheetList, Document* doc)
    : m_impl(styleSheetList)
    , m_doc(doc) 
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

JSStyleSheetList::~JSStyleSheetList()
{
    ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* JSStyleSheetList::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
        case Length:
            return jsNumber(m_impl->length());
        default:
            ASSERT_NOT_REACHED();
            return jsUndefined();
    }
}

JSValue* JSStyleSheetList::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSStyleSheetList* thisObj = static_cast<JSStyleSheetList*>(slot.slotBase());
    return toJS(exec, thisObj->m_impl->item(slot.index()));
}

JSValue* JSStyleSheetList::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSStyleSheetList* thisObj = static_cast<JSStyleSheetList*>(slot.slotBase());
    Element* element = thisObj->m_doc->getElementById(propertyName);
    return toJS(exec, static_cast<HTMLStyleElement*>(element)->sheet());
}

bool JSStyleSheetList::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    const HashEntry* entry = Lookup::findEntry(&JSStyleSheetListTable, propertyName);
  
    if (entry) {
        switch(entry->value) {
            case Length:
                slot.setStaticEntry(this, entry, staticValueGetter<JSStyleSheetList>);
                return true;
            case Item:
                slot.setStaticEntry(this, entry, staticFunctionGetter<JSStyleSheetListFunc>);
                return true;
        }
    }

    StyleSheetList* styleSheetList = m_impl.get();

    // Retrieve stylesheet by index
    bool ok;
    unsigned u = propertyName.toUInt32(&ok);
    if (ok && u < styleSheetList->length()) {
        slot.setCustomIndex(this, u, indexGetter);
        return true;
    }

    // IE also supports retrieving a stylesheet by name, using the name/id of the <style> tag
    // (this is consistent with all the other collections)
    // ### Bad implementation because returns a single element (are IDs always unique?)
    // and doesn't look for name attribute (see implementation above).
    // But unicity of stylesheet ids is good practice anyway ;)
    Element* element = m_doc->getElementById(propertyName);
    if (element && element->hasTagName(styleTag)) {
        slot.setCustom(this, nameGetter);
        return true;
    }

    return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue* toJS(ExecState* exec, StyleSheetList* styleSheetList, Document* doc)
{
    // Can't use the cacheDOMObject macro because of the doc argument
    if (!styleSheetList)
        return jsNull();

    ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
    DOMObject* ret = interp->getDOMObject(styleSheetList);
    if (ret)
        return ret;

    ret = new JSStyleSheetList(exec, styleSheetList, doc);
    interp->putDOMObject(styleSheetList, ret);
    return ret;
}

JSValue* JSStyleSheetListFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSStyleSheetList::info))
        return throwError(exec, TypeError);

    StyleSheetList* styleSheetList = static_cast<JSStyleSheetList*>(thisObj)->impl();
    if (id == JSStyleSheetList::Item)
        return toJS(exec, styleSheetList->item(args[0]->toInt32(exec)));
    return jsUndefined();
}

// -------------------------------------------------------------------------

const ClassInfo JSRGBColor::info = { "RGBColor", 0, &JSRGBColorTable, 0 };

/*
@begin JSRGBColorTable 3
  red   WebCore::JSRGBColor::Red        DontDelete|ReadOnly
  green WebCore::JSRGBColor::Green      DontDelete|ReadOnly
  blue  WebCore::JSRGBColor::Blue       DontDelete|ReadOnly
@end
*/

JSRGBColor::JSRGBColor(ExecState* exec, unsigned color) 
    : m_color(color) 
{ 
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
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
    return new JSRGBColor(exec, color);
}

} // namespace WebCore
