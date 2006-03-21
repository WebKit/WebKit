/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef KJS_DOM_H
#define KJS_DOM_H

#include "NodeList.h"
#include "Shared.h"
#include "kjs_binding.h"
#include <qvaluelist.h>


namespace WebCore {
    class AtomicString;
    class Attr;
    class CharacterData;
    class DocumentType;
    class DOMImplementation;
    class Element;
    class Entity;
    class EventTargetNode;
    class NamedNodeMap;
    class Notation;
    class ProcessingInstruction;
    class Text;
}

namespace KJS {

  KJS_DEFINE_PROTOTYPE(DOMNodeProto)

  class DOMNode : public DOMObject {
  public:
    DOMNode(ExecState *exec, WebCore::Node *n);
    virtual ~DOMNode();
    virtual bool toBoolean(ExecState *) const;
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void mark();
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, JSValue *value, int attr);
    WebCore::Node *impl() const { return m_impl.get(); }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    virtual JSValue *toPrimitive(ExecState *exec, JSType preferred = UndefinedType) const;
    virtual UString toString(ExecState *exec) const;

    enum { NodeName, NodeValue, NodeType, ParentNode, ParentElement,
           ChildNodes, FirstChild, LastChild, PreviousSibling, NextSibling, Item,
           Attributes, NamespaceURI, Prefix, LocalName, OwnerDocument, InsertBefore,
           ReplaceChild, RemoveChild, AppendChild, HasAttributes, HasChildNodes,
           CloneNode, Normalize, IsSupported, Contains, IsSameNode, IsEqualNode, TextContent,
           IsDefaultNamespace, LookupNamespaceURI, LookupPrefix,
    };

  protected:
    // Constructor for inherited classes; doesn't set up a prototype.
    DOMNode(WebCore::Node *n);
    RefPtr<WebCore::Node> m_impl;
  };
    
  KJS_DEFINE_PROTOTYPE_WITH_PROTOTYPE(DOMEventTargetNodeProto, DOMNodeProto)

  class DOMEventTargetNode : public DOMNode
  {
  public:

      DOMEventTargetNode(ExecState *exec, WebCore::Node *n);

      void setListener(ExecState* exec, const WebCore::AtomicString &eventType, JSValue* func) const;
      JSValue* getListener(const WebCore::AtomicString &eventType) const;
      virtual void pushEventHandlerScope(ExecState* exec, ScopeChain &scope) const;
      
      bool getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot);
      JSValue* getValueProperty(ExecState* exec, int token) const;
      virtual void put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr);
      void putValueProperty(ExecState* exec, int token, JSValue* value, int);

      enum {  
          AddEventListener, RemoveEventListener, DispatchEvent,
          OnAbort, OnBlur, OnChange, OnClick, OnContextMenu, OnDblClick, OnDragDrop, OnError,
          OnDragEnter, OnDragOver, OnDragLeave, OnDrop, OnDragStart, OnDrag, OnDragEnd,
          OnBeforeCut, OnCut, OnBeforeCopy, OnCopy, OnBeforePaste, OnPaste, OnSelectStart,
          OnFocus, OnInput, OnKeyDown, OnKeyPress, OnKeyUp, OnLoad, OnMouseDown,
          OnMouseMove, OnMouseOut, OnMouseOver, OnMouseUp, OnMouseWheel, OnMove, OnReset,
          OnResize, OnScroll, OnSearch, OnSelect, OnSubmit, OnUnload
      };
      
protected:
      // Constructor for inherited classes; doesn't set up a prototype.
      DOMEventTargetNode(WebCore::Node *n);
  };

  WebCore::Node *toNode(JSValue *); // returns 0 if passed-in value is not a DOMNode object

  class DOMNodeList : public DOMObject {
  public:
    DOMNodeList(ExecState *, WebCore::NodeList *l) : m_impl(l) { }
    ~DOMNodeList();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List&args);
    virtual bool implementsCall() const { return true; }
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState *) const { return true; }
    static const ClassInfo info;
    enum { Length, Item };
    WebCore::NodeList *impl() const { return m_impl.get(); }

    virtual JSValue *toPrimitive(ExecState *exec, JSType preferred = UndefinedType) const;

  private:
    static JSValue *indexGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *nameGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);

    RefPtr<WebCore::NodeList> m_impl;
  };

  class DOMDocument : public DOMEventTargetNode {
  public:
    DOMDocument(ExecState *exec, WebCore::Document *d);
    ~DOMDocument();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, JSValue *value, int attr);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { DocType, Implementation, DocumentElement, 
           Charset, DefaultCharset, CharacterSet, ActualEncoding, InputEncoding,
           // Functions
           AdoptNode,
           CreateElement, CreateDocumentFragment, CreateTextNode, CreateComment,
           CreateCDATASection, CreateProcessingInstruction, CreateAttribute,
           CreateEntityReference, GetElementsByTagName, ImportNode, CreateElementNS,
           CreateAttributeNS, GetElementsByTagNameNS, GetElementById,
           CreateRange, CreateNodeIterator, CreateTreeWalker, DefaultView,
           CreateEvent, ElementFromPoint, StyleSheets, PreferredStylesheetSet, 
           SelectedStylesheetSet, GetOverrideStyle, ReadyState, 
           ExecCommand, QueryCommandEnabled, QueryCommandIndeterm, QueryCommandState, 
           QueryCommandSupported, QueryCommandValue };

  protected:
    // Constructor for inherited classes; doesn't set up a prototype.
    DOMDocument(WebCore::Document *d);
  };

  WebCore::Attr* toAttr(JSValue*, bool& ok);

  KJS_DEFINE_PROTOTYPE_WITH_PROTOTYPE(DOMElementProto, DOMEventTargetNodeProto)

  class DOMElement : public DOMEventTargetNode {
  public:
    DOMElement(ExecState *exec, WebCore::Element *e);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    void putValueProperty(ExecState *exec, int token, JSValue *value, int /*attr*/);
    virtual void put(ExecState *exec, const Identifier& propertyName, JSValue *value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { 
        TagName, ScrollByLines, ScrollByPages, ScrollIntoView, ScrollIntoViewIfNeeded,
        OffsetLeft, OffsetTop, OffsetWidth, OffsetHeight, OffsetParent,
        ClientWidth, ClientHeight, ScrollLeft, ScrollTop, ScrollWidth, ScrollHeight
    };
  protected:
    // Constructor for inherited classes; doesn't set up a prototype.
    DOMElement(WebCore::Element *e);
  private:
    static JSValue *attributeGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
  };

  WebCore::Element *toElement(JSValue *); // returns 0 if passed-in value is not a DOMElement object

  WebCore::DocumentType *toDocumentType(JSValue *); // returns 0 if passed-in value is not a DOMDocumentType object

  class DOMNamedNodeMap : public DOMObject {
  public:
    DOMNamedNodeMap(ExecState *, WebCore::NamedNodeMap *m);
    ~DOMNamedNodeMap();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState *) const { return true; }
    static const ClassInfo info;
    enum { GetNamedItem, SetNamedItem, RemoveNamedItem, Item,
           GetNamedItemNS, SetNamedItemNS, RemoveNamedItemNS };
    WebCore::NamedNodeMap *impl() const { return m_impl.get(); }
  private:
    static JSValue *lengthGetter(ExecState* exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *indexGetter(ExecState* exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *nameGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);

    RefPtr<WebCore::NamedNodeMap> m_impl;
  };

  // Constructor for Node - constructor stuff not implemented yet
  class NodeConstructor : public DOMObject {
  public:
    NodeConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  // Constructor for DOMException - constructor stuff not implemented yet
  class DOMExceptionConstructor : public DOMObject {
  public:
    DOMExceptionConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  JSValue* toJS(ExecState*, WebCore::Document*);
  bool checkNodeSecurity(ExecState *exec, WebCore::Node *n);
  JSValue *getRuntimeObject(ExecState *exec, WebCore::Node *n);
  JSValue* toJS(ExecState*, PassRefPtr<WebCore::Node>);
  JSValue* toJS(ExecState*, WebCore::NamedNodeMap *);
  JSValue* toJS(ExecState*, PassRefPtr<WebCore::NodeList>);
  JSObject *getNodeConstructor(ExecState *exec);
  JSObject *getDOMExceptionConstructor(ExecState *exec);

  // Internal class, used for the collection return by e.g. document.forms.myinput
  // when multiple nodes have the same name.
  class DOMNamedNodesCollection : public DOMObject {
  public:
    DOMNamedNodesCollection(ExecState *exec, const DeprecatedValueList< RefPtr<WebCore::Node> >& nodes );
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
  private:
    static JSValue *lengthGetter(ExecState* exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *indexGetter(ExecState* exec, JSObject *, const Identifier&, const PropertySlot& slot);

    DeprecatedValueList< RefPtr<WebCore::Node> > m_nodes;
  };

} // namespace

#endif
