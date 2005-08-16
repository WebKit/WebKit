// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "array_object.h"

#include "error_object.h"
#include "internal.h"
#include "interpreter.h"
#include "object.h"
#include "operations.h"
#include "reference_list.h"
#include "types.h"
#include "value.h"

#include "array_object.lut.h"

#include <stdio.h>
#include <assert.h>

using namespace KJS;

// ------------------------------ ArrayInstanceImp -----------------------------

const unsigned sparseArrayCutoff = 10000;

const ClassInfo ArrayInstanceImp::info = {"Array", 0, 0, 0};

ArrayInstanceImp::ArrayInstanceImp(ObjectImp *proto, unsigned initialLength)
  : ObjectImp(proto)
  , length(initialLength)
  , storageLength(initialLength < sparseArrayCutoff ? initialLength : 0)
  , capacity(storageLength)
  , storage(capacity ? (ValueImp **)calloc(capacity, sizeof(ValueImp *)) : 0)
{
}

ArrayInstanceImp::ArrayInstanceImp(ObjectImp *proto, const List &list)
  : ObjectImp(proto)
  , length(list.size())
  , storageLength(length)
  , capacity(storageLength)
  , storage(capacity ? (ValueImp **)malloc(sizeof(ValueImp *) * capacity) : 0)
{
  ListIterator it = list.begin();
  unsigned l = length;
  for (unsigned i = 0; i < l; ++i) {
    storage[i] = it++;
  }
}

ArrayInstanceImp::~ArrayInstanceImp()
{
  free(storage);
}

ValueImp *ArrayInstanceImp::lengthGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
  return Number(static_cast<ArrayInstanceImp *>(slot.slotBase())->length);
}

bool ArrayInstanceImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
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
      ValueImp *v = storage[index];
      if (!v || v->isUndefined())
        return false;      
      slot.setValueSlot(this, &storage[index]);
      return true;
    }
  }

  return ObjectImp::getOwnPropertySlot(exec, propertyName, slot);
}

bool ArrayInstanceImp::getOwnPropertySlot(ExecState *exec, unsigned index, PropertySlot& slot)
{
  if (index >= length)
    return false;
  if (index < storageLength) {
    ValueImp *v = storage[index];
    if (!v || v->isUndefined())
      return false;
    slot.setValueSlot(this, &storage[index]);
    return true;
  }

  return ObjectImp::getOwnPropertySlot(exec, index, slot);
}

// Special implementation of [[Put]] - see ECMA 15.4.5.1
void ArrayInstanceImp::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
  if (propertyName == lengthPropertyName) {
    setLength(value->toUInt32(exec), exec);
    return;
  }
  
  bool ok;
  unsigned index = propertyName.toArrayIndex(&ok);
  if (ok) {
    put(exec, index, value, attr);
    return;
  }
  
  ObjectImp::put(exec, propertyName, value, attr);
}

void ArrayInstanceImp::put(ExecState *exec, unsigned index, ValueImp *value, int attr)
{
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
  ObjectImp::put(exec, Identifier::from(index), value, attr);
}

bool ArrayInstanceImp::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
  if (propertyName == lengthPropertyName)
    return false;
  
  bool ok;
  uint32_t index = propertyName.toUInt32(&ok);
  if (ok) {
    if (index >= length)
      return true;
    if (index < storageLength) {
      storage[index] = 0;
      return true;
    }
  }
  
  return ObjectImp::deleteProperty(exec, propertyName);
}

bool ArrayInstanceImp::deleteProperty(ExecState *exec, unsigned index)
{
  if (index >= length)
    return true;
  if (index < storageLength) {
    storage[index] = 0;
    return true;
  }
  
  return ObjectImp::deleteProperty(exec, Identifier::from(index));
}

ReferenceList ArrayInstanceImp::propList(ExecState *exec, bool recursive)
{
  ReferenceList properties = ObjectImp::propList(exec,recursive);

  // avoid fetching this every time through the loop
  ValueImp *undefined = jsUndefined();

  for (unsigned i = 0; i < storageLength; ++i) {
    ValueImp *imp = storage[i];
    if (imp && imp != undefined) {
      properties.append(Reference(this, i));
    }
  }
  return properties;
}

void ArrayInstanceImp::resizeStorage(unsigned newLength)
{
    if (newLength < storageLength) {
      memset(storage + newLength, 0, sizeof(ValueImp *) * (storageLength - newLength));
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
      storage = (ValueImp **)realloc(storage, newCapacity * sizeof (ValueImp *));
      memset(storage + capacity, 0, sizeof(ValueImp *) * (newCapacity - capacity));
      capacity = newCapacity;
    }
    storageLength = newLength;
}

void ArrayInstanceImp::setLength(unsigned newLength, ExecState *exec)
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

void ArrayInstanceImp::mark()
{
  ObjectImp::mark();
  unsigned l = storageLength;
  for (unsigned i = 0; i < l; ++i) {
    ValueImp *imp = storage[i];
    if (imp && !imp->marked())
      imp->mark();
  }
}

static ExecState *execForCompareByStringForQSort;

static int compareByStringForQSort(const void *a, const void *b)
{
    ExecState *exec = execForCompareByStringForQSort;
    ValueImp *va = *(ValueImp **)a;
    ValueImp *vb = *(ValueImp **)b;
    if (va->isUndefined()) {
        return vb->isUndefined() ? 0 : 1;
    }
    if (vb->isUndefined()) {
        return -1;
    }
    return compare(va->toString(exec), vb->toString(exec));
}

void ArrayInstanceImp::sort(ExecState *exec)
{
    int lengthNotIncludingUndefined = pushUndefinedObjectsToEnd(exec);
    
    execForCompareByStringForQSort = exec;
    qsort(storage, lengthNotIncludingUndefined, sizeof(ValueImp *), compareByStringForQSort);
    execForCompareByStringForQSort = 0;
}

struct CompareWithCompareFunctionArguments {
    CompareWithCompareFunctionArguments(ExecState *e, ObjectImp *cf)
        : exec(e)
        , compareFunction(cf)
        , globalObject(e->dynamicInterpreter()->globalObject())
    {
        arguments.append(Undefined());
        arguments.append(Undefined());
    }

    ExecState *exec;
    ObjectImp *compareFunction;
    List arguments;
    ObjectImp *globalObject;
};

static CompareWithCompareFunctionArguments *compareWithCompareFunctionArguments;

static int compareWithCompareFunctionForQSort(const void *a, const void *b)
{
    CompareWithCompareFunctionArguments *args = compareWithCompareFunctionArguments;

    ValueImp *va = *(ValueImp **)a;
    ValueImp *vb = *(ValueImp **)b;
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

void ArrayInstanceImp::sort(ExecState *exec, ObjectImp *compareFunction)
{
    int lengthNotIncludingUndefined = pushUndefinedObjectsToEnd(exec);
    
    CompareWithCompareFunctionArguments args(exec, compareFunction);
    compareWithCompareFunctionArguments = &args;
    qsort(storage, lengthNotIncludingUndefined, sizeof(ValueImp *), compareWithCompareFunctionForQSort);
    compareWithCompareFunctionArguments = 0;
}

unsigned ArrayInstanceImp::pushUndefinedObjectsToEnd(ExecState *exec)
{
    ValueImp *undefined = jsUndefined();

    unsigned o = 0;
    
    for (unsigned i = 0; i != storageLength; ++i) {
        ValueImp *v = storage[i];
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
      ObjectImp::deleteProperty(exec, ref.getPropertyName(exec));
      o++;
    }
    
    if (newLength != storageLength)
        memset(storage + o, 0, sizeof(ValueImp *) * (storageLength - o));
    
    return o;
}

// ------------------------------ ArrayPrototypeImp ----------------------------

const ClassInfo ArrayPrototypeImp::info = {"Array", &ArrayInstanceImp::info, &arrayTable, 0};

/* Source for array_object.lut.h
@begin arrayTable 16
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
  every          ArrayProtoFuncImp::Every          DontEnum|Function 5
  forEach        ArrayProtoFuncImp::ForEach        DontEnum|Function 5
  some           ArrayProtoFuncImp::Some           DontEnum|Function 5
@end
*/

// ECMA 15.4.4
ArrayPrototypeImp::ArrayPrototypeImp(ExecState *exec,
                                     ObjectPrototypeImp *objProto)
  : ArrayInstanceImp(objProto, 0)
{
  setInternalValue(Null());
}

bool ArrayPrototypeImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticFunctionSlot<ArrayProtoFuncImp, ArrayInstanceImp>(exec, &arrayTable, this, propertyName, slot);
}

// ------------------------------ ArrayProtoFuncImp ----------------------------

ArrayProtoFuncImp::ArrayProtoFuncImp(ExecState *exec, int i, int len)
  : InternalFunctionImp(
    static_cast<FunctionPrototypeImp*>(exec->lexicalInterpreter()->builtinFunctionPrototype())
    ), id(i)
{
  put(exec,lengthPropertyName,Number(len),DontDelete|ReadOnly|DontEnum);
}

bool ArrayProtoFuncImp::implementsCall() const
{
  return true;
}

static ValueImp *getProperty(ExecState *exec, ObjectImp *obj, unsigned index)
{
    PropertySlot slot;
    if (!obj->getPropertySlot(exec, index, slot))
        return NULL;
    return slot.getValue(exec, index);
}

// ECMA 15.4.4
ValueImp *ArrayProtoFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  unsigned length = thisObj->get(exec,lengthPropertyName)->toUInt32(exec);

  ValueImp *result = 0; // work around gcc 4.0 bug in uninitialized variable warning
  
  switch (id) {
  case ToLocaleString:
  case ToString:

    if (!thisObj->inherits(&ArrayInstanceImp::info))
      return throwError(exec, TypeError);

    // fall through
  case Join: {
    UString separator = ",";
    UString str = "";

    if (!args[0]->isUndefined())
      separator = args[0]->toString(exec);
    for (unsigned int k = 0; k < length; k++) {
      if (k >= 1)
        str += separator;
      
      ValueImp *element = thisObj->get(exec, k);
      if (element->isUndefinedOrNull())
        continue;

      bool fallback = false;
      if (id == ToLocaleString) {
        ObjectImp *o = element->toObject(exec);
        ValueImp *conversionFunction = o->get(exec, toLocaleStringPropertyName);
        if (conversionFunction->isObject() && static_cast<ObjectImp *>(conversionFunction)->implementsCall()) {
          str += static_cast<ObjectImp *>(conversionFunction)->call(exec, o, List())->toString(exec);
        } else {
          // try toString() fallback
          fallback = true;
        }
      }

      if (id == ToString || id == Join || fallback) {
        if (element->isObject()) {
          ObjectImp *o = static_cast<ObjectImp *>(element);
          ValueImp *conversionFunction = o->get(exec, toStringPropertyName);
          if (conversionFunction->isObject() && static_cast<ObjectImp *>(conversionFunction)->implementsCall()) {
            str += static_cast<ObjectImp *>(conversionFunction)->call(exec, o, List())->toString(exec);
          } else {
            return throwError(exec, RangeError, "Can't convert " + o->className() + " object to string");
          }
        } else {
          str += element->toString(exec);
        }
      }

      if ( exec->hadException() )
        break;
    }
    result = String(str);
    break;
  }
  case Concat: {
    ObjectImp *arr = static_cast<ObjectImp *>(exec->lexicalInterpreter()->builtinArray()->construct(exec,List::empty()));
    int n = 0;
    ValueImp *curArg = thisObj;
    ObjectImp *curObj = static_cast<ObjectImp *>(thisObj);
    ListIterator it = args.begin();
    for (;;) {
      if (curArg->isObject() &&
          curObj->inherits(&ArrayInstanceImp::info)) {
        unsigned int k = 0;
        // Older versions tried to optimize out getting the length of thisObj
        // by checking for n != 0, but that doesn't work if thisObj is an empty array.
        length = curObj->get(exec,lengthPropertyName)->toUInt32(exec);
        while (k < length) {
          if (ValueImp *v = getProperty(exec, curObj, k))
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
      curObj = static_cast<ObjectImp *>(it++); // may be 0
    }
    arr->put(exec,lengthPropertyName, Number(n), DontEnum | DontDelete);

    result = arr;
    break;
  }
  case Pop:{
    if (length == 0) {
      thisObj->put(exec, lengthPropertyName, Number(length), DontEnum | DontDelete);
      result = Undefined();
    } else {
      result = thisObj->get(exec, length - 1);
      thisObj->put(exec, lengthPropertyName, Number(length - 1), DontEnum | DontDelete);
    }
    break;
  }
  case Push: {
    for (int n = 0; n < args.size(); n++)
      thisObj->put(exec, length + n, args[n]);
    length += args.size();
    thisObj->put(exec,lengthPropertyName, Number(length), DontEnum | DontDelete);
    result = Number(length);
    break;
  }
  case Reverse: {

    unsigned int middle = length / 2;

    for (unsigned int k = 0; k < middle; k++) {
      unsigned lk1 = length - k - 1;
      ValueImp *obj2 = getProperty(exec, thisObj, lk1);
      ValueImp *obj = getProperty(exec, thisObj, k);

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
      thisObj->put(exec, lengthPropertyName, Number(length), DontEnum | DontDelete);
      result = Undefined();
    } else {
      result = thisObj->get(exec, 0);
      for(unsigned int k = 1; k < length; k++) {
        if (ValueImp *obj = getProperty(exec, thisObj, k))
          thisObj->put(exec, k-1, obj);
        else
          thisObj->deleteProperty(exec, k-1);
      }
      thisObj->deleteProperty(exec, length - 1);
      thisObj->put(exec, lengthPropertyName, Number(length - 1), DontEnum | DontDelete);
    }
    break;
  }
  case Slice: {
    // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713 or 15.4.4.10

    // We return a new array
    ObjectImp *resObj = static_cast<ObjectImp *>(exec->lexicalInterpreter()->builtinArray()->construct(exec,List::empty()));
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
      if (ValueImp *v = getProperty(exec, thisObj, k))
        resObj->put(exec, n, v);
    }
    resObj->put(exec, lengthPropertyName, Number(n), DontEnum | DontDelete);
    break;
  }
  case Sort:{
#if 0
    printf("KJS Array::Sort length=%d\n", length);
    for ( unsigned int i = 0 ; i<length ; ++i )
      printf("KJS Array::Sort: %d: %s\n", i, thisObj->get(exec, i)->toString(exec).ascii() );
#endif
    ObjectImp *sortFunction = NULL;
    if (!args[0]->isUndefined())
      {
        sortFunction = args[0]->toObject(exec);
        if (!sortFunction->implementsCall())
          sortFunction = NULL;
      }
    
    if (thisObj->classInfo() == &ArrayInstanceImp::info) {
      if (sortFunction)
        ((ArrayInstanceImp *)thisObj)->sort(exec, sortFunction);
      else
        ((ArrayInstanceImp *)thisObj)->sort(exec);
      result = thisObj;
      break;
    }

    if (length == 0) {
      thisObj->put(exec, lengthPropertyName, Number(0), DontEnum | DontDelete);
      result = thisObj;
      break;
    }

    // "Min" sort. Not the fastest, but definitely less code than heapsort
    // or quicksort, and much less swapping than bubblesort/insertionsort.
    for ( unsigned int i = 0 ; i<length-1 ; ++i )
      {
        ValueImp *iObj = thisObj->get(exec,i);
        unsigned int themin = i;
        ValueImp *minObj = iObj;
        for ( unsigned int j = i+1 ; j<length ; ++j )
          {
            ValueImp *jObj = thisObj->get(exec,j);
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
    ObjectImp *resObj = static_cast<ObjectImp *>(exec->lexicalInterpreter()->builtinArray()->construct(exec,List::empty()));
    result = resObj;
    int begin = args[0]->toUInt32(exec);
    if ( begin < 0 )
      begin = maxInt( begin + length, 0 );
    else
      begin = minInt( begin, length );
    unsigned int deleteCount = minInt( maxInt( args[1]->toUInt32(exec), 0 ), length - begin );

    //printf( "Splicing from %d, deleteCount=%d \n", begin, deleteCount );
    for(unsigned int k = 0; k < deleteCount; k++) {
      if (ValueImp *v = getProperty(exec, thisObj, k+begin))
        resObj->put(exec, k, v);
    }
    resObj->put(exec, lengthPropertyName, Number(deleteCount), DontEnum | DontDelete);

    unsigned int additionalArgs = maxInt( args.size() - 2, 0 );
    if ( additionalArgs != deleteCount )
    {
      if ( additionalArgs < deleteCount )
      {
        for ( unsigned int k = begin; k < length - deleteCount; ++k )
        {
          if (ValueImp *v = getProperty(exec, thisObj, k+deleteCount))
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
          if (ValueImp *obj = getProperty(exec, thisObj, k + deleteCount - 1))
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
    thisObj->put(exec, lengthPropertyName, Number(length - deleteCount + additionalArgs), DontEnum | DontDelete);
    break;
  }
  case UnShift: { // 15.4.4.13
    unsigned int nrArgs = args.size();
    for ( unsigned int k = length; k > 0; --k )
    {
      if (ValueImp *v = getProperty(exec, thisObj, k - 1))
        thisObj->put(exec, k+nrArgs-1, v);
      else
        thisObj->deleteProperty(exec, k+nrArgs-1);
    }
    for ( unsigned int k = 0; k < nrArgs; ++k )
      thisObj->put(exec, k, args[k]);
    result = Number(length + nrArgs);
    thisObj->put(exec, lengthPropertyName, result, DontEnum | DontDelete);
    break;
  }
  case Every:
  case ForEach:
  case Some: {
    //Documentation for these three is available at:
    //http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:every
    //http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:forEach
    //http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:some
    
    ObjectImp *eachFunction = args[0]->toObject(exec);
    
    if (!eachFunction->implementsCall())
      return throwError(exec, TypeError);
    
    ObjectImp *applyThis = args[1]->isUndefinedOrNull() ? exec->dynamicInterpreter()->globalObject() :  args[1]->toObject(exec);
    
    if (id == Some || id == Every)
      result = Boolean(id == Every);
    else
      result = thisObj;
    
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
      
      List eachArguments;
      
      eachArguments.append(thisObj->get(exec, k));
      eachArguments.append(Number(k));
      eachArguments.append(thisObj);
      
      bool predicateResult = eachFunction->call(exec, applyThis, eachArguments)->toBoolean(exec);
      
      if (id == Every && !predicateResult) {
        result = Boolean(false);
        break;
      }
      if (id == Some && predicateResult) {
        result = Boolean(true);
        break;
      }
    }
    break;
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
                               FunctionPrototypeImp *funcProto,
                               ArrayPrototypeImp *arrayProto)
  : InternalFunctionImp(funcProto)
{
  // ECMA 15.4.3.1 Array.prototype
  put(exec,prototypePropertyName, arrayProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec,lengthPropertyName, Number(1), ReadOnly|DontDelete|DontEnum);
}

bool ArrayObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.4.2
ObjectImp *ArrayObjectImp::construct(ExecState *exec, const List &args)
{
  // a single numeric argument denotes the array size (!)
  if (args.size() == 1 && args[0]->isNumber()) {
    uint32_t n = args[0]->toUInt32(exec);
    if (n != args[0]->toNumber(exec))
      return throwError(exec, RangeError, "Array size is not a small enough positive integer.");
    return new ArrayInstanceImp(exec->lexicalInterpreter()->builtinArrayPrototype(), n);
  }

  // otherwise the array is constructed with the arguments in it
  return new ArrayInstanceImp(exec->lexicalInterpreter()->builtinArrayPrototype(), args);
}

bool ArrayObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.6.1
ValueImp *ArrayObjectImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  // equivalent to 'new Array(....)'
  return construct(exec,args);
}
