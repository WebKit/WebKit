/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
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

#ifndef _KJS_DOM_H_
#define _KJS_DOM_H_

#include <dom_node.h>
#include <dom_doc.h>
#include <dom_element.h>
#include <dom_xml.h>

#include <kjs/object.h>
#include <kjs/function.h>

#include "kjs_binding.h"
#include "kjs_css.h"

#if defined(APPLE_CHANGES) && defined(__OBJC__)
#define id id_
#endif /* APPLE_CHANGES, __OBJC__ */
namespace KJS {

  class DOMNode : public DOMObject {
  public:
    DOMNode(DOM::Node n) : node(n) { }
    ~DOMNode();
    virtual Boolean toBoolean() const;
    virtual bool hasProperty(const UString &p, bool recursive = true) const;
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual DOM::Node toNode() const { return node; }
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;

    virtual KJSO toPrimitive(Type preferred = UndefinedType) const;
    virtual String toString() const;
    void setListener(int eventId,KJSO func) const;
    KJSO getListener(int eventId) const;
    virtual List *eventHandlerScope() const;

  protected:
    DOM::Node node;
  };

  class DOMNodeFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMNodeFunc(DOM::Node n, int i) : node(n), id(i) { }
    Completion tryExecute(const List &);
    enum { InsertBefore, ReplaceChild, RemoveChild, AppendChild,
	   HasChildNodes, CloneNode, AddEventListener, RemoveEventListener,
	   DispatchEvent };
  private:
    DOM::Node node;
    int id;
  };

  class DOMNodeList : public DOMObject {
  public:
    DOMNodeList(DOM::NodeList l) : list(l) { }
    ~DOMNodeList();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    virtual Boolean toBoolean() const { return Boolean(true); }
    static const TypeInfo info;
  private:
    DOM::NodeList list;
  };

  class DOMNodeListFunc : public DOMFunction {
    friend class DOMNodeList;
  public:
    DOMNodeListFunc(DOM::NodeList l, int i) : list(l), id(i) { }
    Completion tryExecute(const List &);
    enum { Item };
  private:
    DOM::NodeList list;
    int id;
  };

  class DOMDocument : public DOMNode {
  public:
    DOMDocument(DOM::Document d) : DOMNode(d) { }
    virtual KJSO tryGet(const UString &p) const;
    virtual bool hasProperty(const UString &p, bool recursive = true) const;
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMDocFunction : public DOMFunction {
  public:
    DOMDocFunction(DOM::Document d, int i);
    Completion tryExecute(const List &);
    enum { CreateElement, CreateDocumentFragment, CreateTextNode,
	   CreateComment, CreateCDATASection, CreateProcessingInstruction,
	   CreateAttribute, CreateEntityReference, GetElementsByTagName,
	   ImportNode, CreateElementNS, CreateAttributeNS, GetElementsByTagNameNS, GetElementById,
	   CreateRange, CreateNodeIterator, CreateTreeWalker, CreateEvent, GetOverrideStyle };
  private:
    DOM::Document doc;
    int id;
  };

  class DOMAttr : public DOMNode {
  public:
    DOMAttr(DOM::Attr a) : DOMNode(a) { }
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMElement : public DOMNode {
  public:
    DOMElement(DOM::Element e) : DOMNode(e) { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMElementFunction : public DOMFunction {
  public:
    DOMElementFunction(DOM::Element e, int i);
    Completion tryExecute(const List &);
    enum { GetAttribute, SetAttribute, RemoveAttribute, GetAttributeNode,
           SetAttributeNode, RemoveAttributeNode, GetElementsByTagName,
           GetAttributeNS, SetAttributeNS, RemoveAttributeNS, GetAttributeNodeNS,
           SetAttributeNodeNS, GetElementsByTagNameNS, HasAttribute, HasAttributeNS,
           Normalize };
  private:
    DOM::Element element;
    int id;
  };

  class DOMDOMImplementation : public DOMObject {
  public:
    DOMDOMImplementation(DOM::DOMImplementation i) : implementation(i) { }
    ~DOMDOMImplementation();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all functions
    virtual const TypeInfo* typeInfo() const { return &info; }
    virtual Boolean toBoolean() const { return Boolean(true); }
    static const TypeInfo info;
  private:
    DOM::DOMImplementation implementation;
  };

  class DOMDOMImplementationFunction : public DOMFunction {
  public:
    DOMDOMImplementationFunction(DOM::DOMImplementation impl, int i);
    Completion tryExecute(const List &);
    enum { HasFeature, CreateDocumentType, CreateDocument, CreateCSSStyleSheet };
  private:
    DOM::DOMImplementation implementation;
    int id;
  };

  class DOMDocumentType : public DOMNode {
  public:
    DOMDocumentType(DOM::DocumentType dt) : DOMNode(dt) { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMNamedNodeMap : public DOMObject {
  public:
    DOMNamedNodeMap(DOM::NamedNodeMap m) : map(m) { }
    ~DOMNamedNodeMap();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    virtual Boolean toBoolean() const { return Boolean(true); }
    static const TypeInfo info;
  private:
    DOM::NamedNodeMap map;
  };

  class DOMNamedNodeMapFunction : public DOMFunction {
  public:
    DOMNamedNodeMapFunction(DOM::NamedNodeMap m, int i);
    Completion tryExecute(const List &);
    enum { GetNamedItem, SetNamedItem, RemoveNamedItem, Item,
           GetNamedItemNS, SetNamedItemNS, RemoveNamedItemNS };
  private:
    DOM::NamedNodeMap map;
    int id;
  };

  class DOMProcessingInstruction : public DOMNode {
  public:
    DOMProcessingInstruction(DOM::ProcessingInstruction pi) : DOMNode(pi) { }
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMNotation : public DOMNode {
  public:
    DOMNotation(DOM::Notation n) : DOMNode(n) { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMEntity : public DOMNode {
  public:
    DOMEntity(DOM::Entity e) : DOMNode(e) { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  // Prototype object Node
  class NodePrototype : public DOMObject {
  public:
    NodePrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  // Prototype object DOMException
  class DOMExceptionPrototype : public DOMObject {
  public:
    DOMExceptionPrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  KJSO getDOMNode(DOM::Node n);
  KJSO getDOMNamedNodeMap(DOM::NamedNodeMap m);
  KJSO getDOMNodeList(DOM::NodeList l);
  KJSO getDOMDOMImplementation(DOM::DOMImplementation i);
  KJSO getNodePrototype();
  KJSO getDOMExceptionPrototype();

}; // namespace
#if defined(APPLE_CHANGES) && defined(__OBJC__)
#undef id
#endif /* APPLE_CHANGES, __OBJC__ */

#endif
