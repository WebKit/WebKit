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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

using namespace WebCore;
using namespace HTMLNames;

namespace KJS {

const ClassInfo DOMStyleSheetList::info = { "StyleSheetList", 0, &DOMStyleSheetListTable, 0 };

/*
@begin DOMStyleSheetListTable 2
  length        DOMStyleSheetList::Length       DontDelete|ReadOnly
  item          DOMStyleSheetList::Item         DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMStyleSheetListFunc) // not really a prototype, but doesn't matter

DOMStyleSheetList::DOMStyleSheetList(ExecState* exec, StyleSheetList* ssl, Document* doc)
  : m_impl(ssl)
  , m_doc(doc) 
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

DOMStyleSheetList::~DOMStyleSheetList()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* DOMStyleSheetList::getValueProperty(ExecState* exec, int token) const
{
    switch(token) {
    case Length:
      return jsNumber(m_impl->length());
    default:
      ASSERT(0);
      return jsUndefined();
    }
}

JSValue* DOMStyleSheetList::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMStyleSheetList *thisObj = static_cast<DOMStyleSheetList*>(slot.slotBase());
  return toJS(exec, thisObj->m_impl->item(slot.index()));
}

JSValue* DOMStyleSheetList::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMStyleSheetList *thisObj = static_cast<DOMStyleSheetList*>(slot.slotBase());
  Element *element = thisObj->m_doc->getElementById(propertyName);
  return toJS(exec, static_cast<HTMLStyleElement*>(element)->sheet());
}

bool DOMStyleSheetList::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  const HashEntry* entry = Lookup::findEntry(&DOMStyleSheetListTable, propertyName);
  
  if (entry) {
    switch(entry->value) {
    case Length:
      slot.setStaticEntry(this, entry, staticValueGetter<DOMStyleSheetList>);
      return true;
    case Item:
      slot.setStaticEntry(this, entry, staticFunctionGetter<DOMStyleSheetListFunc>);
      return true;
    }
  }

  StyleSheetList &styleSheetList = *m_impl;

  // Retrieve stylesheet by index
  bool ok;
  unsigned u = propertyName.toUInt32(&ok);
  if (ok && u < styleSheetList.length()) {
    slot.setCustomIndex(this, u, indexGetter);
    return true;
  }

  // IE also supports retrieving a stylesheet by name, using the name/id of the <style> tag
  // (this is consistent with all the other collections)
  // ### Bad implementation because returns a single element (are IDs always unique?)
  // and doesn't look for name attribute (see implementation above).
  // But unicity of stylesheet ids is good practice anyway ;)
  Element *element = m_doc->getElementById(propertyName);
  if (element && element->hasTagName(styleTag)) {
    slot.setCustom(this, nameGetter);
    return true;
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue* toJS(ExecState* exec, StyleSheetList *ssl, Document *doc)
{
  // Can't use the cacheDOMObject macro because of the doc argument
  DOMObject *ret;
  if (!ssl)
    return jsNull();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
  if ((ret = interp->getDOMObject(ssl)))
    return ret;
  else {
    ret = new DOMStyleSheetList(exec, ssl, doc);
    interp->putDOMObject(ssl, ret);
    return ret;
  }
}

JSValue* DOMStyleSheetListFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMStyleSheetList::info))
    return throwError(exec, TypeError);
  StyleSheetList &styleSheetList = *static_cast<DOMStyleSheetList*>(thisObj)->impl();
  if (id == DOMStyleSheetList::Item)
    return toJS(exec, styleSheetList.item(args[0]->toInt32(exec)));
  return jsUndefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMRGBColor::info = { "RGBColor", 0, &DOMRGBColorTable, 0 };

/*
@begin DOMRGBColorTable 3
  red   DOMRGBColor::Red        DontDelete|ReadOnly
  green DOMRGBColor::Green      DontDelete|ReadOnly
  blue  DOMRGBColor::Blue       DontDelete|ReadOnly
@end
*/
DOMRGBColor::DOMRGBColor(ExecState* exec, unsigned color) 
: m_color(color) 
{ 
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

DOMRGBColor::~DOMRGBColor()
{
  //rgbColors.remove(rgbColor.handle());
}

bool DOMRGBColor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMRGBColor, DOMObject>(exec, &DOMRGBColorTable, this, propertyName, slot);
}

JSValue* DOMRGBColor::getValueProperty(ExecState* exec, int token) const
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
    return NULL;
  }
}

JSValue* getDOMRGBColor(ExecState* exec, unsigned c)
{
  // ### implement equals for RGBColor since they're not refcounted objects
  return new DOMRGBColor(exec, c);
}

// -------------------------------------------------------------------------

const ClassInfo DOMRect::info = { "Rect", 0, &DOMRectTable, 0 };
/*
@begin DOMRectTable 4
  top    DOMRect::Top     DontDelete|ReadOnly
  right  DOMRect::Right   DontDelete|ReadOnly
  bottom DOMRect::Bottom  DontDelete|ReadOnly
  left   DOMRect::Left    DontDelete|ReadOnly
@end
*/
DOMRect::DOMRect(ExecState* exec, RectImpl* r) 
: m_rect(r) 
{ 
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

DOMRect::~DOMRect()
{
  ScriptInterpreter::forgetDOMObject(m_rect.get());
}

bool DOMRect::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMRect, DOMObject>(exec,  &DOMRectTable, this, propertyName, slot);
}

JSValue* DOMRect::getValueProperty(ExecState* exec, int token) const
{
  RectImpl &rect = *m_rect;
  switch (token) {
  case Top:
    return toJS(exec, rect.top());
  case Right:
    return toJS(exec, rect.right());
  case Bottom:
    return toJS(exec, rect.bottom());
  case Left:
    return toJS(exec, rect.left());
  default:
    return NULL;
  }
}

JSValue* toJS(ExecState* exec, RectImpl *r)
{
  return cacheDOMObject<RectImpl, DOMRect>(exec, r);
}

}
