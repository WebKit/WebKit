// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *
 */

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "array_object.h"
#include "internal.h"
#include "error_object.h"

#include "array_object.lut.h"

#include <stdio.h>
#include <assert.h>

using namespace KJS;

// ------------------------------ ArrayInstanceImp -----------------------------

const ClassInfo ArrayInstanceImp::info = {"Array", 0, 0, 0};

ArrayInstanceImp::ArrayInstanceImp(const Object &proto)
  : ObjectImp(proto)
{
}

// Special implementation of [[Put]] - see ECMA 15.4.5.1
void ArrayInstanceImp::put(ExecState *exec, const UString &propertyName, const Value &value, int attr)
{
  if ((attr == None || attr == DontDelete) && !canPut(exec,propertyName))
    return;

  if (hasProperty(exec,propertyName)) {
    if (propertyName == "length") {
      Value len = get(exec,"length");
      unsigned int oldLen = len.toUInt32(exec);
      unsigned int newLen = value.toUInt32(exec);
      // shrink array
      for (unsigned int u = newLen; u < oldLen; u++) {
	UString p = UString::from(u);
	if (hasOwnProperty(exec, p))
	  deleteProperty(exec, p);
      }
      ObjectImp::put(exec, "length", Number(newLen), DontEnum | DontDelete);
      return;
    }
    //    put(p, v);
  } //  } else
    ObjectImp::put(exec, propertyName, value, attr);

  // array index ?
  unsigned int idx;
  if (!sscanf(propertyName.cstring().c_str(), "%u", &idx)) /* TODO */
    return;

  // do we need to update/create the length property ?
  if (hasOwnProperty(exec, "length")) {
    Value len = get(exec, "length");
    if (idx < len.toUInt32(exec))
      return;
  }

  ObjectImp::put(exec, "length", Number(idx+1), DontDelete | DontEnum);
}

void ArrayInstanceImp::putDirect(ExecState *exec, const UString &propertyName, const Value &value, int attr)
{
  ObjectImp::put(exec,propertyName,value,attr);
}

bool ArrayInstanceImp::hasOwnProperty(ExecState *exec,
                                      const UString &propertyName)
{
  // disable this object's prototype temporarily for the hasProperty() call
  Value protoBackup = prototype();
  setPrototype(Undefined());
  bool b = hasProperty(exec, propertyName);
  setPrototype(protoBackup);
  return b;
}

// ------------------------------ ArrayPrototypeImp ----------------------------

const ClassInfo ArrayPrototypeImp::info = {"Array", &ArrayInstanceImp::info, &arrayTable, 0};

/* Source for array_object.lut.h
@begin arrayTable 13
  toString       ArrayProtoFuncImp::ToString       DontEnum|Function 0
  toLocaleString ArrayProtoFuncImp::ToLocaleString DontEnum|Function 0
  concat         ArrayProtoFuncImp::Concat         DontEnum|Function 1
  join           ArrayProtoFuncImp::Join           DontEnum|Function 1
  pop            ArrayProtoFuncImp::Pop            DontEnum|Function 0
  push           ArrayProtoFuncImp::Push           DontEnum|Function 1
  reverse        ArrayProtoFuncImp::Reverse        DontEnum|Function 0
  shift          ArrayProtoFuncImp::Shift          DontEnum|Function 0
  slice          ArrayProtoFuncImp::Slice          DontEnum|Function 2
  sort           ArrayProtoFuncImp::Sort           DontEnum|Function 1
  splice         ArrayProtoFuncImp::Splice         DontEnum|Function 2
  unshift        ArrayProtoFuncImp::UnShift        DontEnum|Function 1
@end
*/

// ECMA 15.4.4
ArrayPrototypeImp::ArrayPrototypeImp(ExecState *exec,
                                     ObjectPrototypeImp *objProto)
  : ArrayInstanceImp(Object(objProto))
{
  Value protect(this);
  setInternalValue(Null());

  // The constructor will be added later, by InterpreterImp, once ArrayObjectImp has been constructed.
  put(exec,"length", Number(0), DontEnum | DontDelete);
}

Value ArrayPrototypeImp::get(ExecState *exec, const UString &propertyName) const
{
  //fprintf( stderr, "ArrayPrototypeImp::get(%s)\n", propertyName.ascii() );
  return lookupGetFunction<ArrayProtoFuncImp, ArrayInstanceImp>( exec, propertyName, &arrayTable, this );
}

// ------------------------------ ArrayProtoFuncImp ----------------------------

ArrayProtoFuncImp::ArrayProtoFuncImp(ExecState *exec, int i, int len)
  : InternalFunctionImp(
    static_cast<FunctionPrototypeImp*>(exec->interpreter()->builtinFunctionPrototype().imp())
    ), id(i)
{
  Value protect(this);
  put(exec,"length",Number(len),DontDelete|ReadOnly|DontEnum);
}

bool ArrayProtoFuncImp::implementsCall() const
{
  return true;
}

// ECMA 15.4.4
Value ArrayProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &args)
{
  unsigned int length = thisObj.get(exec,"length").toUInt32(exec);

  Value result;
  switch (id) {
  case ToLocaleString:
    // TODO  - see 15.4.4.3
    // fall through
  case ToString:

    if (!thisObj.inherits(&ArrayInstanceImp::info)) {
      Object err = Error::create(exec,TypeError);
      exec->setException(err);
      return err;
    }

    // fall through

  case Join: {
    UString separator = ",";
    UString str = "";

    if (args.size() > 0)
      separator = args[0].toString(exec);
    for (unsigned int k = 0; k < length; k++) {
      if (k >= 1)
        str += separator;
      Value element = thisObj.get(exec,UString::from(k));
      if (element.type() != UndefinedType && element.type() != NullType)
        str += element.toString(exec);
    }
    result = String(str);
    break;
  }
  case Concat: {
    Object arr = Object::dynamicCast(exec->interpreter()->builtinArray().construct(exec,List::empty()));
    int n = 0;
    Value curArg = thisObj;
    Object curObj = Object::dynamicCast(thisObj);
    ListIterator it = args.begin();
    for (;;) {
      if (curArg.type() == ObjectType &&
          curObj.inherits(&ArrayInstanceImp::info)) {
        unsigned int k = 0;
        if (n > 0)
          length = curObj.get(exec,"length").toUInt32(exec);
        while (k < length) {
          UString p = UString::from(k);
          if (curObj.hasProperty(exec,p))
            arr.put(exec,UString::from(n), curObj.get(exec,p));
          n++;
          k++;
        }
      } else {
        arr.put(exec,UString::from(n), curArg);
        n++;
      }
      if (it == args.end())
        break;
      curArg = *it;
      curObj = Object::dynamicCast(it++); // may be 0
    }
    arr.put(exec,"length", Number(n), DontEnum | DontDelete);

    result = arr;
    break;
  }
  case Pop:{

    if (length == 0) {
      thisObj.put(exec, "length", Number(length), DontEnum | DontDelete);
      result = Undefined();
    } else {
      UString str = UString::from(length - 1);
      result = thisObj.get(exec,str);
      thisObj.deleteProperty(exec, str);
      thisObj.put(exec, "length", Number(length - 1), DontEnum | DontDelete);
    }
    break;
  }
  case Push: {
    for (int n = 0; n < args.size(); n++)
      thisObj.put(exec,UString::from(length + n), args[n]);
    length += args.size();
    thisObj.put(exec,"length", Number(length), DontEnum | DontDelete);
    result = Number(length);
    break;
  }
  case Reverse: {

    unsigned int middle = length / 2;

    for (unsigned int k = 0; k < middle; k++) {
      UString str = UString::from(k);
      UString str2 = UString::from(length - k - 1);
      Value obj = thisObj.get(exec,str);
      Value obj2 = thisObj.get(exec,str2);
      if (thisObj.hasProperty(exec,str2)) {
        if (thisObj.hasProperty(exec,str)) {
          thisObj.put(exec, str, obj2);
          thisObj.put(exec, str2, obj);
        } else {
          thisObj.put(exec, str, obj2);
          thisObj.deleteProperty(exec, str2);
        }
      } else {
        if (thisObj.hasProperty(exec, str)) {
          thisObj.deleteProperty(exec, str);
          thisObj.put(exec, str2, obj);
        } else {
          // why delete something that's not there ? Strange.
          thisObj.deleteProperty(exec, str);
          thisObj.deleteProperty(exec, str2);
        }
      }
    }
    result = thisObj;
    break;
  }
  case Shift: {
    if (length == 0) {
      thisObj.put(exec, "length", Number(length), DontEnum | DontDelete);
      result = Undefined();
    } else {
      result = thisObj.get(exec, "0");
      for(unsigned int k = 1; k < length; k++) {
        UString str = UString::from(k);
        UString str2 = UString::from(k-1);
        if (thisObj.hasProperty(exec, str)) {
          Value obj = thisObj.get(exec, str);
          thisObj.put(exec, str2, obj);
        } else
          thisObj.deleteProperty(exec, str2);
      }
      thisObj.deleteProperty(exec, UString::from(length - 1));
      thisObj.put(exec, "length", Number(length - 1), DontEnum | DontDelete);
    }
    break;
  }
  case Slice: {
    // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713 or 15.4.4.10

    // We return a new array
    Object resObj = Object::dynamicCast(exec->interpreter()->builtinArray().construct(exec,List::empty()));
    result = resObj;
    int begin = args[0].toUInt32(exec);
    if ( begin < 0 )
      begin = maxInt( begin + length, 0 );
    else
      begin = minInt( begin, length );
    int end = length;
    if (args[1].type() != UndefinedType)
    {
      end = args[1].toUInt32(exec);
      if ( end < 0 )
        end = maxInt( end + length, 0 );
      else
        end = minInt( end, length );
    }

    //printf( "Slicing from %d to %d \n", begin, end );
    for(unsigned int k = 0; k < (unsigned int) end-begin; k++) {
      UString str = UString::from(k+begin);
      if (thisObj.hasProperty(exec,str)) {
        UString str2 = UString::from(k);
        Value obj = thisObj.get(exec, str);
        resObj.put(exec, str2, obj);
      }
    }
    resObj.put(exec, "length", Number(end - begin), DontEnum | DontDelete);
    break;
  }
  case Sort:{
#if 0
    printf("KJS Array::Sort length=%d\n", length);
    for ( unsigned int i = 0 ; i<length ; ++i )
      printf("KJS Array::Sort: %d: %s\n", i, thisObj.get(UString::from(i)).toString().value().ascii() );
#endif
    Object sortFunction;
    bool useSortFunction = (args[0].type() != UndefinedType);
    if (useSortFunction)
      {
        sortFunction = args[0].toObject(exec);
        if (!sortFunction.implementsCall())
          useSortFunction = false;
      }

    if (length == 0) {
      thisObj.put(exec, "length", Number(0), DontEnum | DontDelete);
      result = Undefined();
      break;
    }

    // "Min" sort. Not the fastest, but definitely less code than heapsort
    // or quicksort, and much less swapping than bubblesort/insertionsort.
    for ( unsigned int i = 0 ; i<length-1 ; ++i )
      {
        Value iObj = thisObj.get(exec,UString::from(i));
        unsigned int themin = i;
        Value minObj = iObj;
        for ( unsigned int j = i+1 ; j<length ; ++j )
          {
            Value jObj = thisObj.get(exec,UString::from(j));
            int cmp;
            if (jObj.type() == UndefinedType) {
              cmp = 1;
            } else if (minObj.type() == UndefinedType) {
              cmp = -1;
            } else if (useSortFunction) {
                List l;
                l.append(jObj);
                l.append(minObj);
                Object thisObj = exec->interpreter()->globalObject();
                cmp = sortFunction.call(exec,thisObj, l ).toInt32(exec);
            } else {
              cmp = (jObj.toString(exec) < minObj.toString(exec)) ? -1 : 1;
            }
            if ( cmp < 0 )
              {
                themin = j;
                minObj = jObj;
              }
          }
        // Swap themin and i
        if ( themin > i )
          {
            //printf("KJS Array::Sort: swapping %d and %d\n", i, themin );
            thisObj.put( exec, UString::from(i), minObj );
            thisObj.put( exec, UString::from(themin), iObj );
          }
      }
#if 0
    printf("KJS Array::Sort -- Resulting array:\n");
    for ( unsigned int i = 0 ; i<length ; ++i )
      printf("KJS Array::Sort: %d: %s\n", i, thisObj.get(UString::from(i)).toString().value().ascii() );
#endif
    result = thisObj;
    break;
  }
  case Splice: {
    // 15.4.4.12 - oh boy this is huge
    Object resObj = Object::dynamicCast(exec->interpreter()->builtinArray().construct(exec,List::empty()));
    result = resObj;
    int begin = args[0].toUInt32(exec);
    if ( begin < 0 )
      begin = maxInt( begin + length, 0 );
    else
      begin = minInt( begin, length );
    unsigned int deleteCount = minInt( maxInt( args[1].toUInt32(exec), 0 ), length - begin );

    //printf( "Splicing from %d, deleteCount=%d \n", begin, deleteCount );
    for(unsigned int k = 0; k < deleteCount; k++) {
      UString str = UString::from(k+begin);
      if (thisObj.hasProperty(exec,str)) {
        UString str2 = UString::from(k);
        Value obj = thisObj.get(exec, str);
        resObj.put(exec, str2, obj);
      }
    }
    resObj.put(exec, "length", Number(deleteCount), DontEnum | DontDelete);

    unsigned int additionalArgs = maxInt( args.size() - 2, 0 );
    if ( additionalArgs != deleteCount )
    {
      if ( additionalArgs < deleteCount )
      {
        for ( unsigned int k = begin; k < length - deleteCount; ++k )
        {
          UString str = UString::from(k+deleteCount);
          UString str2 = UString::from(k+additionalArgs);
          if (thisObj.hasProperty(exec,str)) {
            Value obj = thisObj.get(exec, str);
            thisObj.put(exec, str2, obj);
          }
          else
            thisObj.deleteProperty(exec, str2);
        }
        for ( unsigned int k = length ; k > length - deleteCount + additionalArgs; --k )
          thisObj.deleteProperty(exec, UString::from(k-1));
      }
      else
      {
        for ( unsigned int k = length - deleteCount; (int)k > begin; --k )
        {
          UString str = UString::from(k+deleteCount-1);
          UString str2 = UString::from(k+additionalArgs-1);
          if (thisObj.hasProperty(exec,str)) {
            Value obj = thisObj.get(exec, str);
            thisObj.put(exec, str2, obj);
          }
          else
            thisObj.deleteProperty(exec, str2);
        }
      }
    }
    for ( unsigned int k = 0; k < additionalArgs; ++k )
    {
      thisObj.put(exec, UString::from(k+begin), args[k+2]);
    }
    thisObj.put(exec, "length", Number(length - deleteCount + additionalArgs), DontEnum | DontDelete);
    break;
  }
  case UnShift: { // 15.4.4.13
    unsigned int nrArgs = args.size();
    for ( unsigned int k = length; k > 0; --k )
    {
      UString str = UString::from(k-1);
      UString str2 = UString::from(k+nrArgs-1);
      if (thisObj.hasProperty(exec,str)) {
        Value obj = thisObj.get(exec, str);
        thisObj.put(exec, str2, obj);
      } else {
        thisObj.deleteProperty(exec, str2);
      }
    }
    for ( unsigned int k = 0; k < nrArgs; ++k )
      thisObj.put(exec, UString::from(k), args[k]);
    result = Number(length + nrArgs);
    thisObj.put(exec, "length", result, DontEnum | DontDelete);
    break;
  }
  default:
    assert(0);
    break;
  }
  return result;
}

// ------------------------------ ArrayObjectImp -------------------------------

ArrayObjectImp::ArrayObjectImp(ExecState *exec,
                               FunctionPrototypeImp *funcProto,
                               ArrayPrototypeImp *arrayProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  // ECMA 15.4.3.1 Array.prototype
  put(exec,"prototype", Object(arrayProto), DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec,"length", Number(1), ReadOnly|DontDelete|DontEnum);
}

bool ArrayObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.4.2
Object ArrayObjectImp::construct(ExecState *exec, const List &args)
{
  Object result(new ArrayInstanceImp(exec->interpreter()->builtinArrayPrototype()));

  unsigned int len;
  ListIterator it = args.begin();
  // a single argument might denote the array size
  if (args.size() == 1 && it->type() == NumberType)
    len = it->toUInt32(exec);
  else {
    // initialize array
    len = args.size();
    for (unsigned int u = 0; it != args.end(); it++, u++)
      result.put(exec, UString::from(u), *it);
  }

  // array size
  result.put(exec, "length", Number(len), DontEnum | DontDelete);
  static_cast<ArrayInstanceImp*>(result.imp())->putDirect(exec, "length", Number(len), DontEnum | DontDelete);

  return result;
}

bool ArrayObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.6.1
Value ArrayObjectImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  // equivalent to 'new Array(....)'
  return construct(exec,args);
}

