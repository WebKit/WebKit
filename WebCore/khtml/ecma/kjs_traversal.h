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

#ifndef _KJS_TRAVERSAL_H_
#define _KJS_TRAVERSAL_H_

#include "kjs_dom.h"
#include "dom/dom2_traversal.h"
#include "kjs/protect.h"

namespace DOM {
    class NodeFilterImpl;
    class NodeIteratorImpl;
    class TreeWalkerImpl;
}

namespace KJS {

  class DOMNodeIterator : public DOMObject {
  public:
    DOMNodeIterator(ExecState *exec, DOM::NodeIteratorImpl *ni);
    ~DOMNodeIterator();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Filter, Root, WhatToShow, ExpandEntityReferences, ReferenceNode, PointerBeforeReferenceNode,
           NextNode, PreviousNode, Detach };
    DOM::NodeIteratorImpl *impl() const { return m_impl.get(); }
  private:
    RefPtr<DOM::NodeIteratorImpl> m_impl;
  };

  // Constructor object NodeFilter
  class NodeFilterConstructor : public DOMObject {
  public:
    NodeFilterConstructor(ExecState *) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot& slot);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class DOMNodeFilter : public DOMObject {
  public:
    DOMNodeFilter(ExecState *exec, DOM::NodeFilterImpl *nf);
    ~DOMNodeFilter();
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    DOM::NodeFilterImpl *impl() const { return m_impl.get(); }
    enum { AcceptNode };
  private:
    RefPtr<DOM::NodeFilterImpl> m_impl;
  };

  class DOMTreeWalker : public DOMObject {
  public:
    DOMTreeWalker(ExecState *exec, DOM::TreeWalkerImpl *tw);
    ~DOMTreeWalker();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot& slot);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Root, WhatToShow, Filter, ExpandEntityReferences, CurrentNode,
           ParentNode, FirstChild, LastChild, PreviousSibling, NextSibling,
           PreviousNode, NextNode };
    DOM::TreeWalkerImpl *impl() const { return m_impl.get(); }
  private:
    RefPtr<DOM::TreeWalkerImpl> m_impl;
  };

  JSValue *getDOMNodeIterator(ExecState *exec, DOM::NodeIteratorImpl *ni);
  JSValue *getNodeFilterConstructor(ExecState *exec);
  JSValue *getDOMNodeFilter(ExecState *exec, DOM::NodeFilterImpl *nf);
  JSValue *getDOMTreeWalker(ExecState *exec, DOM::TreeWalkerImpl *tw);

  DOM::NodeFilterImpl *toNodeFilter(const JSValue *); // returns 0 if value is not a DOMNodeFilter

  class JSNodeFilterCondition : public DOM::NodeFilterCondition {
  public:
    JSNodeFilterCondition(JSObject * _filter);
    virtual ~JSNodeFilterCondition() {}
    virtual short acceptNode(DOM::NodeImpl*) const;
  protected:
    ProtectedPtr<JSObject> filter;
  };

} // namespace

#endif
