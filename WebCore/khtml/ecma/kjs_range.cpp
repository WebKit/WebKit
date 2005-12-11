// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#include "config.h"
#include "kjs_range.h"

#include <kdebug.h>
#include "dom/dom2_range.h"
#include "xml/dom_docimpl.h"
#include "xml/dom2_rangeimpl.h"

using DOM::DOMString;
using DOM::Range;
using DOM::RangeImpl;

#include "kjs_range.lut.h"

namespace KJS {

// -------------------------------------------------------------------------

const ClassInfo DOMRange::info = { "Range", 0, &DOMRangeTable, 0 };
/*
@begin DOMRangeTable 7
  startContainer	DOMRange::StartContainer	DontDelete|ReadOnly
  startOffset		DOMRange::StartOffset		DontDelete|ReadOnly
  endContainer		DOMRange::EndContainer		DontDelete|ReadOnly
  endOffset		DOMRange::EndOffset		DontDelete|ReadOnly
  collapsed		DOMRange::Collapsed		DontDelete|ReadOnly
  commonAncestorContainer DOMRange::CommonAncestorContainer	DontDelete|ReadOnly
@end
@begin DOMRangeProtoTable 17
  setStart		    DOMRange::SetStart			DontDelete|Function 2
  setEnd		    DOMRange::SetEnd			DontDelete|Function 2
  setStartBefore	    DOMRange::SetStartBefore		DontDelete|Function 1
  setStartAfter		    DOMRange::SetStartAfter		DontDelete|Function 1
  setEndBefore		    DOMRange::SetEndBefore		DontDelete|Function 1
  setEndAfter		    DOMRange::SetEndAfter		DontDelete|Function 1
  collapse		    DOMRange::Collapse			DontDelete|Function 1
  selectNode		    DOMRange::SelectNode		DontDelete|Function 1
  selectNodeContents	    DOMRange::SelectNodeContents	DontDelete|Function 1
  compareBoundaryPoints	    DOMRange::CompareBoundaryPoints	DontDelete|Function 2
  deleteContents	    DOMRange::DeleteContents		DontDelete|Function 0
  extractContents	    DOMRange::ExtractContents		DontDelete|Function 0
  cloneContents		    DOMRange::CloneContents		DontDelete|Function 0
  insertNode		    DOMRange::InsertNode		DontDelete|Function 1
  surroundContents	    DOMRange::SurroundContents		DontDelete|Function 1
  cloneRange		    DOMRange::CloneRange		DontDelete|Function 0
  toString		    DOMRange::ToString			DontDelete|Function 0
  detach		    DOMRange::Detach			DontDelete|Function 0
  createContextualFragment  DOMRange::CreateContextualFragment  DontDelete|Function 1
@end
*/
DEFINE_PROTOTYPE("DOMRange",DOMRangeProto)
IMPLEMENT_PROTOFUNC(DOMRangeProtoFunc)
IMPLEMENT_PROTOTYPE(DOMRangeProto,DOMRangeProtoFunc)

DOMRange::DOMRange(ExecState *exec, RangeImpl *r)
 : m_impl(r) 
{
  setPrototype(DOMRangeProto::self(exec));
}

DOMRange::~DOMRange()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

bool DOMRange::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<DOMRange, DOMObject>(exec, &DOMRangeTable, this, propertyName, slot);
}

ValueImp *DOMRange::getValueProperty(ExecState *exec, int token) const
{
  DOMExceptionTranslator exception(exec);
  RangeImpl &range = *m_impl;
  switch (token) {
  case StartContainer:
    return getDOMNode(exec, range.startContainer(exception));
  case StartOffset:
    return jsNumber(range.startOffset(exception));
  case EndContainer:
    return getDOMNode(exec, range.endContainer(exception));
  case EndOffset:
    return jsNumber(range.endOffset(exception));
  case Collapsed:
    return jsBoolean(range.collapsed(exception));
  case CommonAncestorContainer:
    return getDOMNode(exec, range.commonAncestorContainer(exception));
  default:
    kdWarning() << "Unhandled token in DOMRange::getValueProperty : " << token << endl;
    return NULL;
  }
}

ValueImp *DOMRangeProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMRange::info))
    return throwError(exec, TypeError);
  RangeImpl &range = *static_cast<DOMRange *>(thisObj)->impl();
  ValueImp *result = jsUndefined();
  int exception = 0;

  switch (id) {
    case DOMRange::SetStart:
      range.setStart(toNode(args[0]), args[1]->toInt32(exec), exception);
      break;
    case DOMRange::SetEnd:
      range.setEnd(toNode(args[0]), args[1]->toInt32(exec), exception);
      break;
    case DOMRange::SetStartBefore:
      range.setStartBefore(toNode(args[0]), exception);
      break;
    case DOMRange::SetStartAfter:
      range.setStartAfter(toNode(args[0]), exception);
      break;
    case DOMRange::SetEndBefore:
      range.setEndBefore(toNode(args[0]), exception);
      break;
    case DOMRange::SetEndAfter:
      range.setEndAfter(toNode(args[0]), exception);
      break;
    case DOMRange::Collapse:
      range.collapse(args[0]->toBoolean(exec), exception);
      break;
    case DOMRange::SelectNode:
      range.selectNode(toNode(args[0]), exception);
      break;
    case DOMRange::SelectNodeContents:
      range.selectNodeContents(toNode(args[0]), exception);
      break;
    case DOMRange::CompareBoundaryPoints:
        result = jsNumber(range.compareBoundaryPoints(static_cast<Range::CompareHow>(args[0]->toInt32(exec)), toRange(args[1]), exception));
        break;
    case DOMRange::DeleteContents:
      range.deleteContents(exception);
      break;
    case DOMRange::ExtractContents:
      result = getDOMNode(exec, range.extractContents(exception));
      break;
    case DOMRange::CloneContents:
      result = getDOMNode(exec, range.cloneContents(exception));
      break;
    case DOMRange::InsertNode:
      range.insertNode(toNode(args[0]), exception);
      break;
    case DOMRange::SurroundContents:
      range.surroundContents(toNode(args[0]), exception);
      break;
    case DOMRange::CloneRange:
      result = getDOMRange(exec, range.cloneRange(exception));
      break;
    case DOMRange::ToString:
      result = jsStringOrNull(range.toString(exception));
      break;
    case DOMRange::Detach:
      range.detach(exception);
      break;
    case DOMRange::CreateContextualFragment:
      result = getDOMNode(exec, range.createContextualFragment(args[0]->toString(exec).domString(), exception));
      break;
  };

  setDOMException(exec, exception);
  return result;
}

ValueImp *getDOMRange(ExecState *exec, RangeImpl *r)
{
  return cacheDOMObject<RangeImpl, DOMRange>(exec, r);
}

// -------------------------------------------------------------------------

const ClassInfo RangeConstructor::info = { "RangeConstructor", 0, &RangeConstructorTable, 0 };
/*
@begin RangeConstructorTable 5
  START_TO_START	DOM::Range::START_TO_START	DontDelete|ReadOnly
  START_TO_END		DOM::Range::START_TO_END	DontDelete|ReadOnly
  END_TO_END		DOM::Range::END_TO_END		DontDelete|ReadOnly
  END_TO_START		DOM::Range::END_TO_START	DontDelete|ReadOnly
@end
*/
bool RangeConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<RangeConstructor,DOMObject>(exec, &RangeConstructorTable, this, propertyName, slot);
}

ValueImp *RangeConstructor::getValueProperty(ExecState *, int token) const
{
  return jsNumber(token);
}

ValueImp *getRangeConstructor(ExecState *exec)
{
  return cacheGlobalObject<RangeConstructor>(exec, "[[range.constructor]]");
}


RangeImpl *toRange(ValueImp *val)
{
  if (!val || !val->isObject(&DOMRange::info))
    return 0;
  return static_cast<DOMRange *>(val)->impl();
}

}
