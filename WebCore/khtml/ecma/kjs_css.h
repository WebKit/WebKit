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

#ifndef _KJS_CSS_H_
#define _KJS_CSS_H_

#include <dom/dom_node.h>
#include <kjs/object.h>
#include <dom/css_value.h>
#include <dom/css_stylesheet.h>
#include <dom/css_rule.h>
#include "kjs_binding.h"

namespace KJS {

  class DOMCSSStyleDeclaration : public DOMObject {
  public:
    DOMCSSStyleDeclaration(DOM::CSSStyleDeclaration s) : styleDecl(s) { }
    virtual ~DOMCSSStyleDeclaration();
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual bool hasProperty(const UString &p, bool recursive = true) const;
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  protected:
    DOM::CSSStyleDeclaration styleDecl;
  };

  class DOMCSSStyleDeclarationFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMCSSStyleDeclarationFunc(DOM::CSSStyleDeclaration s, int i) : styleDecl(s), id(i) { }
    Completion tryExecute(const List &);
    enum { GetPropertyValue, GetPropertyCSSValue, RemoveProperty, GetPropertyPriority,
           SetProperty, Item };
  private:
    DOM::CSSStyleDeclaration styleDecl;
    int id;
  };

  KJSO getDOMCSSStyleDeclaration(DOM::CSSStyleDeclaration n);

  class DOMStyleSheet : public DOMObject {
  public:
    DOMStyleSheet(DOM::StyleSheet ss) : styleSheet(ss) { }
    virtual ~DOMStyleSheet();
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual const TypeInfo* typeInfo() const { return &info; }
    virtual Boolean toBoolean() const { return Boolean(true); }
    static const TypeInfo info;
  protected:
    DOM::StyleSheet styleSheet;
  };

  KJSO getDOMStyleSheet(DOM::StyleSheet ss);

  class DOMStyleSheetList : public DOMObject {
  public:
    DOMStyleSheetList(DOM::StyleSheetList ssl) : styleSheetList(ssl) { }
    virtual ~DOMStyleSheetList();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    virtual Boolean toBoolean() const { return Boolean(true); }
    static const TypeInfo info;
  private:
    DOM::StyleSheetList styleSheetList;
  };

  KJSO getDOMStyleSheetList(DOM::StyleSheetList ss);

  class DOMStyleSheetListFunc : public DOMFunction {
    friend class DOMStyleSheetList;
  public:
    DOMStyleSheetListFunc(DOM::StyleSheetList ssl, int i) : styleSheetList(ssl), id(i) { }
    Completion tryExecute(const List &);
    enum { Item };
  private:
    DOM::StyleSheetList styleSheetList;
    int id;
  };

  class DOMMediaList : public DOMObject {
  public:
    DOMMediaList(DOM::MediaList ml) : mediaList(ml) { }
    virtual ~DOMMediaList();
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual const TypeInfo* typeInfo() const { return &info; }
    virtual Boolean toBoolean() const { return Boolean(true); }
    static const TypeInfo info;
  private:
    DOM::MediaList mediaList;
  };

  KJSO getDOMMediaList(DOM::MediaList ss);

  class DOMMediaListFunc : public DOMFunction {
    friend class DOMMediaList;
  public:
    DOMMediaListFunc(DOM::MediaList ml, int i) : mediaList(ml), id(i) { }
    Completion tryExecute(const List &);
    enum { Item, DeleteMedium, AppendMedium };
  private:
    DOM::MediaList mediaList;
    int id;
  };

  class DOMCSSStyleSheet : public DOMStyleSheet {
  public:
    DOMCSSStyleSheet(DOM::CSSStyleSheet ss) : DOMStyleSheet(ss) { }
    virtual ~DOMCSSStyleSheet();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMCSSStyleSheetFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMCSSStyleSheetFunc(DOM::CSSStyleSheet ss, int i) : styleSheet(ss), id(i) { }
    Completion tryExecute(const List &);
    enum { InsertRule, DeleteRule };
  private:
    DOM::CSSStyleSheet styleSheet;
    int id;
  };

  class DOMCSSRuleList : public DOMObject {
  public:
    DOMCSSRuleList(DOM::CSSRuleList rl) : cssRuleList(rl) { }
    virtual ~DOMCSSRuleList();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  protected:
    DOM::CSSRuleList cssRuleList;
  };

  class DOMCSSRuleListFunc : public DOMFunction {
  public:
    DOMCSSRuleListFunc(DOM::CSSRuleList rl, int i) : cssRuleList(rl), id(i) { }
    Completion tryExecute(const List &);
    enum { Item };
  private:
    DOM::CSSRuleList cssRuleList;
    int id;
  };

  KJSO getDOMCSSRuleList(DOM::CSSRuleList rl);

  class DOMCSSRule : public DOMObject {
  public:
    DOMCSSRule(DOM::CSSRule r) : cssRule(r) { }
    virtual ~DOMCSSRule();
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
    DOM::CSSRule toCSSRule() const { return cssRule; }
  protected:
    DOM::CSSRule cssRule;
  };

  class DOMCSSRuleFunc : public DOMFunction {
  public:
    DOMCSSRuleFunc(DOM::CSSRule r, int i) : cssRule(r), id(i) { }
    Completion tryExecute(const List &);
    enum { Item, InsertRule, DeleteRule };
  private:
    DOM::CSSRule cssRule;
    int id;
  };

  KJSO getDOMCSSRule(DOM::CSSRule r);

  /**
   * Convert an object to a Node. Returns a null Node if not possible.
   */
  DOM::CSSRule toCSSRule(const KJSO&);

  // Prototype object CSSRule
  class CSSRulePrototype : public DOMObject {
  public:
    CSSRulePrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  KJSO getCSSRulePrototype();

  class DOMCSSValue : public DOMObject {
  public:
    DOMCSSValue(DOM::CSSValue v) : cssValue(v) { }
    virtual ~DOMCSSValue();
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  protected:
    DOM::CSSValue cssValue;
  };

  KJSO getDOMCSSValue(DOM::CSSValue v);

  // Prototype object CSSValue
  class CSSValuePrototype : public DOMObject {
  public:
    CSSValuePrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  KJSO getCSSValuePrototype();

  class DOMCSSPrimitiveValue : public DOMCSSValue {
  public:
    DOMCSSPrimitiveValue(DOM::CSSPrimitiveValue v) : DOMCSSValue(v) { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMCSSPrimitiveValueFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMCSSPrimitiveValueFunc(DOM::CSSPrimitiveValue v, int i) : val(v), id(i) { }
    Completion tryExecute(const List &);
    enum { SetFloatValue, GetFloatValue, SetStringValue, GetStringValue,
           GetCounterValue, GetRectValue, GetRGBColorValue };
  private:
    DOM::CSSPrimitiveValue val;
    int id;
  };

  // Prototype object CSSPrimitiveValue
  class CSSPrimitiveValuePrototype : public CSSValuePrototype {
  public:
    CSSPrimitiveValuePrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  KJSO getCSSPrimitiveValuePrototype();

  class DOMCSSValueList : public DOMCSSValue {
  public:
    DOMCSSValueList(DOM::CSSValueList v) : DOMCSSValue(v) { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMCSSValueListFunc : public DOMFunction {
  public:
    DOMCSSValueListFunc(DOM::CSSValueList vl, int i) : valueList(vl), id(i) { }
    Completion tryExecute(const List &);
    enum { Item };
  private:
    DOM::CSSValueList valueList;
    int id;
  };

  class DOMRGBColor : public DOMObject {
  public:
    DOMRGBColor(DOM::RGBColor c) : rgbColor(c) { }
    ~DOMRGBColor();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  protected:
    DOM::RGBColor rgbColor;
  };

  KJSO getDOMRGBColor(DOM::RGBColor c);

  class DOMRect : public DOMObject {
  public:
    DOMRect(DOM::Rect r) : rect(r) { }
    ~DOMRect();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  protected:
    DOM::Rect rect;
  };

  KJSO getDOMRect(DOM::Rect r);

  class DOMCounter : public DOMObject {
  public:
    DOMCounter(DOM::Counter c) : counter(c) { }
    ~DOMCounter();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  protected:
    DOM::Counter counter;
  };

  KJSO getDOMCounter(DOM::Counter c);

}; // namespace

#endif
