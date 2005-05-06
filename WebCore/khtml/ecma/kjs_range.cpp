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

#include "kjs_range.h"
#include "kjs_range.lut.h"
#include <kdebug.h>

using namespace KJS;

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

DOMRange::DOMRange(ExecState *exec, DOM::Range r)
 : range(r) 
{
  setPrototype(DOMRangeProto::self(exec));
}

DOMRange::~DOMRange()
{
  ScriptInterpreter::forgetDOMObject(range.handle());
}

Value DOMRange::tryGet(ExecState *exec, const Identifier &p) const
{
  return DOMObjectLookupGetValue<DOMRange,DOMObject>(exec,p,&DOMRangeTable,this);
}

Value DOMRange::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
  case StartContainer:
    return getDOMNode(exec,range.startContainer());
  case StartOffset:
    return Number(range.startOffset());
  case EndContainer:
    return getDOMNode(exec,range.endContainer());
  case EndOffset:
    return Number(range.endOffset());
  case Collapsed:
    return Boolean(range.collapsed());
  case CommonAncestorContainer: {
    DOM::Range range2 = range; // avoid const error
    return getDOMNode(exec,range2.commonAncestorContainer());
  }
  default:
    kdWarning() << "Unhandled token in DOMRange::getValueProperty : " << token << endl;
    return Value();
  }
}

Value DOMRangeProtoFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&KJS::DOMRange::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  DOM::Range range = static_cast<DOMRange *>(thisObj.imp())->toRange();
  Value result;

  switch (id) {
    case DOMRange::SetStart:
      range.setStart(toNode(args[0]),args[1].toInt32(exec));
      result = Undefined();
      break;
    case DOMRange::SetEnd:
      range.setEnd(toNode(args[0]),args[1].toInt32(exec));
      result = Undefined();
      break;
    case DOMRange::SetStartBefore:
      range.setStartBefore(toNode(args[0]));
      result = Undefined();
      break;
    case DOMRange::SetStartAfter:
      range.setStartAfter(toNode(args[0]));
      result = Undefined();
      break;
    case DOMRange::SetEndBefore:
      range.setEndBefore(toNode(args[0]));
      result = Undefined();
      break;
    case DOMRange::SetEndAfter:
      range.setEndAfter(toNode(args[0]));
      result = Undefined();
      break;
    case DOMRange::Collapse:
      range.collapse(args[0].toBoolean(exec));
      result = Undefined();
      break;
    case DOMRange::SelectNode:
      range.selectNode(toNode(args[0]));
      result = Undefined();
      break;
    case DOMRange::SelectNodeContents:
      range.selectNodeContents(toNode(args[0]));
      result = Undefined();
      break;
    case DOMRange::CompareBoundaryPoints:
      result = Number(range.compareBoundaryPoints(static_cast<DOM::Range::CompareHow>(args[0].toInt32(exec)),toRange(args[1])));
      break;
    case DOMRange::DeleteContents:
      range.deleteContents();
      result = Undefined();
      break;
    case DOMRange::ExtractContents:
      result = getDOMNode(exec,range.extractContents());
      break;
    case DOMRange::CloneContents:
      result = getDOMNode(exec,range.cloneContents());
      break;
    case DOMRange::InsertNode:
      range.insertNode(toNode(args[0]));
      result = Undefined();
      break;
    case DOMRange::SurroundContents:
      range.surroundContents(toNode(args[0]));
      result = Undefined();
      break;
    case DOMRange::CloneRange:
      result = getDOMRange(exec,range.cloneRange());
      break;
    case DOMRange::ToString:
      result = getStringOrNull(range.toString());
      break;
    case DOMRange::Detach:
      range.detach();
      result = Undefined();
      break;
    case DOMRange::CreateContextualFragment:
      Value value = args[0];
      DOM::DOMString str = value.isA(NullType) ? DOM::DOMString() : value.toString(exec).string();
      result = getDOMNode(exec, range.createContextualFragment(str));
      break;
  };

  return result;
}

Value KJS::getDOMRange(ExecState *exec, DOM::Range r)
{
  return cacheDOMObject<DOM::Range, KJS::DOMRange>(exec, r);
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
Value RangeConstructor::tryGet(ExecState *exec, const Identifier &p) const
{
  return DOMObjectLookupGetValue<RangeConstructor,DOMObject>(exec,p,&RangeConstructorTable,this);
}

Value RangeConstructor::getValueProperty(ExecState *, int token) const
{
  return Number(token);
}

Value KJS::getRangeConstructor(ExecState *exec)
{
  return cacheGlobalObject<RangeConstructor>(exec, "[[range.constructor]]");
}


DOM::Range KJS::toRange(const Value& val)
{
  Object obj = Object::dynamicCast(val);
  if (obj.isNull() || !obj.inherits(&DOMRange::info))
    return DOM::Range();

  const DOMRange *dobj = static_cast<const DOMRange*>(obj.imp());
  return dobj->toRange();
}
