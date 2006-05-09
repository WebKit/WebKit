// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
 *  Copyright (C) 2003 Peter Kelly (pmk@post.com)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "array_object.h"

#include "error_object.h"
#include "lookup.h"
#include "operations.h"
#include "reference_list.h"
#include <wtf/HashSet.h>
#include <stdio.h>

#include "array_object.lut.h"

using namespace KJS;

// ------------------------------ ArrayInstance -----------------------------

const unsigned sparseArrayCutoff = 10000;

const ClassInfo ArrayInstance::info = {"Array", 0, 0, 0};

ArrayInstance::ArrayInstance(JSObject *proto, unsigned initialLength)
  : JSObject(proto)
  , length(initialLength)
  , storageLength(initialLength < sparseArrayCutoff ? initialLength : 0)
  , capacity(storageLength)
  , storage(capacity ? (JSValue **)fastCalloc(capacity, sizeof(JSValue *)) : 0)
{
}

ArrayInstance::ArrayInstance(JSObject *proto, const List &list)
  : JSObject(proto)
  , length(list.size())
  , storageLength(length)
  , capacity(storageLength)
  , storage(capacity ? (JSValue **)fastMalloc(sizeof(JSValue *) * capacity) : 0)
{
  ListIterator it = list.begin();
  unsigned l = length;
  for (unsigned i = 0; i < l; ++i) {
    storage[i] = it++;
  }
}

ArrayInstance::~ArrayInstance()
{
  fastFree(storage);
}

JSValue *ArrayInstance::lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
  return jsNumber(static_cast<ArrayInstance *>(slot.slotBase())->length);
}

bool ArrayInstance::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (propertyName == lengthPropertyName) {
    slot.setCustom(this, lengthGetter);
    return true;
  }

  bool ok;
  unsigned index = propertyName.toArrayIndex(&ok);
  if (ok) {
    if (index >= length)
      return false;
    if (index < storageLength) {
      JSValue *v = storage[index];
      if (!v || v->isUndefined())
        return false;      
      slot.setValueSlot(this, &storage[index]);
      return true;
    }
  }

  return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

bool ArrayInstance::getOwnPropertySlot(ExecState *exec, unsigned index, PropertySlot& slot)
{
  if (index > MAX_ARRAY_INDEX)
    return getOwnPropertySlot(exec, Identifier::from(index), slot);

  if (index >= length)
    return false;
  if (index < storageLength) {
    JSValue *v = storage[index];
    if (!v || v->isUndefined())
      return false;
    slot.setValueSlot(this, &storage[index]);
    return true;
  }

  return JSObject::getOwnPropertySlot(exec, index, slot);
}

// Special implementation of [[Put]] - see ECMA 15.4.5.1
void ArrayInstance::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
  if (propertyName == lengthPropertyName) {
    unsigned int newLen = value->toUInt32(exec);
    if (value->toNumber(exec) != double(newLen)) {
      throwError(exec, RangeError, "Invalid array length.");
      return;
    }
    setLength(newLen, exec);
    return;
  }
  
  bool ok;
  unsigned index = propertyName.toArrayIndex(&ok);
  if (ok) {
    put(exec, index, value, attr);
    return;
  }
  
  JSObject::put(exec, propertyName, value, attr);
}

void ArrayInstance::put(ExecState *exec, unsigned index, JSValue *value, int attr)
{
  //0xFFFF FFFF is a bit weird --- it should be treated as a non-array index, even when
  //it's a string 
  if (index > MAX_ARRAY_INDEX) {
    put(exec, Identifier::from(index), value, attr);
    return;
  }

  if (index < sparseArrayCutoff && index >= storageLength) {
    resizeStorage(index + 1);
  }

  if (index >= length) {
    length = index + 1;
  }

  if (index < storageLength) {
    storage[index] = value;
    return;
  }
  
  assert(index >= sparseArrayCutoff);
  JSObject::put(exec, Identifier::from(index), value, attr);
}

bool ArrayInstance::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
  if (propertyName == lengthPropertyName)
    return false;
  
  bool ok;
  uint32_t index = propertyName.toArrayIndex(&ok);
  if (ok) {
    if (index >= length)
      return true;
    if (index < storageLength) {
      storage[index] = 0;
      return true;
    }
  }
  
  return JSObject::deleteProperty(exec, propertyName);
}

bool ArrayInstance::deleteProperty(ExecState *exec, unsigned index)
{
  if (index > MAX_ARRAY_INDEX)
    return deleteProperty(exec, Identifier::from(index));

  if (index >= length)
    return true;
  if (index < storageLength) {
    storage[index] = 0;
    return true;
  }
  
  return JSObject::deleteProperty(exec, Identifier::from(index));
}

ReferenceList ArrayInstance::propList(ExecState *exec, bool recursive)
{
  ReferenceList properties = JSObject::propList(exec,recursive);

  // avoid fetching this every time through the loop
  JSValue *undefined = jsUndefined();

  //### FIXME: should avoid duplicates with prototype
  for (unsigned i = 0; i < storageLength; ++i) {
    JSValue *imp = storage[i];
    if (imp && imp != undefined) {
      properties.append(Reference(this, i));
    }
  }
  return properties;
}

void ArrayInstance::resizeStorage(unsigned newLength)
{
    if (newLength < storageLength) {
      memset(storage + newLength, 0, sizeof(JSValue *) * (storageLength - newLength));
    }
    if (newLength > capacity) {
      unsigned newCapacity;
      if (newLength > sparseArrayCutoff) {
        newCapacity = newLength;
      } else {
        newCapacity = (newLength * 3 + 1) / 2;
        if (newCapacity > sparseArrayCutoff) {
          newCapacity = sparseArrayCutoff;
        }
      }
      storage = (JSValue **)fastRealloc(storage, newCapacity * sizeof (JSValue *));
      memset(storage + capacity, 0, sizeof(JSValue *) * (newCapacity - capacity));
      capacity = newCapacity;
    }
    storageLength = newLength;
}

void ArrayInstance::setLength(unsigned newLength, ExecState *exec)
{
  if (newLength <= storageLength) {
    resizeStorage(newLength);
  }

  if (newLength < length) {
    ReferenceList sparseProperties;
    
    _prop.addSparseArrayPropertiesToReferenceList(sparseProperties, this);
    
    ReferenceListIterator it = sparseProperties.begin();
    while (it != sparseProperties.end()) {
      Reference ref = it++;
      bool ok;
      unsigned index = ref.getPropertyName(exec).toArrayIndex(&ok);
      if (ok && index > newLength) {
        ref.deleteValue(exec);
      }
    }
  }
  
  length = newLength;
}

void ArrayInstance::mark()
{
  JSObject::mark();
  unsigned l = storageLength;
  for (unsigned i = 0; i < l; ++i) {
    JSValue *imp = storage[i];
    if (imp && !imp->marked())
      imp->mark();
  }
}

static ExecState *execForCompareByStringForQSort;

static int compareByStringForQSort(const void *a, const void *b)
{
    ExecState *exec = execForCompareByStringForQSort;
    JSValue *va = *(JSValue **)a;
    JSValue *vb = *(JSValue **)b;
    if (va->isUndefined()) {
        return vb->isUndefined() ? 0 : 1;
    }
    if (vb->isUndefined()) {
        return -1;
    }
    return compare(va->toString(exec), vb->toString(exec));
}

void ArrayInstance::sort(ExecState *exec)
{
    int lengthNotIncludingUndefined = pushUndefinedObjectsToEnd(exec);
    
    execForCompareByStringForQSort = exec;
    qsort(storage, lengthNotIncludingUndefined, sizeof(JSValue *), compareByStringForQSort);
    execForCompareByStringForQSort = 0;
}

struct CompareWithCompareFunctionArguments {
    CompareWithCompareFunctionArguments(ExecState *e, JSObject *cf)
        : exec(e)
        , compareFunction(cf)
        , globalObject(e->dynamicInterpreter()->globalObject())
    {
        arguments.append(jsUndefined());
        arguments.append(jsUndefined());
    }

    ExecState *exec;
    JSObject *compareFunction;
    List arguments;
    JSObject *globalObject;
};

static CompareWithCompareFunctionArguments *compareWithCompareFunctionArguments;

static int compareWithCompareFunctionForQSort(const void *a, const void *b)
{
    CompareWithCompareFunctionArguments *args = compareWithCompareFunctionArguments;

    JSValue *va = *(JSValue **)a;
    JSValue *vb = *(JSValue **)b;
    if (va->isUndefined()) {
        return vb->isUndefined() ? 0 : 1;
    }
    if (vb->isUndefined()) {
        return -1;
    }

    args->arguments.clear();
    args->arguments.append(va);
    args->arguments.append(vb);
    double compareResult = args->compareFunction->call
        (args->exec, args->globalObject, args->arguments)->toNumber(args->exec);
    return compareResult < 0 ? -1 : compareResult > 0 ? 1 : 0;
}

void ArrayInstance::sort(ExecState *exec, JSObject *compareFunction)
{
    int lengthNotIncludingUndefined = pushUndefinedObjectsToEnd(exec);
    
    CompareWithCompareFunctionArguments args(exec, compareFunction);
    compareWithCompareFunctionArguments = &args;
    qsort(storage, lengthNotIncludingUndefined, sizeof(JSValue *), compareWithCompareFunctionForQSort);
    compareWithCompareFunctionArguments = 0;
}

unsigned ArrayInstance::pushUndefinedObjectsToEnd(ExecState *exec)
{
    JSValue *undefined = jsUndefined();

    unsigned o = 0;
    
    for (unsigned i = 0; i != storageLength; ++i) {
        JSValue *v = storage[i];
        if (v && v != undefined) {
            if (o != i)
                storage[o] = v;
            o++;
        }
    }
    
    ReferenceList sparseProperties;
    _prop.addSparseArrayPropertiesToReferenceList(sparseProperties, this);
    unsigned newLength = o + sparseProperties.length();

    if (newLength > storageLength) {
      resizeStorage(newLength);
    } 

    ReferenceListIterator it = sparseProperties.begin();
    while (it != sparseProperties.end()) {
      Reference ref = it++;
      storage[o] = ref.getValue(exec);
      JSObject::deleteProperty(exec, ref.getPropertyName(exec));
      o++;
    }
    
    if (newLength != storageLength)
        memset(storage + o, 0, sizeof(JSValue *) * (storageLength - o));
    
    return o;
}

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
  filter         ArrayProtoFunc::Filter         DontEnum|Function 1
  map            ArrayProtoFunc::Map            DontEnum|Function 1
@end
*/

// ECMA 15.4.4
ArrayPrototype::ArrayPrototype(ExecState*, ObjectPrototype* objProto)
  : ArrayInstance(objProto, 0)
{
  setInternalValue(jsNull());
}

bool ArrayPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticFunctionSlot<ArrayProtoFunc, ArrayInstance>(exec, &arrayTable, this, propertyName, slot);
}

// ------------------------------ ArrayProtoFunc ----------------------------

ArrayProtoFunc::ArrayProtoFunc(ExecState *exec, int i, int len, const Identifier& name)
  : InternalFunctionImp(static_cast<FunctionPrototype*>
                        (exec->lexicalInterpreter()->builtinFunctionPrototype()), name)
  , id(i)
{
  put(exec,lengthPropertyName,jsNumber(len),DontDelete|ReadOnly|DontEnum);
}

static JSValue *getProperty(ExecState *exec, JSObject *obj, unsigned index)
{
    PropertySlot slot;
    if (!obj->getPropertySlot(exec, index, slot))
        return NULL;
    return slot.getValue(exec, obj, index);
}

// ECMA 15.4.4
JSValue *ArrayProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  unsigned length = thisObj->get(exec,lengthPropertyName)->toUInt32(exec);

  JSValue *result = 0; // work around gcc 4.0 bug in uninitialized variable warning
  
  switch (id) {
  case ToLocaleString:
  case ToString:

    if (!thisObj->inherits(&ArrayInstance::info))
      return throwError(exec, TypeError);

    // fall through
  case Join: {
    static HashSet<JSObject*> visitedElems;
    if (visitedElems.contains(thisObj))
      return jsString("");
    UString separator = ",";
    UString str = "";

    visitedElems.add(thisObj);
    if (id == Join && !args[0]->isUndefined())
      separator = args[0]->toString(exec);
    for (unsigned int k = 0; k < length; k++) {
      if (k >= 1)
        str += separator;
      
      JSValue *element = thisObj->get(exec, k);
      if (element->isUndefinedOrNull())
        continue;

      bool fallback = false;
      if (id == ToLocaleString) {
        JSObject *o = element->toObject(exec);
        JSValue *conversionFunction = o->get(exec, toLocaleStringPropertyName);
        if (conversionFunction->isObject() && static_cast<JSObject *>(conversionFunction)->implementsCall()) {
          str += static_cast<JSObject *>(conversionFunction)->call(exec, o, List())->toString(exec);
        } else {
          // try toString() fallback
          fallback = true;
        }
      }

      if (id == ToString || id == Join || fallback) {
        if (element->isObject()) {
          JSObject *o = static_cast<JSObject *>(element);
          JSValue *conversionFunction = o->get(exec, toStringPropertyName);
          if (conversionFunction->isObject() && static_cast<JSObject *>(conversionFunction)->implementsCall()) {
            str += static_cast<JSObject *>(conversionFunction)->call(exec, o, List())->toString(exec);
          } else {
            visitedElems.remove(thisObj);
            return throwError(exec, RangeError, "Can't convert " + o->className() + " object to string");
          }
        } else {
          str += element->toString(exec);
        }
      }

      if ( exec->hadException() )
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
        length = curObj->get(exec,lengthPropertyName)->toUInt32(exec);
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
    arr->put(exec,lengthPropertyName, jsNumber(n), DontEnum | DontDelete);

    result = arr;
    break;
  }
  case Pop:{
    if (length == 0) {
      thisObj->put(exec, lengthPropertyName, jsNumber(length), DontEnum | DontDelete);
      result = jsUndefined();
    } else {
      result = thisObj->get(exec, length - 1);
      thisObj->put(exec, lengthPropertyName, jsNumber(length - 1), DontEnum | DontDelete);
    }
    break;
  }
  case Push: {
    for (int n = 0; n < args.size(); n++)
      thisObj->put(exec, length + n, args[n]);
    length += args.size();
    thisObj->put(exec,lengthPropertyName, jsNumber(length), DontEnum | DontDelete);
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
      thisObj->put(exec, lengthPropertyName, jsNumber(length), DontEnum | DontDelete);
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
      thisObj->put(exec, lengthPropertyName, jsNumber(length - 1), DontEnum | DontDelete);
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
    resObj->put(exec, lengthPropertyName, jsNumber(n), DontEnum | DontDelete);
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
      thisObj->put(exec, lengthPropertyName, jsNumber(0), DontEnum | DontDelete);
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
    resObj->put(exec, lengthPropertyName, jsNumber(deleteCount), DontEnum | DontDelete);

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
    thisObj->put(exec, lengthPropertyName, jsNumber(length - deleteCount + additionalArgs), DontEnum | DontDelete);
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
    thisObj->put(exec, lengthPropertyName, result, DontEnum | DontDelete);
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
    else {
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
      result = thisObj;
    
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
            e = jsUndefined();
        if (strictEqual(exec, searchElement, e))
            return jsNumber(index);
    }

    return jsNumber(-1);
  }

  default:
    assert(0);
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
  put(exec, prototypePropertyName, arrayProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec, lengthPropertyName, jsNumber(1), ReadOnly|DontDelete|DontEnum);
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
JSValue *ArrayObjectImp::callAsFunction(ExecState *exec, JSObject */*thisObj*/, const List &args)
{
  // equivalent to 'new Array(....)'
  return construct(exec,args);
}
