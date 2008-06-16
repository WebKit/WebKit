/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
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
#include "ArrayPrototype.h"
#include "ArrayPrototype.lut.h"

#include "Machine.h"
#include "error_object.h"
#include "lookup.h"
#include "operations.h"
#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

#include <algorithm> // for std::min

namespace KJS {

// ------------------------------ ArrayPrototype ----------------------------

const ClassInfo ArrayPrototype::info = {"Array", &JSArray::info, 0, ExecState::arrayTable};

/* Source for ArrayPrototype.lut.h
@begin arrayTable 16
  toString       arrayProtoFuncToString       DontEnum|Function 0
  toLocaleString arrayProtoFuncToLocaleString DontEnum|Function 0
  concat         arrayProtoFuncConcat         DontEnum|Function 1
  join           arrayProtoFuncJoin           DontEnum|Function 1
  pop            arrayProtoFuncPop            DontEnum|Function 0
  push           arrayProtoFuncPush           DontEnum|Function 1
  reverse        arrayProtoFuncReverse        DontEnum|Function 0
  shift          arrayProtoFuncShift          DontEnum|Function 0
  slice          arrayProtoFuncSlice          DontEnum|Function 2
  sort           arrayProtoFuncSort           DontEnum|Function 1
  splice         arrayProtoFuncSplice         DontEnum|Function 2
  unshift        arrayProtoFuncUnShift        DontEnum|Function 1
  every          arrayProtoFuncEvery          DontEnum|Function 1
  forEach        arrayProtoFuncForEach        DontEnum|Function 1
  some           arrayProtoFuncSome           DontEnum|Function 1
  indexOf        arrayProtoFuncIndexOf        DontEnum|Function 1
  lastIndexOf    arrayProtoFuncLastIndexOf    DontEnum|Function 1
  filter         arrayProtoFuncFilter         DontEnum|Function 1
  map            arrayProtoFuncMap            DontEnum|Function 1
@end
*/

// ECMA 15.4.4
ArrayPrototype::ArrayPrototype(ExecState*, ObjectPrototype* objProto)
    : JSArray(objProto, 0)
{
}

bool ArrayPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSArray>(exec, ExecState::arrayTable(exec), this, propertyName, slot);
}


// ------------------------------ Array Functions ----------------------------

// Helper function
static JSValue* getProperty(ExecState* exec, JSObject* obj, unsigned index)
{
    PropertySlot slot(obj);
    if (!obj->getPropertySlot(exec, index, slot))
        return 0;
    return slot.getValue(exec, index);
}

JSValue* arrayProtoFuncToString(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&JSArray::info))
        return throwError(exec, TypeError);

    HashSet<JSObject*>& arrayVisitedElements = exec->dynamicGlobalObject()->arrayVisitedElements();
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    Vector<UChar, 256> strBuffer;
    if (alreadyVisited)
        return jsString(UString(0, 0)); // return an empty string, avoding infinite recursion.

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(',');
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue* element = thisObj->get(exec, k);
        if (element->isUndefinedOrNull())
            continue;

        UString str = element->toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    exec->dynamicGlobalObject()->arrayVisitedElements().remove(thisObj);
    return jsString(UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValue* arrayProtoFuncToLocaleString(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&JSArray::info))
        return throwError(exec, TypeError);

    HashSet<JSObject*>& arrayVisitedElements = exec->dynamicGlobalObject()->arrayVisitedElements();
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    Vector<UChar, 256> strBuffer;
    if (alreadyVisited)
        return jsString(UString(0, 0)); // return an empty string, avoding infinite recursion.

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(',');
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue* element = thisObj->get(exec, k);
        if (element->isUndefinedOrNull())
            continue;

        JSObject* o = element->toObject(exec);
        JSValue* conversionFunction = o->get(exec, exec->propertyNames().toLocaleString);
        UString str;
        if (conversionFunction->isObject() && static_cast<JSObject*>(conversionFunction)->implementsCall())
            str = static_cast<JSObject*>(conversionFunction)->callAsFunction(exec, o, exec->emptyList())->toString(exec);
        else
            str = element->toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    exec->dynamicGlobalObject()->arrayVisitedElements().remove(thisObj);
    return jsString(UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValue* arrayProtoFuncJoin(ExecState* exec, JSObject* thisObj, const List& args)
{
    HashSet<JSObject*>& arrayVisitedElements = exec->dynamicGlobalObject()->arrayVisitedElements();
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    Vector<UChar, 256> strBuffer;
    if (alreadyVisited)
        return jsString(UString(0, 0)); // return an empty string, avoding infinite recursion.

    UChar comma = ',';
    UString separator = args[0]->isUndefined() ? UString(&comma, 1) : args[0]->toString(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(separator.data(), separator.size());
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue* element = thisObj->get(exec, k);
        if (element->isUndefinedOrNull())
            continue;

        UString str = element->toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    exec->dynamicGlobalObject()->arrayVisitedElements().remove(thisObj);
    return jsString(UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValue* arrayProtoFuncConcat(ExecState* exec, JSObject* thisObj, const List& args)
{
    JSObject* arr = static_cast<JSObject*>(exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, exec->emptyList()));
    int n = 0;
    JSValue* curArg = thisObj;
    JSObject* curObj = static_cast<JSObject* >(thisObj);
    List::const_iterator it = args.begin();
    List::const_iterator end = args.end();
    while (1) {
        if (curArg->isObject() && curObj->inherits(&JSArray::info)) {
            unsigned k = 0;
            // Older versions tried to optimize out getting the length of thisObj
            // by checking for n != 0, but that doesn't work if thisObj is an empty array.
            unsigned length = curObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
            while (k < length) {
                if (JSValue* v = getProperty(exec, curObj, k))
                    arr->put(exec, n, v);
                n++;
                k++;
            }
        } else {
            arr->put(exec, n, curArg);
            n++;
        }
        if (it == end)
            break;
        curArg = *it;
        curObj = static_cast<JSObject*>(curArg); // may be 0
        ++it;
    }
    arr->put(exec, exec->propertyNames().length, jsNumber(n));
    return arr;
}

JSValue* arrayProtoFuncPop(ExecState* exec, JSObject* thisObj, const List&)
{
    JSValue* result = 0;
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (length == 0) {
        thisObj->put(exec, exec->propertyNames().length, jsNumber(length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, length - 1);
        thisObj->deleteProperty(exec, length - 1);
        thisObj->put(exec, exec->propertyNames().length, jsNumber(length - 1));
    }
    return result;
}

JSValue* arrayProtoFuncPush(ExecState* exec, JSObject* thisObj, const List& args)
{
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned n = 0; n < args.size(); n++)
        thisObj->put(exec, length + n, args[n]);
    length += args.size();
    thisObj->put(exec, exec->propertyNames().length, jsNumber(length));
    return jsNumber(length);
}

JSValue* arrayProtoFuncReverse(ExecState* exec, JSObject* thisObj, const List&)
{
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    unsigned middle = length / 2;

    for (unsigned k = 0; k < middle; k++) {
        unsigned lk1 = length - k - 1;
        JSValue* obj2 = getProperty(exec, thisObj, lk1);
        JSValue* obj = getProperty(exec, thisObj, k);

        if (obj2)
            thisObj->put(exec, k, obj2);
        else
            thisObj->deleteProperty(exec, k);

        if (obj)
            thisObj->put(exec, lk1, obj);
        else
            thisObj->deleteProperty(exec, lk1);
    }
    return thisObj;
}

JSValue* arrayProtoFuncShift(ExecState* exec, JSObject* thisObj, const List&)
{
    JSValue* result = 0;

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (length == 0) {
        thisObj->put(exec, exec->propertyNames().length, jsNumber(length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, 0);
        for (unsigned k = 1; k < length; k++) {
            if (JSValue* obj = getProperty(exec, thisObj, k))
                thisObj->put(exec, k - 1, obj);
            else
                thisObj->deleteProperty(exec, k - 1);
        }
        thisObj->deleteProperty(exec, length - 1);
        thisObj->put(exec, exec->propertyNames().length, jsNumber(length - 1));
    }
    return result;
}

JSValue* arrayProtoFuncSlice(ExecState* exec, JSObject* thisObj, const List& args)
{
    // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713 or 15.4.4.10

    // We return a new array
    JSObject* resObj = static_cast<JSObject* >(exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, exec->emptyList()));
    JSValue* result = resObj;
    double begin = args[0]->toInteger(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (begin >= 0) {
        if (begin > length)
            begin = length;
    } else {
        begin += length;
        if (begin < 0)
            begin = 0;
    }
    double end;
    if (args[1]->isUndefined())
        end = length;
    else {
        end = args[1]->toInteger(exec);
        if (end < 0) {
            end += length;
            if (end < 0)
                end = 0;
        } else {
            if (end > length)
                end = length;
        }
    }

    int n = 0;
    int b = static_cast<int>(begin);
    int e = static_cast<int>(end);
    for (int k = b; k < e; k++, n++) {
        if (JSValue* v = getProperty(exec, thisObj, k))
            resObj->put(exec, n, v);
    }
    resObj->put(exec, exec->propertyNames().length, jsNumber(n));
    return result;
}

JSValue* arrayProtoFuncSort(ExecState* exec, JSObject* thisObj, const List& args)
{
    JSObject* sortFunction = 0;
    if (!args[0]->isUndefined()) {
        sortFunction = args[0]->toObject(exec);
        if (!sortFunction->implementsCall())
            sortFunction = 0;
    }

    if (thisObj->classInfo() == &JSArray::info) {
        if (sortFunction)
            static_cast<JSArray*>(thisObj)->sort(exec, sortFunction);
        else
            static_cast<JSArray*>(thisObj)->sort(exec);
        return thisObj;
    }

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);

    if (!length)
        return thisObj;

    // "Min" sort. Not the fastest, but definitely less code than heapsort
    // or quicksort, and much less swapping than bubblesort/insertionsort.
    for (unsigned i = 0; i < length - 1; ++i) {
        JSValue* iObj = thisObj->get(exec, i);
        unsigned themin = i;
        JSValue* minObj = iObj;
        for (unsigned j = i + 1; j < length; ++j) {
            JSValue* jObj = thisObj->get(exec, j);
            double compareResult;
            if (jObj->isUndefined())
                compareResult = 1; // don't check minObj because there's no need to differentiate == (0) from > (1)
            else if (minObj->isUndefined())
                compareResult = -1;
            else if (sortFunction) {
                List l;
                l.append(jObj);
                l.append(minObj);
                compareResult = sortFunction->callAsFunction(exec, exec->globalThisValue(), l)->toNumber(exec);
            } else
                compareResult = (jObj->toString(exec) < minObj->toString(exec)) ? -1 : 1;

            if (compareResult < 0) {
                themin = j;
                minObj = jObj;
            }
        }
        // Swap themin and i
        if (themin > i) {
            thisObj->put(exec, i, minObj);
            thisObj->put(exec, themin, iObj);
        }
    }
    return thisObj;
}

JSValue* arrayProtoFuncSplice(ExecState* exec, JSObject* thisObj, const List& args)
{
    // 15.4.4.12
    JSObject* resObj = static_cast<JSObject* >(exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, exec->emptyList()));
    JSValue* result = resObj;
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (!args.size())
        return jsUndefined();
    int begin = args[0]->toUInt32(exec);
    if (begin < 0)
        begin = std::max<int>(begin + length, 0);
    else
        begin = std::min<int>(begin, length);

    unsigned deleteCount;
    if (args.size() > 1)
        deleteCount = std::min<int>(std::max<int>(args[1]->toUInt32(exec), 0), length - begin);
    else
        deleteCount = length - begin;

    for (unsigned k = 0; k < deleteCount; k++) {
        if (JSValue* v = getProperty(exec, thisObj, k + begin))
            resObj->put(exec, k, v);
    }
    resObj->put(exec, exec->propertyNames().length, jsNumber(deleteCount));

    unsigned additionalArgs = std::max<int>(args.size() - 2, 0);
    if (additionalArgs != deleteCount) {
        if (additionalArgs < deleteCount) {
            for (unsigned k = begin; k < length - deleteCount; ++k) {
                if (JSValue* v = getProperty(exec, thisObj, k + deleteCount))
                    thisObj->put(exec, k + additionalArgs, v);
                else
                    thisObj->deleteProperty(exec, k + additionalArgs);
            }
            for (unsigned k = length; k > length - deleteCount + additionalArgs; --k)
                thisObj->deleteProperty(exec, k - 1);
        } else {
            for (unsigned k = length - deleteCount; (int)k > begin; --k) {
                if (JSValue* obj = getProperty(exec, thisObj, k + deleteCount - 1))
                    thisObj->put(exec, k + additionalArgs - 1, obj);
                else
                    thisObj->deleteProperty(exec, k + additionalArgs - 1);
            }
        }
    }
    for (unsigned k = 0; k < additionalArgs; ++k)
        thisObj->put(exec, k + begin, args[k + 2]);

    thisObj->put(exec, exec->propertyNames().length, jsNumber(length - deleteCount + additionalArgs));
    return result;
}

JSValue* arrayProtoFuncUnShift(ExecState* exec, JSObject* thisObj, const List& args)
{
    // 15.4.4.13
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    unsigned nrArgs = args.size();
    if (nrArgs) {
        for (unsigned k = length; k > 0; --k) {
            if (JSValue* v = getProperty(exec, thisObj, k - 1))
                thisObj->put(exec, k + nrArgs - 1, v);
            else
                thisObj->deleteProperty(exec, k + nrArgs - 1);
        }
    }
    for (unsigned k = 0; k < nrArgs; ++k)
        thisObj->put(exec, k, args[k]);
    JSValue* result = jsNumber(length + nrArgs);
    thisObj->put(exec, exec->propertyNames().length, result);
    return result;
}

JSValue* arrayProtoFuncFilter(ExecState* exec, JSObject* thisObj, const List& args)
{
    JSObject* eachFunction = args[0]->toObject(exec);

    if (!eachFunction->implementsCall())
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() :  args[1]->toObject(exec);
    JSObject* resultArray = static_cast<JSObject*>(exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, exec->emptyList()));

    unsigned filterIndex = 0;
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);

        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        JSValue* v = slot.getValue(exec, k);

        List eachArguments;

        eachArguments.append(v);
        eachArguments.append(jsNumber(k));
        eachArguments.append(thisObj);

        JSValue* result = eachFunction->callAsFunction(exec, applyThis, eachArguments);

        if (result->toBoolean(exec))
            resultArray->put(exec, filterIndex++, v);
    }
    return resultArray;
}

JSValue* arrayProtoFuncMap(ExecState* exec, JSObject* thisObj, const List& args)
{
    JSObject* eachFunction = args[0]->toObject(exec);
    if (!eachFunction->implementsCall())
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() :  args[1]->toObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);

    List mapArgs;
    mapArgs.append(jsNumber(length));
    JSObject* resultArray = static_cast<JSObject*>(exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, mapArgs));

    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        JSValue* v = slot.getValue(exec, k);

        List eachArguments;

        eachArguments.append(v);
        eachArguments.append(jsNumber(k));
        eachArguments.append(thisObj);

        JSValue* result = eachFunction->callAsFunction(exec, applyThis, eachArguments);
        resultArray->put(exec, k, result);
    }

    return resultArray;
}

// Documentation for these three is available at:
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:every
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:forEach
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:some

JSValue* arrayProtoFuncEvery(ExecState* exec, JSObject* thisObj, const List& args)
{
    JSObject* eachFunction = args[0]->toObject(exec);

    if (!eachFunction->implementsCall())
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() :  args[1]->toObject(exec);

    JSValue* result = jsBoolean(true);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);

        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        List eachArguments;

        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(k));
        eachArguments.append(thisObj);

        bool predicateResult = eachFunction->callAsFunction(exec, applyThis, eachArguments)->toBoolean(exec);

        if (!predicateResult) {
            result = jsBoolean(false);
            break;
        }
    }

    return result;
}

JSValue* arrayProtoFuncForEach(ExecState* exec, JSObject* thisObj, const List& args)
{
    JSObject* eachFunction = args[0]->toObject(exec);

    if (!eachFunction->implementsCall())
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() :  args[1]->toObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        List eachArguments;
        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(k));
        eachArguments.append(thisObj);

        eachFunction->callAsFunction(exec, applyThis, eachArguments);
    }
    return jsUndefined();
}

JSValue* arrayProtoFuncSome(ExecState* exec, JSObject* thisObj, const List& args)
{
    JSObject* eachFunction = args[0]->toObject(exec);

    if (!eachFunction->implementsCall())
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() :  args[1]->toObject(exec);

    JSValue* result = jsBoolean(false);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        List eachArguments;
        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(k));
        eachArguments.append(thisObj);

        bool predicateResult = eachFunction->callAsFunction(exec, applyThis, eachArguments)->toBoolean(exec);

        if (predicateResult) {
            result = jsBoolean(true);
            break;
        }
    }
    return result;
}

JSValue* arrayProtoFuncIndexOf(ExecState* exec, JSObject* thisObj, const List& args)
{
    // JavaScript 1.5 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:indexOf

    unsigned index = 0;
    double d = args[1]->toInteger(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
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
        if (strictEqual(searchElement, e))
            return jsNumber(index);
    }

    return jsNumber(-1);
}

JSValue* arrayProtoFuncLastIndexOf(ExecState* exec, JSObject* thisObj, const List& args)
{
    // JavaScript 1.6 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:lastIndexOf

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    int index = length - 1;
    double d = args[1]->toIntegerPreserveNaN(exec);

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
        if (strictEqual(searchElement, e))
            return jsNumber(index);
    }

    return jsNumber(-1);
}

// ------------------------------ ArrayConstructor -------------------------------

ArrayConstructor::ArrayConstructor(ExecState* exec, FunctionPrototype* funcProto, ArrayPrototype* arrayProto)
    : InternalFunction(funcProto, arrayProto->classInfo()->className)
{
    // ECMA 15.4.3.1 Array.prototype
    putDirect(exec->propertyNames().prototype, arrayProto, DontEnum|DontDelete|ReadOnly);

    // no. of arguments for constructor
    putDirect(exec->propertyNames().length, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}

ConstructType ArrayConstructor::getConstructData(ConstructData&)
{
    return ConstructTypeNative;
}

// ECMA 15.4.2
JSObject* ArrayConstructor::construct(ExecState* exec, const List& args)
{
    // a single numeric argument denotes the array size (!)
    if (args.size() == 1 && args[0]->isNumber()) {
        uint32_t n = args[0]->toUInt32(exec);
        if (n != args[0]->toNumber(exec))
            return throwError(exec, RangeError, "Array size is not a small enough positive integer.");
        return new JSArray(exec->lexicalGlobalObject()->arrayPrototype(), n);
    }

    // otherwise the array is constructed with the arguments in it
    return new JSArray(exec->lexicalGlobalObject()->arrayPrototype(), args);
}

// ECMA 15.6.1
JSValue* ArrayConstructor::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    // equivalent to 'new Array(....)'
    return construct(exec,args);
}

}
