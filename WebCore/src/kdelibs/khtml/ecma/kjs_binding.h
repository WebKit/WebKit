/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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

#ifndef _KJS_BINDING_H_
#define _KJS_BINDING_H_

#include <kjs/object.h>
#include <kjs/function.h>
#include <dom/dom_node.h>
#include <dom/dom_doc.h>
#include <kurl.h>
#include <qguardedptr.h>
#include <qvariant.h>

namespace KJS {

  /** Base class for all objects in this binding - get() and put() run
      tryGet() and tryPut() respectively, and catch exceptions if they
      occur. */
  class DOMObject : public HostImp {
  public:
    KJSO get(const UString &p) const;
    virtual KJSO tryGet(const UString &p) const { return HostImp::get(p); }
    void put(const UString &p, const KJSO& v);
    virtual void tryPut(const UString &p, const KJSO& v) { HostImp::put(p,v); }
    virtual String toString() const;
  };

  /** Base class for all functions in this binding - get() and execute() run
      tryGet() and tryExecute() respectively, and catch exceptions if they
      occur. */
  class DOMFunction : public InternalFunctionImp {
  public:
    KJSO get(const UString &p) const;
    virtual KJSO tryGet(const UString &p) const { return InternalFunctionImp::get(p); }
    Completion execute(const List &);
    virtual Completion tryExecute(const List &args) { return InternalFunctionImp::execute(args); }
    virtual Boolean toBoolean() const { return Boolean(true); }
    virtual String toString() const { return UString("[function]"); }
  };

  /**
   * Convert an object to a Node. Returns a null Node if not possible.
   */
  DOM::Node toNode(const KJSO&);
  /**
   *  Get a String object, or Null() if s is null
   */
  KJSO getString(DOM::DOMString s);

  bool originCheck(const KURL &kurl1, const KURL &url2);

  QVariant KJSOToVariant(KJSO obj);

}; // namespace

#endif
