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

#include "kjs_binding.h"

#include <qcolor.h>
#include "misc/shared.h"

namespace DOM {
    class CounterImpl;
    class CSSPrimitiveValueImpl;
    class CSSRuleImpl;
    class CSSRuleListImpl;
    class CSSStyleDeclarationImpl;
    class CSSStyleSheetImpl;
    class CSSValueImpl;
    class CSSValueListImpl;
    class MediaListImpl;
    class RectImpl;
    class StyleSheetImpl;
    class StyleSheetListImpl;
}

namespace KJS {

  class DOMCSSStyleDeclaration : public DOMObject {
  public:
    DOMCSSStyleDeclaration(ExecState *exec, DOM::CSSStyleDeclarationImpl *s);
    virtual ~DOMCSSStyleDeclaration();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    virtual bool hasOwnProperty(ExecState *exec, const Identifier &propertyName) const;
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CssText, Length, ParentRule,
           GetPropertyValue, GetPropertyCSSValue, RemoveProperty, GetPropertyPriority,
           SetProperty, Item };
    DOM::CSSStyleDeclarationImpl *impl() const { return m_impl.get(); }
  private:
    khtml::SharedPtr<DOM::CSSStyleDeclarationImpl> m_impl;
  };

  ValueImp *getDOMCSSStyleDeclaration(ExecState *exec, DOM::CSSStyleDeclarationImpl *d);

  class DOMStyleSheet : public DOMObject {
  public:
    DOMStyleSheet(ExecState *, DOM::StyleSheetImpl *ss) : m_impl(ss) { }
    virtual ~DOMStyleSheet();
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Type, Disabled, OwnerNode, ParentStyleSheet, Href, Title, Media };
    DOM::StyleSheetImpl *impl() const { return m_impl.get(); }
  protected:
    // Constructor for derived classes; doesn't set up a prototype.
    DOMStyleSheet(DOM::StyleSheetImpl *ss) : m_impl(ss) { }
  private:
    khtml::SharedPtr<DOM::StyleSheetImpl> m_impl;
  };

  ValueImp *getDOMStyleSheet(ExecState *exec, DOM::StyleSheetImpl *ss);

  class DOMStyleSheetList : public DOMObject {
  public:
    DOMStyleSheetList(ExecState *, DOM::StyleSheetListImpl *ssl, DOM::DocumentImpl *doc)
      : m_impl(ssl), m_doc(doc) { }
    virtual ~DOMStyleSheetList();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState* ) const { return true; }
    static const ClassInfo info;
    DOM::StyleSheetListImpl *impl() const { return m_impl.get(); }
    enum { Item, Length };
  private:
    khtml::SharedPtr<DOM::StyleSheetListImpl> m_impl;
    khtml::SharedPtr<DOM::DocumentImpl> m_doc;
  };

  // The document is only used for get-stylesheet-by-name (make optional if necessary)
  ValueImp *getDOMStyleSheetList(ExecState *exec, DOM::StyleSheetListImpl *ss, DOM::DocumentImpl *doc);

  class DOMMediaList : public DOMObject {
  public:
    DOMMediaList(ExecState *, DOM::MediaListImpl *ml);
    virtual ~DOMMediaList();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState* ) const { return true; }
    static const ClassInfo info;
    enum { MediaText, Length,
           Item, DeleteMedium, AppendMedium };
    DOM::MediaListImpl *impl() const { return m_impl.get(); }
  private:
    khtml::SharedPtr<DOM::MediaListImpl> m_impl;
  };

  ValueImp *getDOMMediaList(ExecState *exec, DOM::MediaListImpl *ml);

  class DOMCSSStyleSheet : public DOMStyleSheet {
  public:
    DOMCSSStyleSheet(ExecState *exec, DOM::CSSStyleSheetImpl *ss);
    virtual ~DOMCSSStyleSheet();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { OwnerRule, CssRules, Rules,
           InsertRule, DeleteRule, AddRule };
  };

  class DOMCSSRuleList : public DOMObject {
  public:
    DOMCSSRuleList(ExecState *, DOM::CSSRuleListImpl *rl) : m_impl(rl) { }
    virtual ~DOMCSSRuleList();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Item, Length };
    DOM::CSSRuleListImpl *impl() const { return m_impl.get(); }
  private:
    khtml::SharedPtr<DOM::CSSRuleListImpl> m_impl;
  };

  ValueImp *getDOMCSSRuleList(ExecState *exec, DOM::CSSRuleListImpl *rl);

  class DOMCSSRule : public DOMObject {
  public:
    DOMCSSRule(ExecState *, DOM::CSSRuleImpl *r) : m_impl(r) { }
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
    DOM::CSSRuleImpl *impl() const { return m_impl.get(); }
  private:
    khtml::SharedPtr<DOM::CSSRuleImpl> m_impl;
  };

  ValueImp *getDOMCSSRule(ExecState *exec, DOM::CSSRuleImpl *r);

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
    DOMCSSValue(ExecState *, DOM::CSSValueImpl *v) : m_impl(v) { }
    virtual ~DOMCSSValue();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    virtual void tryPut(ExecState *exec, const Identifier &propertyName, const Value& value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CssText, CssValueType };
    DOM::CSSValueImpl *impl() const { return m_impl.get(); }
  protected:
    // Constructor for derived classes; doesn't set up a prototype.
    DOMCSSValue(DOM::CSSValueImpl *v) : m_impl(v) { }
  private:
    khtml::SharedPtr<DOM::CSSValueImpl> m_impl;
  };

  ValueImp *getDOMCSSValue(ExecState *exec, DOM::CSSValueImpl *v);

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
    DOMCSSPrimitiveValue(ExecState *exec, DOM::CSSPrimitiveValueImpl *v);
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
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
    DOMCSSValueList(ExecState *exec, DOM::CSSValueListImpl *l);
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Item, Length };
  };

  class DOMRGBColor : public DOMObject {
  public:
    DOMRGBColor(unsigned color) : m_color(color) { }
    ~DOMRGBColor();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Red, Green, Blue };
  private:
    unsigned m_color;
  };

  ValueImp *getDOMRGBColor(ExecState *exec, unsigned color);

  class DOMRect : public DOMObject {
  public:
    DOMRect(ExecState *, DOM::RectImpl *r) : m_rect(r) { }
    ~DOMRect();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Top, Right, Bottom, Left };
  private:
    khtml::SharedPtr<DOM::RectImpl> m_rect;
  };

  ValueImp *getDOMRect(ExecState *exec, DOM::RectImpl *r);

  class DOMCounter : public DOMObject {
  public:
    DOMCounter(ExecState *, DOM::CounterImpl *c) : m_counter(c) { }
    ~DOMCounter();
    virtual Value tryGet(ExecState *exec,const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { identifier, listStyle, separator };
  protected:
    khtml::SharedPtr<DOM::CounterImpl> m_counter;
  };

  ValueImp *getDOMCounter(ExecState *exec, DOM::CounterImpl *c);

} // namespace

#endif
