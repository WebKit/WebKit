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

#ifndef _KJS_RANGE_H_
#define _KJS_RANGE_H_

#include "kjs_dom.h"
#include <dom2_range.h>

namespace KJS {

  class DOMRange : public DOMObject {
  public:
    DOMRange(DOM::Range r) : range(r) {}
    ~DOMRange();
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
    virtual DOM::Range toRange() const { return range; }
//    virtual String toString() const;
  protected:
    DOM::Range range;
  };

  class DOMRangeFunc : public DOMFunction {
    friend class DOMNode;
  public:
    DOMRangeFunc(DOM::Range r, int i) : range(r), id(i) { }
    Completion tryExecute(const List &);
    enum { SetStart, SetEnd, SetStartBefore, SetStartAfter, SetEndBefore,
           SetEndAfter, Collapse, SelectNode, SelectNodeContents,
           CompareBoundaryPoints, DeleteContents, ExtractContents,
           CloneContents, InsertNode, SurroundContents, CloneRange, ToString,
           Detach };
  private:
    DOM::Range range;
    int id;
  };

  // Prototype object Range
  class RangePrototype : public DOMObject {
  public:
    RangePrototype() { }
    virtual KJSO tryGet(const UString &p) const;
    // no put - all read-only
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  KJSO getDOMRange(DOM::Range r);
  KJSO getRangePrototype();

  /**
   * Convert an object to a Range. Returns a null Node if not possible.
   */
  DOM::Range toRange(const KJSO&);

}; // namespace

#endif
