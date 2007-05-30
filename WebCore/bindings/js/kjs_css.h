 // -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef kjs_css_h
#define kjs_css_h

#include "Color.h"
#include "RectImpl.h"
#include "JSStyleSheet.h"
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
    class JSCSSRule;
    class JSCSSStyleDeclaration;
    class MediaList;
    class StyleSheet;
    class StyleSheetList;
}

namespace KJS {

  class DOMStyleSheetList : public DOMObject {
  public:
    DOMStyleSheetList(ExecState*, WebCore::StyleSheetList*, WebCore::Document*);
    virtual ~DOMStyleSheetList();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState*) const { return true; }
    static const ClassInfo info;
    WebCore::StyleSheetList* impl() const { return m_impl.get(); }
    enum { Item, Length };
  private:
    static JSValue* indexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* nameGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);

    RefPtr<WebCore::StyleSheetList> m_impl;
    RefPtr<WebCore::Document> m_doc;
  };

  // The document is only used for get-stylesheet-by-name (make optional if necessary)
  JSValue* toJS(ExecState*, WebCore::StyleSheetList*, WebCore::Document*);

  class DOMMediaList : public DOMObject {
  public:
    DOMMediaList(ExecState*, WebCore::MediaList*);
    virtual ~DOMMediaList();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState*) const { return true; }
    static const ClassInfo info;
    enum { MediaText, Length,
           Item, DeleteMedium, AppendMedium };
    WebCore::MediaList* impl() const { return m_impl.get(); }
  private:
    static JSValue* indexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    RefPtr<WebCore::MediaList> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::MediaList*);

  class DOMCSSStyleSheet : public WebCore::JSStyleSheet {
  public:
    DOMCSSStyleSheet(ExecState*, WebCore::CSSStyleSheet*);
    virtual ~DOMCSSStyleSheet();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { OwnerRule, CssRules, Rules, InsertRule, DeleteRule, AddRule, RemoveRule };
  };

  class DOMCSSRule : public DOMObject {
  public:
    virtual ~DOMCSSRule();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*, int attr = None);
    void putValueProperty(ExecState*, int token, JSValue*, int attr);
    virtual const ClassInfo* classInfo() const;
    static const ClassInfo info;
    static const ClassInfo style_info, media_info, fontface_info, page_info, import_info, charset_info;
    enum { ParentStyleSheet, Type, CssText, ParentRule, Style_SelectorText, Style_Style,
           Media_Media, Media_InsertRule, Media_DeleteRule, Media_CssRules,
           FontFace_Style, Page_SelectorText, Page_Style,
           Import_Href, Import_Media, Import_StyleSheet, Charset_Encoding };
    WebCore::CSSRule* impl() const { return m_impl.get(); }
  private:
    friend class WebCore::JSCSSRule;
    DOMCSSRule(ExecState*, WebCore::CSSRule*);
    RefPtr<WebCore::CSSRule> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::CSSRule*);

  class DOMCSSValue : public DOMObject {
  public:
    DOMCSSValue(ExecState*, WebCore::CSSValue*);
    virtual ~DOMCSSValue();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { CssText, CssValueType };
    WebCore::CSSValue* impl() const { return m_impl.get(); }
  protected:
    // Constructor for derived classes; doesn't set up a prototype.
    DOMCSSValue(WebCore::CSSValue *v) : m_impl(v) { }
  private:
    RefPtr<WebCore::CSSValue> m_impl;
  };

  KJS_DEFINE_PROTOTYPE(DOMCSSValuePrototype)

  JSValue* toJS(ExecState*, WebCore::CSSValue*);

  class DOMRGBColor : public DOMObject {
  public:
    DOMRGBColor(ExecState*, unsigned color);
    ~DOMRGBColor();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Red, Green, Blue };
    unsigned impl() const { return m_color; }
  private:
    unsigned m_color;
  };

  JSValue* getDOMRGBColor(ExecState*, unsigned color);

  class DOMRect : public DOMObject {
  public:
    DOMRect(ExecState*, WebCore::RectImpl* r);
    ~DOMRect();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Top, Right, Bottom, Left };
    WebCore::RectImpl* impl() const { return m_rect.get(); }
  private:
    RefPtr<WebCore::RectImpl> m_rect;
  };

  JSValue* toJS(ExecState*, WebCore::RectImpl*);

} // namespace

#endif
