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
#include <dom2_traversal.h>

namespace KJS {

  class DOMNodeIterator : public DOMObject {
  public:
    DOMNodeIterator(DOM::NodeIterator ni) : nodeIterator(ni) {}
    ~DOMNodeIterator();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  protected:
    DOM::NodeIterator nodeIterator;
  };

  class DOMNodeIteratorFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMNodeIteratorFunc(DOM::NodeIterator ni, int i) : nodeIterator(ni), id(i) { }
    Completion tryExecute(const List &);
    enum { NextNode, PreviousNode, Detach };
  private:
    DOM::NodeIterator nodeIterator;
    int id;
  };

  // Prototype object NodeFilter
  class NodeFilterPrototype : public DOMObject {
  public:
    NodeFilterPrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMNodeFilter : public DOMObject {
  public:
    DOMNodeFilter(DOM::NodeFilter nf) : nodeFilter(nf) {}
    ~DOMNodeFilter();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
    virtual DOM::NodeFilter toNodeFilter() const { return nodeFilter; }
  protected:
    DOM::NodeFilter nodeFilter;
  };

  class DOMNodeFilterFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMNodeFilterFunc(DOM::NodeFilter nf, int i): nodeFilter(nf), id(i) {}

    Completion tryExecute(const List &);
    enum { AcceptNode };
  private:
    DOM::NodeFilter nodeFilter;
    int id;
  };


  class DOMTreeWalker : public DOMObject {
  public:
    DOMTreeWalker(DOM::TreeWalker tw) : treeWalker(tw) {}
    ~DOMTreeWalker();
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  protected:
    DOM::TreeWalker treeWalker;
  };

  class DOMTreewalkerFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMTreewalkerFunc(DOM::TreeWalker tw, int i) : treeWalker(tw), id(i) { }
    Completion tryExecute(const List &);
    enum { ParentNode, FirstChild, LastChild, PreviousSibling, NextSibling,
           PreviousNode, NextNode };
  private:
    DOM::TreeWalker treeWalker;
    int id;
  };

  KJSO getDOMNodeIterator(DOM::NodeIterator ni);
  KJSO getNodeFilterPrototype();
  KJSO getDOMNodeFilter(DOM::NodeFilter nf);
  KJSO getDOMTreeWalker(DOM::TreeWalker tw);

  /**
   * Convert an object to a NodeFilter. Returns a null Node if not possible.
   */
  DOM::NodeFilter toNodeFilter(const KJSO&);

  class JSNodeFilter : public DOM::CustomNodeFilter {
  public:
    JSNodeFilter(KJSO _filter);
    virtual ~JSNodeFilter();
    virtual short acceptNode (const DOM::Node &n);
  protected:
    KJSO filter;
  };

}; // namespace

#endif
