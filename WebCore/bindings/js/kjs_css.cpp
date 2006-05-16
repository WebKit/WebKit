// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004 Apple Computer, Inc.
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
#include "JSCSSStyleDeclaration.h"
#include "MediaList.h"
#include "StyleSheetList.h"
#include "css_base.h"
#include "kjs_dom.h"

#include "kjs_css.lut.h"

using namespace WebCore;
using namespace HTMLNames;

namespace KJS {

static String cssPropertyName(const Identifier &p, bool *hadPixelOrPosPrefix = 0)
{
    DeprecatedString prop = p;

    int i = prop.length();
    while (--i) {
        ::UChar c = prop[i].unicode();
        if (c >= 'A' && c <= 'Z')
            prop.insert(i, '-');
    }

    prop = prop.lower();

    if (hadPixelOrPosPrefix)
        *hadPixelOrPosPrefix = false;

    if (prop.startsWith("css-")) {
        prop = prop.mid(4);
    } else if (prop.startsWith("pixel-")) {
        prop = prop.mid(6);
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
    } else if (prop.startsWith("pos-")) {
        prop = prop.mid(4);
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
    } else if (prop.startsWith("khtml-") || prop.startsWith("apple-") || prop.startsWith("webkit-")) {
        prop.insert(0, '-');
    }

    return prop;
}

static bool isCSSPropertyName(const Identifier &JSPropertyName)
{
    return CSSStyleDeclaration::isPropertyName(cssPropertyName(JSPropertyName));
}

/*
@begin DOMCSSStyleDeclarationProtoTable 7
  getPropertyValue      DOMCSSStyleDeclaration::GetPropertyValue        DontDelete|Function 1
  getPropertyCSSValue   DOMCSSStyleDeclaration::GetPropertyCSSValue     DontDelete|Function 1
  removeProperty        DOMCSSStyleDeclaration::RemoveProperty          DontDelete|Function 1
  getPropertyPriority   DOMCSSStyleDeclaration::GetPropertyPriority     DontDelete|Function 1
  getPropertyShorthand  DOMCSSStyleDeclaration::GetPropertyShorthand    DontDelete|Function 1
  isPropertyImplicit    DOMCSSStyleDeclaration::IsPropertyImplicit      DontDelete|Function 1
  setProperty           DOMCSSStyleDeclaration::SetProperty             DontDelete|Function 3
  item                  DOMCSSStyleDeclaration::Item                    DontDelete|Function 1
@end
@begin DOMCSSStyleDeclarationTable 3
  cssText               DOMCSSStyleDeclaration::CssText         DontDelete
  length                DOMCSSStyleDeclaration::Length          DontDelete|ReadOnly
  parentRule            DOMCSSStyleDeclaration::ParentRule      DontDelete|ReadOnly
@end
*/
KJS_IMPLEMENT_PROTOFUNC(DOMCSSStyleDeclarationProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMCSSStyleDeclaration", DOMCSSStyleDeclarationProto, DOMCSSStyleDeclarationProtoFunc)

const ClassInfo DOMCSSStyleDeclaration::info = { "CSSStyleDeclaration", 0, &DOMCSSStyleDeclarationTable, 0 };

DOMCSSStyleDeclaration::DOMCSSStyleDeclaration(ExecState* exec, CSSStyleDeclaration *s)
  : m_impl(s)
{ 
  setPrototype(DOMCSSStyleDeclarationProto::self(exec));
}

DOMCSSStyleDeclaration::~DOMCSSStyleDeclaration()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* DOMCSSStyleDeclaration::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMCSSStyleDeclaration *thisObj = static_cast<DOMCSSStyleDeclaration*>(slot.slotBase());
  return jsStringOrNull(thisObj->m_impl->item(slot.index()));
}

JSValue* DOMCSSStyleDeclaration::cssPropertyGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMCSSStyleDeclaration *thisObj = static_cast<DOMCSSStyleDeclaration*>(slot.slotBase());

  // Set up pixelOrPos boolean to handle the fact that
  // pixelTop returns "CSS Top" as number value in unit pixels
  // posTop returns "CSS top" as number value in unit pixels _if_ its a
  // positioned element. if it is not a positioned element, return 0
  // from MSIE documentation ### IMPLEMENT THAT (Dirk)
  bool pixelOrPos;
  String prop = cssPropertyName(propertyName, &pixelOrPos);
  RefPtr<CSSValue> v = thisObj->m_impl->getPropertyCSSValue(prop);
  if (v) {
    if (pixelOrPos && v->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE)
      return jsNumber(static_pointer_cast<CSSPrimitiveValue>(v)->getFloatValue(CSSPrimitiveValue::CSS_PX));
    return jsStringOrNull(v->cssText());
  }
  // If the property is a shorthand property (such as "padding"), 
  // it can only be accessed using getPropertyValue.
  return jsString(thisObj->m_impl->getPropertyValue(prop));
}

bool DOMCSSStyleDeclaration::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  const HashEntry* entry = Lookup::findEntry(&DOMCSSStyleDeclarationTable, propertyName);

  if (entry) {
    slot.setStaticEntry(this, entry, staticValueGetter<DOMCSSStyleDeclaration>);
    return true;
  }

  bool ok;
  unsigned u = propertyName.toUInt32(&ok);
  if (ok) {
    slot.setCustomIndex(this, u, indexGetter);
    return true;
  }

  if (isCSSPropertyName(propertyName)) {
    slot.setCustom(this, cssPropertyGetter);
    return true;
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue* DOMCSSStyleDeclaration::getValueProperty(ExecState* exec, int token)
{
  switch (token) {
  case CssText:
    return jsStringOrNull(m_impl->cssText());
  case Length:
    return jsNumber(m_impl->length());
  case ParentRule:
    return toJS(exec, m_impl->parentRule());
  default:
    assert(0);
    return jsUndefined();
  }
}

void DOMCSSStyleDeclaration::put(ExecState* exec, const Identifier &propertyName, JSValue* value, int attr )
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMCSSStyleDeclaration::put " << propertyName << endl;
#endif
  DOMExceptionTranslator exception(exec);
  CSSStyleDeclaration &styleDecl = *m_impl;
  if (propertyName == "cssText") {
    styleDecl.setCssText(value->toString(exec), exception);
  } else {
    if (isCSSPropertyName(propertyName)) {
      bool pixelOrPos;
      String prop = cssPropertyName(propertyName, &pixelOrPos);
      String propvalue = value->toString(exec);
      if (pixelOrPos)
        propvalue += "px";
#ifdef KJS_VERBOSE
      kdDebug(6070) << "DOMCSSStyleDeclaration: prop=" << prop << " propvalue=" << propvalue << endl;
#endif
      styleDecl.removeProperty(prop, exception);
      if (!exception && !propvalue.isEmpty()) {
        // We have to ignore exceptions here, because of the following unfortunate situation:
        //   1) Older versions ignored exceptions here by accident, because the put function
        //      that translated exceptions did not translate CSS exceptions.
        //   2) Gecko does not raise an exception in this case, although WinIE does.
        //   3) At least some Dashboard widgets are depending on this behavior.
        // It would be nice to fix this some day, perhaps with some kind of "quirks mode",
        // but it's likely that the Dashboard widgets are already using a strict mode DOCTYPE.
        ExceptionCode ec = 0;
        styleDecl.setProperty(prop, propvalue, "", ec);
      }
    } else {
      DOMObject::put(exec, propertyName, value, attr);
    }
  }
}

JSValue* DOMCSSStyleDeclarationProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMCSSStyleDeclaration::info))
    return throwError(exec, TypeError);
  DOMExceptionTranslator exception(exec);
  CSSStyleDeclaration &styleDecl = *static_cast<DOMCSSStyleDeclaration*>(thisObj)->impl();
  UString str = args[0]->toString(exec);
  WebCore::String s = str;

  switch (id) {
    case DOMCSSStyleDeclaration::GetPropertyValue:
      return jsStringOrNull(styleDecl.getPropertyValue(s));
    case DOMCSSStyleDeclaration::GetPropertyCSSValue:
      return toJS(exec, styleDecl.getPropertyCSSValue(s).get());
    case DOMCSSStyleDeclaration::RemoveProperty:
      return jsStringOrNull(styleDecl.removeProperty(s, exception));
    case DOMCSSStyleDeclaration::GetPropertyPriority:
      return jsStringOrNull(styleDecl.getPropertyPriority(s));
    case DOMCSSStyleDeclaration::GetPropertyShorthand:
      return jsStringOrNull(styleDecl.getPropertyShorthand(s));
    case DOMCSSStyleDeclaration::IsPropertyImplicit:
      return jsBoolean(styleDecl.isPropertyImplicit(s));
    case DOMCSSStyleDeclaration::SetProperty:
      styleDecl.setProperty(s, args[1]->toString(exec), args[2]->toString(exec), exception);
      return jsUndefined();
    case DOMCSSStyleDeclaration::Item:
      return jsStringOrNull(styleDecl.item(args[0]->toInt32(exec)));
    default:
      return jsUndefined();
  }
}

JSValue* toJS(ExecState* exec, CSSStyleDeclaration *s)
{
  return cacheDOMObject<CSSStyleDeclaration, WebCore::JSCSSStyleDeclaration>(exec, s);
}

// -------------------------------------------------------------------------

const ClassInfo DOMStyleSheet::info = { "StyleSheet", 0, &DOMStyleSheetTable, 0 };
/*
@begin DOMStyleSheetTable 7
  type          DOMStyleSheet::Type             DontDelete|ReadOnly
  disabled      DOMStyleSheet::Disabled         DontDelete
  ownerNode     DOMStyleSheet::OwnerNode        DontDelete|ReadOnly
  parentStyleSheet DOMStyleSheet::ParentStyleSheet      DontDelete|ReadOnly
  href          DOMStyleSheet::Href             DontDelete|ReadOnly
  title         DOMStyleSheet::Title            DontDelete|ReadOnly
  media         DOMStyleSheet::Media            DontDelete|ReadOnly
@end
*/

DOMStyleSheet::DOMStyleSheet(ExecState*, WebCore::StyleSheet* ss) 
  : m_impl(ss) 
{
}

DOMStyleSheet::DOMStyleSheet(WebCore::StyleSheet *ss) 
  : m_impl(ss) 
{
}

DOMStyleSheet::~DOMStyleSheet()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

bool DOMStyleSheet::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMStyleSheet, DOMObject>(exec, &DOMStyleSheetTable, this, propertyName, slot);
}

JSValue* DOMStyleSheet::getValueProperty(ExecState* exec, int token) const
{
  StyleSheet& styleSheet = *m_impl;
  switch (token) {
  case Type:
    return jsStringOrNull(styleSheet.type());
  case Disabled:
    return jsBoolean(styleSheet.disabled());
  case OwnerNode:
    return toJS(exec,styleSheet.ownerNode());
  case ParentStyleSheet:
    return toJS(exec,styleSheet.parentStyleSheet());
  case Href:
    return jsStringOrNull(styleSheet.href());
  case Title:
    return jsStringOrNull(styleSheet.title());
  case Media:
    return toJS(exec, styleSheet.media());
  }
  return NULL;
}

void DOMStyleSheet::put(ExecState* exec, const Identifier &propertyName, JSValue* value, int attr)
{
  if (propertyName == "disabled") {
    m_impl->setDisabled(value->toBoolean(exec));
  }
  else
    DOMObject::put(exec, propertyName, value, attr);
}

JSValue* toJS(ExecState* exec, PassRefPtr<StyleSheet> ss)
{
  DOMObject *ret;
  if (!ss)
    return jsNull();
  ScriptInterpreter *interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
  if ((ret = interp->getDOMObject(ss.get())))
    return ret;
  else {
    if (ss->isCSSStyleSheet())
      ret = new DOMCSSStyleSheet(exec, static_cast<CSSStyleSheet*>(ss.get()));
    else
      ret = new DOMStyleSheet(exec, ss.get());
    interp->putDOMObject(ss.get(), ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMStyleSheetList::info = { "StyleSheetList", 0, &DOMStyleSheetListTable, 0 };

/*
@begin DOMStyleSheetListTable 2
  length        DOMStyleSheetList::Length       DontDelete|ReadOnly
  item          DOMStyleSheetList::Item         DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOFUNC(DOMStyleSheetListFunc) // not really a proto, but doesn't matter

DOMStyleSheetList::DOMStyleSheetList(ExecState*, WebCore::StyleSheetList* ssl, WebCore::Document* doc)
  : m_impl(ssl)
  , m_doc(doc) 
{
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
      assert(0);
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
@begin DOMMediaListProtoTable 3
  item          DOMMediaList::Item              DontDelete|Function 1
  deleteMedium  DOMMediaList::DeleteMedium      DontDelete|Function 1
  appendMedium  DOMMediaList::AppendMedium      DontDelete|Function 1
@end
*/
KJS_DEFINE_PROTOTYPE(DOMMediaListProto)
KJS_IMPLEMENT_PROTOFUNC(DOMMediaListProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMMediaList", DOMMediaListProto, DOMMediaListProtoFunc)

DOMMediaList::DOMMediaList(ExecState* exec, MediaList *ml)
    : m_impl(ml)
{
  setPrototype(DOMMediaListProto::self(exec));
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
    assert(0);
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
  MediaList &mediaList = *m_impl;
  if (propertyName == "mediaText")
    mediaList.setMediaText(value->toString(exec));
  else
    DOMObject::put(exec, propertyName, value, attr);
}

JSValue* toJS(ExecState* exec, MediaList *ml)
{
  return cacheDOMObject<MediaList, DOMMediaList>(exec, ml);
}

JSValue* KJS::DOMMediaListProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMMediaList::info))
    return throwError(exec, TypeError);
  MediaList &mediaList = *static_cast<DOMMediaList*>(thisObj)->impl();
  switch (id) {
    case DOMMediaList::Item:
      return jsStringOrNull(mediaList.item(args[0]->toInt32(exec)));
    case DOMMediaList::DeleteMedium:
      mediaList.deleteMedium(args[0]->toString(exec));
      return jsUndefined();
    case DOMMediaList::AppendMedium:
      mediaList.appendMedium(args[0]->toString(exec));
      return jsUndefined();
    default:
      return jsUndefined();
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSStyleSheet::info = { "CSSStyleSheet", 0, &DOMCSSStyleSheetTable, 0 };

/*
@begin DOMCSSStyleSheetTable 5
  ownerRule     DOMCSSStyleSheet::OwnerRule     DontDelete|ReadOnly
  cssRules      DOMCSSStyleSheet::CssRules      DontDelete|ReadOnly
# MSIE extension
  rules         DOMCSSStyleSheet::Rules         DontDelete|ReadOnly
@end
@begin DOMCSSStyleSheetProtoTable 6
  insertRule    DOMCSSStyleSheet::InsertRule    DontDelete|Function 2
  deleteRule    DOMCSSStyleSheet::DeleteRule    DontDelete|Function 1
# MSIE extensions
  addRule       DOMCSSStyleSheet::AddRule       DontDelete|Function 2
  removeRule    DOMCSSStyleSheet::RemoveRule    DontDelete|Function 1
@end
*/
KJS_DEFINE_PROTOTYPE(DOMCSSStyleSheetProto)
KJS_IMPLEMENT_PROTOFUNC(DOMCSSStyleSheetProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMCSSStyleSheet",DOMCSSStyleSheetProto,DOMCSSStyleSheetProtoFunc) // warning, use _WITH_PARENT if DOMStyleSheet gets a proto

DOMCSSStyleSheet::DOMCSSStyleSheet(ExecState* exec, CSSStyleSheet *ss)
  : DOMStyleSheet(ss) 
{
  setPrototype(DOMCSSStyleSheetProto::self(exec));
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
  case Rules:
    return toJS(exec, static_cast<CSSStyleSheet*>(impl())->cssRules());
  default:
    assert(0);
    return jsUndefined();
  }
}

bool DOMCSSStyleSheet::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMCSSStyleSheet, DOMStyleSheet>(exec, &DOMCSSStyleSheetTable, this, propertyName, slot);
}

JSValue* DOMCSSStyleSheetProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
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

const ClassInfo DOMCSSRuleList::info = { "CSSRuleList", 0, &DOMCSSRuleListTable, 0 };
/*
@begin DOMCSSRuleListTable 2
  length                DOMCSSRuleList::Length          DontDelete|ReadOnly
  item                  DOMCSSRuleList::Item            DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOFUNC(DOMCSSRuleListFunc) // not really a proto, but doesn't matter

DOMCSSRuleList::DOMCSSRuleList(ExecState *, WebCore::CSSRuleList *rl) 
  : m_impl(rl) 
{ 
}

DOMCSSRuleList::~DOMCSSRuleList()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* DOMCSSRuleList::getValueProperty(ExecState* exec, int token) const
{
  switch (token) {
  case Length:
    return jsNumber(m_impl->length());
  default:
    assert(0);
    return jsUndefined();
  }
}

JSValue* DOMCSSRuleList::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMCSSRuleList *thisObj = static_cast<DOMCSSRuleList*>(slot.slotBase());
  return toJS(exec, thisObj->m_impl->item(slot.index()));
}

bool DOMCSSRuleList::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  const HashEntry* entry = Lookup::findEntry(&DOMCSSRuleListTable, propertyName);

  if (entry) {
    if (entry->attr & Function)
      slot.setStaticEntry(this, entry, staticFunctionGetter<DOMCSSRuleListFunc>);
    else
      slot.setStaticEntry(this, entry, staticValueGetter<DOMCSSRuleList>);
    return true;
  }

  CSSRuleList &cssRuleList = *m_impl;

  bool ok;
  unsigned u = propertyName.toUInt32(&ok);
  if (ok && u < cssRuleList.length()) {
    slot.setCustomIndex(this, u, indexGetter);
    return true;
  }

  return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue* DOMCSSRuleListFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMCSSRuleList::info))
    return throwError(exec, TypeError);
  CSSRuleList &cssRuleList = *static_cast<DOMCSSRuleList*>(thisObj)->impl();
  switch (id) {
    case DOMCSSRuleList::Item:
      return toJS(exec,cssRuleList.item(args[0]->toInt32(exec)));
    default:
      return jsUndefined();
  }
}

JSValue* toJS(ExecState* exec, CSSRuleList *rl)
{
  return cacheDOMObject<CSSRuleList, DOMCSSRuleList>(exec, rl);
}

// -------------------------------------------------------------------------

KJS_IMPLEMENT_PROTOFUNC(DOMCSSRuleFunc) // Not a proto, but doesn't matter

DOMCSSRule::DOMCSSRule(ExecState*, WebCore::CSSRule* r)
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
  const HashEntry* entry = Lookup::findEntry(DOMCSSRule::classInfo()->propHashTable, propertyName);
  if (entry) {
    slot.setStaticEntry(this, entry, staticValueGetter<DOMCSSRule>);
    return true;
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
    assert(0);
  }
  return jsUndefined();
}

void DOMCSSRule::put(ExecState* exec, const Identifier &propertyName, JSValue* value, int attr)
{
  const HashTable* table = DOMCSSRule::classInfo()->propHashTable; // get the right hashtable
  const HashEntry* entry = Lookup::findEntry(table, propertyName);
  if (entry) {
    if (entry->attr & Function) // function: put as override property
    {
      JSObject::put(exec, propertyName, value, attr);
      return;
    }
    else if ((entry->attr & ReadOnly) == 0) // let lookupPut print the warning if not
    {
      putValueProperty(exec, entry->value, value, attr);
      return;
    }
  }
  lookupPut<DOMCSSRule, DOMObject>(exec, propertyName, value, attr, &DOMCSSRuleTable, this);
}

void DOMCSSRule::putValueProperty(ExecState* exec, int token, JSValue* value, int)
{
  switch (token) {
  // for STYLE_RULE:
  case Style_SelectorText:
    static_cast<CSSStyleRule*>(m_impl.get())->setSelectorText(value->toString(exec));
    return;

  // for PAGE_RULE:
  case Page_SelectorText:
    static_cast<CSSPageRule*>(m_impl.get())->setSelectorText(value->toString(exec));
    return;

  // for CHARSET_RULE:
  case Charset_Encoding:
    static_cast<CSSCharsetRule*>(m_impl.get())->setEncoding(value->toString(exec));
    return;
  }
}

JSValue* DOMCSSRuleFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMCSSRule::info))
    return throwError(exec, TypeError);
  CSSRule &cssRule = *static_cast<DOMCSSRule*>(thisObj)->impl();

  if (cssRule.type() == CSSRule::MEDIA_RULE) {
    CSSMediaRule &rule = static_cast<CSSMediaRule &>(cssRule);
    if (id == DOMCSSRule::Media_InsertRule)
      return jsNumber(rule.insertRule(args[0]->toString(exec), args[1]->toInt32(exec)));
    else if (id == DOMCSSRule::Media_DeleteRule)
      rule.deleteRule(args[0]->toInt32(exec));
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
@begin DOMCSSValueProtoTable 0
@end
@begin DOMCSSValueTable 2
  cssText       DOMCSSValue::CssText            DontDelete|ReadOnly
  cssValueType  DOMCSSValue::CssValueType       DontDelete|ReadOnly
@end
*/
KJS_IMPLEMENT_PROTOFUNC(DOMCSSValueProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMCSSValue", DOMCSSValueProto, DOMCSSValueProtoFunc)

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
    assert(0);
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
    cssValue.setCssText(value->toString(exec));
  else
    DOMObject::put(exec, propertyName, value, attr);
}

JSValue* DOMCSSValueProtoFunc::callAsFunction(ExecState*, JSObject*, const List&)
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
      ret = new DOMCSSValueList(exec, static_cast<CSSValueList*>(v));
    else if (v->isPrimitiveValue())
      ret = new JSCSSPrimitiveValue(exec, static_cast<CSSPrimitiveValue*>(v));
    else
      ret = new DOMCSSValue(exec,v);
    interp->putDOMObject(v, ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSValueList::info = { "CSSValueList", 0, &DOMCSSValueListTable, 0 };

/*
@begin DOMCSSValueListTable 3
  length                DOMCSSValueList::Length         DontDelete|ReadOnly
  item                  DOMCSSValueList::Item           DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOFUNC(DOMCSSValueListFunc) // not really a proto, but doesn't matter

DOMCSSValueList::DOMCSSValueList(ExecState* exec, CSSValueList *v)
  : DOMCSSValue(exec, v) 
{ 
}

JSValue* DOMCSSValueList::getValueProperty(ExecState* exec, int token) const
{
  assert(token == Length);
  return jsNumber(static_cast<CSSValueList*>(impl())->length());
}

JSValue* DOMCSSValueList::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  DOMCSSValueList *thisObj = static_cast<DOMCSSValueList*>(slot.slotBase());
  return toJS(exec, static_cast<CSSValueList*>(thisObj->impl())->item(slot.index()));
}

bool DOMCSSValueList::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  CSSValueList &valueList = *static_cast<CSSValueList*>(impl());
  const HashEntry* entry = Lookup::findEntry(&DOMCSSValueListTable, propertyName);
  if (entry) {
    if (entry->attr & Function)
      slot.setStaticEntry(this, entry, staticFunctionGetter<DOMCSSValueListFunc>);
    else
      slot.setStaticEntry(this, entry, staticValueGetter<DOMCSSValueList>);
  }

  bool ok;
  unsigned u = propertyName.toUInt32(&ok);
  if (ok && u < valueList.length()) {
    slot.setCustomIndex(this, u, indexGetter);
    return true;
  }

  return DOMCSSValue::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue* DOMCSSValueListFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMCSSValue::info))
    return throwError(exec, TypeError);
  CSSValueList &valueList = *static_cast<CSSValueList*>(static_cast<DOMCSSValueList*>(thisObj)->impl());
  switch (id) {
    case DOMCSSValueList::Item:
      return toJS(exec,valueList.item(args[0]->toInt32(exec)));
    default:
      return jsUndefined();
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

JSValue* getDOMRGBColor(ExecState *, unsigned c)
{
  // ### implement equals for RGBColor since they're not refcounted objects
  return new DOMRGBColor(c);
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
