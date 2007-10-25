/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2003 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "array_object.h"
#include "array_object.lut.h"

#include "error_object.h"
#include "lookup.h"
#include "operations.h"
#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

namespace KJS {

// ------------------------------ ArrayPrototype ----------------------------

const ClassInfo ArrayPrototype::info = {"Array", &ArrayInstance::info, &arrayTable, 0};

/* Source for array_object.lut.h
@begin arrayTable 16
  toString       ArrayProtoFunc::ToString       DontEnum|Function 0
  toLocaleString ArrayProtoFunc::ToLocaleString DontEnum|Function 0
  concat         ArrayProtoFunc::Concat         DontEnum|Function 1
  join           ArrayProtoFunc::Join           DontEnum|Function 1
  pop            ArrayProtoFunc::Pop            DontEnum|Function 0
  push           ArrayProtoFunc::Push           DontEnum|Function 1
  reverse        ArrayProtoFunc::Reverse        DontEnum|Function 0
  shift          ArrayProtoFunc::Shift          DontEnum|Function 0
  slice          ArrayProtoFunc::Slice          DontEnum|Function 2
  sort           ArrayProtoFunc::Sort           DontEnum|Function 1
  splice         ArrayProtoFunc::Splice         DontEnum|Function 2
  unshift        ArrayProtoFunc::UnShift        DontEnum|Function 1
  every          ArrayProtoFunc::Every          DontEnum|Function 1
  forEach        ArrayProtoFunc::ForEach        DontEnum|Function 1
  some           ArrayProtoFunc::Some           DontEnum|Function 1
  indexOf        ArrayProtoFunc::IndexOf        DontEnum|Function 1
  lastIndexOf    ArrayProtoFunc::LastIndexOf    DontEnum|Function 1
  filter         ArrayProtoFunc::Filter         DontEnum|Function 1
  map            ArrayProtoFunc::Map            DontEnum|Function 1
@end
*/

// ECMA 15.4.4
ArrayPrototype::ArrayPrototype(ExecState*, ObjectPrototype* objProto)
  : ArrayInstance(objProto, 0)
{
}

bool ArrayPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticFunctionSlot<ArrayProtoFunc, ArrayInstance>(exec, &arrayTable, this, propertyName, slot);
}

// ------------------------------ ArrayProtoFunc ----------------------------

ArrayProtoFunc::ArrayProtoFunc(ExecState* exec, int i, int len, const Identifier& name)
  : InternalFunctionImp(static_cast<FunctionPrototype*>
                        (exec->lexicalInterpreter()->builtinFunctionPrototype()), name)
  , id(i)
{
  put(exec, exec->propertyNames().length, jsNumber(len), DontDelete | ReadOnly | DontEnum);
}

static JSValue *getProperty(ExecState *exec, JSObject *obj, unsigned index)
{
    PropertySlot slot;
    if (!obj->getPropertySlot(exec, index, slot))
        return NULL;
    return slot.getValue(exec, obj, index);
}

// ECMA 15.4.4
JSValue* ArrayProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
  unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);

  JSValue *result = 0; // work around gcc 4.0 bug in uninitialized variable warning
  
  switch (id) {
  case ToLocaleString:
  case ToString:

    if (!thisObj->inherits(&ArrayInstance::info))
      return throwError(exec, TypeError);

    // fall through
  case Join: {
    static HashSet<JSObject*> visitedElems;
    static const UString* empty = new UString("");
    static const UString* comma = new UString(",");
    bool alreadyVisited = !visitedElems.add(thisObj).second;
    if (alreadyVisited)
        return jsString(*empty);
    UString separator = *comma;
    UString str = *empty;

    if (id == Join && !args[0]->isUndefined())
        separator = args[0]->toString(exec);
    for (unsigned int k = 0; k < length; k++) {
        if (k >= 1)
            str += separator;
        if (str.isNull()) {
            JSObject *error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue* element = thisObj->get(exec, k);
        if (element->isUndefinedOrNull())
            continue;

        bool fallback = false;
        if (id == ToLocaleString) {
            JSObject* o = element->toObject(exec);
            JSValue* conversionFunction = o->get(exec, exec->propertyNames().toLocaleString);
            if (conversionFunction->isObject() && static_cast<JSObject*>(conversionFunction)->implementsCall())
                str += static_cast<JSObject*>(conversionFunction)->call(exec, o, List())->toString(exec);
            else
                // try toString() fallback
                fallback = true;
        }

        if (id == ToString || id == Join || fallback)
            str += element->toString(exec);

        if (str.isNull()) {
            JSObject *error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    visitedElems.remove(thisObj);
    result = jsString(str);
    break;
  }
  case Concat: {
    JSObject *arr = static_cast<JSObject *>(exec->lexicalInterpreter()->builtinArray()->construct(exec,List::empty()));
    int n = 0;
    JSValue *curArg = thisObj;
    JSObject *curObj = static_cast<JSObject *>(thisObj);
    ListIterator it = args.begin();
    for (;;) {
      if (curArg->isObject() &&
          curObj->inherits(&ArrayInstance::info)) {
        unsigned int k = 0;
        // Older versions tried to optimize out getting the length of thisObj
        // by checking for n != 0, but that doesn't work if thisObj is an empty array.
        length = curObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
        while (k < length) {
          if (JSValue *v = getProperty(exec, curObj, k))
            arr->put(exec, n, v);
          n++;
          k++;
        }
      } else {
        arr->put(exec, n, curArg);
        n++;
      }
      if (it == args.end())
        break;
      curArg = *it;
      curObj = static_cast<JSObject *>(it++); // may be 0
    }
    arr->put(exec, exec->propertyNames().length, jsNumber(n), DontEnum | DontDelete);

    result = arr;
    break;
  }
  case Pop:{
    if (length == 0) {
      thisObj->put(exec, exec->propertyNames().length, jsNumber(length), DontEnum | DontDelete);
      result = jsUndefined();
    } else {
      result = thisObj->get(exec, length - 1);
      thisObj->put(exec, exec->propertyNames().length, jsNumber(length - 1), DontEnum | DontDelete);
    }
    break;
  }
  case Push: {
    for (int n = 0; n < args.size(); n++)
      thisObj->put(exec, length + n, args[n]);
    length += args.size();
    thisObj->put(exec, exec->propertyNames().length, jsNumber(length), DontEnum | DontDelete);
    result = jsNumber(length);
    break;
  }
  case Reverse: {

    unsigned int middle = length / 2;

    for (unsigned int k = 0; k < middle; k++) {
      unsigned lk1 = length - k - 1;
      JSValue *obj2 = getProperty(exec, thisObj, lk1);
      JSValue *obj = getProperty(exec, thisObj, k);

      if (obj2) 
        thisObj->put(exec, k, obj2);
      else
        thisObj->deleteProperty(exec, k);

      if (obj)
        thisObj->put(exec, lk1, obj);
      else
        thisObj->deleteProperty(exec, lk1);
    }
    result = thisObj;
    break;
  }
  case Shift: {
    if (length == 0) {
      thisObj->put(exec, exec->propertyNames().length, jsNumber(length), DontEnum | DontDelete);
      result = jsUndefined();
    } else {
      result = thisObj->get(exec, 0);
      for(unsigned int k = 1; k < length; k++) {
        if (JSValue *obj = getProperty(exec, thisObj, k))
          thisObj->put(exec, k-1, obj);
        else
          thisObj->deleteProperty(exec, k-1);
      }
      thisObj->deleteProperty(exec, length - 1);
      thisObj->put(exec, exec->propertyNames().length, jsNumber(length - 1), DontEnum | DontDelete);
    }
    break;
  }
  case Slice: {
    // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713 or 15.4.4.10

    // We return a new array
    JSObject *resObj = static_cast<JSObject *>(exec->lexicalInterpreter()->builtinArray()->construct(exec,List::empty()));
    result = resObj;
    double begin = 0;
    if (!args[0]->isUndefined()) {
        begin = args[0]->toInteger(exec);
        if (begin >= 0) { // false for NaN
            if (begin > length)
                begin = length;
        } else {
            begin += length;
            if (!(begin >= 0)) // true for NaN
                begin = 0;
        }
    }
    double end = length;
    if (!args[1]->isUndefined()) {
      end = args[1]->toInteger(exec);
      if (end < 0) { // false for NaN
        end += length;
        if (end < 0)
          end = 0;
      } else {
        if (!(end <= length)) // true for NaN
          end = length;
      }
    }

    //printf( "Slicing from %d to %d \n", begin, end );
    int n = 0;
    int b = static_cast<int>(begin);
    int e = static_cast<int>(end);
    for(int k = b; k < e; k++, n++) {
      if (JSValue *v = getProperty(exec, thisObj, k))
        resObj->put(exec, n, v);
    }
    resObj->put(exec, exec->propertyNames().length, jsNumber(n), DontEnum | DontDelete);
    break;
  }
  case Sort:{
#if 0
    printf("KJS Array::Sort length=%d\n", length);
    for ( unsigned int i = 0 ; i<length ; ++i )
      printf("KJS Array::Sort: %d: %s\n", i, thisObj->get(exec, i)->toString(exec).ascii() );
#endif
    JSObject *sortFunction = NULL;
    if (!args[0]->isUndefined())
      {
        sortFunction = args[0]->toObject(exec);
        if (!sortFunction->implementsCall())
          sortFunction = NULL;
      }
    
    if (thisObj->classInfo() == &ArrayInstance::info) {
      if (sortFunction)
        ((ArrayInstance *)thisObj)->sort(exec, sortFunction);
      else
        ((ArrayInstance *)thisObj)->sort(exec);
      result = thisObj;
      break;
    }

    if (length == 0) {
      thisObj->put(exec, exec->propertyNames().length, jsNumber(0), DontEnum | DontDelete);
      result = thisObj;
      break;
    }

    // "Min" sort. Not the fastest, but definitely less code than heapsort
    // or quicksort, and much less swapping than bubblesort/insertionsort.
    for ( unsigned int i = 0 ; i<length-1 ; ++i )
      {
        JSValue *iObj = thisObj->get(exec,i);
        unsigned int themin = i;
        JSValue *minObj = iObj;
        for ( unsigned int j = i+1 ; j<length ; ++j )
          {
            JSValue *jObj = thisObj->get(exec,j);
            double cmp;
            if (jObj->isUndefined()) {
              cmp = 1; // don't check minObj because there's no need to differentiate == (0) from > (1)
            } else if (minObj->isUndefined()) {
              cmp = -1;
            } else if (sortFunction) {
                List l;
                l.append(jObj);
                l.append(minObj);
                cmp = sortFunction->call(exec, exec->dynamicInterpreter()->globalObject(), l)->toNumber(exec);
            } else {
              cmp = (jObj->toString(exec) < minObj->toString(exec)) ? -1 : 1;
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
            thisObj->put( exec, i, minObj );
            thisObj->put( exec, themin, iObj );
          }
      }
#if 0
    printf("KJS Array::Sort -- Resulting array:\n");
    for ( unsigned int i = 0 ; i<length ; ++i )
      printf("KJS Array::Sort: %d: %s\n", i, thisObj->get(exec, i)->toString(exec).ascii() );
#endif
    result = thisObj;
    break;
  }
  case Splice: {
    // 15.4.4.12 - oh boy this is huge
    JSObject *resObj = static_cast<JSObject *>(exec->lexicalInterpreter()->builtinArray()->construct(exec,List::empty()));
    result = resObj;
    int begin = args[0]->toUInt32(exec);
    if ( begin < 0 )
      begin = maxInt( begin + length, 0 );
    else
      begin = minInt( begin, length );
    unsigned int deleteCount = minInt( maxInt( args[1]->toUInt32(exec), 0 ), length - begin );

    //printf( "Splicing from %d, deleteCount=%d \n", begin, deleteCount );
    for(unsigned int k = 0; k < deleteCount; k++) {
      if (JSValue *v = getProperty(exec, thisObj, k+begin))
        resObj->put(exec, k, v);
    }
    resObj->put(exec, exec->propertyNames().length, jsNumber(deleteCount), DontEnum | DontDelete);

    unsigned int additionalArgs = maxInt( args.size() - 2, 0 );
    if ( additionalArgs != deleteCount )
    {
      if ( additionalArgs < deleteCount )
      {
        for ( unsigned int k = begin; k < length - deleteCount; ++k )
        {
          if (JSValue *v = getProperty(exec, thisObj, k+deleteCount))
            thisObj->put(exec, k+additionalArgs, v);
          else
            thisObj->deleteProperty(exec, k+additionalArgs);
        }
        for ( unsigned int k = length ; k > length - deleteCount + additionalArgs; --k )
          thisObj->deleteProperty(exec, k-1);
      }
      else
      {
        for ( unsigned int k = length - deleteCount; (int)k > begin; --k )
        {
          if (JSValue *obj = getProperty(exec, thisObj, k + deleteCount - 1))
            thisObj->put(exec, k + additionalArgs - 1, obj);
          else
            thisObj->deleteProperty(exec, k+additionalArgs-1);
        }
      }
    }
    for ( unsigned int k = 0; k < additionalArgs; ++k )
    {
      thisObj->put(exec, k+begin, args[k+2]);
    }
    thisObj->put(exec, exec->propertyNames().length, jsNumber(length - deleteCount + additionalArgs), DontEnum | DontDelete);
    break;
  }
  case UnShift: { // 15.4.4.13
    unsigned int nrArgs = args.size();
    for ( unsigned int k = length; k > 0; --k )
    {
      if (JSValue *v = getProperty(exec, thisObj, k - 1))
        thisObj->put(exec, k+nrArgs-1, v);
      else
        thisObj->deleteProperty(exec, k+nrArgs-1);
    }
    for ( unsigned int k = 0; k < nrArgs; ++k )
      thisObj->put(exec, k, args[k]);
    result = jsNumber(length + nrArgs);
    thisObj->put(exec, exec->propertyNames().length, result, DontEnum | DontDelete);
    break;
  }
  case Filter:
  case Map: {
    JSObject *eachFunction = args[0]->toObject(exec);
    
    if (!eachFunction->implementsCall())
      return throwError(exec, TypeError);
    
    JSObject *applyThis = args[1]->isUndefinedOrNull() ? exec->dynamicInterpreter()->globalObject() :  args[1]->toObject(exec);
    JSObject *resultArray;
    
    if (id == Filter) 
      resultArray = static_cast<JSObject *>(exec->lexicalInterpreter()->builtinArray()->construct(exec, List::empty()));
    else {
      List args;
      args.append(jsNumber(length));
      resultArray = static_cast<JSObject *>(exec->lexicalInterpreter()->builtinArray()->construct(exec, args));
    }
    
    unsigned filterIndex = 0;
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
      PropertySlot slot;

      if (!thisObj->getPropertySlot(exec, k, slot))
         continue;
        
      JSValue *v = slot.getValue(exec, thisObj, k);
      
      List eachArguments;
      
      eachArguments.append(v);
      eachArguments.append(jsNumber(k));
      eachArguments.append(thisObj);
      
      JSValue *result = eachFunction->call(exec, applyThis, eachArguments);
      
      if (id == Map)
        resultArray->put(exec, k, result);
      else if (result->toBoolean(exec)) 
        resultArray->put(exec, filterIndex++, v);
    }
    
    return resultArray;
  }
  case Every:
  case ForEach:
  case Some: {
    //Documentation for these three is available at:
    //http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:every
    //http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:forEach
    //http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:some
    
    JSObject *eachFunction = args[0]->toObject(exec);
    
    if (!eachFunction->implementsCall())
      return throwError(exec, TypeError);
    
    JSObject *applyThis = args[1]->isUndefinedOrNull() ? exec->dynamicInterpreter()->globalObject() :  args[1]->toObject(exec);
    
    if (id == Some || id == Every)
      result = jsBoolean(id == Every);
    else
      result = jsUndefined();
    
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
      PropertySlot slot;
        
      if (!thisObj->getPropertySlot(exec, k, slot))
        continue;
      
      List eachArguments;
      
      eachArguments.append(slot.getValue(exec, thisObj, k));
      eachArguments.append(jsNumber(k));
      eachArguments.append(thisObj);
      
      bool predicateResult = eachFunction->call(exec, applyThis, eachArguments)->toBoolean(exec);
      
      if (id == Every && !predicateResult) {
        result = jsBoolean(false);
        break;
      }
      if (id == Some && predicateResult) {
        result = jsBoolean(true);
        break;
      }
    }
    break;
  }

  case IndexOf: {
    // JavaScript 1.5 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:indexOf

    unsigned index = 0;
    double d = args[1]->toInteger(exec);
    if (d < 0)
        d += length;
    if (d > 0) {
        if (d > length)
            index = length;
        else
            index = static_cast<unsigned>(d);
    }

    JSValue* searchElement = args[0];
    for (; index < length; ++index) {
        JSValue* e = getProperty(exec, thisObj, index);
        if (!e)
            continue;
        if (strictEqual(exec, searchElement, e))
            return jsNumber(index);
    }

    return jsNumber(-1);
  }
  case LastIndexOf: {
       // JavaScript 1.6 Extension by Mozilla
      // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:lastIndexOf 

    int index = length - 1;
    double d = args[1]->toInteger(exec);

    if (d < 0) {
        d += length;
        if (d < 0) 
            return jsNumber(-1);
    }
    if (d < length)
        index = static_cast<int>(d);
          
    JSValue* searchElement = args[0];
    for (; index >= 0; --index) {
        JSValue* e = getProperty(exec, thisObj, index);
        if (!e)
            continue;
        if (strictEqual(exec, searchElement, e))
            return jsNumber(index);
    }
          
    return jsNumber(-1);
}
  default:
    ASSERT(0);
    result = 0;
    break;
  }
  return result;
}

// ------------------------------ ArrayObjectImp -------------------------------

ArrayObjectImp::ArrayObjectImp(ExecState *exec,
                               FunctionPrototype *funcProto,
                               ArrayPrototype *arrayProto)
  : InternalFunctionImp(funcProto)
{
  // ECMA 15.4.3.1 Array.prototype
  put(exec, exec->propertyNames().prototype, arrayProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec, exec->propertyNames().length, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}

bool ArrayObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.4.2
JSObject *ArrayObjectImp::construct(ExecState *exec, const List &args)
{
  // a single numeric argument denotes the array size (!)
  if (args.size() == 1 && args[0]->isNumber()) {
    uint32_t n = args[0]->toUInt32(exec);
    if (n != args[0]->toNumber(exec))
      return throwError(exec, RangeError, "Array size is not a small enough positive integer.");
    return new ArrayInstance(exec->lexicalInterpreter()->builtinArrayPrototype(), n);
  }

  // otherwise the array is constructed with the arguments in it
  return new ArrayInstance(exec->lexicalInterpreter()->builtinArrayPrototype(), args);
}

// ECMA 15.6.1
JSValue *ArrayObjectImp::callAsFunction(ExecState *exec, JSObject *, const List &args)
{
  // equivalent to 'new Array(....)'
  return construct(exec,args);
}

}
