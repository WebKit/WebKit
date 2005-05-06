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

#ifndef _KJS_CSS_H_
#define _KJS_CSS_H_

#include <dom/dom_node.h>
#include <dom/dom_doc.h>
#include <kjs/object.h>
#include <dom/css_value.h>
#include <dom/css_stylesheet.h>
#include <dom/css_rule.h>
#include "kjs_binding.h"

namespace KJS {

  class DOMCSSStyleDeclaration : public DOMObject {
  public:
    DOMCSSStyleDeclaration(ExecState *exec, DOM::CSSStyleDeclaration s);
    virtual ~DOMCSSStyleDeclaration();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    virtual bool hasProperty(ExecState *exec, const Identifier &propertyName) const;
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CssText, Length, ParentRule,
           GetPropertyValue, GetPropertyCSSValue, RemoveProperty, GetPropertyPriority,
           SetProperty, Item };
    DOM::CSSStyleDeclaration toStyleDecl() const { return styleDecl; }
  protected:
    DOM::CSSStyleDeclaration styleDecl;
  };

  Value getDOMCSSStyleDeclaration(ExecState *exec, DOM::CSSStyleDeclaration n);

  class DOMStyleSheet : public DOMObject {
  public:
    // Build a DOMStyleSheet
    DOMStyleSheet(ExecState *, DOM::StyleSheet ss) : styleSheet(ss) { }
    // Constructor for inherited classes
    DOMStyleSheet(DOM::StyleSheet ss) : styleSheet(ss) { }
    virtual ~DOMStyleSheet();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Type, Disabled, OwnerNode, ParentStyleSheet, Href, Title, Media };
  protected:
    DOM::StyleSheet styleSheet;
  };

  Value getDOMStyleSheet(ExecState *exec, DOM::StyleSheet ss);

  class DOMStyleSheetList : public DOMObject {
  public:
    DOMStyleSheetList(ExecState *, DOM::StyleSheetList ssl, DOM::Document doc)
      : styleSheetList(ssl), m_doc(doc) { }
    virtual ~DOMStyleSheetList();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState* ) const { return true; }
    static const ClassInfo info;
    DOM::StyleSheetList toStyleSheetList() const { return styleSheetList; }
    enum { Item, Length };
  private:
    DOM::StyleSheetList styleSheetList;
    DOM::Document m_doc;
  };

  // The document is only used for get-stylesheet-by-name (make optional if necessary)
  Value getDOMStyleSheetList(ExecState *exec, DOM::StyleSheetList ss, DOM::Document doc);

  class DOMMediaList : public DOMObject {
  public:
    DOMMediaList(ExecState *, DOM::MediaList ml);
    virtual ~DOMMediaList();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState* ) const { return true; }
    static const ClassInfo info;
    enum { MediaText, Length,
           Item, DeleteMedium, AppendMedium };
    DOM::MediaList toMediaList() const { return mediaList; }
  private:
    DOM::MediaList mediaList;
  };

  Value getDOMMediaList(ExecState *exec, DOM::MediaList ss);

  class DOMCSSStyleSheet : public DOMStyleSheet {
  public:
    DOMCSSStyleSheet(ExecState *exec, DOM::CSSStyleSheet ss);
    virtual ~DOMCSSStyleSheet();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { OwnerRule, CssRules, Rules,
           InsertRule, DeleteRule, AddRule };
    DOM::CSSStyleSheet toCSSStyleSheet() const { return static_cast<DOM::CSSStyleSheet>(styleSheet); }
  };

  class DOMCSSRuleList : public DOMObject {
  public:
    DOMCSSRuleList(ExecState *, DOM::CSSRuleList rl) : cssRuleList(rl) { }
    virtual ~DOMCSSRuleList();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Item, Length };
    DOM::CSSRuleList toCSSRuleList() const { return cssRuleList; }
  protected:
    DOM::CSSRuleList cssRuleList;
  };

  Value getDOMCSSRuleList(ExecState *exec, DOM::CSSRuleList rl);

  class DOMCSSRule : public DOMObject {
  public:
    DOMCSSRule(ExecState *, DOM::CSSRule r) : cssRule(r) { }
    virtual ~DOMCSSRule();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    void putValue(ExecState *exec, int token, const Value& value, int attr);
    virtual const ClassInfo* classInfo() const;
    static const ClassInfo info;
    static const ClassInfo style_info, media_info, fontface_info, page_info, import_info, charset_info;
    enum { ParentStyleSheet, Type, CssText, ParentRule,
           Style_SelectorText, Style_Style,
           Media_Media, Media_InsertRule, Media_DeleteRule, Media_CssRules,
           FontFace_Style, Page_SelectorText, Page_Style,
           Import_Href, Import_Media, Import_StyleSheet, Charset_Encoding };
    DOM::CSSRule toCSSRule() const { return cssRule; }
  protected:
    DOM::CSSRule cssRule;
  };

  Value getDOMCSSRule(ExecState *exec, DOM::CSSRule r);

  /**
   * Convert an object to a CSSRule. Returns a null CSSRule if not possible.
   */
  DOM::CSSRule toCSSRule(const Value&);

  // Constructor for CSSRule - currently only used for some global values
  class CSSRuleConstructor : public DOMObject {
  public:
    CSSRuleConstructor(ExecState *) { }
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { UNKNOWN_RULE, STYLE_RULE, CHARSET_RULE, IMPORT_RULE, MEDIA_RULE, FONT_FACE_RULE, PAGE_RULE };
  };

  Value getCSSRuleConstructor(ExecState *exec);

  class DOMCSSValue : public DOMObject {
  public:
    DOMCSSValue(ExecState *, DOM::CSSValue v) : cssValue(v) { }
    DOMCSSValue(DOM::CSSValue v) : cssValue(v) { }
    virtual ~DOMCSSValue();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CssText, CssValueType };
  protected:
    DOM::CSSValue cssValue;
  };

  Value getDOMCSSValue(ExecState *exec, DOM::CSSValue v);

  // Constructor for CSSValue - currently only used for some global values
  class CSSValueConstructor : public DOMObject {
  public:
    CSSValueConstructor(ExecState *) { }
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CSS_VALUE_LIST, CSS_PRIMITIVE_VALUE, CSS_CUSTOM, CSS_INHERIT };
  };

  Value getCSSValueConstructor(ExecState *exec);

  class DOMCSSPrimitiveValue : public DOMCSSValue {
  public:
    DOMCSSPrimitiveValue(ExecState *exec, DOM::CSSPrimitiveValue v);
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    DOM::CSSPrimitiveValue toCSSPrimitiveValue() const { return static_cast<DOM::CSSPrimitiveValue>(cssValue); }
    enum { PrimitiveType, SetFloatValue, GetFloatValue, SetStringValue, GetStringValue,
           GetCounterValue, GetRectValue, GetRGBColorValue };
  };

  // Constructor for CSSPrimitiveValue - currently only used for some global values
  class CSSPrimitiveValueConstructor : public CSSValueConstructor {
  public:
    CSSPrimitiveValueConstructor(ExecState *exec) : CSSValueConstructor(exec) { }
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  Value getCSSPrimitiveValueConstructor(ExecState *exec);

  class DOMCSSValueList : public DOMCSSValue {
  public:
    DOMCSSValueList(ExecState *exec, DOM::CSSValueList v);
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Item, Length };
    DOM::CSSValueList toValueList() const { return static_cast<DOM::CSSValueList>(cssValue); }
  };

  class DOMRGBColor : public DOMObject {
  public:
    DOMRGBColor(DOM::RGBColor c) : rgbColor(c) { }
    ~DOMRGBColor();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Red, Green, Blue };
  protected:
    DOM::RGBColor rgbColor;
  };

  Value getDOMRGBColor(ExecState *exec, DOM::RGBColor c);

  class DOMRect : public DOMObject {
  public:
    DOMRect(ExecState *, DOM::Rect r) : rect(r) { }
    ~DOMRect();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Top, Right, Bottom, Left };
  protected:
    DOM::Rect rect;
  };

  Value getDOMRect(ExecState *exec, DOM::Rect r);

  class DOMCounter : public DOMObject {
  public:
    DOMCounter(ExecState *, DOM::Counter c) : counter(c) { }
    ~DOMCounter();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { identifier, listStyle, separator };
  protected:
    DOM::Counter counter;
  };

  Value getDOMCounter(ExecState *exec, DOM::Counter c);

}; // namespace

#endif
