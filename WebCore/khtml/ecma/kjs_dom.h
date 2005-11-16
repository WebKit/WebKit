/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#include "kjs_binding.h"

#include <qvaluelist.h>
#include "misc/shared.h"

namespace DOM {
    class AtomicString;
    class AttrImpl;
    class CharacterDataImpl;
    class DocumentTypeImpl;
    class DOMImplementationImpl;
    class ElementImpl;
    class EntityImpl;
    class NamedNodeMapImpl;
    class NodeImpl;
    class NodeListImpl;
    class NotationImpl;
    class ProcessingInstructionImpl;
    class TextImpl;
}

namespace KJS {

  class DOMNode : public DOMObject {
  public:
    DOMNode(ExecState *exec, DOM::NodeImpl *n);
    virtual ~DOMNode();
    virtual bool toBoolean(ExecState *) const;
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void mark();
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, ValueImp *value, int attr);
    DOM::NodeImpl *impl() const { return m_impl.get(); }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    virtual ValueImp *toPrimitive(ExecState *exec, Type preferred = UndefinedType) const;
    virtual UString toString(ExecState *exec) const;
    void setListener(ExecState *exec, const DOM::AtomicString &eventType, ValueImp *func) const;
    ValueImp *getListener(const DOM::AtomicString &eventType) const;
    virtual void pushEventHandlerScope(ExecState *exec, ScopeChain &scope) const;

    enum { NodeName, NodeValue, NodeType, ParentNode, ParentElement,
           ChildNodes, FirstChild, LastChild, PreviousSibling, NextSibling, Item,
           Attributes, NamespaceURI, Prefix, LocalName, OwnerDocument, InsertBefore,
           ReplaceChild, RemoveChild, AppendChild, HasAttributes, HasChildNodes,
           CloneNode, Normalize, IsSupported, AddEventListener, RemoveEventListener,
           DispatchEvent, Contains, IsSameNode, IsEqualNode, TextContent,
           IsDefaultNamespace, LookupNamespaceURI, LookupPrefix,
           OnAbort, OnBlur, OnChange, OnClick, OnContextMenu, OnDblClick, OnDragDrop, OnError,
           OnDragEnter, OnDragOver, OnDragLeave, OnDrop, OnDragStart, OnDrag, OnDragEnd,
           OnBeforeCut, OnCut, OnBeforeCopy, OnCopy, OnBeforePaste, OnPaste, OnSelectStart,
           OnFocus, OnInput, OnKeyDown, OnKeyPress, OnKeyUp, OnLoad, OnMouseDown,
           OnMouseMove, OnMouseOut, OnMouseOver, OnMouseUp, OnMouseWheel, OnMove, OnReset,
           OnResize, OnScroll, OnSearch, OnSelect, OnSubmit, OnUnload,
           OffsetLeft, OffsetTop, OffsetWidth, OffsetHeight, OffsetParent,
           ClientWidth, ClientHeight, ScrollLeft, ScrollTop, ScrollWidth, ScrollHeight, ScrollIntoView };

  protected:
    // Constructor for inherited classes; doesn't set up a prototype.
    DOMNode(DOM::NodeImpl *n);
    SharedPtr<DOM::NodeImpl> m_impl;
  };

  DOM::NodeImpl *toNode(ValueImp *); // returns 0 if passed-in value is not a DOMNode object

  class DOMNodeList : public DOMObject {
  public:
    DOMNodeList(ExecState *, DOM::NodeListImpl *l) : m_impl(l) { }
    ~DOMNodeList();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List&args);
    virtual bool implementsCall() const { return true; }
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState *) const { return true; }
    static const ClassInfo info;
    enum { Length, Item };
    DOM::NodeListImpl *impl() const { return m_impl.get(); }

    virtual ValueImp *toPrimitive(ExecState *exec, Type preferred = UndefinedType) const;

  private:
    static ValueImp *indexGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *nameGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);

    SharedPtr<DOM::NodeListImpl> m_impl;
  };

  class DOMDocument : public DOMNode {
  public:
    DOMDocument(ExecState *exec, DOM::DocumentImpl *d);
    ~DOMDocument();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, ValueImp *value, int attr);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { DocType, Implementation, DocumentElement,
           // Functions
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
    DOMDocument(DOM::DocumentImpl *d);
  };

  class DOMAttr : public DOMNode {
  public:
    DOMAttr(ExecState *exec, DOM::AttrImpl *a);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    void putValueProperty(ExecState *exec, int token, ValueImp *value, int attr);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Name, Specified, ValueProperty, OwnerElement };
  };

  DOM::AttrImpl *toAttr(ValueImp *); // returns 0 if passed-in value is not a DOMAttr object

  class DOMElement : public DOMNode {
  public:
    DOMElement(ExecState *exec, DOM::ElementImpl *e);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { TagName, Style,
           GetAttribute, SetAttribute, RemoveAttribute, GetAttributeNode,
           SetAttributeNode, RemoveAttributeNode, GetElementsByTagName,
           GetAttributeNS, SetAttributeNS, RemoveAttributeNS, GetAttributeNodeNS,
           SetAttributeNodeNS, GetElementsByTagNameNS, HasAttribute, HasAttributeNS,
           ScrollByLines, ScrollByPages, ScrollIntoView, ElementFocus, ElementBlur};
  protected:
    // Constructor for inherited classes; doesn't set up a prototype.
    DOMElement(DOM::ElementImpl *e);
  private:
    static ValueImp *attributeGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);
  };

  DOM::ElementImpl *toElement(ValueImp *); // returns 0 if passed-in value is not a DOMElement object

  class DOMDOMImplementation : public DOMObject {
  public:
    // Build a DOMDOMImplementation
    DOMDOMImplementation(ExecState *, DOM::DOMImplementationImpl *i);
    ~DOMDOMImplementation();
    // no put - all functions
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState *) const { return true; }
    static const ClassInfo info;
    enum { HasFeature, CreateDocumentType, CreateDocument, CreateCSSStyleSheet, CreateHTMLDocument };
    DOM::DOMImplementationImpl *impl() const { return m_impl.get(); }
  private:
    SharedPtr<DOM::DOMImplementationImpl> m_impl;
  };

  class DOMDocumentType : public DOMNode {
  public:
    DOMDocumentType(ExecState *exec, DOM::DocumentTypeImpl *dt);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Name, Entities, Notations, PublicId, SystemId, InternalSubset };
  };

  DOM::DocumentTypeImpl *toDocumentType(ValueImp *); // returns 0 if passed-in value is not a DOMDocumentType object

  class DOMNamedNodeMap : public DOMObject {
  public:
    DOMNamedNodeMap(ExecState *, DOM::NamedNodeMapImpl *m);
    ~DOMNamedNodeMap();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual bool toBoolean(ExecState *) const { return true; }
    static const ClassInfo info;
    enum { GetNamedItem, SetNamedItem, RemoveNamedItem, Item,
           GetNamedItemNS, SetNamedItemNS, RemoveNamedItemNS };
    DOM::NamedNodeMapImpl *impl() const { return m_impl.get(); }
  private:
    static ValueImp *lengthGetter(ExecState* exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *indexGetter(ExecState* exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *nameGetter(ExecState *exec, const Identifier&, const PropertySlot& slot);

    SharedPtr<DOM::NamedNodeMapImpl> m_impl;
  };

  class DOMProcessingInstruction : public DOMNode {
  public:
    DOMProcessingInstruction(ExecState *exec, DOM::ProcessingInstructionImpl *pi);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Target, Data, Sheet };
  };

  class DOMNotation : public DOMNode {
  public:
    DOMNotation(ExecState *exec, DOM::NotationImpl *n);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { PublicId, SystemId };
  };

  class DOMEntity : public DOMNode {
  public:
    DOMEntity(ExecState *exec, DOM::EntityImpl *e);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { PublicId, SystemId, NotationName };
  };

  // Constructor for Node - constructor stuff not implemented yet
  class NodeConstructor : public DOMObject {
  public:
    NodeConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  // Constructor for DOMException - constructor stuff not implemented yet
  class DOMExceptionConstructor : public DOMObject {
  public:
    DOMExceptionConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  ValueImp *getDOMDocumentNode(ExecState *exec, DOM::DocumentImpl *n);
  bool checkNodeSecurity(ExecState *exec, DOM::NodeImpl *n);
  ValueImp *getRuntimeObject(ExecState *exec, DOM::NodeImpl *n);
  ValueImp *getDOMNode(ExecState *exec, DOM::NodeImpl *n);
  ValueImp *getDOMNamedNodeMap(ExecState *exec, DOM::NamedNodeMapImpl *m);
  ValueImp *getDOMNodeList(ExecState *exec, DOM::NodeListImpl *l);
  ValueImp *getDOMDOMImplementation(ExecState *exec, DOM::DOMImplementationImpl *i);
  ObjectImp *getNodeConstructor(ExecState *exec);
  ObjectImp *getDOMExceptionConstructor(ExecState *exec);

  // Internal class, used for the collection return by e.g. document.forms.myinput
  // when multiple nodes have the same name.
  class DOMNamedNodesCollection : public DOMObject {
  public:
    DOMNamedNodesCollection(ExecState *exec, const QValueList< SharedPtr<DOM::NodeImpl> >& nodes );
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
  private:
    static ValueImp *lengthGetter(ExecState* exec, const Identifier&, const PropertySlot& slot);
    static ValueImp *indexGetter(ExecState* exec, const Identifier&, const PropertySlot& slot);

    QValueList< SharedPtr<DOM::NodeImpl> > m_nodes;
  };

  class DOMCharacterData : public DOMNode {
  public:
    DOMCharacterData(ExecState *exec, DOM::CharacterDataImpl *d);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    DOM::CharacterDataImpl *toData() const;
    enum { Data, Length,
           SubstringData, AppendData, InsertData, DeleteData, ReplaceData };
  protected:
    // Constructor for inherited classes; doesn't set up a prototype.
    DOMCharacterData(DOM::CharacterDataImpl *d);
  };

  class DOMText : public DOMCharacterData {
  public:
    DOMText(ExecState *exec, DOM::TextImpl *t);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    DOM::TextImpl *toText() const;
    enum { SplitText };
  };

} // namespace

#endif
