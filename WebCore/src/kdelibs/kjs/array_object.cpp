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
 */

#include "kjs.h"
#include "operations.h"
#include "types.h"
#include "array_object.h"
#include <stdio.h>

using namespace KJS;

ArrayObject::ArrayObject(const Object &funcProto,
			 const Object &arrayProto)
    : ConstructorImp(funcProto, 1)
{
  // ECMA 15.4.3.1 Array.prototype
  setPrototypeProperty(arrayProto);
}

// ECMA 15.6.1
Completion ArrayObject::execute(const List &args)
{
  // equivalent to 'new Array(....)'
  KJSO result = construct(args);

  return Completion(ReturnValue, result);
}

// ECMA 15.6.2
Object ArrayObject::construct(const List &args)
{
  Object result = Object::create(ArrayClass);

  unsigned int len;
  ListIterator it = args.begin();
  // a single argument might denote the array size
  if (args.size() == 1 && it->isA(NumberType))
    len = it->toUInt32();
  else {
    // initialize array
    len = args.size();
    for (unsigned int u = 0; it != args.end(); it++, u++)
      result.put(UString::from(u), *it);
  }

  // array size
  result.put("length", len, DontEnum | DontDelete);

  return result;
}

// ECMA 15.6.4
ArrayPrototype::ArrayPrototype(const Object& proto)
  : ObjectImp(ArrayClass, Null(), proto)
{
  // The constructor will be added later in ArrayObject's constructor

  put("length", 0u, DontEnum | DontDelete);
}

KJSO ArrayPrototype::get(const UString &p) const
{
  int id;
  if(p == "toString")
    id = ArrayProtoFunc::ToString;
  else if(p == "toLocaleString")
    id = ArrayProtoFunc::ToLocaleString;
  else if(p == "concat")
    id = ArrayProtoFunc::Concat;
  else if (p == "join")
    id = ArrayProtoFunc::Join;
  else if(p == "pop")
    id = ArrayProtoFunc::Pop;
  else if(p == "push")
    id = ArrayProtoFunc::Push;
  else if(p == "reverse")
    id = ArrayProtoFunc::Reverse;
  else if(p == "shift")
    id = ArrayProtoFunc::Shift;
  else if(p == "slice")
    id = ArrayProtoFunc::Slice;
  else if(p == "sort")
    id = ArrayProtoFunc::Sort;
  else if(p == "splice")
    id = ArrayProtoFunc::Splice;
  else if(p == "unshift")
    id = ArrayProtoFunc::UnShift;
  else
    return Imp::get(p);

  return Function(new ArrayProtoFunc(id));
}

// ECMA 15.4.4
Completion ArrayProtoFunc::execute(const List &args)
{
  KJSO result, obj, obj2;
  Object thisObj = Object::dynamicCast(thisValue());
  unsigned int length = thisObj.get("length").toUInt32();
  unsigned int middle;
  UString str = "", str2;
  UString separator = ",";

  switch (id) {
  case ToLocaleString:
    /* TODO */
    // fall trough
  case ToString:
    if (!thisObj.getClass() == ArrayClass) {
      result = Error::create(TypeError);
      break;
    }
    // fall trough
  case Join:
    {
      if (!args[0].isA(UndefinedType))
	separator = args[0].toString().value();
      for (unsigned int k = 0; k < length; k++) {
	if (k >= 1)
	  str += separator;
	obj = thisObj.get(UString::from(k));
	if (!obj.isA(UndefinedType) && !obj.isA(NullType))
	  str += obj.toString().value();
      }
    }
    result = String(str);
    break;
  case Concat: {
    result = Object::create(ArrayClass);
    int n = 0;
    obj = thisObj;
    ListIterator it = args.begin();
    for (;;) {
      if (obj.isA(ObjectType) &&
	  static_cast<Object&>(obj).getClass() == ArrayClass) {
	unsigned int k = 0;
	if (n > 0)
	  length = obj.get("length").toUInt32();
	while (k < length) {
	  UString p = UString::from(k);
	  if (obj.hasProperty(p))
	    result.put(UString::from(n), obj.get(p));
	  n++;
	  k++;
	}
      } else {
	result.put(UString::from(n), obj);
	n++;      
      }
      if (it == args.end())
	break;
      obj = it++;
    }
    result.put("length", Number(n), DontEnum | DontDelete);
  }
    break;
  case Pop:
    if (length == 0) {
      thisObj.put("length", Number(length), DontEnum | DontDelete);
      result = Undefined();
    } else {
      str = UString::from(length - 1);
      result = thisObj.get(str);
      thisObj.deleteProperty(str);
      thisObj.put("length", length - 1, DontEnum | DontDelete);
    }
    break;
  case Push:
    {
      for (int n = 0; n < args.size(); n++)
	thisObj.put(UString::from(length + n), args[n]);
      length += args.size();
      thisObj.put("length", length, DontEnum | DontDelete);
      result = Number(length);
    }
    break;
  case Reverse:
    {
      middle = length / 2;
      for (unsigned int k = 0; k < middle; k++) {
	str = UString::from(k);
	str2 = UString::from(length - k - 1);
	obj = thisObj.get(str);
	obj2 = thisObj.get(str2);
	if (thisObj.hasProperty(str2)) {
	  if (thisObj.hasProperty(str)) {
	    thisObj.put(str, obj2);
	    thisObj.put(str2, obj);
	  } else {
	    thisObj.put(str, obj2);
	    thisObj.deleteProperty(str2);
	  }
	} else {
	  if (thisObj.hasProperty(str)) {
	    thisObj.deleteProperty(str);
	    thisObj.put(str2, obj);
	  } else {
	    // why delete something that's not there ? Strange.
	    thisObj.deleteProperty(str);
	    thisObj.deleteProperty(str2);
	  }
	}
      }
    }
    result = thisObj;
    break;
  case Shift:
    if (length == 0) {
      thisObj.put("length", Number(length), DontEnum | DontDelete);
      result = Undefined();
    } else {
      result = thisObj.get("0");
      for(unsigned int k = 1; k < length; k++) {
	str = UString::from(k);
	str2 = UString::from(k-1);
	if (thisObj.hasProperty(str)) {
	  obj = thisObj.get(str);
	  thisObj.put(str2, obj);
	} else
	  thisObj.deleteProperty(str2);
      }
      thisObj.deleteProperty(UString::from(length - 1));
      thisObj.put("length", length - 1, DontEnum | DontDelete);
    }
    break;
  case Slice: // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713
    {
        result = Object::create(ArrayClass); // We return a new array
        int begin = args[0].toUInt32();
        int end = length;
        if (!args[1].isA(UndefinedType))
        {
          end = args[1].toUInt32();
          if ( end < 0 )
            end += length;
        }
        // safety tests
        if ( begin < 0 || end < 0 || begin >= end ) {
            result.put("length", Number(0), DontEnum | DontDelete);
            break;
        }
        //printf( "Slicing from %d to %d \n", begin, end );
        for(unsigned int k = 0; k < (unsigned int) end-begin; k++) {
            str = UString::from(k+begin);
            str2 = UString::from(k);
            if (thisObj.hasProperty(str)) {
                obj = thisObj.get(str);
                result.put(str2, obj);
            }
        }
        result.put("length", end - begin, DontEnum | DontDelete);
        break;
    }
  case Sort:
    {
#if 0
        printf("KJS Array::Sort length=%d\n", length);
        for ( unsigned int i = 0 ; i<length ; ++i )
            printf("KJS Array::Sort: %d: %s\n", i, thisObj.get(UString::from(i)).toString().value().ascii() );
#endif
        Object sortFunction;
        bool useSortFunction = !args[0].isA(UndefinedType);
        if (useSortFunction)
        {
            sortFunction = args[0].toObject();
            if (!sortFunction.implementsCall())
                useSortFunction = false;
        }

        if (length == 0) {
            thisObj.put("length", Number(0), DontEnum | DontDelete);
            result = Undefined();
            break;
        }

        // "Min" sort. Not the fastest, but definitely less code than heapsort
        // or quicksort, and much less swapping than bubblesort/insertionsort.
        for ( unsigned int i = 0 ; i<length-1 ; ++i )
        {
            KJSO iObj = thisObj.get(UString::from(i));
            unsigned int themin = i;
            KJSO minObj = iObj;
            for ( unsigned int j = i+1 ; j<length ; ++j )
            {
                KJSO jObj = thisObj.get(UString::from(j));
                int cmp;
                if ( useSortFunction )
                {
                    List l;
                    l.append(jObj);
                    l.append(minObj);
                    cmp = sortFunction.executeCall( Global::current(), &l ).toInt32();
                }
                else
                    cmp = ( jObj.toString().value() < minObj.toString().value() ) ? -1 : 1;
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
                thisObj.put( UString::from(i), minObj );
                thisObj.put( UString::from(themin), iObj );
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
  // TODO Splice
  // TODO Unshift
  default:
    result = Undefined();
  }

  return Completion(ReturnValue, result);
}
