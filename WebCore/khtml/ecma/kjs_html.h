/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
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

#ifndef _KJS_HTML_H_
#define _KJS_HTML_H_

#include <html_document.h>
#include <html_base.h>
#include <html_misc.h>
#include <html_form.h>
#include "misc/loader_client.h"

#include <kjs/object.h>
#include <kjs/function.h>

#include "kjs_binding.h"
#include "kjs_dom.h"

class KHTMLPart;
class HTMLElement;

namespace KJS {

  class HTMLDocFunction : public DOMFunction {
  public:
    HTMLDocFunction(DOM::HTMLDocument d, int i) : doc(d), id(i) { };
    virtual KJSO tryGet(const UString &p) const;
    Completion tryExecute(const List &);
    enum { Images, Applets, Links, Forms, Anchors, All, Open, Close,
	   Write, WriteLn, GetElementById, GetElementsByName };
  private:
    DOM::HTMLDocument doc;
    int id;
  };

  class HTMLDocument : public DOMDocument {
  public:
    HTMLDocument(DOM::HTMLDocument d) : DOMDocument(d) { }
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual bool hasProperty(const UString &p, bool recursive = true) const;
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class HTMLElement : public DOMElement {
  public:
    HTMLElement(DOM::HTMLElement e) : DOMElement(e) { }
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual bool hasProperty(const UString &p, bool recursive = true) const;
    virtual const TypeInfo* typeInfo() const { return &info; }
    virtual String toString() const;
    virtual List *eventHandlerScope() const;
    static const TypeInfo info;
  };


  class HTMLElementFunction : public DOMFunction {
  public:
    HTMLElementFunction(DOM::HTMLElement e, int i) : element(e), id(i) { };
    Completion tryExecute(const List &);
    enum { Submit, Reset, Add, Remove, Blur, Focus, Select, Click,
           CreateTHead, DeleteTHead, CreateTFoot, DeleteTFoot,
           CreateCaption, DeleteCaption, InsertRow, DeleteRow,
           InsertCell, DeleteCell };
  private:
    DOM::HTMLElement element;
    int id;
  };

  class HTMLCollection : public DOMObject {
  public:
    HTMLCollection(DOM::HTMLCollection c) : collection(c) { }
    ~HTMLCollection();
    virtual KJSO tryGet(const UString &p) const;
    virtual Boolean toBoolean() const { return Boolean(true); }
  protected:
    DOM::HTMLCollection collection;
  };

  class HTMLSelectCollection : public HTMLCollection {
  public:
    HTMLSelectCollection(DOM::HTMLCollection c, DOM::HTMLSelectElement e)
      : HTMLCollection(c), element(e) { }
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
  private:
      DOM::Element dummyElement();
      DOM::HTMLSelectElement element;
  };

  class HTMLCollectionFunc : public DOMFunction {
  public:
    HTMLCollectionFunc(DOM::HTMLCollection c, int i) : coll(c), id(i) { };
    Completion tryExecute(const List &);
    enum { Item, NamedItem, Tags };
  private:
    DOM::HTMLCollection coll;
    int id;
  };

  ////////////////////// Option Object ////////////////////////

  class OptionConstructor : public ConstructorImp {
  public:
    OptionConstructor(const DOM::Document &d);
    Object construct(const List &);
  private:
    Global global;
    DOM::Document doc;
  };

  ////////////////////// Image Object ////////////////////////

#if 0
  class ImageObject : public DOMFunction {
  public:
    ImageObject(const Global &global);
    Completion tryExecute(const List &);
  private:
    UString src;
  };
#endif

  class ImageConstructor : public ConstructorImp {
  public:
    ImageConstructor(const Global& global, const DOM::Document &d);
    Object construct(const List &);
  private:
    Global global;
    DOM::Document doc;
  };

  class Image : public DOMObject, public khtml::CachedObjectClient {
  public:
    Image(const DOM::Document &d) : doc(d), img(0) { }
    ~Image();
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual Boolean toBoolean() const { return Boolean(true); }
  private:
    UString src;
    DOM::Document doc;
    khtml::CachedImage* img;
  };

  KJSO getHTMLCollection(DOM::HTMLCollection c);
  KJSO getSelectHTMLCollection(DOM::HTMLCollection c, DOM::HTMLSelectElement e);


}; // namespace

#endif
