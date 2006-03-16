// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
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

#ifndef KJS_TRAVERSAL_H_
#define KJS_TRAVERSAL_H_

#include "dom2_traversalimpl.h"
#include "kjs_dom.h"
#include <kjs/protect.h>

namespace WebCore {
    class NodeFilterImpl;
    class NodeIteratorImpl;
    class TreeWalkerImpl;
}

namespace KJS {

  class DOMNodeIterator : public DOMObject {
  public:
    DOMNodeIterator(ExecState*, WebCore::NodeIteratorImpl*);
    ~DOMNodeIterator();
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState*, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Filter, Root, WhatToShow, ExpandEntityReferences, ReferenceNode, PointerBeforeReferenceNode,
           NextNode, PreviousNode, Detach };
    WebCore::NodeIteratorImpl* impl() const { return m_impl.get(); }
  private:
    RefPtr<WebCore::NodeIteratorImpl> m_impl;
  };

  // Constructor object NodeFilter
  class NodeFilterConstructor : public DOMObject {
  public:
    NodeFilterConstructor(ExecState*) { }
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState*, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class DOMNodeFilter : public DOMObject {
  public:
    DOMNodeFilter(ExecState*, WebCore::NodeFilterImpl*);
    ~DOMNodeFilter();
    virtual void mark();
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    WebCore::NodeFilterImpl* impl() const { return m_impl.get(); }
    enum { AcceptNode };
  private:
    RefPtr<WebCore::NodeFilterImpl> m_impl;
  };

  class DOMTreeWalker : public DOMObject {
  public:
    DOMTreeWalker(ExecState*, WebCore::TreeWalkerImpl*);
    ~DOMTreeWalker();
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Root, WhatToShow, Filter, ExpandEntityReferences, CurrentNode,
           ParentNode, FirstChild, LastChild, PreviousSibling, NextSibling,
           PreviousNode, NextNode };
    WebCore::TreeWalkerImpl* impl() const { return m_impl.get(); }
  private:
    RefPtr<WebCore::TreeWalkerImpl> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::NodeIteratorImpl*);
  JSValue* getNodeFilterConstructor(ExecState*);
  JSValue* toJS(ExecState*, WebCore::NodeFilterImpl*);
  JSValue* toJS(ExecState*, WebCore::TreeWalkerImpl*);

  WebCore::NodeFilterImpl* toNodeFilter(const JSValue*); // returns 0 if value is not a DOMNodeFilter

  class JSNodeFilterCondition : public WebCore::NodeFilterCondition {
  public:
    JSNodeFilterCondition(JSObject* filter);
    virtual short acceptNode(WebCore::NodeImpl*) const;
    virtual void mark();
  protected:
    JSObject *filter;
  };

} // namespace

#endif
