/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#include <qptrdict.h>
#include <qregexp.h>

#include <xml/dom_nodeimpl.h>
#include <dom/html_element.h>
#include <dom/dom_string.h>
#include <html/html_elementimpl.h>
#include <rendering/render_style.h>
#include <kjs/types.h>
#include <css/cssproperties.h>
#include <css/cssparser.h>
#include "kjs_binding.h"
#include "kjs_dom.h"

using namespace KJS;
#include <kdebug.h>

QPtrDict<DOMCSSStyleDeclaration> domCSSStyleDeclarations;
QPtrDict<DOMStyleSheet> styleSheets;
QPtrDict<DOMStyleSheetList> styleSheetLists;
QPtrDict<DOMMediaList> mediaLists;
QPtrDict<DOMCSSRuleList> cssRuleLists;
QPtrDict<DOMCSSRule> cssRules;
QPtrDict<DOMCSSValue> cssValues;
QPtrDict<DOMRect> rects;
QPtrDict<DOMCounter> counters;

static QString jsNameToProp( const UString &p )
{
    QString prop = p.qstring();
    int i = prop.length();
    while( --i ) {
	char c = prop[i].latin1();
	if ( c < 'A' || c > 'Z' )
	    continue;
	prop.insert( i, '-' );
    }

    return prop.lower();
}

DOMCSSStyleDeclaration::~DOMCSSStyleDeclaration()
{
  domCSSStyleDeclarations.remove(styleDecl.handle());
}

const TypeInfo DOMCSSStyleDeclaration::info = { "CSSStyleDeclaration", HostType, 0, 0, 0 };


bool DOMCSSStyleDeclaration::hasProperty(const UString &p,
					 bool recursive) const
{
  if (p == "cssText" ||
      p == "getPropertyValue" ||
      p == "getPropertyCSSValue" ||
      p == "removeProperty" ||
      p == "getPropertyPriority" ||
      p == "setProperty" ||
      p == "length" ||
      p == "item")
      return true;
  
  DOM::DOMString cssprop = jsNameToProp(p);
  if (DOM::getPropertyID(cssprop.string().ascii(), cssprop.length()))
      return true;
  
  return (recursive && HostImp::hasProperty(p, true));
}

KJSO DOMCSSStyleDeclaration::tryGet(const UString &p) const
{
  if (p == "cssText" )
    return getString(styleDecl.cssText());
  else if (p == "getPropertyValue")
    return new DOMCSSStyleDeclarationFunc(styleDecl,DOMCSSStyleDeclarationFunc::GetPropertyValue);
  else if (p == "getPropertyCSSValue")
    return new DOMCSSStyleDeclarationFunc(styleDecl,DOMCSSStyleDeclarationFunc::GetPropertyCSSValue);
  else if (p == "removeProperty")
    return new DOMCSSStyleDeclarationFunc(styleDecl,DOMCSSStyleDeclarationFunc::RemoveProperty);
  else if (p == "getPropertyPriority")
    return new DOMCSSStyleDeclarationFunc(styleDecl,DOMCSSStyleDeclarationFunc::GetPropertyPriority);
  else if (p == "setProperty")
    return new DOMCSSStyleDeclarationFunc(styleDecl,DOMCSSStyleDeclarationFunc::SetProperty);
  else if (p == "length" )
    return Number(styleDecl.length());
  else if (p == "item")
    return new DOMCSSStyleDeclarationFunc(styleDecl,DOMCSSStyleDeclarationFunc::Item);
  else if (p == "parentRule" )
    return Undefined(); // ###
  else {
    bool ok;
    long unsigned int u = p.toULong(&ok);
    if (ok)
      return getString(DOM::CSSStyleDeclaration(styleDecl).item(u));

    DOM::CSSStyleDeclaration styleDecl2 = styleDecl;
    DOM::DOMString v = styleDecl2.getPropertyValue(DOM::DOMString(jsNameToProp(p)));
    if (!v.isNull())
	return getString(v);
  }
  return DOMObject::tryGet(p);
}


void DOMCSSStyleDeclaration::tryPut(const UString &p, const KJSO& v)
{
  if (p == "cssText") {
    styleDecl.setCssText(v.toString().value().string());
  }
  else {
    QString prop = jsNameToProp(p);
    QString propvalue = v.toString().value().qstring();

    if(prop.left(6) == "pixel-")
    {
      prop = prop.mid(6); // cut it away
      propvalue += "px";
    }
    styleDecl.removeProperty(prop);
    if(!propvalue.isEmpty())
      styleDecl.setProperty(prop,DOM::DOMString(propvalue),""); // ### is "" ok for priority?
  }
}

Completion DOMCSSStyleDeclarationFunc::tryExecute(const List &args)
{
  KJSO result;
  String str = args[0].toString();
  DOM::DOMString s = str.value().string();

  switch (_id) {
    case GetPropertyValue:
      result = getString(styleDecl.getPropertyValue(s));
      break;
    case GetPropertyCSSValue:
      result = Undefined(); // ###
      break;
    case RemoveProperty:
      result = getString(styleDecl.removeProperty(s));
      break;
    case GetPropertyPriority:
      result = getString(styleDecl.getPropertyPriority(s));
      break;
    case SetProperty:
      styleDecl.setProperty(args[0].toString().value().string(),
                            args[1].toString().value().string(),
                            args[2].toString().value().string());
      result = Undefined();
      break;
    case Item:
      result = getString(styleDecl.item(args[0].toNumber().intValue()));
      break;
    default:
      result = Undefined();
  }

  return Completion(ReturnValue, result);
}

KJSO KJS::getDOMCSSStyleDeclaration(DOM::CSSStyleDeclaration s)
{
  DOMCSSStyleDeclaration *ret;
  if (s.isNull())
    return Null();
  else if ((ret = domCSSStyleDeclarations[s.handle()]))
    return ret;
  else {
    ret = new DOMCSSStyleDeclaration(s);
    domCSSStyleDeclarations.insert(s.handle(),ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const TypeInfo DOMStyleSheet::info = { "StyleSheet", HostType, 0, 0, 0 };

DOMStyleSheet::~DOMStyleSheet()
{
  styleSheets.remove(styleSheet.handle());
}

KJSO DOMStyleSheet::tryGet(const UString &p) const
{
  KJSO result;

  if (p == "type")
    return getString(styleSheet.type());
  else if (p == "disabled")
    return Boolean(styleSheet.disabled());
  else if (p == "ownerNode")
    return getDOMNode(styleSheet.ownerNode());
  else if (p == "parentStyleSheet")
    return getDOMStyleSheet(styleSheet.parentStyleSheet());
  else if (p == "href")
    return getString(styleSheet.href());
  else if (p == "title")
    return getString(styleSheet.title());
//  else if ( p == "media") ###
//    return getDOMMediaList(styleSheet.media());
  return DOMObject::tryGet(p);
}

void DOMStyleSheet::tryPut(const UString &p, const KJSO& v)
{
  if (p == "disabled") {
    styleSheet.setDisabled(v.toBoolean().value());
  }
  else
    DOMObject::tryPut(p, v);
}

KJSO KJS::getDOMStyleSheet(DOM::StyleSheet ss)
{
  DOMStyleSheet *ret;
  if (ss.isNull())
    return Null();
  else if ((ret = styleSheets[ss.handle()]))
    return ret;
  else {
    if (ss.isCSSStyleSheet()) {
      DOM::CSSStyleSheet cs;
      cs = ss;
      ret = new DOMCSSStyleSheet(cs);
    }
    else
      ret = new DOMStyleSheet(ss);
    styleSheets.insert(ss.handle(),ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const TypeInfo DOMStyleSheetList::info = { "StyleSheetList", HostType, 0, 0, 0 };

DOMStyleSheetList::~DOMStyleSheetList()
{
  styleSheetLists.remove(styleSheetList.handle());
}

KJSO DOMStyleSheetList::tryGet(const UString &p) const
{
  if (p == "length")
    return Number(styleSheetList.length());
  else if (p == "item")
    return new DOMStyleSheetListFunc(styleSheetList,DOMStyleSheetListFunc::Item);

  bool ok;
  long unsigned int u = p.toULong(&ok);
  if (ok)
    return getDOMStyleSheet(DOM::StyleSheetList(styleSheetList).item(u));

  return DOMObject::tryGet(p);
}

KJSO KJS::getDOMStyleSheetList(DOM::StyleSheetList ssl)
{
  DOMStyleSheetList *ret;
  if (ssl.isNull())
    return Null();
  else if ((ret = styleSheetLists[ssl.handle()]))
    return ret;
  else {
    ret = new DOMStyleSheetList(ssl);
    styleSheetLists.insert(ssl.handle(),ret);
    return ret;
  }
}

Completion DOMStyleSheetListFunc::tryExecute(const List &args)
{
  KJSO result;

  if (_id == Item)
    result = getDOMStyleSheet(styleSheetList.item(args[0].toNumber().intValue()));
  return Completion(ReturnValue, result);
}

// -------------------------------------------------------------------------

const TypeInfo DOMMediaList::info = { "MediaList", HostType, 0, 0, 0 };

DOMMediaList::~DOMMediaList()
{
  mediaLists.remove(mediaList.handle());
}

KJSO DOMMediaList::tryGet(const UString &p) const
{
  DOM::MediaList list = DOM::MediaList(mediaList.handle());
//  DOM::MediaListImpl *handle = mediaList.handle();
  if (p == "mediaText")
    return getString(list.mediaText());
  else if (p == "length")
    return Number(list.length());
  else if (p == "item")
    return new DOMMediaListFunc(list,DOMMediaListFunc::Item);
  else if (p == "deleteMedium")
    return new DOMMediaListFunc(list,DOMMediaListFunc::DeleteMedium);
  else if (p == "appendMedium")
    return new DOMMediaListFunc(list,DOMMediaListFunc::AppendMedium);

  bool ok;
  long unsigned int u = p.toULong(&ok);
  if (ok)
    return getString(list.item(u));

  return DOMObject::tryGet(p);
}

void DOMMediaList::tryPut(const UString &p, const KJSO& v)
{
  if (p == "mediaText")
    mediaList.setMediaText(v.toString().value().string());
  else
    DOMObject::tryPut(p, v);
}

KJSO KJS::getDOMMediaList(DOM::MediaList ml)
{
  DOMMediaList *ret;
  if (ml.isNull())
    return Null();
  else if ((ret = mediaLists[ml.handle()]))
    return ret;
  else {
    ret = new DOMMediaList(ml);
    mediaLists.insert(ml.handle(),ret);
    return ret;
  }
}

Completion DOMMediaListFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (_id) {
    case Item:
      result = getString(mediaList.item(args[0].toNumber().intValue()));
      break;
    case DeleteMedium:
      mediaList.deleteMedium(args[0].toString().value().string());
      result = Undefined();
      break;
    case AppendMedium:
      mediaList.appendMedium(args[0].toString().value().string());
      result = Undefined();
      break;
    default:
      break;
  }

  return Completion(ReturnValue, result);
}

// -------------------------------------------------------------------------

const TypeInfo DOMCSSStyleSheet::info = { "CSSStyleSheet", HostType, 0, 0, 0 };

DOMCSSStyleSheet::~DOMCSSStyleSheet()
{
}

KJSO DOMCSSStyleSheet::tryGet(const UString &p) const
{
  KJSO result;

  DOM::CSSStyleSheet cssStyleSheet = static_cast<DOM::CSSStyleSheet>(styleSheet);

  if (p == "ownerRule")
    return getDOMCSSRule(cssStyleSheet.ownerRule());
  else if (p == "cssRules")
    return getDOMCSSRuleList(cssStyleSheet.cssRules());
  else if (p == "insertRule")
    return new DOMCSSStyleSheetFunc(cssStyleSheet,DOMCSSStyleSheetFunc::InsertRule);
  else if (p == "deleteRule")
    return new DOMCSSStyleSheetFunc(cssStyleSheet,DOMCSSStyleSheetFunc::DeleteRule);

  return DOMStyleSheet::tryGet(p);
}

Completion DOMCSSStyleSheetFunc::tryExecute(const List &args)
{
  KJSO result;
  String str = args[0].toString();
  DOM::DOMString s = str.value().string();

  switch (_id) {
    case InsertRule:
      result = Number(styleSheet.insertRule(args[0].toString().value().string(),(long unsigned int)args[1].toNumber().intValue()));
      break;
    case DeleteRule:
      styleSheet.deleteRule(args[0].toNumber().intValue());
      break;
    default:
      result = Undefined();
  }

  return Completion(ReturnValue, result);
}

// -------------------------------------------------------------------------

const TypeInfo DOMCSSRuleList::info = { "CSSRuleList", HostType, 0, 0, 0 };

DOMCSSRuleList::~DOMCSSRuleList()
{
  cssRuleLists.remove(cssRuleList.handle());
}

KJSO DOMCSSRuleList::tryGet(const UString &p) const
{
  KJSO result;

  if (p == "length")
    return Number(cssRuleList.length());
  else if (p == "item")
    return new DOMCSSRuleListFunc(cssRuleList,DOMCSSRuleListFunc::Item);

  bool ok;
  long unsigned int u = p.toULong(&ok);
  if (ok)
    return getDOMCSSRule(DOM::CSSRuleList(cssRuleList).item(u));

  return DOMObject::tryGet(p);
}

Completion DOMCSSRuleListFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (_id) {
    case Item:
      result = getDOMCSSRule(cssRuleList.item(args[0].toNumber().intValue()));
      break;
    default:
      result = Undefined();
  }

  return Completion(ReturnValue, result);
}

KJSO KJS::getDOMCSSRuleList(DOM::CSSRuleList rl)
{
  DOMCSSRuleList *ret;
  if (rl.isNull())
    return Null();
  else if ((ret = cssRuleLists[rl.handle()]))
    return ret;
  else {
    ret = new DOMCSSRuleList(rl);
    cssRuleLists.insert(rl.handle(),ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const TypeInfo DOMCSSRule::info = { "CSSRule", HostType, 0, 0, 0 };

DOMCSSRule::~DOMCSSRule()
{
  cssRules.remove(cssRule.handle());
}

KJSO DOMCSSRule::tryGet(const UString &p) const
{
  KJSO result;

  switch (cssRule.type()) {
    case DOM::CSSRule::STYLE_RULE: {
        DOM::CSSStyleRule rule = static_cast<DOM::CSSStyleRule>(cssRule);
        if (p == "selectorText") return getString(rule.selectorText());
        if (p == "style") return getDOMCSSStyleDeclaration(rule.style());
        break;
      }
    case DOM::CSSRule::MEDIA_RULE: {
        DOM::CSSMediaRule rule = static_cast<DOM::CSSMediaRule>(cssRule);
        if (p == "media") return getDOMMediaList(rule.media());
        if (p == "cssRules") return getDOMCSSRuleList(rule.cssRules());
        if (p == "insertRule") return new DOMCSSRuleFunc(rule,DOMCSSRuleFunc::InsertRule);
        if (p == "deleteRule") return new DOMCSSRuleFunc(rule,DOMCSSRuleFunc::DeleteRule);
        break;
      }
    case DOM::CSSRule::FONT_FACE_RULE: {
        DOM::CSSFontFaceRule rule = static_cast<DOM::CSSFontFaceRule>(cssRule);
        if (p == "style") return getDOMCSSStyleDeclaration(rule.style());
        break;
      }
    case DOM::CSSRule::PAGE_RULE: {
        DOM::CSSPageRule rule = static_cast<DOM::CSSPageRule>(cssRule);
        if (p == "selectorText") return getString(rule.selectorText());
        if (p == "style") return getDOMCSSStyleDeclaration(rule.style());
        break;
      }
    case DOM::CSSRule::IMPORT_RULE: {
        DOM::CSSImportRule rule = static_cast<DOM::CSSImportRule>(cssRule);
        if (p == "href") return getString(rule.href());
        if (p == "media") return getDOMMediaList(rule.media());
        if (p == "styleSheet") return getDOMStyleSheet(rule.styleSheet());
        break;
      }
    case DOM::CSSRule::CHARSET_RULE: {
        DOM::CSSCharsetRule rule = static_cast<DOM::CSSCharsetRule>(cssRule);
        if (p == "encoding") return getString(rule.encoding());
        break;
      }
    case DOM::CSSRule::UNKNOWN_RULE:
      break;
  }

  if (p == "type")
    return Number(cssRule.type());
  else if (p == "cssText")
    return getString(cssRule.cssText());
  else if (p == "parentStyleSheet")
    return getDOMStyleSheet(cssRule.parentStyleSheet());
  else if (p == "parentRule")
    return getDOMCSSRule(cssRule.parentRule());

  return DOMObject::tryGet(p);
};

void DOMCSSRule::tryPut(const UString &p, const KJSO& v)
{

  switch (cssRule.type()) {
    case DOM::CSSRule::STYLE_RULE: {
        DOM::CSSStyleRule rule = static_cast<DOM::CSSStyleRule>(cssRule);
        if (p == "selectorText") {
          rule.setSelectorText(v.toString().value().string());
          return;
        }
        break;
      }
    case DOM::CSSRule::PAGE_RULE: {
        DOM::CSSPageRule rule = static_cast<DOM::CSSPageRule>(cssRule);
        if (p == "selectorText") {
          rule.setSelectorText(v.toString().value().string());
          return;
        }
        break;
      }
    case DOM::CSSRule::CHARSET_RULE: {
        DOM::CSSCharsetRule rule = static_cast<DOM::CSSCharsetRule>(cssRule);
        if (p == "encoding") {
          rule.setEncoding(v.toString().value().string());
          return;
        }
        break;
      }
    default:
      break;
  }

  DOMObject::tryPut(p,v);

}

Completion DOMCSSRuleFunc::tryExecute(const List &args)
{
  KJSO result = Undefined();

  if (cssRule.type() == DOM::CSSRule::MEDIA_RULE) {
    DOM::CSSMediaRule rule = static_cast<DOM::CSSMediaRule>(cssRule);
    if (_id == InsertRule)
      result = Number(rule.insertRule(args[0].toString().value().string(),args[1].toNumber().intValue()));
    else if (_id == DeleteRule)
      rule.deleteRule(args[0].toNumber().intValue());
  }

  return Completion(ReturnValue, result);
}

KJSO KJS::getDOMCSSRule(DOM::CSSRule r)
{
  DOMCSSRule *ret;
  if (r.isNull())
    return Null();
  else if ((ret = cssRules[r.handle()]))
    return ret;
  else {
    ret = new DOMCSSRule(r);
    cssRules.insert(r.handle(),ret);
    return ret;
  }
}

// -------------------------------------------------------------------------


DOM::CSSRule KJS::toCSSRule(const KJSO& obj)
{
  if (!obj.derivedFrom("CSSRule"))
    return DOM::CSSRule();

  const DOMCSSRule *dobj = static_cast<const DOMCSSRule*>(obj.imp());
  return dobj->toCSSRule();
}

// -------------------------------------------------------------------------

const TypeInfo CSSRulePrototype::info = { "CSSRulePrototype", HostType, 0, 0, 0 };

KJSO CSSRulePrototype::tryGet(const UString &p) const
{

// also prototype of CSSRule objects?
  if (p == "UNKNOWN_RULE")
    return Number(DOM::CSSRule::UNKNOWN_RULE);
  else if (p == "STYLE_RULE")
    return Number(DOM::CSSRule::STYLE_RULE);
  else if (p == "CHARSET_RULE")
    return Number(DOM::CSSRule::CHARSET_RULE);
  else if (p == "IMPORT_RULE")
    return Number(DOM::CSSRule::IMPORT_RULE);
  else if (p == "MEDIA_RULE")
    return Number(DOM::CSSRule::MEDIA_RULE);
  else if (p == "FONT_FACE_RULE")
    return Number(DOM::CSSRule::FONT_FACE_RULE);
  else if (p == "PAGE_RULE")
    return Number(DOM::CSSRule::PAGE_RULE);

  return DOMObject::tryGet(p);
}

KJSO KJS::getCSSRulePrototype()
{
    KJSO proto = Global::current().get("[[cssRule.prototype]]");
    if (proto.isDefined())
        return proto;
    else
    {
        Object cssRuleProto( new CSSRulePrototype );
        Global::current().put("[[cssRule.prototype]]", cssRuleProto);
        return cssRuleProto;
    }
}

// -------------------------------------------------------------------------

const TypeInfo DOMCSSValue::info = { "CSSValue", HostType, 0, 0, 0 };

DOMCSSValue::~DOMCSSValue()
{
  cssValues.remove(cssValue.handle());
}

KJSO DOMCSSValue::tryGet(const UString &p) const
{
  KJSO result;

  if (p == "cssText")
    return getString(cssValue.cssText());
  else if (p == "cssValueType");
    return Number(cssValue.cssValueType());

  return DOMObject::tryGet(p);
};

void DOMCSSValue::tryPut(const UString &p, const KJSO& v)
{
  if (p == "cssText")
    cssValue.setCssText(v.toString().value().string());
  else
    DOMObject::tryPut(p, v);
}

KJSO KJS::getDOMCSSValue(DOM::CSSValue v)
{
  DOMCSSValue *ret;
  if (v.isNull())
    return Null();
  else if ((ret = cssValues[v.handle()]))
    return ret;
  else {
    if (v.isCSSValueList())
      ret = new DOMCSSValueList(v);
    else if (v.isCSSPrimitiveValue())
      ret = new DOMCSSPrimitiveValue(v);
    else
      ret = new DOMCSSValue(v);
    cssValues.insert(v.handle(),ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const TypeInfo CSSValuePrototype::info = { "CSSValuePrototype", HostType, 0, 0, 0 };

KJSO CSSValuePrototype::tryGet(const UString &p) const
{
// also prototype of CSSValue objects?
  if (p == "CSS_INHERIT")
    return Number(DOM::CSSValue::CSS_INHERIT);
  else if (p == "CSS_PRIMITIVE_VALUE")
    return Number(DOM::CSSValue::CSS_PRIMITIVE_VALUE);
  else if (p == "CSS_VALUE_LIST")
    return Number(DOM::CSSValue::CSS_VALUE_LIST);
  else if (p == "CSS_CUSTOM")
    return Number(DOM::CSSValue::CSS_CUSTOM);

  return DOMObject::tryGet(p);
}

KJSO KJS::getCSSValuePrototype()
{
    KJSO proto = Global::current().get("[[cssValue.prototype]]");
    if (proto.isDefined())
        return proto;
    else
    {
        Object cssValueProto( new CSSRulePrototype );
        Global::current().put("[[cssValue.prototype]]", cssValueProto);
        return cssValueProto;
    }
}

// -------------------------------------------------------------------------

const TypeInfo DOMCSSPrimitiveValue::info = { "CSSPrimitiveValue", HostType, 0, 0, 0 };

KJSO DOMCSSPrimitiveValue::tryGet(const UString &p) const
{
  KJSO result;
  DOM::CSSPrimitiveValue val = static_cast<DOM::CSSPrimitiveValue>(cssValue);

  if (p == "primitiveType")
    return Number(val.primitiveType());
  if (p == "setFloatValue")
    return new DOMCSSPrimitiveValueFunc(val,DOMCSSPrimitiveValueFunc::SetFloatValue);
  if (p == "getFloatValue")
    return new DOMCSSPrimitiveValueFunc(val,DOMCSSPrimitiveValueFunc::GetFloatValue);
  if (p == "setStringValue")
    return new DOMCSSPrimitiveValueFunc(val,DOMCSSPrimitiveValueFunc::SetStringValue);
  if (p == "getStringValue")
    return new DOMCSSPrimitiveValueFunc(val,DOMCSSPrimitiveValueFunc::GetStringValue);
  if (p == "getCounterValue")
    return new DOMCSSPrimitiveValueFunc(val,DOMCSSPrimitiveValueFunc::GetCounterValue);
  if (p == "getRectValue")
    return new DOMCSSPrimitiveValueFunc(val,DOMCSSPrimitiveValueFunc::GetRectValue);
  if (p == "getRGBColorValue")
    return new DOMCSSPrimitiveValueFunc(val,DOMCSSPrimitiveValueFunc::GetRGBColorValue);

  return DOMObject::tryGet(p);
};

Completion DOMCSSPrimitiveValueFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (_id) {
    case SetFloatValue:
      val.setFloatValue(args[0].toNumber().intValue(),args[1].toNumber().value());
      result = Undefined();
      break;
    case GetFloatValue:
      result = Number(val.getFloatValue(args[0].toNumber().intValue()));
      break;
    case SetStringValue:
      val.setStringValue(args[0].toNumber().intValue(),args[1].toString().value().string());
      result = Undefined();
      break;
    case GetStringValue:
      result = getString(val.getStringValue());
      break;
    case GetCounterValue:
      result = getDOMCounter(val.getCounterValue());
      break;
    case GetRectValue:
      result = getDOMRect(val.getRectValue());
      break;
    case GetRGBColorValue:
      result = getDOMRGBColor(val.getRGBColorValue());
      break;
    default:
      result = Undefined();
  }

  return Completion(ReturnValue, result);
}

// -------------------------------------------------------------------------

const TypeInfo CSSPrimitiveValuePrototype::info = { "CSSPrimitiveValuePrototype", HostType, 0, 0, 0 };

KJSO CSSPrimitiveValuePrototype::tryGet(const UString &p) const
{
// also prototype of CSSPrimitiveValue objects?
  if (p == "CSS_UNKNOWN")    return Number(DOM::CSSPrimitiveValue::CSS_UNKNOWN);
  if (p == "CSS_NUMBER")     return Number(DOM::CSSPrimitiveValue::CSS_NUMBER);
  if (p == "CSS_PERCENTAGE") return Number(DOM::CSSPrimitiveValue::CSS_PERCENTAGE);
  if (p == "CSS_EMS")        return Number(DOM::CSSPrimitiveValue::CSS_EMS);
  if (p == "CSS_EXS")        return Number(DOM::CSSPrimitiveValue::CSS_EXS);
  if (p == "CSS_PX")         return Number(DOM::CSSPrimitiveValue::CSS_PX);
  if (p == "CSS_CM")         return Number(DOM::CSSPrimitiveValue::CSS_CM);
  if (p == "CSS_MM")         return Number(DOM::CSSPrimitiveValue::CSS_MM);
  if (p == "CSS_IN")         return Number(DOM::CSSPrimitiveValue::CSS_IN);
  if (p == "CSS_PT")         return Number(DOM::CSSPrimitiveValue::CSS_PT);
  if (p == "CSS_PC")         return Number(DOM::CSSPrimitiveValue::CSS_PC);
  if (p == "CSS_DEG")        return Number(DOM::CSSPrimitiveValue::CSS_DEG);
  if (p == "CSS_RAD")        return Number(DOM::CSSPrimitiveValue::CSS_RAD);
  if (p == "CSS_GRAD")       return Number(DOM::CSSPrimitiveValue::CSS_GRAD);
  if (p == "CSS_MS")         return Number(DOM::CSSPrimitiveValue::CSS_MS);
  if (p == "CSS_S")          return Number(DOM::CSSPrimitiveValue::CSS_S);
  if (p == "CSS_HZ")         return Number(DOM::CSSPrimitiveValue::CSS_HZ);
  if (p == "CSS_KHZ")        return Number(DOM::CSSPrimitiveValue::CSS_KHZ);
  if (p == "CSS_DIMENSION")  return Number(DOM::CSSPrimitiveValue::CSS_DIMENSION);
  if (p == "CSS_STRING")     return Number(DOM::CSSPrimitiveValue::CSS_STRING);
  if (p == "CSS_URI")        return Number(DOM::CSSPrimitiveValue::CSS_URI);
  if (p == "CSS_IDENT")      return Number(DOM::CSSPrimitiveValue::CSS_IDENT);
  if (p == "CSS_ATTR")       return Number(DOM::CSSPrimitiveValue::CSS_ATTR);
  if (p == "CSS_COUNTER")    return Number(DOM::CSSPrimitiveValue::CSS_COUNTER);
  if (p == "CSS_RECT")       return Number(DOM::CSSPrimitiveValue::CSS_RECT);
  if (p == "CSS_RGBCOLOR")   return Number(DOM::CSSPrimitiveValue::CSS_RGBCOLOR);
  return CSSValuePrototype::tryGet(p);
}

KJSO KJS::getCSSPrimitiveValuePrototype()
{
    KJSO proto = Global::current().get("[[cssPrimitiveValue.prototype]]");
    if (proto.isDefined())
        return proto;
    else
    {
        Object cssPrimitiveValueProto( new CSSRulePrototype );
        Global::current().put("[[cssPrimitiveValue.prototype]]", cssPrimitiveValueProto);
        return cssPrimitiveValueProto;
    }
}

// -------------------------------------------------------------------------

const TypeInfo DOMCSSValueList::info = { "CSSValueList", HostType, 0, 0, 0 };

KJSO DOMCSSValueList::tryGet(const UString &p) const
{
  KJSO result;
  DOM::CSSValueList valueList = static_cast<DOM::CSSValueList>(cssValue);

  if (p == "length")
    return Number(valueList.length());
  else if (p == "item")
    return new DOMCSSValueListFunc(valueList,DOMCSSValueListFunc::Item);

  bool ok;
  long unsigned int u = p.toULong(&ok);
  if (ok)
    return getDOMCSSValue(valueList.item(u));

  return DOMCSSValue::tryGet(p);
};

Completion DOMCSSValueListFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (_id) {
    case Item:
      result = getDOMCSSValue(valueList.item(args[0].toNumber().intValue()));
      break;
    default:
      result = Undefined();
      break;
  }

  return Completion(ReturnValue, result);
}

// -------------------------------------------------------------------------

const TypeInfo DOMRGBColor::info = { "RGBColor", HostType, 0, 0, 0 };

DOMRGBColor::~DOMRGBColor()
{
  //rgbColors.remove(rgbColor.handle());
}

KJSO DOMRGBColor::tryGet(const UString &p) const
{
  if (p == "red")
    return getDOMCSSValue(rgbColor.red());
  if (p == "green")
    return getDOMCSSValue(rgbColor.green());
  if (p == "blue")
    return getDOMCSSValue(rgbColor.blue());

  return DOMObject::tryGet(p);
}

KJSO KJS::getDOMRGBColor(DOM::RGBColor c)
{
  // ### implement equals for RGBColor since they're not refcounted objects
  return new DOMRGBColor(c);
}

// -------------------------------------------------------------------------

const TypeInfo DOMRect::info = { "Rect", HostType, 0, 0, 0 };

DOMRect::~DOMRect()
{
  rects.remove(rect.handle());
}

KJSO DOMRect::tryGet(const UString &p) const
{
  if (p == "top")
    return getDOMCSSValue(rect.top());
  if (p == "right")
    return getDOMCSSValue(rect.right());
  if (p == "bottom")
    return getDOMCSSValue(rect.bottom());
  if (p == "left")
    return getDOMCSSValue(rect.left());

  return DOMObject::tryGet(p);
}

KJSO KJS::getDOMRect(DOM::Rect r)
{
  DOMRect *ret;
  if (r.isNull())
    return Null();
  else if ((ret = rects[r.handle()]))
    return ret;
  else {
    ret = new DOMRect(r);
    rects.insert(r.handle(),ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const TypeInfo DOMCounter::info = { "Counter", HostType, 0, 0, 0 };

DOMCounter::~DOMCounter()
{
  counters.remove(counter.handle());
}

KJSO DOMCounter::tryGet(const UString &p) const
{
  if (p == "identifier")
    return getString(counter.identifier());
  if (p == "listStyle")
    return getString(counter.listStyle());
  if (p == "separator")
    return getString(counter.separator());

  return DOMObject::tryGet(p);
}

KJSO KJS::getDOMCounter(DOM::Counter c)
{
  DOMCounter *ret;
  if (c.isNull())
    return Null();
  else if ((ret = counters[c.handle()]))
    return ret;
  else {
    ret = new DOMCounter(c);
    counters.insert(c.handle(),ret);
    return ret;
  }
}




