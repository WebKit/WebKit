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

#include "kjs_css.h"

#include "css/css_base.h"
#include "css/css_ruleimpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/css_valueimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_headimpl.h" // for HTMLStyleElement
#include "kjs_dom.h"
#include "misc/htmltags.h"

#include <kdebug.h>

#include "kjs_css.lut.h"

using DOM::CSSCharsetRuleImpl;
using DOM::CSSFontFaceRuleImpl;
using DOM::CSSImportRuleImpl;
using DOM::CSSMediaRuleImpl;
using DOM::CSSPageRuleImpl;
using DOM::CSSPrimitiveValue;
using DOM::CSSPrimitiveValueImpl;
using DOM::CSSRule;
using DOM::CSSRuleImpl;
using DOM::CSSRuleListImpl;
using DOM::CSSStyleDeclarationImpl;
using DOM::CSSStyleRuleImpl;
using DOM::CSSStyleSheetImpl;
using DOM::CSSValue;
using DOM::CSSValueImpl;
using DOM::CSSValueListImpl;
using DOM::CounterImpl;
using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::HTMLStyleElementImpl;
using DOM::MediaListImpl;
using DOM::RectImpl;
using DOM::StyleSheetImpl;
using DOM::StyleSheetListImpl;

namespace KJS {

static DOMString cssPropertyName(const Identifier &p, bool *hadPixelOrPosPrefix = 0)
{
    QString prop = p.qstring();

    int i = prop.length();
    while (--i) {
	char c = prop[i].latin1();
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
    } else if (prop.startsWith("khtml-") || prop.startsWith("apple-") || prop.startsWith("moz-")) {
        prop.insert(0, '-');
    }

    return prop;
}

static bool isCSSPropertyName(const Identifier &JSPropertyName)
{
    return CSSStyleDeclarationImpl::isPropertyName(cssPropertyName(JSPropertyName));
}

/*
@begin DOMCSSStyleDeclarationProtoTable 7
  getPropertyValue	DOMCSSStyleDeclaration::GetPropertyValue	DontDelete|Function 1
  getPropertyCSSValue	DOMCSSStyleDeclaration::GetPropertyCSSValue	DontDelete|Function 1
  removeProperty	DOMCSSStyleDeclaration::RemoveProperty		DontDelete|Function 1
  getPropertyPriority	DOMCSSStyleDeclaration::GetPropertyPriority	DontDelete|Function 1
  setProperty		DOMCSSStyleDeclaration::SetProperty		DontDelete|Function 3
  item			DOMCSSStyleDeclaration::Item			DontDelete|Function 1
@end
@begin DOMCSSStyleDeclarationTable 3
  cssText		DOMCSSStyleDeclaration::CssText		DontDelete
  length		DOMCSSStyleDeclaration::Length		DontDelete|ReadOnly
  parentRule		DOMCSSStyleDeclaration::ParentRule	DontDelete|ReadOnly
@end
*/
DEFINE_PROTOTYPE("DOMCSSStyleDeclaration", DOMCSSStyleDeclarationProto)
IMPLEMENT_PROTOFUNC(DOMCSSStyleDeclarationProtoFunc)
IMPLEMENT_PROTOTYPE(DOMCSSStyleDeclarationProto, DOMCSSStyleDeclarationProtoFunc)

const ClassInfo DOMCSSStyleDeclaration::info = { "CSSStyleDeclaration", 0, &DOMCSSStyleDeclarationTable, 0 };

DOMCSSStyleDeclaration::DOMCSSStyleDeclaration(ExecState *exec, CSSStyleDeclarationImpl *s)
  : m_impl(s)
{ 
  setPrototype(DOMCSSStyleDeclarationProto::self(exec));
}

DOMCSSStyleDeclaration::~DOMCSSStyleDeclaration()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

bool DOMCSSStyleDeclaration::hasProperty(ExecState *exec, const Identifier &p) const
{
  if (p == "cssText")
    return true;
  if (isCSSPropertyName(p))
    return true;
  return ObjectImp::hasProperty(exec, p);
}

Value DOMCSSStyleDeclaration::tryGet(ExecState *exec, const Identifier &propertyName) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMCSSStyleDeclaration::tryGet " << propertyName.qstring() << endl;
#endif
  const HashEntry* entry = Lookup::findEntry(&DOMCSSStyleDeclarationTable, propertyName);
  CSSStyleDeclarationImpl &styleDecl = *m_impl;
  if (entry)
    switch (entry->value) {
    case CssText:
      return getStringOrNull(styleDecl.cssText());
    case Length:
      return Number(styleDecl.length());
    case ParentRule:
      return getDOMCSSRule(exec,styleDecl.parentRule());
    default:
      break;
    }

  // Look in the prototype (for functions) before assuming it's a name
  Object proto = Object::dynamicCast(prototype());
  if (!proto.isNull() && proto.hasProperty(exec,propertyName))
    return proto.get(exec,propertyName);

  bool ok;
  long unsigned int u = propertyName.toULong(&ok);
  if (ok)
    return getStringOrNull(styleDecl.item(u));

#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMCSSStyleDeclaration: converting to css property name: " << cssPropertyName(propertyName) << endl;
#endif

  // Set up pixelOrPos boolean to handle the fact that
  // pixelTop returns "CSS Top" as number value in unit pixels
  // posTop returns "CSS top" as number value in unit pixels _if_ its a
  // positioned element. if it is not a positioned element, return 0
  // from MSIE documentation ### IMPLEMENT THAT (Dirk)
  if (isCSSPropertyName(propertyName)) {
    bool pixelOrPos;
    DOMString prop = cssPropertyName(propertyName, &pixelOrPos);
    CSSValueImpl *v = styleDecl.getPropertyCSSValue(prop);
    if (v) {
      if (pixelOrPos && v->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE)
        return Number(static_cast<CSSPrimitiveValueImpl *>(v)->getFloatValue(CSSPrimitiveValue::CSS_PX));
      return getStringOrNull(v->cssText());
    }
    return String("");
  }

  return DOMObject::tryGet(exec, propertyName);
}


void DOMCSSStyleDeclaration::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr )
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMCSSStyleDeclaration::tryPut " << propertyName.qstring() << endl;
#endif
  DOMExceptionTranslator exception(exec);
  CSSStyleDeclarationImpl &styleDecl = *m_impl;
  if (propertyName == "cssText") {
    styleDecl.setCssText(value.toString(exec).string(), exception);
  } else {
    if (isCSSPropertyName(propertyName)) {
      bool pixelOrPos;
      DOMString prop = cssPropertyName(propertyName, &pixelOrPos);
      QString propvalue = value.toString(exec).qstring();
      if (pixelOrPos)
	propvalue += "px";
#ifdef KJS_VERBOSE
      kdDebug(6070) << "DOMCSSStyleDeclaration: prop=" << prop << " propvalue=" << propvalue << endl;
#endif
      styleDecl.removeProperty(prop, exception);
      if (!exception && !propvalue.isEmpty())
        styleDecl.setProperty(prop, DOMString(propvalue), "", exception); // ### is "" ok for priority?
    } else {
      DOMObject::tryPut(exec, propertyName, value, attr);
    }
  }
}

Value DOMCSSStyleDeclarationProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMCSSStyleDeclaration::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOMExceptionTranslator exception(exec);
  CSSStyleDeclarationImpl &styleDecl = *static_cast<DOMCSSStyleDeclaration *>(thisObj.imp())->impl();
  String str = args[0].toString(exec);
  DOM::DOMString s = str.value().string();

  switch (id) {
    case DOMCSSStyleDeclaration::GetPropertyValue:
      return getStringOrNull(styleDecl.getPropertyValue(s));
    case DOMCSSStyleDeclaration::GetPropertyCSSValue:
      return getDOMCSSValue(exec,styleDecl.getPropertyCSSValue(s));
    case DOMCSSStyleDeclaration::RemoveProperty:
      return getStringOrNull(styleDecl.removeProperty(s, exception));
    case DOMCSSStyleDeclaration::GetPropertyPriority:
      return getStringOrNull(styleDecl.getPropertyPriority(s));
    case DOMCSSStyleDeclaration::SetProperty:
      styleDecl.setProperty(s, args[1].toString(exec).string(), args[2].toString(exec).string(), exception);
      return Undefined();
    case DOMCSSStyleDeclaration::Item:
      return getStringOrNull(styleDecl.item(args[0].toInt32(exec)));
    default:
      return Undefined();
  }
}

ValueImp *getDOMCSSStyleDeclaration(ExecState *exec, CSSStyleDeclarationImpl *s)
{
  return cacheDOMObject<CSSStyleDeclarationImpl, DOMCSSStyleDeclaration>(exec, s);
}

// -------------------------------------------------------------------------

const ClassInfo DOMStyleSheet::info = { "StyleSheet", 0, &DOMStyleSheetTable, 0 };
/*
@begin DOMStyleSheetTable 7
  type		DOMStyleSheet::Type		DontDelete|ReadOnly
  disabled	DOMStyleSheet::Disabled		DontDelete
  ownerNode	DOMStyleSheet::OwnerNode	DontDelete|ReadOnly
  parentStyleSheet DOMStyleSheet::ParentStyleSheet	DontDelete|ReadOnly
  href		DOMStyleSheet::Href		DontDelete|ReadOnly
  title		DOMStyleSheet::Title		DontDelete|ReadOnly
  media		DOMStyleSheet::Media		DontDelete|ReadOnly
@end
*/

DOMStyleSheet::~DOMStyleSheet()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

Value DOMStyleSheet::tryGet(ExecState *exec, const Identifier &propertyName) const
{
  return DOMObjectLookupGetValue<DOMStyleSheet,DOMObject>(exec,propertyName,&DOMStyleSheetTable,this);
}

Value DOMStyleSheet::getValueProperty(ExecState *exec, int token) const
{
  StyleSheetImpl &styleSheet = *m_impl;
  switch (token) {
  case Type:
    return getStringOrNull(styleSheet.type());
  case Disabled:
    return Boolean(styleSheet.disabled());
  case OwnerNode:
    return getDOMNode(exec,styleSheet.ownerNode());
  case ParentStyleSheet:
    return getDOMStyleSheet(exec,styleSheet.parentStyleSheet());
  case Href:
    return getStringOrNull(styleSheet.href());
  case Title:
    return getStringOrNull(styleSheet.title());
  case Media:
    return getDOMMediaList(exec, styleSheet.media());
  }
  return Value();
}

void DOMStyleSheet::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
  if (propertyName == "disabled") {
    m_impl->setDisabled(value.toBoolean(exec));
  }
  else
    DOMObject::tryPut(exec, propertyName, value, attr);
}

ValueImp *getDOMStyleSheet(ExecState *exec, StyleSheetImpl *ss)
{
  DOMObject *ret;
  if (!ss)
    return Null();
  ScriptInterpreter *interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());
  if ((ret = interp->getDOMObject(ss)))
    return ret;
  else {
    if (ss->isCSSStyleSheet())
      ret = new DOMCSSStyleSheet(exec, static_cast<CSSStyleSheetImpl *>(ss));
    else
      ret = new DOMStyleSheet(exec,ss);
    interp->putDOMObject(ss, ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMStyleSheetList::info = { "StyleSheetList", 0, &DOMStyleSheetListTable, 0 };

/*
@begin DOMStyleSheetListTable 2
  length	DOMStyleSheetList::Length	DontDelete|ReadOnly
  item		DOMStyleSheetList::Item		DontDelete|Function 1
@end
*/
IMPLEMENT_PROTOFUNC(DOMStyleSheetListFunc) // not really a proto, but doesn't matter

DOMStyleSheetList::~DOMStyleSheetList()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

Value DOMStyleSheetList::tryGet(ExecState *exec, const Identifier &p) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMStyleSheetList::tryGet " << p.qstring() << endl;
#endif
  StyleSheetListImpl &styleSheetList = *m_impl;
  if (p == lengthPropertyName)
    return Number(styleSheetList.length());
  else if (p == "item")
    return lookupOrCreateFunction<DOMStyleSheetListFunc>(exec,p,this,DOMStyleSheetList::Item,1,DontDelete|Function);

  // Retrieve stylesheet by index
  bool ok;
  long unsigned int u = p.toULong(&ok);
  if (ok)
    return getDOMStyleSheet(exec, styleSheetList.item(u));

  // IE also supports retrieving a stylesheet by name, using the name/id of the <style> tag
  // (this is consistent with all the other collections)
#if 0
  // Bad implementation because DOM::StyleSheet doesn't inherit DOM::Node
  // so we can't use DOMNamedNodesCollection.....
  // We could duplicate it for stylesheets though - worth it ?
  // Other problem of this implementation: it doesn't look for the ID attribute!
  DOM::NameNodeListImpl namedList( m_doc.documentElement().handle(), p.string() );
  int len = namedList.length();
  if ( len ) {
    QValueList<DOM::Node> styleSheets;
    for ( int i = 0 ; i < len ; ++i ) {
      DOM::HTMLStyleElement elem = DOM::Node(namedList.item(i));
      if (!elem.isNull())
        styleSheets.append(elem.sheet());
    }
    if ( styleSheets.count() == 1 ) // single result
      return getDOMStyleSheet(exec, styleSheets[0]);
    else if ( styleSheets.count() > 1 ) {
      return new DOMNamedItemsCollection(exec,styleSheets);
    }
  }
#endif
  // ### Bad implementation because returns a single element (are IDs always unique?)
  // and doesn't look for name attribute (see implementation above).
  // But unicity of stylesheet ids is good practice anyway ;)
  ElementImpl *element = m_doc->getElementById(p.string());
  if (element && element->id() == ID_STYLE)
    return getDOMStyleSheet(exec, static_cast<HTMLStyleElementImpl *>(element)->sheet());

  return DOMObject::tryGet(exec, p);
}

ValueImp *getDOMStyleSheetList(ExecState *exec, StyleSheetListImpl *ssl, DocumentImpl *doc)
{
  // Can't use the cacheDOMObject macro because of the doc argument
  DOMObject *ret;
  if (!ssl)
    return Null();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());
  if ((ret = interp->getDOMObject(ssl)))
    return ret;
  else {
    ret = new DOMStyleSheetList(exec, ssl, doc);
    interp->putDOMObject(ssl, ret);
    return ret;
  }
}

Value DOMStyleSheetListFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMStyleSheetList::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  StyleSheetListImpl &styleSheetList = *static_cast<DOMStyleSheetList *>(thisObj.imp())->impl();
  if (id == DOMStyleSheetList::Item)
    return getDOMStyleSheet(exec, styleSheetList.item(args[0].toInt32(exec)));
  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMMediaList::info = { "MediaList", 0, &DOMMediaListTable, 0 };

/*
@begin DOMMediaListTable 2
  mediaText	DOMMediaList::MediaText		DontDelete|ReadOnly
  length	DOMMediaList::Length		DontDelete|ReadOnly
@end
@begin DOMMediaListProtoTable 3
  item		DOMMediaList::Item		DontDelete|Function 1
  deleteMedium	DOMMediaList::DeleteMedium	DontDelete|Function 1
  appendMedium	DOMMediaList::AppendMedium	DontDelete|Function 1
@end
*/
DEFINE_PROTOTYPE("DOMMediaList", DOMMediaListProto)
IMPLEMENT_PROTOFUNC(DOMMediaListProtoFunc)
IMPLEMENT_PROTOTYPE(DOMMediaListProto, DOMMediaListProtoFunc)

DOMMediaList::DOMMediaList(ExecState *exec, MediaListImpl *ml)
    : m_impl(ml)
{
  setPrototype(DOMMediaListProto::self(exec));
}

DOMMediaList::~DOMMediaList()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

Value DOMMediaList::tryGet(ExecState *exec, const Identifier &p) const
{
  MediaListImpl &mediaList = *m_impl;
  if (p == "mediaText")
    return getStringOrNull(mediaList.mediaText());
  else if (p == lengthPropertyName)
    return Number(mediaList.length());

  bool ok;
  long unsigned int u = p.toULong(&ok);
  if (ok)
    return getStringOrNull(mediaList.item(u));

  return DOMObject::tryGet(exec, p);
}

void DOMMediaList::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
  MediaListImpl &mediaList = *m_impl;
  if (propertyName == "mediaText")
    mediaList.setMediaText(value.toString(exec).string());
  else
    DOMObject::tryPut(exec, propertyName, value, attr);
}

ValueImp *getDOMMediaList(ExecState *exec, MediaListImpl *ml)
{
  return cacheDOMObject<MediaListImpl, DOMMediaList>(exec, ml);
}

Value KJS::DOMMediaListProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMMediaList::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  MediaListImpl &mediaList = *static_cast<DOMMediaList *>(thisObj.imp())->impl();
  switch (id) {
    case DOMMediaList::Item:
      return getStringOrNull(mediaList.item(args[0].toInt32(exec)));
    case DOMMediaList::DeleteMedium:
      mediaList.deleteMedium(args[0].toString(exec).string());
      return Undefined();
    case DOMMediaList::AppendMedium:
      mediaList.appendMedium(args[0].toString(exec).string());
      return Undefined();
    default:
      return Undefined();
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSStyleSheet::info = { "CSSStyleSheet", 0, &DOMCSSStyleSheetTable, 0 };

/*
@begin DOMCSSStyleSheetTable 5
  ownerRule	DOMCSSStyleSheet::OwnerRule	DontDelete|ReadOnly
  cssRules	DOMCSSStyleSheet::CssRules	DontDelete|ReadOnly
# MSIE extension
  rules		DOMCSSStyleSheet::Rules		DontDelete|ReadOnly
@end
@begin DOMCSSStyleSheetProtoTable 6
  insertRule	DOMCSSStyleSheet::InsertRule	DontDelete|Function 2
  deleteRule	DOMCSSStyleSheet::DeleteRule	DontDelete|Function 1
# MSIE extension
  addRule	DOMCSSStyleSheet::AddRule	DontDelete|Function 2
@end
*/
DEFINE_PROTOTYPE("DOMCSSStyleSheet",DOMCSSStyleSheetProto)
IMPLEMENT_PROTOFUNC(DOMCSSStyleSheetProtoFunc)
IMPLEMENT_PROTOTYPE(DOMCSSStyleSheetProto,DOMCSSStyleSheetProtoFunc) // warning, use _WITH_PARENT if DOMStyleSheet gets a proto

DOMCSSStyleSheet::DOMCSSStyleSheet(ExecState *exec, CSSStyleSheetImpl *ss)
  : DOMStyleSheet(ss) 
{
  setPrototype(DOMCSSStyleSheetProto::self(exec));
}

DOMCSSStyleSheet::~DOMCSSStyleSheet()
{
}

Value DOMCSSStyleSheet::tryGet(ExecState *exec, const Identifier &p) const
{
  CSSStyleSheetImpl &cssStyleSheet = *static_cast<CSSStyleSheetImpl *>(impl());
  if (p == "ownerRule")
    return getDOMCSSRule(exec,cssStyleSheet.ownerRule());
  else if (p == "cssRules" || p == "rules" /* MSIE extension */)
    return getDOMCSSRuleList(exec,cssStyleSheet.cssRules());
  return DOMStyleSheet::tryGet(exec,p);
}

Value DOMCSSStyleSheetProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMCSSStyleSheet::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOMExceptionTranslator exception(exec);
  CSSStyleSheetImpl &styleSheet = *static_cast<CSSStyleSheetImpl *>(static_cast<DOMCSSStyleSheet *>(thisObj.imp())->impl());
  Value result;
  switch (id) {
    case DOMCSSStyleSheet::InsertRule:
      return Number(styleSheet.insertRule(args[0].toString(exec).string(), args[1].toInt32(exec), exception));
      break;
    case DOMCSSStyleSheet::DeleteRule:
      styleSheet.deleteRule(args[0].toInt32(exec), exception);
      return Undefined();
    case DOMCSSStyleSheet::AddRule: {
      long index = args.size() >= 3 ? args[2].toInt32(exec) : -1;
      styleSheet.addRule(args[0].toString(exec).string(), args[1].toString(exec).string(), index, exception);
      // As per Microsoft documentation, always return -1.
      return Number(-1);
    }
  }
  return Undefined();
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSRuleList::info = { "CSSRuleList", 0, &DOMCSSRuleListTable, 0 };
/*
@begin DOMCSSRuleListTable 3
  length		DOMCSSRuleList::Length		DontDelete|ReadOnly
  item			DOMCSSRuleList::Item		DontDelete|Function 1
@end
*/
IMPLEMENT_PROTOFUNC(DOMCSSRuleListFunc) // not really a proto, but doesn't matter

DOMCSSRuleList::~DOMCSSRuleList()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

Value DOMCSSRuleList::tryGet(ExecState *exec, const Identifier &p) const
{
  CSSRuleListImpl &cssRuleList = *m_impl;
  if (p == lengthPropertyName)
    return Number(cssRuleList.length());
  else if (p == "item")
    return lookupOrCreateFunction<DOMCSSRuleListFunc>(exec, p, this, DOMCSSRuleList::Item, 1, DontDelete|Function);

  bool ok;
  long unsigned int u = p.toULong(&ok);
  if (ok)
    return getDOMCSSRule(exec, cssRuleList.item(u));

  return DOMObject::tryGet(exec, p);
}

Value DOMCSSRuleListFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMCSSRuleList::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  CSSRuleListImpl &cssRuleList = *static_cast<DOMCSSRuleList *>(thisObj.imp())->impl();
  switch (id) {
    case DOMCSSRuleList::Item:
      return getDOMCSSRule(exec,cssRuleList.item(args[0].toInt32(exec)));
    default:
      return Undefined();
  }
}

ValueImp *getDOMCSSRuleList(ExecState *exec, CSSRuleListImpl *rl)
{
  return cacheDOMObject<CSSRuleListImpl, DOMCSSRuleList>(exec, rl);
}

// -------------------------------------------------------------------------

IMPLEMENT_PROTOFUNC(DOMCSSRuleFunc) // Not a proto, but doesn't matter

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
  CSSRuleImpl &cssRule = *m_impl;
  switch (cssRule.type()) {
  case DOM::CSSRule::STYLE_RULE:
    return &style_info;
  case DOM::CSSRule::MEDIA_RULE:
    return &media_info;
  case DOM::CSSRule::FONT_FACE_RULE:
    return &fontface_info;
  case DOM::CSSRule::PAGE_RULE:
    return &page_info;
  case DOM::CSSRule::IMPORT_RULE:
    return &import_info;
  case DOM::CSSRule::CHARSET_RULE:
    return &charset_info;
  case DOM::CSSRule::UNKNOWN_RULE:
  default:
    return &info;
  }
}
/*
@begin DOMCSSRuleTable 4
  type			DOMCSSRule::Type	DontDelete|ReadOnly
  cssText		DOMCSSRule::CssText	DontDelete|ReadOnly
  parentStyleSheet	DOMCSSRule::ParentStyleSheet	DontDelete|ReadOnly
  parentRule		DOMCSSRule::ParentRule	DontDelete|ReadOnly
@end
@begin DOMCSSStyleRuleTable 2
  selectorText		DOMCSSRule::Style_SelectorText	DontDelete
  style			DOMCSSRule::Style_Style		DontDelete|ReadOnly
@end
@begin DOMCSSMediaRuleTable 4
  media			DOMCSSRule::Media_Media		DontDelete|ReadOnly
  cssRules		DOMCSSRule::Media_CssRules	DontDelete|ReadOnly
  insertRule		DOMCSSRule::Media_InsertRule	DontDelete|Function 2
  deleteRule		DOMCSSRule::Media_DeleteRule	DontDelete|Function 1
@end
@begin DOMCSSFontFaceRuleTable 1
  style			DOMCSSRule::FontFace_Style	DontDelete|ReadOnly
@end
@begin DOMCSSPageRuleTable 2
  selectorText		DOMCSSRule::Page_SelectorText	DontDelete
  style			DOMCSSRule::Page_Style		DontDelete|ReadOnly
@end
@begin DOMCSSImportRuleTable 3
  href			DOMCSSRule::Import_Href		DontDelete|ReadOnly
  media			DOMCSSRule::Import_Media	DontDelete|ReadOnly
  styleSheet		DOMCSSRule::Import_StyleSheet	DontDelete|ReadOnly
@end
@begin DOMCSSCharsetRuleTable 1
  encoding		DOMCSSRule::Charset_Encoding	DontDelete
@end
*/
Value DOMCSSRule::tryGet(ExecState *exec, const Identifier &propertyName) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "DOMCSSRule::tryGet " << propertyName.qstring() << endl;
#endif
  const HashEntry *entry = Lookup::findEntry(classInfo()->propHashTable, propertyName);
  if (entry) {
    if (entry->attr & Function)
      return lookupOrCreateFunction<DOMCSSRuleFunc>(exec, propertyName, this, entry->value, entry->params, entry->attr);
    return getValueProperty(exec, entry->value);
  }

  // Base CSSRule stuff or parent class forward, as usual
  return DOMObjectLookupGet<DOMCSSRuleFunc, DOMCSSRule, DOMObject>(exec, propertyName, &DOMCSSRuleTable, this);
}

Value DOMCSSRule::getValueProperty(ExecState *exec, int token) const
{
  CSSRuleImpl &cssRule = *m_impl;
  switch (token) {
  case Type:
    return Number(cssRule.type());
  case CssText:
    return getStringOrNull(cssRule.cssText());
  case ParentStyleSheet:
    return getDOMStyleSheet(exec,cssRule.parentStyleSheet());
  case ParentRule:
    return getDOMCSSRule(exec,cssRule.parentRule());

  // for DOM::CSSRule::STYLE_RULE:
  case Style_SelectorText:
    return getStringOrNull(static_cast<CSSStyleRuleImpl *>(m_impl.get())->selectorText());
  case Style_Style:
    return getDOMCSSStyleDeclaration(exec, static_cast<CSSStyleRuleImpl *>(m_impl.get())->style());

  // for DOM::CSSRule::MEDIA_RULE:
  case Media_Media:
    return getDOMMediaList(exec, static_cast<CSSMediaRuleImpl *>(m_impl.get())->media());
  case Media_CssRules:
    return getDOMCSSRuleList(exec, static_cast<CSSMediaRuleImpl *>(m_impl.get())->cssRules());

  // for DOM::CSSRule::FONT_FACE_RULE:
  case FontFace_Style:
    return getDOMCSSStyleDeclaration(exec, static_cast<CSSFontFaceRuleImpl *>(m_impl.get())->style());

  // for DOM::CSSRule::PAGE_RULE:
  case Page_SelectorText:
    return getStringOrNull(static_cast<CSSPageRuleImpl *>(m_impl.get())->selectorText());
  case Page_Style:
    return getDOMCSSStyleDeclaration(exec, static_cast<CSSPageRuleImpl *>(m_impl.get())->style());

  // for DOM::CSSRule::IMPORT_RULE:
  case Import_Href:
    return getStringOrNull(static_cast<CSSImportRuleImpl *>(m_impl.get())->href());
  case Import_Media:
    return getDOMMediaList(exec, static_cast<CSSImportRuleImpl *>(m_impl.get())->media());
  case Import_StyleSheet:
    return getDOMStyleSheet(exec, static_cast<CSSImportRuleImpl *>(m_impl.get())->styleSheet());

  // for DOM::CSSRule::CHARSET_RULE:
  case Charset_Encoding:
    return getStringOrNull(static_cast<CSSCharsetRuleImpl *>(m_impl.get())->encoding());

  default:
    kdWarning() << "DOMCSSRule::getValueProperty unhandled token " << token << endl;
  }
  return Undefined();
}

void DOMCSSRule::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
  const HashTable* table = classInfo()->propHashTable; // get the right hashtable
  const HashEntry* entry = Lookup::findEntry(table, propertyName);
  if (entry) {
    if (entry->attr & Function) // function: put as override property
    {
      ObjectImp::put(exec, propertyName, value, attr);
      return;
    }
    else if ((entry->attr & ReadOnly) == 0) // let DOMObjectLookupPut print the warning if not
    {
      putValue(exec, entry->value, value, attr);
      return;
    }
  }
  DOMObjectLookupPut<DOMCSSRule, DOMObject>(exec, propertyName, value, attr, &DOMCSSRuleTable, this);
}

void DOMCSSRule::putValue(ExecState *exec, int token, const Value& value, int)
{
  switch (token) {
  // for DOM::CSSRule::STYLE_RULE:
  case Style_SelectorText:
    static_cast<CSSStyleRuleImpl *>(m_impl.get())->setSelectorText(value.toString(exec).string());
    return;

  // for DOM::CSSRule::PAGE_RULE:
  case Page_SelectorText:
    static_cast<CSSPageRuleImpl *>(m_impl.get())->setSelectorText(value.toString(exec).string());
    return;

  // for DOM::CSSRule::CHARSET_RULE:
  case Charset_Encoding:
    static_cast<CSSCharsetRuleImpl *>(m_impl.get())->setEncoding(value.toString(exec).string());
    return;

  default:
    kdWarning() << "DOMCSSRule::putValue unhandled token " << token << endl;
  }
}

Value DOMCSSRuleFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMCSSRule::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  CSSRuleImpl &cssRule = *static_cast<DOMCSSRule *>(thisObj.imp())->impl();

  if (cssRule.type() == DOM::CSSRule::MEDIA_RULE) {
    CSSMediaRuleImpl &rule = static_cast<CSSMediaRuleImpl &>(cssRule);
    if (id == DOMCSSRule::Media_InsertRule)
      return Number(rule.insertRule(args[0].toString(exec).string(),args[1].toInt32(exec)));
    else if (id == DOMCSSRule::Media_DeleteRule)
      rule.deleteRule(args[0].toInt32(exec));
  }

  return Undefined();
}

ValueImp *getDOMCSSRule(ExecState *exec, CSSRuleImpl *r)
{
  return cacheDOMObject<CSSRuleImpl, DOMCSSRule>(exec, r);
}

// -------------------------------------------------------------------------

const ClassInfo CSSRuleConstructor::info = { "CSSRuleConstructor", 0, &CSSRuleConstructorTable, 0 };
/*
@begin CSSRuleConstructorTable 7
  UNKNOWN_RULE	CSSRuleConstructor::UNKNOWN_RULE	DontDelete|ReadOnly
  STYLE_RULE	CSSRuleConstructor::STYLE_RULE		DontDelete|ReadOnly
  CHARSET_RULE	CSSRuleConstructor::CHARSET_RULE	DontDelete|ReadOnly
  IMPORT_RULE	CSSRuleConstructor::IMPORT_RULE		DontDelete|ReadOnly
  MEDIA_RULE	CSSRuleConstructor::MEDIA_RULE		DontDelete|ReadOnly
  FONT_FACE_RULE CSSRuleConstructor::FONT_FACE_RULE	DontDelete|ReadOnly
  PAGE_RULE	CSSRuleConstructor::PAGE_RULE		DontDelete|ReadOnly
@end
*/

Value CSSRuleConstructor::tryGet(ExecState *exec, const Identifier &p) const
{
  return DOMObjectLookupGetValue<CSSRuleConstructor,DOMObject>(exec,p,&CSSRuleConstructorTable,this);
}

Value CSSRuleConstructor::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case UNKNOWN_RULE:
    return Number(DOM::CSSRule::UNKNOWN_RULE);
  case STYLE_RULE:
    return Number(DOM::CSSRule::STYLE_RULE);
  case CHARSET_RULE:
    return Number(DOM::CSSRule::CHARSET_RULE);
  case IMPORT_RULE:
    return Number(DOM::CSSRule::IMPORT_RULE);
  case MEDIA_RULE:
    return Number(DOM::CSSRule::MEDIA_RULE);
  case FONT_FACE_RULE:
    return Number(DOM::CSSRule::FONT_FACE_RULE);
  case PAGE_RULE:
    return Number(DOM::CSSRule::PAGE_RULE);
  }
  return Value();
}

Value getCSSRuleConstructor(ExecState *exec)
{
  return cacheGlobalObject<CSSRuleConstructor>( exec, "[[cssRule.constructor]]" );
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSValue::info = { "CSSValue", 0, &DOMCSSValueTable, 0 };

/*
@begin DOMCSSValueTable 2
  cssText	DOMCSSValue::CssText		DontDelete|ReadOnly
  cssValueType	DOMCSSValue::CssValueType	DontDelete|ReadOnly
@end
*/
DOMCSSValue::~DOMCSSValue()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

Value DOMCSSValue::tryGet(ExecState *exec, const Identifier &p) const
{
  CSSValueImpl &cssValue = *m_impl;
  if (p == "cssText")
    return getStringOrNull(cssValue.cssText());
  else if (p == "cssValueType");
    return Number(cssValue.cssValueType());
  return DOMObject::tryGet(exec,p);
}

void DOMCSSValue::tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
  CSSValueImpl &cssValue = *m_impl;
  if (propertyName == "cssText")
    cssValue.setCssText(value.toString(exec).string());
  else
    DOMObject::tryPut(exec, propertyName, value, attr);
}

ValueImp *getDOMCSSValue(ExecState *exec, CSSValueImpl *v)
{
  DOMObject *ret;
  if (!v)
    return Null();
  ScriptInterpreter* interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());
  if ((ret = interp->getDOMObject(v)))
    return Value(ret);
  else {
    if (v->isValueList())
      ret = new DOMCSSValueList(exec, static_cast<CSSValueListImpl *>(v));
    else if (v->isPrimitiveValue())
      ret = new DOMCSSPrimitiveValue(exec, static_cast<CSSPrimitiveValueImpl *>(v));
    else
      ret = new DOMCSSValue(exec,v);
    interp->putDOMObject(v, ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const ClassInfo CSSValueConstructor::info = { "CSSValueConstructor", 0, &CSSValueConstructorTable, 0 };
/*
@begin CSSValueConstructorTable 5
  CSS_INHERIT		CSSValueConstructor::CSS_INHERIT		DontDelete|ReadOnly
  CSS_PRIMITIVE_VALUE	CSSValueConstructor::CSS_PRIMITIVE_VALUE	DontDelete|ReadOnly
  CSS_VALUE_LIST	CSSValueConstructor::CSS_VALUE_LIST		DontDelete|ReadOnly
  CSS_CUSTOM		CSSValueConstructor::CSS_CUSTOM			DontDelete|ReadOnly
@end
*/
Value CSSValueConstructor::tryGet(ExecState *exec, const Identifier &p) const
{
  return DOMObjectLookupGetValue<CSSValueConstructor,DOMObject>(exec,p,&CSSValueConstructorTable,this);
}

Value CSSValueConstructor::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case CSS_INHERIT:
    return Number(DOM::CSSValue::CSS_INHERIT);
  case CSS_PRIMITIVE_VALUE:
    return Number(DOM::CSSValue::CSS_PRIMITIVE_VALUE);
  case CSS_VALUE_LIST:
    return Number(DOM::CSSValue::CSS_VALUE_LIST);
  case CSS_CUSTOM:
    return Number(DOM::CSSValue::CSS_CUSTOM);
  }
  return Value();
}

Value getCSSValueConstructor(ExecState *exec)
{
  return cacheGlobalObject<CSSValueConstructor>( exec, "[[cssValue.constructor]]" );
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSPrimitiveValue::info = { "CSSPrimitiveValue", 0, &DOMCSSPrimitiveValueTable, 0 };
/*
@begin DOMCSSPrimitiveValueTable 1
  primitiveType		DOMCSSPrimitiveValue::PrimitiveType	DontDelete|ReadOnly
@end
@begin DOMCSSPrimitiveValueProtoTable 3
  setFloatValue		DOMCSSPrimitiveValue::SetFloatValue	DontDelete|Function 2
  getFloatValue		DOMCSSPrimitiveValue::GetFloatValue	DontDelete|Function 1
  setStringValue	DOMCSSPrimitiveValue::SetStringValue	DontDelete|Function 2
  getStringValue	DOMCSSPrimitiveValue::GetStringValue	DontDelete|Function 0
  getCounterValue	DOMCSSPrimitiveValue::GetCounterValue	DontDelete|Function 0
  getRectValue		DOMCSSPrimitiveValue::GetRectValue	DontDelete|Function 0
  getRGBColorValue	DOMCSSPrimitiveValue::GetRGBColorValue	DontDelete|Function 0
@end
*/
DEFINE_PROTOTYPE("DOMCSSPrimitiveValue",DOMCSSPrimitiveValueProto)
IMPLEMENT_PROTOFUNC(DOMCSSPrimitiveValueProtoFunc)
IMPLEMENT_PROTOTYPE(DOMCSSPrimitiveValueProto,DOMCSSPrimitiveValueProtoFunc)

DOMCSSPrimitiveValue::DOMCSSPrimitiveValue(ExecState *exec, CSSPrimitiveValueImpl *v)
  : DOMCSSValue(v) 
{ 
  setPrototype(DOMCSSPrimitiveValueProto::self(exec));
}

Value DOMCSSPrimitiveValue::tryGet(ExecState *exec, const Identifier &p) const
{
  if (p == "primitiveType")
    return Number(static_cast<CSSPrimitiveValueImpl *>(impl())->primitiveType());
  return DOMObject::tryGet(exec,p);
}

Value DOMCSSPrimitiveValueProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMCSSPrimitiveValue::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOMExceptionTranslator exception(exec);
  CSSPrimitiveValueImpl &val = *static_cast<CSSPrimitiveValueImpl *>(static_cast<DOMCSSPrimitiveValue *>(thisObj.imp())->impl());
  switch (id) {
    case DOMCSSPrimitiveValue::SetFloatValue:
      val.setFloatValue(args[0].toInt32(exec), args[1].toNumber(exec), exception);
      return Undefined();
    case DOMCSSPrimitiveValue::GetFloatValue:
      return Number(val.getFloatValue(args[0].toInt32(exec)));
    case DOMCSSPrimitiveValue::SetStringValue:
      val.setStringValue(args[0].toInt32(exec), args[1].toString(exec).string(), exception);
      return Undefined();
    case DOMCSSPrimitiveValue::GetStringValue:
      return getStringOrNull(val.getStringValue());
    case DOMCSSPrimitiveValue::GetCounterValue:
      return getDOMCounter(exec,val.getCounterValue());
    case DOMCSSPrimitiveValue::GetRectValue:
      return getDOMRect(exec,val.getRectValue());
    case DOMCSSPrimitiveValue::GetRGBColorValue:
      return getDOMRGBColor(exec,val.getRGBColorValue());
    default:
      return Undefined();
  }
}

// -------------------------------------------------------------------------

const ClassInfo CSSPrimitiveValueConstructor::info = { "CSSPrimitiveValueConstructor", 0, &CSSPrimitiveValueConstructorTable, 0 };

/*
@begin CSSPrimitiveValueConstructorTable 27
  CSS_UNKNOWN   	DOM::CSSPrimitiveValue::CSS_UNKNOWN	DontDelete|ReadOnly
  CSS_NUMBER    	DOM::CSSPrimitiveValue::CSS_NUMBER	DontDelete|ReadOnly
  CSS_PERCENTAGE	DOM::CSSPrimitiveValue::CSS_PERCENTAGE	DontDelete|ReadOnly
  CSS_EMS       	DOM::CSSPrimitiveValue::CSS_EMS		DontDelete|ReadOnly
  CSS_EXS       	DOM::CSSPrimitiveValue::CSS_EXS		DontDelete|ReadOnly
  CSS_PX        	DOM::CSSPrimitiveValue::CSS_PX		DontDelete|ReadOnly
  CSS_CM        	DOM::CSSPrimitiveValue::CSS_CM		DontDelete|ReadOnly
  CSS_MM        	DOM::CSSPrimitiveValue::CSS_MM		DontDelete|ReadOnly
  CSS_IN        	DOM::CSSPrimitiveValue::CSS_IN		DontDelete|ReadOnly
  CSS_PT        	DOM::CSSPrimitiveValue::CSS_PT		DontDelete|ReadOnly
  CSS_PC        	DOM::CSSPrimitiveValue::CSS_PC		DontDelete|ReadOnly
  CSS_DEG       	DOM::CSSPrimitiveValue::CSS_DEG		DontDelete|ReadOnly
  CSS_RAD       	DOM::CSSPrimitiveValue::CSS_RAD		DontDelete|ReadOnly
  CSS_GRAD      	DOM::CSSPrimitiveValue::CSS_GRAD	DontDelete|ReadOnly
  CSS_MS        	DOM::CSSPrimitiveValue::CSS_MS		DontDelete|ReadOnly
  CSS_S			DOM::CSSPrimitiveValue::CSS_S		DontDelete|ReadOnly
  CSS_HZ        	DOM::CSSPrimitiveValue::CSS_HZ		DontDelete|ReadOnly
  CSS_KHZ       	DOM::CSSPrimitiveValue::CSS_KHZ		DontDelete|ReadOnly
  CSS_DIMENSION 	DOM::CSSPrimitiveValue::CSS_DIMENSION	DontDelete|ReadOnly
  CSS_STRING    	DOM::CSSPrimitiveValue::CSS_STRING	DontDelete|ReadOnly
  CSS_URI       	DOM::CSSPrimitiveValue::CSS_URI		DontDelete|ReadOnly
  CSS_IDENT     	DOM::CSSPrimitiveValue::CSS_IDENT	DontDelete|ReadOnly
  CSS_ATTR      	DOM::CSSPrimitiveValue::CSS_ATTR	DontDelete|ReadOnly
  CSS_COUNTER   	DOM::CSSPrimitiveValue::CSS_COUNTER	DontDelete|ReadOnly
  CSS_RECT      	DOM::CSSPrimitiveValue::CSS_RECT	DontDelete|ReadOnly
  CSS_RGBCOLOR  	DOM::CSSPrimitiveValue::CSS_RGBCOLOR	DontDelete|ReadOnly
@end
*/

Value CSSPrimitiveValueConstructor::tryGet(ExecState *exec, const Identifier &p) const
{
  return DOMObjectLookupGetValue<CSSPrimitiveValueConstructor,CSSValueConstructor>(exec,p,&CSSPrimitiveValueConstructorTable,this);
}

Value CSSPrimitiveValueConstructor::getValueProperty(ExecState *, int token) const
{
  // We use the token as the value to return directly
  return Number(token);
}

Value getCSSPrimitiveValueConstructor(ExecState *exec)
{
  return cacheGlobalObject<CSSPrimitiveValueConstructor>( exec, "[[cssPrimitiveValue.constructor]]" );
}

// -------------------------------------------------------------------------

const ClassInfo DOMCSSValueList::info = { "CSSValueList", 0, &DOMCSSValueListTable, 0 };

/*
@begin DOMCSSValueListTable 3
  length		DOMCSSValueList::Length		DontDelete|ReadOnly
  item			DOMCSSValueList::Item		DontDelete|Function 1
@end
*/
IMPLEMENT_PROTOFUNC(DOMCSSValueListFunc) // not really a proto, but doesn't matter

DOMCSSValueList::DOMCSSValueList(ExecState *exec, CSSValueListImpl *v)
  : DOMCSSValue(exec, v) 
{ 
}

Value DOMCSSValueList::tryGet(ExecState *exec, const Identifier &p) const
{
  CSSValueListImpl &valueList = *static_cast<CSSValueListImpl *>(impl());

  if (p == lengthPropertyName)
    return Number(valueList.length());
  else if (p == "item")
    return lookupOrCreateFunction<DOMCSSValueListFunc>(exec,p,this,DOMCSSValueList::Item,1,DontDelete|Function);

  bool ok;
  long unsigned int u = p.toULong(&ok);
  if (ok)
    return getDOMCSSValue(exec,valueList.item(u));

  return DOMCSSValue::tryGet(exec,p);
}

Value DOMCSSValueListFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMCSSValue::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  CSSValueListImpl &valueList = *static_cast<CSSValueListImpl *>(static_cast<DOMCSSValueList *>(thisObj.imp())->impl());
  switch (id) {
    case DOMCSSValueList::Item:
      return getDOMCSSValue(exec,valueList.item(args[0].toInt32(exec)));
    default:
      return Undefined();
  }
}

// -------------------------------------------------------------------------

const ClassInfo DOMRGBColor::info = { "RGBColor", 0, &DOMRGBColorTable, 0 };

/*
@begin DOMRGBColorTable 3
  red	DOMRGBColor::Red	DontDelete|ReadOnly
  green	DOMRGBColor::Green	DontDelete|ReadOnly
  blue	DOMRGBColor::Blue	DontDelete|ReadOnly
@end
*/
DOMRGBColor::~DOMRGBColor()
{
  //rgbColors.remove(rgbColor.handle());
}

Value DOMRGBColor::tryGet(ExecState *exec, const Identifier &p) const
{
  return DOMObjectLookupGetValue<DOMRGBColor,DOMObject>(exec, p,
						       &DOMRGBColorTable,
						       this);
}

Value DOMRGBColor::getValueProperty(ExecState *exec, int token) const
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
    return new DOMCSSPrimitiveValue(exec, new CSSPrimitiveValueImpl(color & 0xFF, CSSPrimitiveValue::CSS_NUMBER));
  default:
    return Value();
  }
}

ValueImp *getDOMRGBColor(ExecState *, unsigned c)
{
  // ### implement equals for RGBColor since they're not refcounted objects
  return new DOMRGBColor(c);
}

// -------------------------------------------------------------------------

const ClassInfo DOMRect::info = { "Rect", 0, &DOMRectTable, 0 };
/*
@begin DOMRectTable 4
  top	DOMRect::Top	DontDelete|ReadOnly
  right	DOMRect::Right	DontDelete|ReadOnly
  bottom DOMRect::Bottom DontDelete|ReadOnly
  left	DOMRect::Left	DontDelete|ReadOnly
@end
*/
DOMRect::~DOMRect()
{
  ScriptInterpreter::forgetDOMObject(m_rect.get());
}

Value DOMRect::tryGet(ExecState *exec, const Identifier &p) const
{
  return DOMObjectLookupGetValue<DOMRect,DOMObject>(exec, p,
						    &DOMRectTable, this);
}

Value DOMRect::getValueProperty(ExecState *exec, int token) const
{
  RectImpl &rect = *m_rect;
  switch (token) {
  case Top:
    return getDOMCSSValue(exec, rect.top());
  case Right:
    return getDOMCSSValue(exec, rect.right());
  case Bottom:
    return getDOMCSSValue(exec, rect.bottom());
  case Left:
    return getDOMCSSValue(exec, rect.left());
  default:
    return Value();
  }
}

ValueImp *getDOMRect(ExecState *exec, RectImpl *r)
{
  return cacheDOMObject<RectImpl, DOMRect>(exec, r);
}

// -------------------------------------------------------------------------

const ClassInfo DOMCounter::info = { "Counter", 0, &DOMCounterTable, 0 };
/*
@begin DOMCounterTable 3
  identifier	DOMCounter::identifier	DontDelete|ReadOnly
  listStyle	DOMCounter::listStyle	DontDelete|ReadOnly
  separator	DOMCounter::separator	DontDelete|ReadOnly
@end
*/

DOMCounter::~DOMCounter()
{
  ScriptInterpreter::forgetDOMObject(m_counter.get());
}

Value DOMCounter::tryGet(ExecState *exec, const Identifier &p) const
{
  return DOMObjectLookupGetValue<DOMCounter,DOMObject>(exec, p,
						       &DOMCounterTable, this);
}

Value DOMCounter::getValueProperty(ExecState *, int token) const
{
  CounterImpl &counter = *m_counter;
  switch (token) {
  case identifier:
    return getStringOrNull(counter.identifier());
  case listStyle:
    return getStringOrNull(counter.listStyle());
  case separator:
    return getStringOrNull(counter.separator());
  default:
    return Value();
  }
}

ValueImp *getDOMCounter(ExecState *exec, CounterImpl *c)
{
  return cacheDOMObject<CounterImpl, DOMCounter>(exec, c);
}

}
