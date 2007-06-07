// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
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

#ifndef kjs_html_h
#define kjs_html_h

#include "JSDocument.h"
#include "JSElement.h"
#include "JSHTMLElement.h"
#include "kjs_dom.h"

namespace WebCore {
    class HTMLCollection;
    class HTMLDocument;
    class HTMLElement;
    class HTMLOptionsCollection;
}

namespace KJS {

  class JSAbstractEventListener;

  KJS_DEFINE_PROTOTYPE(JSHTMLCollectionPrototype)

  class JSHTMLCollection : public DOMObject {
  public:
    JSHTMLCollection(ExecState*, WebCore::HTMLCollection*);
    ~JSHTMLCollection();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObj, const List&args);
    virtual bool implementsCall() const { return true; }
    virtual bool toBoolean(ExecState*) const { return true; }
    enum { Item, NamedItem, Tags };
    JSValue* getNamedItems(ExecState*, const Identifier& propertyName) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    WebCore::HTMLCollection* impl() const { return m_impl.get(); }
  protected:
    RefPtr<WebCore::HTMLCollection> m_impl;
  private:
    static JSValue* lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* indexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* nameGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
  };

  class HTMLAllCollection : public JSHTMLCollection {
  public:
    HTMLAllCollection(ExecState* exec, WebCore::HTMLCollection* c) :
      JSHTMLCollection(exec, c) { }
    virtual bool toBoolean(ExecState*) const { return false; }
    virtual bool masqueradeAsUndefined() const { return true; }
  };
  
  ////////////////////// Image Object ////////////////////////

  class ImageConstructorImp : public DOMObject {
  public:
    ImageConstructorImp(ExecState*, WebCore::Document*);
    virtual bool implementsConstruct() const;
    virtual JSObject* construct(ExecState*, const List& args);
  private:
    RefPtr<WebCore::Document> m_doc;
  };

  JSValue* toJS(ExecState*, WebCore::HTMLOptionsCollection*);
  JSValue* getHTMLCollection(ExecState*, WebCore::HTMLCollection*);
  JSValue* getAllHTMLCollection(ExecState*, WebCore::HTMLCollection*);

  JSValue* runtimeObjectGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
  JSValue* runtimeObjectPropertyGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
  bool runtimeObjectCustomGetOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&, WebCore::JSHTMLElement*, WebCore::HTMLElement*);
  bool runtimeObjectCustomPut(ExecState*, const Identifier&, JSValue*, int attr, WebCore::HTMLElement*);
  bool runtimeObjectImplementsCall(WebCore::HTMLElement*);
  JSValue* runtimeObjectCallAsFunction(ExecState*, JSObject*, const List&, WebCore::HTMLElement*);

} // namespace KJS

#endif // kjs_html_h
