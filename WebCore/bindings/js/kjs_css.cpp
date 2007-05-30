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

#include "CSSCharsetRule.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSValueList.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "JSCSSPrimitiveValue.h"
#include "JSCSSRule.h"
#include "JSCSSRuleList.h"
#include "JSCSSStyleDeclaration.h"
#include "JSCSSValueList.h"
#include "JSStyleSheet.h"
#include "MediaList.h"
#include "StyleSheetList.h"
#include "kjs_dom.h"

#include <kjs/string_object.h>

#include "kjs_css.lut.h"

#if ENABLE(SVG)
#include "JSSVGColor.h"
#include "JSSVGPaint.h"
#include "SVGColor.h"
#include "SVGPaint.h"
#endif

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

const ClassInfo DOMMediaList::info = { "MediaList", 0, &DOMMediaListTable, 0 };

/*
@begin DOMMediaListTable 2
  mediaText     DOMMediaList::MediaText         DontDelete|ReadOnly
  length        DOMMediaList::Length            DontDelete|ReadOnly
@end
@begin DOMMediaListPrototypeTable 3
  item          DOMMediaList::Item              DontDelete|Function 1
  deleteMedium  DOMMediaList::DeleteMedium      DontDelete|Function 1
  appendMedium  DOMMediaList::AppendMedium      DontDelete|Function 1
@end
*/
KJS_DEFINE_PROTOTYPE(DOMMediaListPrototype)
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMMediaListPrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("DOMMediaList", DOMMediaListPrototype, DOMMediaListPrototypeFunction)

DOMMediaList::DOMMediaList(ExecState* exec, MediaList *ml)
    : m_impl(ml)
{
  setPrototype(DOMMediaListPrototype::self(exec));
}

DOMMediaList::~DOMMediaList()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* DOMMediaList::getValueProperty(ExecState* exec, int token)
{
  switch (token) {
  case MediaText:
    return jsStringOrNull(m_impl->mediaText());
  case Length:
    return jsNumber(m_impl->length());
  default:
    ASSERT(0);
    return jsUndefined();
  }
}

JSValue* DOMMediaList::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMMediaList *thisObj = static_cast<DOMMediaList*>(slot.slotBase());
  return jsStringOrNull(thisObj->m_impl->item(slot.index()));
}

bool DOMMediaList::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  const HashEntry* entry = Lookup::findEntry(&DOMMediaListTable, propertyName);
  if (entry) {
    slot.setStaticEntry(this, entry, staticValueGetter<DOMMediaList>);
    return true;
  }

  bool ok;
  unsigned u = propertyName.toUInt32(&ok);
  if (ok && u < m_impl->length()) {
    slot.setCustomIndex(this, u, indexGetter);
    return true;
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

void DOMMediaList::put(ExecState* exec, const Identifier &propertyName, JSValue* value, int attr)
{
  DOMExceptionTranslator exception(exec);
  MediaList &mediaList = *m_impl;
  if (propertyName == "mediaText")
    mediaList.setMediaText(valueToStringWithNullCheck(exec, value), exception);
  else
    DOMObject::put(exec, propertyName, value, attr);
}

JSValue* toJS(ExecState* exec, MediaList *ml)
{
  return cacheDOMObject<MediaList, DOMMediaList>(exec, ml);
}

JSValue* KJS::DOMMediaListPrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMMediaList::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  MediaList& mediaList = *static_cast<DOMMediaList *>(thisObj)->impl();
  switch (id) {
    case DOMMediaList::Item:
      return jsStringOrNull(mediaList.item(args[0]->toInt32(exec)));
    case DOMMediaList::DeleteMedium:
      mediaList.deleteMedium(args[0]->toString(exec), exception);
      return jsUndefined();
    case DOMMediaList::AppendMedium:
      mediaList.appendMedium(args[0]->toString(exec), exception);
      return jsUndefined();
    default:
      return jsUndefined();
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSStyleSheet::info = { "CSSStyleSheet", &WebCore::JSStyleSheet::info, &DOMCSSStyleSheetTable, 0 };

/*
@begin DOMCSSStyleSheetTable 5
  ownerRule     DOMCSSStyleSheet::OwnerRule     DontDelete|ReadOnly
  cssRules      DOMCSSStyleSheet::CssRules      DontDelete|ReadOnly
# MSIE extension
  rules         DOMCSSStyleSheet::Rules         DontDelete|ReadOnly
@end
@begin DOMCSSStyleSheetPrototypeTable 6
  insertRule    DOMCSSStyleSheet::InsertRule    DontDelete|Function 2
  deleteRule    DOMCSSStyleSheet::DeleteRule    DontDelete|Function 1
# MSIE extensions
  addRule       DOMCSSStyleSheet::AddRule       DontDelete|Function 2
  removeRule    DOMCSSStyleSheet::RemoveRule    DontDelete|Function 1
@end
*/
KJS_DEFINE_PROTOTYPE(DOMCSSStyleSheetPrototype)
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMCSSStyleSheetPrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("DOMCSSStyleSheet",DOMCSSStyleSheetPrototype,DOMCSSStyleSheetPrototypeFunction) // warning, use _WITH_PARENT if DOMStyleSheet gets a prototype

DOMCSSStyleSheet::DOMCSSStyleSheet(ExecState* exec, CSSStyleSheet *ss)
  : JSStyleSheet(exec, ss) 
{
  setPrototype(DOMCSSStyleSheetPrototype::self(exec));
}

DOMCSSStyleSheet::~DOMCSSStyleSheet()
{
}

JSValue* DOMCSSStyleSheet::getValueProperty(ExecState* exec, int token) const
{
  switch (token) {
  case OwnerRule:
    return toJS(exec, static_cast<CSSStyleSheet*>(impl())->ownerRule());
  case CssRules:
    return toJS(exec, static_cast<CSSStyleSheet*>(impl())->cssRules());
  case Rules:
    return toJS(exec, static_cast<CSSStyleSheet*>(impl())->cssRules(true));
  default:
    ASSERT(0);
    return jsUndefined();
  }
}

bool DOMCSSStyleSheet::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMCSSStyleSheet, JSStyleSheet>(exec, &DOMCSSStyleSheetTable, this, propertyName, slot);
}

JSValue* DOMCSSStyleSheetPrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMCSSStyleSheet::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  CSSStyleSheet &styleSheet = *static_cast<CSSStyleSheet*>(static_cast<DOMCSSStyleSheet*>(thisObj)->impl());
  switch (id) {
    case DOMCSSStyleSheet::InsertRule:
      return jsNumber(styleSheet.insertRule(args[0]->toString(exec), args[1]->toInt32(exec), exception));
    case DOMCSSStyleSheet::DeleteRule:
      styleSheet.deleteRule(args[0]->toInt32(exec), exception);
      return jsUndefined();
    case DOMCSSStyleSheet::AddRule: {
      int index = args.size() >= 3 ? args[2]->toInt32(exec) : -1;
      styleSheet.addRule(args[0]->toString(exec), args[1]->toString(exec), index, exception);
      // As per Microsoft documentation, always return -1.
      return jsNumber(-1);
    }
    case DOMCSSStyleSheet::RemoveRule: {
      int index = args.size() >= 1 ? args[0]->toInt32(exec) : 0;
      styleSheet.removeRule(index, exception);
      return jsUndefined();
    }
  }
  return jsUndefined();
}

// -------------------------------------------------------------------------

KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMCSSRuleFunc) // Not a prototype, but doesn't matter

DOMCSSRule::DOMCSSRule(ExecState* exec, CSSRule* r)
  : m_impl(r) 
{
}

DOMCSSRule::~DOMCSSRule()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

const ClassInfo DOMCSSRule::info = { "CSSRule", 0, &DOMCSSRuleTable, 0 };
const ClassInfo DOMCSSRule::style_info = { "CSSStyleRule", &DOMCSSRule::info, &DOMCSSStyleRuleTable, 0 };
const ClassInfo DOMCSSRule::media_info = { "CSSMediaRule", &DOMCSSRule::info, &DOMCSSMediaRuleTable, 0 };
const ClassInfo DOMCSSRule::fontface_info = { "CSSFontFaceRule", &DOMCSSRule::info, &DOMCSSFontFaceRuleTable, 0 };
const ClassInfo DOMCSSRule::page_info = { "CSSPageRule", &DOMCSSRule::info, &DOMCSSPageRuleTable, 0 };
const ClassInfo DOMCSSRule::import_info = { "CSSImportRule", &DOMCSSRule::info, &DOMCSSImportRuleTable, 0 };
const ClassInfo DOMCSSRule::charset_info = { "CSSCharsetRule", &DOMCSSRule::info, &DOMCSSCharsetRuleTable, 0 };

const ClassInfo* DOMCSSRule::classInfo() const
{
  CSSRule& cssRule = *m_impl;
  switch (cssRule.type()) {
  case CSSRule::STYLE_RULE:
    return &style_info;
  case CSSRule::MEDIA_RULE:
    return &media_info;
  case CSSRule::FONT_FACE_RULE:
    return &fontface_info;
  case CSSRule::PAGE_RULE:
    return &page_info;
  case CSSRule::IMPORT_RULE:
    return &import_info;
  case CSSRule::CHARSET_RULE:
    return &charset_info;
  case CSSRule::UNKNOWN_RULE:
  default:
    return &info;
  }
}
/*
@begin DOMCSSRuleTable 4
  type                  DOMCSSRule::Type        DontDelete|ReadOnly
  cssText               DOMCSSRule::CssText     DontDelete|ReadOnly
  parentStyleSheet      DOMCSSRule::ParentStyleSheet    DontDelete|ReadOnly
  parentRule            DOMCSSRule::ParentRule  DontDelete|ReadOnly
@end
@begin DOMCSSStyleRuleTable 2
  selectorText          DOMCSSRule::Style_SelectorText  DontDelete
  style                 DOMCSSRule::Style_Style         DontDelete|ReadOnly
@end
@begin DOMCSSMediaRuleTable 4
  media                 DOMCSSRule::Media_Media         DontDelete|ReadOnly
  cssRules              DOMCSSRule::Media_CssRules      DontDelete|ReadOnly
  insertRule            DOMCSSRule::Media_InsertRule    DontDelete|Function 2
  deleteRule            DOMCSSRule::Media_DeleteRule    DontDelete|Function 1
@end
@begin DOMCSSFontFaceRuleTable 1
  style                 DOMCSSRule::FontFace_Style      DontDelete|ReadOnly
@end
@begin DOMCSSPageRuleTable 2
  selectorText          DOMCSSRule::Page_SelectorText   DontDelete
  style                 DOMCSSRule::Page_Style          DontDelete|ReadOnly
@end
@begin DOMCSSImportRuleTable 3
  href                  DOMCSSRule::Import_Href         DontDelete|ReadOnly
  media                 DOMCSSRule::Import_Media        DontDelete|ReadOnly
  styleSheet            DOMCSSRule::Import_StyleSheet   DontDelete|ReadOnly
@end
@begin DOMCSSCharsetRuleTable 1
  encoding              DOMCSSRule::Charset_Encoding    DontDelete
@end
*/
bool DOMCSSRule::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  // first try the properties specific to this rule type
  const HashTable* table = DOMCSSRule::classInfo()->propHashTable;
  if (table) {
    const HashEntry* entry = Lookup::findEntry(table, propertyName);
    if (entry) {
      // FIXME: for now we single out the media rule functions.
      // This is a temporary hack since we should try to generate
      // them. See http://bugs.webkit.org/show_bug.cgi?id=11898
      if (entry->attr & Function)
        return getStaticPropertySlot<DOMCSSRuleFunc, DOMCSSRule, DOMObject>(exec, &DOMCSSMediaRuleTable, this, propertyName, slot);
      slot.setStaticEntry(this, entry, staticValueGetter<DOMCSSRule>);
      return true;
    }
  }

  // now the stuff that applies to all rules
  return getStaticPropertySlot<DOMCSSRuleFunc, DOMCSSRule, DOMObject>(exec, &DOMCSSRuleTable, this, propertyName, slot);
}

JSValue* DOMCSSRule::getValueProperty(ExecState* exec, int token) const
{
  CSSRule &cssRule = *m_impl;
  switch (token) {
  case Type:
    return jsNumber(cssRule.type());
  case CssText:
    return jsStringOrNull(cssRule.cssText());
  case ParentStyleSheet:
    return toJS(exec,cssRule.parentStyleSheet());
  case ParentRule:
    return toJS(exec,cssRule.parentRule());

  // for STYLE_RULE:
  case Style_SelectorText:
    return jsStringOrNull(static_cast<CSSStyleRule*>(m_impl.get())->selectorText());
  case Style_Style:
    return toJS(exec, static_cast<CSSStyleRule*>(m_impl.get())->style());

  // for MEDIA_RULE:
  case Media_Media:
    return toJS(exec, static_cast<CSSMediaRule*>(m_impl.get())->media());
  case Media_CssRules:
    return toJS(exec, static_cast<CSSMediaRule*>(m_impl.get())->cssRules());

  // for FONT_FACE_RULE:
  case FontFace_Style:
    return toJS(exec, static_cast<CSSFontFaceRule*>(m_impl.get())->style());

  // for PAGE_RULE:
  case Page_SelectorText:
    return jsStringOrNull(static_cast<CSSPageRule*>(m_impl.get())->selectorText());
  case Page_Style:
    return toJS(exec, static_cast<CSSPageRule*>(m_impl.get())->style());

  // for IMPORT_RULE:
  case Import_Href:
    return jsStringOrNull(static_cast<CSSImportRule*>(m_impl.get())->href());
  case Import_Media:
    return toJS(exec, static_cast<CSSImportRule*>(m_impl.get())->media());
  case Import_StyleSheet:
    return toJS(exec, static_cast<CSSImportRule*>(m_impl.get())->styleSheet());

  // for CHARSET_RULE:
  case Charset_Encoding:
    return jsStringOrNull(static_cast<CSSCharsetRule*>(m_impl.get())->encoding());

  default:
    ASSERT(0);
  }
  return jsUndefined();
}

void DOMCSSRule::put(ExecState* exec, const Identifier &propertyName, JSValue* value, int attr)
{
  const HashTable* table = DOMCSSRule::classInfo()->propHashTable; // get the right hashtable
  if (table) {
    const HashEntry* entry = Lookup::findEntry(table, propertyName);
    if (entry) {
      if (entry->attr & Function) {
        // function: put as override property
        JSObject::put(exec, propertyName, value, attr);
        return;
      } else if ((entry->attr & ReadOnly) == 0) {
        // let lookupPut print the warning if not
        putValueProperty(exec, entry->value, value, attr);
        return;
      }
    }
  }

  lookupPut<DOMCSSRule, DOMObject>(exec, propertyName, value, attr, &DOMCSSRuleTable, this);
}

void DOMCSSRule::putValueProperty(ExecState* exec, int token, JSValue* value, int)
{
  switch (token) {
  // for STYLE_RULE:
  case Style_SelectorText:
    static_cast<CSSStyleRule*>(m_impl.get())->setSelectorText(valueToStringWithNullCheck(exec, value));
    return;

  // for PAGE_RULE:
  case Page_SelectorText:
    static_cast<CSSPageRule*>(m_impl.get())->setSelectorText(valueToStringWithNullCheck(exec, value));
    return;

  // for CHARSET_RULE:
  case Charset_Encoding:
    static_cast<CSSCharsetRule*>(m_impl.get())->setEncoding(valueToStringWithNullCheck(exec, value));
    return;
  }
}

JSValue* DOMCSSRuleFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMCSSRule::info))
    return throwError(exec, TypeError);
  CSSRule& cssRule = *static_cast<DOMCSSRule*>(thisObj)->impl();

  if (cssRule.type() == CSSRule::MEDIA_RULE) {
    CSSMediaRule& rule = static_cast<CSSMediaRule&>(cssRule);
    switch (id) {
      case DOMCSSRule::Media_InsertRule: {
        ExceptionCode ec = 0;
        JSValue* result = jsNumber(rule.insertRule(args[0]->toString(exec), args[1]->toInt32(exec), ec));
        setDOMException(exec, ec);
        return result;
      }
      case DOMCSSRule::Media_DeleteRule: {
        ExceptionCode ec = 0;
        rule.deleteRule(args[0]->toInt32(exec), ec);
        setDOMException(exec, ec);
      }
    }
  }

  return jsUndefined();
}

JSValue* toJS(ExecState* exec, CSSRule *r)
{
  return cacheDOMObject<CSSRule, JSCSSRule>(exec, r);
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSValue::info = { "CSSValue", 0, &DOMCSSValueTable, 0 };

/*
@begin DOMCSSValuePrototypeTable 0
@end
@begin DOMCSSValueTable 2
  cssText       DOMCSSValue::CssText            DontDelete|ReadOnly
  cssValueType  DOMCSSValue::CssValueType       DontDelete|ReadOnly
@end
*/
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMCSSValuePrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("DOMCSSValue", DOMCSSValuePrototype, DOMCSSValuePrototypeFunction)

DOMCSSValue::DOMCSSValue(ExecState* exec, CSSValue* v) 
: m_impl(v) 
{ 
    setPrototype(DOMCSSValuePrototype::self(exec));
}

DOMCSSValue::~DOMCSSValue()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* DOMCSSValue::getValueProperty(ExecState* exec, int token) const
{
  CSSValue &cssValue = *m_impl;
  switch (token) {
  case CssText:
    return jsStringOrNull(cssValue.cssText());
  case CssValueType:
    return jsNumber(cssValue.cssValueType());
  default:
    ASSERT(0);
    return jsUndefined();
  }
}

bool DOMCSSValue::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMCSSValue, DOMObject>(exec, &DOMCSSValueTable, this, propertyName, slot);
}

void DOMCSSValue::put(ExecState* exec, const Identifier &propertyName, JSValue* value, int attr)
{
  CSSValue &cssValue = *m_impl;
  if (propertyName == "cssText")
    cssValue.setCssText(valueToStringWithNullCheck(exec, value));
  else
    DOMObject::put(exec, propertyName, value, attr);
}

JSValue* DOMCSSValuePrototypeFunction::callAsFunction(ExecState*, JSObject*, const List&)
{
    return 0;
}

JSValue* toJS(ExecState* exec, CSSValue *v)
{
  DOMObject *ret;
  if (!v)
    return jsNull();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
  if ((ret = interp->getDOMObject(v)))
    return ret;
  else {
    if (v->isValueList())
      ret = new JSCSSValueList(exec, static_cast<CSSValueList*>(v));
#if ENABLE(SVG)
    else if (v->isSVGColor())
      ret = new JSSVGColor(exec, static_cast<SVGColor*>(v));
    else if (v->isSVGPaint())
      ret = new JSSVGPaint(exec, static_cast<SVGPaint*>(v));
#endif
    else if (v->isPrimitiveValue())
      ret = new JSCSSPrimitiveValue(exec, static_cast<CSSPrimitiveValue*>(v));
    else
      ret = new DOMCSSValue(exec, v);
    interp->putDOMObject(v, ret);
    return ret;
  }
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
