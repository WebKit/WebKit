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
    class NodeTreeWalkerImpl;
}

namespace KJS {

  class DOMNodeIterator : public DOMObject {
  public:
    DOMNodeIterator(ExecState *exec, DOM::NodeIteratorImpl *ni);
    ~DOMNodeIterator();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token) const;
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
    ValueImp *getValueProperty(ExecState *exec, int token) const;
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
    ValueImp *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Root, WhatToShow, Filter, ExpandEntityReferences, CurrentNode,
           ParentNode, FirstChild, LastChild, PreviousSibling, NextSibling,
           PreviousNode, NextNode };
    DOM::TreeWalkerImpl *impl() const { return m_impl.get(); }
  private:
    RefPtr<DOM::TreeWalkerImpl> m_impl;
  };

  ValueImp *getDOMNodeIterator(ExecState *exec, DOM::NodeIteratorImpl *ni);
  ValueImp *getNodeFilterConstructor(ExecState *exec);
  ValueImp *getDOMNodeFilter(ExecState *exec, DOM::NodeFilterImpl *nf);
  ValueImp *getDOMTreeWalker(ExecState *exec, DOM::TreeWalkerImpl *tw);

  DOM::NodeFilterImpl *toNodeFilter(const ValueImp *); // returns 0 if value is not a DOMNodeFilter

  class JSNodeFilterCondition : public DOM::NodeFilterCondition {
  public:
    JSNodeFilterCondition(ObjectImp * _filter);
    virtual ~JSNodeFilterCondition() {}
    virtual short acceptNode(DOM::FilterNode) const;
  protected:
    ProtectedPtr<ObjectImp> filter;
  };

} // namespace

#endif
