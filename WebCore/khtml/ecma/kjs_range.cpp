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

#include "kjs_range.h"
#include <qptrdict.h>

using namespace KJS;

QPtrDict<DOMRange> ranges;

// -------------------------------------------------------------------------

const TypeInfo DOMRange::info = { "Range", HostType, 0, 0, 0 };


DOMRange::~DOMRange()
{
  ranges.remove(range.handle());
}

KJSO DOMRange::tryGet(const UString &p) const
{
  KJSO r;

  if (p == "startContainer")
    return getDOMNode(range.startContainer());
  else if (p == "startOffset")
    return Number(range.startOffset());
  else if (p == "endContainer")
    return getDOMNode(range.endContainer());
  else if (p == "endOffset")
    return Number(range.endOffset());
  else if (p == "collapsed")
    return Boolean(range.collapsed());
  else if (p == "commonAncestorContainer") {
    DOM::Range range2 = range; // avoid const error
    return getDOMNode(range2.commonAncestorContainer());
  }
  else if (p == "setStart")
    return new DOMRangeFunc(range,DOMRangeFunc::SetStart);
  else if (p == "setEnd")
    return new DOMRangeFunc(range,DOMRangeFunc::SetEnd);
  else if (p == "setStartBefore")
    return new DOMRangeFunc(range,DOMRangeFunc::SetStartBefore);
  else if (p == "setStartAfter")
    return new DOMRangeFunc(range,DOMRangeFunc::SetStartAfter);
  else if (p == "setEndBefore")
    return new DOMRangeFunc(range,DOMRangeFunc::SetEndBefore);
  else if (p == "setEndAfter")
    return new DOMRangeFunc(range,DOMRangeFunc::SetEndAfter);
  else if (p == "collapse")
    return new DOMRangeFunc(range,DOMRangeFunc::Collapse);
  else if (p == "selectNode")
    return new DOMRangeFunc(range,DOMRangeFunc::SelectNode);
  else if (p == "selectNodeContents")
    return new DOMRangeFunc(range,DOMRangeFunc::SelectNodeContents);
  else if (p == "compareBoundaryPoints")
    return new DOMRangeFunc(range,DOMRangeFunc::CompareBoundaryPoints);
  else if (p == "deleteContents")
    return new DOMRangeFunc(range,DOMRangeFunc::DeleteContents);
  else if (p == "extractContents")
    return new DOMRangeFunc(range,DOMRangeFunc::ExtractContents);
  else if (p == "cloneContents")
    return new DOMRangeFunc(range,DOMRangeFunc::CloneContents);
  else if (p == "insertNode")
    return new DOMRangeFunc(range,DOMRangeFunc::InsertNode);
  else if (p == "surroundContents")
    return new DOMRangeFunc(range,DOMRangeFunc::SurroundContents);
  else if (p == "cloneRange")
    return new DOMRangeFunc(range,DOMRangeFunc::CloneRange);
  else if (p == "toString")
    return new DOMRangeFunc(range,DOMRangeFunc::ToString);
  else if (p == "detach")
    return new DOMRangeFunc(range,DOMRangeFunc::Detach);
  else
    r = DOMObject::tryGet(p);

  return r;
}

Completion DOMRangeFunc::tryExecute(const List &args)
{
  KJSO result;

  switch (id) {
    case SetStart:
      range.setStart(toNode(args[0]),args[1].toNumber().intValue());
      result = Undefined();
      break;
    case SetEnd:
      range.setEnd(toNode(args[0]),args[1].toNumber().intValue());
      result = Undefined();
      break;
    case SetStartBefore:
      range.setStartBefore(toNode(args[0]));
      result = Undefined();
      break;
    case SetStartAfter:
      range.setStartAfter(toNode(args[0]));
      result = Undefined();
      break;
    case SetEndBefore:
      range.setEndBefore(toNode(args[0]));
      result = Undefined();
      break;
    case SetEndAfter:
      range.setEndAfter(toNode(args[0]));
      result = Undefined();
      break;
    case Collapse:
      range.collapse(args[0].toBoolean().value());
      result = Undefined();
      break;
    case SelectNode:
      range.selectNode(toNode(args[0]));
      result = Undefined();
      break;
    case SelectNodeContents:
      range.selectNodeContents(toNode(args[0]));
      result = Undefined();
      break;
    case CompareBoundaryPoints:
      result = Number(range.compareBoundaryPoints(static_cast<DOM::Range::CompareHow>(args[0].toNumber().intValue()),toRange(args[1])));
      break;
    case DeleteContents:
      range.deleteContents();
      result = Undefined();
      break;
    case ExtractContents:
      result = getDOMNode(range.extractContents());
      break;
    case CloneContents:
      result = getDOMNode(range.cloneContents());
      break;
    case InsertNode:
      range.insertNode(toNode(args[0]));
      result = Undefined();
      break;
    case SurroundContents:
      range.surroundContents(toNode(args[0]));
      result = Undefined();
      break;
    case CloneRange:
      result = getDOMRange(range.cloneRange());
      break;
    case ToString:
      result = getString(range.toString());
      break;
    case Detach:
      range.detach();
      result = Undefined();
      break;
  };

  return Completion(ReturnValue,result);
}

KJSO KJS::getDOMRange(DOM::Range r)
{
  DOMRange *ret;
  if (r.isNull())
    return Null();
  else if ((ret = ranges[r.handle()]))
    return ret;
  else {
    ret = new DOMRange(r);
    ranges.insert(r.handle(),ret);
    return ret;
  }
}

// -------------------------------------------------------------------------

const TypeInfo RangePrototype::info = { "RangePrototype", HostType, 0, 0, 0 };
// ### make this protype of Range objects? (also for Node)

KJSO RangePrototype::tryGet(const UString &p) const
{
  if (p == "START_TO_START")
    return Number((unsigned int)DOM::Range::START_TO_START);
  else if (p == "START_TO_END")
    return Number((unsigned int)DOM::Range::START_TO_END);
  else if (p == "END_TO_END")
    return Number((unsigned int)DOM::Range::END_TO_END);
  else if (p == "END_TO_START")
    return Number((unsigned int)DOM::Range::END_TO_START);

  return DOMObject::tryGet(p);
}

KJSO KJS::getRangePrototype()
{
    KJSO proto = Global::current().get("[[range.prototype]]");
    if (proto.isDefined())
        return proto;
    else
    {
        Object rangeProto( new RangePrototype );
        Global::current().put("[[range.prototype]]", rangeProto);
        return rangeProto;
    }
}



DOM::Range KJS::toRange(const KJSO& obj)
{
  if (!obj.derivedFrom("Range"))
    return DOM::Range();

  const DOMRange *dobj = static_cast<const DOMRange*>(obj.imp());
  return dobj->toRange();
}
