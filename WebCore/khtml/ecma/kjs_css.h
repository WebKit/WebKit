 // -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#ifndef KJS_CSS_H_
#define KJS_CSS_H_

#include "Color.h"
#include "kjs_binding.h"

namespace WebCore {
    class Counter;
    class CSSPrimitiveValue;
    class CSSRule;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class CSSStyleSheet;
    class CSSValue;
    class CSSValueList;
    class MediaList;
    class RectImpl;
    class StyleSheet;
    class StyleSheetList;
}

namespace KJS {

  class DOMCSSStyleDeclaration : public DOMObject {
  public:
    DOMCSSStyleDeclaration(ExecState *exec, WebCore::CSSStyleDeclaration *s);
    virtual ~DOMCSSStyleDeclaration();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    JSValue *getValueProperty(ExecState *exec, int token);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CssText, Length, ParentRule };
    enum { GetPropertyValue, GetPropertyCSSValue, RemoveProperty, 
           GetPropertyPriority, GetPropertyShorthand, IsPropertyImplicit, SetProperty, Item };
    WebCore::CSSStyleDeclaration *impl() const { return m_impl.get(); }
  private:
    static JSValue *indexGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue *cssPropertyGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);

    RefPtr<WebCore::CSSStyleDeclaration> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::CSSStyleDeclaration*);

  class DOMStyleSheet : public DOMObject {
  public:
    DOMStyleSheet(ExecState *, WebCore::StyleSheet *ss);
    virtual ~DOMStyleSheet();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Type, Disabled, OwnerNode, ParentStyleSheet, Href, Title, Media };
    WebCore::StyleSheet *impl() const { return m_impl.get(); }
  protected:
    // Constructor for derived classes; doesn't set up a prototype.
    DOMStyleSheet(WebCore::StyleSheet *ss);
  private:
    RefPtr<WebCore::StyleSheet> m_impl;
  };

  JSValue* toJS(ExecState*, PassRefPtr<WebCore::StyleSheet>);

  class DOMStyleSheetList : public DOMObject {
  public:
    DOMStyleSheetList(ExecState *, WebCore::StyleSheetList *ssl, WebCore::Document *doc);
    virtual ~DOMStyleSheetList();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState* ) const { return true; }
    static const ClassInfo info;
    WebCore::StyleSheetList *impl() const { return m_impl.get(); }
    enum { Item, Length };
  private:
    static JSValue *indexGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue *nameGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);

    RefPtr<WebCore::StyleSheetList> m_impl;
    RefPtr<WebCore::Document> m_doc;
  };

  // The document is only used for get-stylesheet-by-name (make optional if necessary)
  JSValue *getDOMStyleSheetList(ExecState *exec, WebCore::StyleSheetList *ss, WebCore::Document *doc);

  class DOMMediaList : public DOMObject {
  public:
    DOMMediaList(ExecState *, WebCore::MediaList *ml);
    virtual ~DOMMediaList();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token);
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState* ) const { return true; }
    static const ClassInfo info;
    enum { MediaText, Length,
           Item, DeleteMedium, AppendMedium };
    WebCore::MediaList *impl() const { return m_impl.get(); }
  private:
    static JSValue *indexGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
    RefPtr<WebCore::MediaList> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::MediaList*);

  class DOMCSSStyleSheet : public DOMStyleSheet {
  public:
    DOMCSSStyleSheet(ExecState *exec, WebCore::CSSStyleSheet *ss);
    virtual ~DOMCSSStyleSheet();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { OwnerRule, CssRules, Rules, InsertRule, DeleteRule, AddRule, RemoveRule };
  };

  class DOMCSSRuleList : public DOMObject {
  public:
    DOMCSSRuleList(ExecState *, WebCore::CSSRuleList *rl);
    virtual ~DOMCSSRuleList();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Item, Length };
    WebCore::CSSRuleList *impl() const { return m_impl.get(); }
  private:
    static JSValue *indexGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);

    RefPtr<WebCore::CSSRuleList> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::CSSRuleList*);

  class DOMCSSRule : public DOMObject {
  public:
    DOMCSSRule(ExecState*, WebCore::CSSRule* r);
    virtual ~DOMCSSRule();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, JSValue *value, int attr);
    virtual const ClassInfo* classInfo() const;
    static const ClassInfo info;
    static const ClassInfo style_info, media_info, fontface_info, page_info, import_info, charset_info;
    enum { ParentStyleSheet, Type, CssText, ParentRule, Style_SelectorText, Style_Style,
           Media_Media, Media_InsertRule, Media_DeleteRule, Media_CssRules,
           FontFace_Style, Page_SelectorText, Page_Style,
           Import_Href, Import_Media, Import_StyleSheet, Charset_Encoding };
    WebCore::CSSRule *impl() const { return m_impl.get(); }
  private:
    RefPtr<WebCore::CSSRule> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::CSSRule*);

  // Constructor for CSSRule - currently only used for some global values
  class CSSRuleConstructor : public DOMObject {
  public:
    CSSRuleConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { UNKNOWN_RULE, STYLE_RULE, CHARSET_RULE, IMPORT_RULE, MEDIA_RULE, FONT_FACE_RULE, PAGE_RULE };
  };

  JSValue *getCSSRuleConstructor(ExecState *exec);

  class DOMCSSValue : public DOMObject {
  public:
    DOMCSSValue(ExecState *, WebCore::CSSValue *v) : m_impl(v) { }
    virtual ~DOMCSSValue();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CssText, CssValueType };
    WebCore::CSSValue *impl() const { return m_impl.get(); }
  protected:
    // Constructor for derived classes; doesn't set up a prototype.
    DOMCSSValue(WebCore::CSSValue *v) : m_impl(v) { }
  private:
    RefPtr<WebCore::CSSValue> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::CSSValue*);

  // Constructor for CSSValue - currently only used for some global values
  class CSSValueConstructor : public DOMObject {
  public:
    CSSValueConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CSS_VALUE_LIST, CSS_PRIMITIVE_VALUE, CSS_CUSTOM, CSS_INHERIT };
  };

  JSValue *getCSSValueConstructor(ExecState *exec);

  class DOMCSSPrimitiveValue : public DOMCSSValue {
  public:
    DOMCSSPrimitiveValue(ExecState *exec, WebCore::CSSPrimitiveValue *v);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token);
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
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  JSValue *getCSSPrimitiveValueConstructor(ExecState *exec);

  class DOMCSSValueList : public DOMCSSValue {
  public:
    DOMCSSValueList(ExecState *exec, WebCore::CSSValueList *l);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Length, Item };
  private:
    static JSValue *indexGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
  };

  class DOMRGBColor : public DOMObject {
  public:
    DOMRGBColor(unsigned color) : m_color(color) { }
    ~DOMRGBColor();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Red, Green, Blue };
  private:
    unsigned m_color;
  };

  JSValue *getDOMRGBColor(ExecState *exec, unsigned color);

  class DOMRect : public DOMObject {
  public:
    DOMRect(ExecState *, WebCore::RectImpl *r) : m_rect(r) { }
    ~DOMRect();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Top, Right, Bottom, Left };
  private:
    RefPtr<WebCore::RectImpl> m_rect;
  };

  JSValue* toJS(ExecState*, WebCore::RectImpl*);

  class DOMCounter : public DOMObject {
  public:
    DOMCounter(ExecState *, WebCore::Counter *c) : m_counter(c) { }
    ~DOMCounter();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { identifier, listStyle, separator };
  protected:
    RefPtr<WebCore::Counter> m_counter;
  };

  JSValue* toJS(ExecState*, WebCore::Counter*);

} // namespace

#endif
