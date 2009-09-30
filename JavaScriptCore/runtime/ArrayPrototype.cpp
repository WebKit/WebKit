/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "CachedCall.h"
#include "Interpreter.h"
#include "JIT.h"
#include "ObjectPrototype.h"
#include "Lookup.h"
#include "Operations.h"
#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ArrayPrototype);

static JSValue JSC_HOST_CALL arrayProtoFuncToString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncToLocaleString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncConcat(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncJoin(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncPop(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncPush(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncReverse(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncShift(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncSlice(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncSort(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncSplice(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncUnShift(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncEvery(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncForEach(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncSome(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncIndexOf(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncFilter(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncMap(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncReduce(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncReduceRight(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL arrayProtoFuncLastIndexOf(ExecState*, JSObject*, JSValue, const ArgList&);

}

#include "ArrayPrototype.lut.h"

namespace JSC {

static inline bool isNumericCompareFunction(ExecState* exec, CallType callType, const CallData& callData)
{
    if (callType != CallTypeJS)
        return false;

#if ENABLE(JIT)
    // If the JIT is enabled then we need to preserve the invariant that every
    // function with a CodeBlock also has JIT code.
    callData.js.functionExecutable->jitCode(exec, callData.js.scopeChain);
    CodeBlock& codeBlock = callData.js.functionExecutable->generatedBytecode();
#else
    CodeBlock& codeBlock = callData.js.functionExecutable->bytecode(exec, callData.js.scopeChain);
#endif

    return codeBlock.isNumericCompareFunction();
}

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
  reduce         arrayProtoFuncReduce         DontEnum|Function 1
  reduceRight    arrayProtoFuncReduceRight    DontEnum|Function 1
  map            arrayProtoFuncMap            DontEnum|Function 1
@end
*/

// ECMA 15.4.4
ArrayPrototype::ArrayPrototype(NonNullPassRefPtr<Structure> structure)
    : JSArray(structure)
{
}

bool ArrayPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSArray>(exec, ExecState::arrayTable(exec), this, propertyName, slot);
}

bool ArrayPrototype::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticFunctionDescriptor<JSArray>(exec, ExecState::arrayTable(exec), this, propertyName, descriptor);
}

// ------------------------------ Array Functions ----------------------------

// Helper function
static JSValue getProperty(ExecState* exec, JSObject* obj, unsigned index)
{
    PropertySlot slot(obj);
    if (!obj->getPropertySlot(exec, index, slot))
        return JSValue();
    return slot.getValue(exec, index);
}

static void putProperty(ExecState* exec, JSObject* obj, const Identifier& propertyName, JSValue value)
{
    PutPropertySlot slot;
    obj->put(exec, propertyName, value, slot);
}

JSValue JSC_HOST_CALL arrayProtoFuncToString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    bool isRealArray = isJSArray(&exec->globalData(), thisValue);
    if (!isRealArray && !thisValue.inherits(&JSArray::info))
        return throwError(exec, TypeError);
    JSArray* thisObj = asArray(thisValue);
    
    HashSet<JSObject*>& arrayVisitedElements = exec->globalData().arrayVisitedElements;
    if (arrayVisitedElements.size() >= MaxSecondaryThreadReentryDepth) {
        if (!isMainThread() || arrayVisitedElements.size() >= MaxMainThreadReentryDepth)
            return throwError(exec, RangeError, "Maximum call stack size exceeded.");
    }

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    if (alreadyVisited)
        return jsEmptyString(exec); // return an empty string, avoiding infinite recursion.

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned totalSize = length ? length - 1 : 0;
    Vector<RefPtr<UString::Rep>, 256> strBuffer(length);
    for (unsigned k = 0; k < length; k++) {
        JSValue element;
        if (isRealArray && thisObj->canGetIndex(k))
            element = thisObj->getIndex(k);
        else
            element = thisObj->get(exec, k);
        
        if (element.isUndefinedOrNull())
            continue;
        
        UString str = element.toString(exec);
        strBuffer[k] = str.rep();
        totalSize += str.size();
        
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }
        
        if (exec->hadException())
            break;
    }
    arrayVisitedElements.remove(thisObj);
    if (!totalSize)
        return jsEmptyString(exec);
    Vector<UChar> buffer;
    buffer.reserveCapacity(totalSize);
    if (!buffer.data())
        return throwError(exec, GeneralError, "Out of memory");
        
    for (unsigned i = 0; i < length; i++) {
        if (i)
            buffer.append(',');
        if (RefPtr<UString::Rep> rep = strBuffer[i])
            buffer.append(rep->data(), rep->size());
    }
    ASSERT(buffer.size() == totalSize);
    unsigned finalSize = buffer.size();
    return jsString(exec, UString(buffer.releaseBuffer(), finalSize, false));
}

JSValue JSC_HOST_CALL arrayProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&JSArray::info))
        return throwError(exec, TypeError);
    JSObject* thisObj = asArray(thisValue);

    HashSet<JSObject*>& arrayVisitedElements = exec->globalData().arrayVisitedElements;
    if (arrayVisitedElements.size() >= MaxSecondaryThreadReentryDepth) {
        if (!isMainThread() || arrayVisitedElements.size() >= MaxMainThreadReentryDepth)
            return throwError(exec, RangeError, "Maximum call stack size exceeded.");
    }

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    if (alreadyVisited)
        return jsEmptyString(exec); // return an empty string, avoding infinite recursion.

    Vector<UChar, 256> strBuffer;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(',');
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue element = thisObj->get(exec, k);
        if (element.isUndefinedOrNull())
            continue;

        JSObject* o = element.toObject(exec);
        JSValue conversionFunction = o->get(exec, exec->propertyNames().toLocaleString);
        UString str;
        CallData callData;
        CallType callType = conversionFunction.getCallData(callData);
        if (callType != CallTypeNone)
            str = call(exec, conversionFunction, callType, callData, element, exec->emptyList()).toString(exec);
        else
            str = element.toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    arrayVisitedElements.remove(thisObj);
    return jsString(exec, UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValue JSC_HOST_CALL arrayProtoFuncJoin(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    HashSet<JSObject*>& arrayVisitedElements = exec->globalData().arrayVisitedElements;
    if (arrayVisitedElements.size() >= MaxSecondaryThreadReentryDepth) {
        if (!isMainThread() || arrayVisitedElements.size() >= MaxMainThreadReentryDepth)
            return throwError(exec, RangeError, "Maximum call stack size exceeded.");
    }

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    if (alreadyVisited)
        return jsEmptyString(exec); // return an empty string, avoding infinite recursion.

    Vector<UChar, 256> strBuffer;

    UChar comma = ',';
    UString separator = args.at(0).isUndefined() ? UString(&comma, 1) : args.at(0).toString(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(separator.data(), separator.size());
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue element = thisObj->get(exec, k);
        if (element.isUndefinedOrNull())
            continue;

        UString str = element.toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    arrayVisitedElements.remove(thisObj);
    return jsString(exec, UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValue JSC_HOST_CALL arrayProtoFuncConcat(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSArray* arr = constructEmptyArray(exec);
    int n = 0;
    JSValue curArg = thisValue.toThisObject(exec);
    ArgList::const_iterator it = args.begin();
    ArgList::const_iterator end = args.end();
    while (1) {
        if (curArg.inherits(&JSArray::info)) {
            unsigned length = curArg.get(exec, exec->propertyNames().length).toUInt32(exec);
            JSObject* curObject = curArg.toObject(exec);
            for (unsigned k = 0; k < length; ++k) {
                if (JSValue v = getProperty(exec, curObject, k))
                    arr->put(exec, n, v);
                n++;
            }
        } else {
            arr->put(exec, n, curArg);
            n++;
        }
        if (it == end)
            break;
        curArg = (*it);
        ++it;
    }
    arr->setLength(n);
    return arr;
}

JSValue JSC_HOST_CALL arrayProtoFuncPop(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (isJSArray(&exec->globalData(), thisValue))
        return asArray(thisValue)->pop();

    JSObject* thisObj = thisValue.toThisObject(exec);
    JSValue result;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (length == 0) {
        putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, length - 1);
        thisObj->deleteProperty(exec, length - 1);
        putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length - 1));
    }
    return result;
}

JSValue JSC_HOST_CALL arrayProtoFuncPush(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (isJSArray(&exec->globalData(), thisValue) && args.size() == 1) {
        JSArray* array = asArray(thisValue);
        array->push(exec, *args.begin());
        return jsNumber(exec, array->length());
    }

    JSObject* thisObj = thisValue.toThisObject(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned n = 0; n < args.size(); n++)
        thisObj->put(exec, length + n, args.at(n));
    length += args.size();
    putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length));
    return jsNumber(exec, length);
}

JSValue JSC_HOST_CALL arrayProtoFuncReverse(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue.toThisObject(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned middle = length / 2;

    for (unsigned k = 0; k < middle; k++) {
        unsigned lk1 = length - k - 1;
        JSValue obj2 = getProperty(exec, thisObj, lk1);
        JSValue obj = getProperty(exec, thisObj, k);

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

JSValue JSC_HOST_CALL arrayProtoFuncShift(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue.toThisObject(exec);
    JSValue result;

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (length == 0) {
        putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, 0);
        for (unsigned k = 1; k < length; k++) {
            if (JSValue obj = getProperty(exec, thisObj, k))
                thisObj->put(exec, k - 1, obj);
            else
                thisObj->deleteProperty(exec, k - 1);
        }
        thisObj->deleteProperty(exec, length - 1);
        putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length - 1));
    }
    return result;
}

JSValue JSC_HOST_CALL arrayProtoFuncSlice(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713 or 15.4.4.10

    JSObject* thisObj = thisValue.toThisObject(exec);

    // We return a new array
    JSArray* resObj = constructEmptyArray(exec);
    JSValue result = resObj;
    double begin = args.at(0).toInteger(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (begin >= 0) {
        if (begin > length)
            begin = length;
    } else {
        begin += length;
        if (begin < 0)
            begin = 0;
    }
    double end;
    if (args.at(1).isUndefined())
        end = length;
    else {
        end = args.at(1).toInteger(exec);
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
        if (JSValue v = getProperty(exec, thisObj, k))
            resObj->put(exec, n, v);
    }
    resObj->setLength(n);
    return result;
}

JSValue JSC_HOST_CALL arrayProtoFuncSort(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValue function = args.at(0);
    CallData callData;
    CallType callType = function.getCallData(callData);

    if (thisObj->classInfo() == &JSArray::info) {
        if (isNumericCompareFunction(exec, callType, callData))
            asArray(thisObj)->sortNumeric(exec, function, callType, callData);
        else if (callType != CallTypeNone)
            asArray(thisObj)->sort(exec, function, callType, callData);
        else
            asArray(thisObj)->sort(exec);
        return thisObj;
    }

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);

    if (!length)
        return thisObj;

    // "Min" sort. Not the fastest, but definitely less code than heapsort
    // or quicksort, and much less swapping than bubblesort/insertionsort.
    for (unsigned i = 0; i < length - 1; ++i) {
        JSValue iObj = thisObj->get(exec, i);
        unsigned themin = i;
        JSValue minObj = iObj;
        for (unsigned j = i + 1; j < length; ++j) {
            JSValue jObj = thisObj->get(exec, j);
            double compareResult;
            if (jObj.isUndefined())
                compareResult = 1; // don't check minObj because there's no need to differentiate == (0) from > (1)
            else if (minObj.isUndefined())
                compareResult = -1;
            else if (callType != CallTypeNone) {
                MarkedArgumentBuffer l;
                l.append(jObj);
                l.append(minObj);
                compareResult = call(exec, function, callType, callData, exec->globalThisValue(), l).toNumber(exec);
            } else
                compareResult = (jObj.toString(exec) < minObj.toString(exec)) ? -1 : 1;

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

JSValue JSC_HOST_CALL arrayProtoFuncSplice(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    // 15.4.4.12
    JSArray* resObj = constructEmptyArray(exec);
    JSValue result = resObj;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (!args.size())
        return jsUndefined();
    int begin = args.at(0).toUInt32(exec);
    if (begin < 0)
        begin = std::max<int>(begin + length, 0);
    else
        begin = std::min<int>(begin, length);

    unsigned deleteCount;
    if (args.size() > 1)
        deleteCount = std::min<int>(std::max<int>(args.at(1).toUInt32(exec), 0), length - begin);
    else
        deleteCount = length - begin;

    for (unsigned k = 0; k < deleteCount; k++) {
        if (JSValue v = getProperty(exec, thisObj, k + begin))
            resObj->put(exec, k, v);
    }
    resObj->setLength(deleteCount);

    unsigned additionalArgs = std::max<int>(args.size() - 2, 0);
    if (additionalArgs != deleteCount) {
        if (additionalArgs < deleteCount) {
            for (unsigned k = begin; k < length - deleteCount; ++k) {
                if (JSValue v = getProperty(exec, thisObj, k + deleteCount))
                    thisObj->put(exec, k + additionalArgs, v);
                else
                    thisObj->deleteProperty(exec, k + additionalArgs);
            }
            for (unsigned k = length; k > length - deleteCount + additionalArgs; --k)
                thisObj->deleteProperty(exec, k - 1);
        } else {
            for (unsigned k = length - deleteCount; (int)k > begin; --k) {
                if (JSValue obj = getProperty(exec, thisObj, k + deleteCount - 1))
                    thisObj->put(exec, k + additionalArgs - 1, obj);
                else
                    thisObj->deleteProperty(exec, k + additionalArgs - 1);
            }
        }
    }
    for (unsigned k = 0; k < additionalArgs; ++k)
        thisObj->put(exec, k + begin, args.at(k + 2));

    putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length - deleteCount + additionalArgs));
    return result;
}

JSValue JSC_HOST_CALL arrayProtoFuncUnShift(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    // 15.4.4.13
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned nrArgs = args.size();
    if (nrArgs) {
        for (unsigned k = length; k > 0; --k) {
            if (JSValue v = getProperty(exec, thisObj, k - 1))
                thisObj->put(exec, k + nrArgs - 1, v);
            else
                thisObj->deleteProperty(exec, k + nrArgs - 1);
        }
    }
    for (unsigned k = 0; k < nrArgs; ++k)
        thisObj->put(exec, k, args.at(k));
    JSValue result = jsNumber(exec, length + nrArgs);
    putProperty(exec, thisObj, exec->propertyNames().length, result);
    return result;
}

JSValue JSC_HOST_CALL arrayProtoFuncFilter(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValue function = args.at(0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(1).toObject(exec);
    JSArray* resultArray = constructEmptyArray(exec);

    unsigned filterIndex = 0;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned k = 0;
    if (callType == CallTypeJS && isJSArray(&exec->globalData(), thisObj)) {
        JSFunction* f = asFunction(function);
        JSArray* array = asArray(thisObj);
        CachedCall cachedCall(exec, f, 3, exec->exceptionSlot());
        for (; k < length && !exec->hadException(); ++k) {
            if (!array->canGetIndex(k))
                break;
            JSValue v = array->getIndex(k);
            cachedCall.setThis(applyThis);
            cachedCall.setArgument(0, v);
            cachedCall.setArgument(1, jsNumber(exec, k));
            cachedCall.setArgument(2, thisObj);
            
            JSValue result = cachedCall.call();
            if (result.toBoolean(exec))
                resultArray->put(exec, filterIndex++, v);
        }
        if (k == length)
            return resultArray;
    }
    for (; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);

        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        JSValue v = slot.getValue(exec, k);

        MarkedArgumentBuffer eachArguments;

        eachArguments.append(v);
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        JSValue result = call(exec, function, callType, callData, applyThis, eachArguments);

        if (result.toBoolean(exec))
            resultArray->put(exec, filterIndex++, v);
    }
    return resultArray;
}

JSValue JSC_HOST_CALL arrayProtoFuncMap(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValue function = args.at(0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(1).toObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);

    JSArray* resultArray = constructEmptyArray(exec, length);
    unsigned k = 0;
    if (callType == CallTypeJS && isJSArray(&exec->globalData(), thisObj)) {
        JSFunction* f = asFunction(function);
        JSArray* array = asArray(thisObj);
        CachedCall cachedCall(exec, f, 3, exec->exceptionSlot());
        for (; k < length && !exec->hadException(); ++k) {
            if (UNLIKELY(!array->canGetIndex(k)))
                break;

            cachedCall.setThis(applyThis);
            cachedCall.setArgument(0, array->getIndex(k));
            cachedCall.setArgument(1, jsNumber(exec, k));
            cachedCall.setArgument(2, thisObj);

            resultArray->JSArray::put(exec, k, cachedCall.call());
        }
    }
    for (; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        JSValue v = slot.getValue(exec, k);

        MarkedArgumentBuffer eachArguments;

        eachArguments.append(v);
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        JSValue result = call(exec, function, callType, callData, applyThis, eachArguments);
        resultArray->put(exec, k, result);
    }

    return resultArray;
}

// Documentation for these three is available at:
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:every
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:forEach
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:some

JSValue JSC_HOST_CALL arrayProtoFuncEvery(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValue function = args.at(0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(1).toObject(exec);

    JSValue result = jsBoolean(true);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned k = 0;
    if (callType == CallTypeJS && isJSArray(&exec->globalData(), thisObj)) {
        JSFunction* f = asFunction(function);
        JSArray* array = asArray(thisObj);
        CachedCall cachedCall(exec, f, 3, exec->exceptionSlot());
        for (; k < length && !exec->hadException(); ++k) {
            if (UNLIKELY(!array->canGetIndex(k)))
                break;
            
            cachedCall.setThis(applyThis);
            cachedCall.setArgument(0, array->getIndex(k));
            cachedCall.setArgument(1, jsNumber(exec, k));
            cachedCall.setArgument(2, thisObj);
            
            if (!cachedCall.call().toBoolean(exec))
                return jsBoolean(false);
        }
    }
    for (; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);

        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        MarkedArgumentBuffer eachArguments;

        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        bool predicateResult = call(exec, function, callType, callData, applyThis, eachArguments).toBoolean(exec);

        if (!predicateResult) {
            result = jsBoolean(false);
            break;
        }
    }

    return result;
}

JSValue JSC_HOST_CALL arrayProtoFuncForEach(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValue function = args.at(0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(1).toObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned k = 0;
    if (callType == CallTypeJS && isJSArray(&exec->globalData(), thisObj)) {
        JSFunction* f = asFunction(function);
        JSArray* array = asArray(thisObj);
        CachedCall cachedCall(exec, f, 3, exec->exceptionSlot());
        for (; k < length && !exec->hadException(); ++k) {
            if (UNLIKELY(!array->canGetIndex(k)))
                break;

            cachedCall.setThis(applyThis);
            cachedCall.setArgument(0, array->getIndex(k));
            cachedCall.setArgument(1, jsNumber(exec, k));
            cachedCall.setArgument(2, thisObj);

            cachedCall.call();
        }
    }
    for (; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        MarkedArgumentBuffer eachArguments;
        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        call(exec, function, callType, callData, applyThis, eachArguments);
    }
    return jsUndefined();
}

JSValue JSC_HOST_CALL arrayProtoFuncSome(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValue function = args.at(0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(1).toObject(exec);

    JSValue result = jsBoolean(false);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned k = 0;
    if (callType == CallTypeJS && isJSArray(&exec->globalData(), thisObj)) {
        JSFunction* f = asFunction(function);
        JSArray* array = asArray(thisObj);
        CachedCall cachedCall(exec, f, 3, exec->exceptionSlot());
        for (; k < length && !exec->hadException(); ++k) {
            if (UNLIKELY(!array->canGetIndex(k)))
                break;
            
            cachedCall.setThis(applyThis);
            cachedCall.setArgument(0, array->getIndex(k));
            cachedCall.setArgument(1, jsNumber(exec, k));
            cachedCall.setArgument(2, thisObj);
            
            if (cachedCall.call().toBoolean(exec))
                return jsBoolean(true);
        }
    }
    for (; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        MarkedArgumentBuffer eachArguments;
        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        bool predicateResult = call(exec, function, callType, callData, applyThis, eachArguments).toBoolean(exec);

        if (predicateResult) {
            result = jsBoolean(true);
            break;
        }
    }
    return result;
}

JSValue JSC_HOST_CALL arrayProtoFuncReduce(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);
    
    JSValue function = args.at(0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    unsigned i = 0;
    JSValue rv;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (!length && args.size() == 1)
        return throwError(exec, TypeError);
    JSArray* array = 0;
    if (isJSArray(&exec->globalData(), thisObj))
        array = asArray(thisObj);

    if (args.size() >= 2)
        rv = args.at(1);
    else if (array && array->canGetIndex(0)){
        rv = array->getIndex(0);
        i = 1;
    } else {
        for (i = 0; i < length; i++) {
            rv = getProperty(exec, thisObj, i);
            if (rv)
                break;
        }
        if (!rv)
            return throwError(exec, TypeError);
        i++;
    }

    if (callType == CallTypeJS && array) {
        CachedCall cachedCall(exec, asFunction(function), 4, exec->exceptionSlot());
        for (; i < length && !exec->hadException(); ++i) {
            cachedCall.setThis(jsNull());
            cachedCall.setArgument(0, rv);
            JSValue v;
            if (LIKELY(array->canGetIndex(i)))
                v = array->getIndex(i);
            else
                break; // length has been made unsafe while we enumerate fallback to slow path
            cachedCall.setArgument(1, v);
            cachedCall.setArgument(2, jsNumber(exec, i));
            cachedCall.setArgument(3, array);
            rv = cachedCall.call();
        }
        if (i == length) // only return if we reached the end of the array
            return rv;
    }

    for (; i < length && !exec->hadException(); ++i) {
        JSValue prop = getProperty(exec, thisObj, i);
        if (!prop)
            continue;
        
        MarkedArgumentBuffer eachArguments;
        eachArguments.append(rv);
        eachArguments.append(prop);
        eachArguments.append(jsNumber(exec, i));
        eachArguments.append(thisObj);
        
        rv = call(exec, function, callType, callData, jsNull(), eachArguments);
    }
    return rv;
}

JSValue JSC_HOST_CALL arrayProtoFuncReduceRight(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);
    
    JSValue function = args.at(0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);
    
    unsigned i = 0;
    JSValue rv;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (!length && args.size() == 1)
        return throwError(exec, TypeError);
    JSArray* array = 0;
    if (isJSArray(&exec->globalData(), thisObj))
        array = asArray(thisObj);
    
    if (args.size() >= 2)
        rv = args.at(1);
    else if (array && array->canGetIndex(length - 1)){
        rv = array->getIndex(length - 1);
        i = 1;
    } else {
        for (i = 0; i < length; i++) {
            rv = getProperty(exec, thisObj, length - i - 1);
            if (rv)
                break;
        }
        if (!rv)
            return throwError(exec, TypeError);
        i++;
    }
    
    if (callType == CallTypeJS && array) {
        CachedCall cachedCall(exec, asFunction(function), 4, exec->exceptionSlot());
        for (; i < length && !exec->hadException(); ++i) {
            unsigned idx = length - i - 1;
            cachedCall.setThis(jsNull());
            cachedCall.setArgument(0, rv);
            if (UNLIKELY(!array->canGetIndex(idx)))
                break; // length has been made unsafe while we enumerate fallback to slow path
            cachedCall.setArgument(1, array->getIndex(idx));
            cachedCall.setArgument(2, jsNumber(exec, idx));
            cachedCall.setArgument(3, array);
            rv = cachedCall.call();
        }
        if (i == length) // only return if we reached the end of the array
            return rv;
    }
    
    for (; i < length && !exec->hadException(); ++i) {
        unsigned idx = length - i - 1;
        JSValue prop = getProperty(exec, thisObj, idx);
        if (!prop)
            continue;
        
        MarkedArgumentBuffer eachArguments;
        eachArguments.append(rv);
        eachArguments.append(prop);
        eachArguments.append(jsNumber(exec, idx));
        eachArguments.append(thisObj);
        
        rv = call(exec, function, callType, callData, jsNull(), eachArguments);
    }
    return rv;        
}

JSValue JSC_HOST_CALL arrayProtoFuncIndexOf(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    // JavaScript 1.5 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:indexOf

    JSObject* thisObj = thisValue.toThisObject(exec);

    unsigned index = 0;
    double d = args.at(1).toInteger(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (d < 0)
        d += length;
    if (d > 0) {
        if (d > length)
            index = length;
        else
            index = static_cast<unsigned>(d);
    }

    JSValue searchElement = args.at(0);
    for (; index < length; ++index) {
        JSValue e = getProperty(exec, thisObj, index);
        if (!e)
            continue;
        if (JSValue::strictEqual(searchElement, e))
            return jsNumber(exec, index);
    }

    return jsNumber(exec, -1);
}

JSValue JSC_HOST_CALL arrayProtoFuncLastIndexOf(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    // JavaScript 1.6 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:lastIndexOf

    JSObject* thisObj = thisValue.toThisObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    int index = length - 1;
    double d = args.at(1).toIntegerPreserveNaN(exec);

    if (d < 0) {
        d += length;
        if (d < 0)
            return jsNumber(exec, -1);
    }
    if (d < length)
        index = static_cast<int>(d);

    JSValue searchElement = args.at(0);
    for (; index >= 0; --index) {
        JSValue e = getProperty(exec, thisObj, index);
        if (!e)
            continue;
        if (JSValue::strictEqual(searchElement, e))
            return jsNumber(exec, index);
    }

    return jsNumber(exec, -1);
}

} // namespace JSC
