/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SpeculatedType.h"

#include "DateInstance.h"
#include "DirectArguments.h"
#include "JSArray.h"
#include "JSBigInt.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSMap.h"
#include "JSSet.h"
#include "JSWeakMap.h"
#include "JSWeakSet.h"
#include "ProxyObject.h"
#include "RegExpObject.h"
#include "ScopedArguments.h"
#include "StringObject.h"
#include "ValueProfile.h"
#include <wtf/CommaPrinter.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

struct PrettyPrinter {
    PrettyPrinter(PrintStream& out)
        : out(out)
        , separator("|")
    { }
    
    template<typename T>
    void print(const T& value)
    {
        out.print(separator, value);
    }
    
    PrintStream& out;
    CommaPrinter separator;
};
    
void dumpSpeculation(PrintStream& outStream, SpeculatedType value)
{
    StringPrintStream strStream;
    PrettyPrinter out(outStream);
    PrettyPrinter strOut(strStream);
    
    if (value == SpecNone) {
        out.print("None");
        return;
    }
    
    bool isTop = true;
    
    if ((value & SpecCell) == SpecCell)
        strOut.print("Cell");
    else {
        if ((value & SpecObject) == SpecObject)
            strOut.print("Object");
        else {
            if (value & SpecCellOther)
                strOut.print("OtherCell");
            else
                isTop = false;
    
            if (value & SpecObjectOther)
                strOut.print("OtherObj");
            else
                isTop = false;
    
            if (value & SpecFinalObject)
                strOut.print("Final");
            else
                isTop = false;

            if (value & SpecArray)
                strOut.print("Array");
            else
                isTop = false;
    
            if (value & SpecInt8Array)
                strOut.print("Int8Array");
            else
                isTop = false;
    
            if (value & SpecInt16Array)
                strOut.print("Int16Array");
            else
                isTop = false;
    
            if (value & SpecInt32Array)
                strOut.print("Int32Array");
            else
                isTop = false;
    
            if (value & SpecUint8Array)
                strOut.print("Uint8Array");
            else
                isTop = false;

            if (value & SpecUint8ClampedArray)
                strOut.print("Uint8ClampedArray");
            else
                isTop = false;
    
            if (value & SpecUint16Array)
                strOut.print("Uint16Array");
            else
                isTop = false;
    
            if (value & SpecUint32Array)
                strOut.print("Uint32Array");
            else
                isTop = false;
    
            if (value & SpecFloat32Array)
                strOut.print("Float32array");
            else
                isTop = false;
    
            if (value & SpecFloat64Array)
                strOut.print("Float64Array");
            else
                isTop = false;
    
            if (value & SpecFunction)
                strOut.print("Function");
            else
                isTop = false;
    
            if (value & SpecDirectArguments)
                strOut.print("DirectArguments");
            else
                isTop = false;
    
            if (value & SpecScopedArguments)
                strOut.print("ScopedArguments");
            else
                isTop = false;
    
            if (value & SpecStringObject)
                strOut.print("StringObject");
            else
                isTop = false;
    
            if (value & SpecRegExpObject)
                strOut.print("RegExpObject");
            else
                isTop = false;

            if (value & SpecDateObject)
                strOut.print("DateObject");
            else
                isTop = false;

            if (value & SpecPromiseObject)
                strOut.print("PromiseObject");
            else
                isTop = false;

            if (value & SpecMapObject)
                strOut.print("MapObject");
            else
                isTop = false;

            if (value & SpecSetObject)
                strOut.print("SetObject");
            else
                isTop = false;

            if (value & SpecWeakMapObject)
                strOut.print("WeakMapObject");
            else
                isTop = false;

            if (value & SpecWeakSetObject)
                strOut.print("WeakSetObject");
            else
                isTop = false;

            if (value & SpecProxyObject)
                strOut.print("ProxyObject");
            else
                isTop = false;

            if (value & SpecDerivedArray)
                strOut.print("DerivedArray");
            else
                isTop = false;

            if (value & SpecDataViewObject)
                strOut.print("DataView");
            else
                isTop = false;
        }

        if ((value & SpecString) == SpecString)
            strOut.print("String");
        else {
            if (value & SpecStringIdent)
                strOut.print("StringIdent");
            else
                isTop = false;
            
            if (value & SpecStringVar)
                strOut.print("StringVar");
            else
                isTop = false;
        }

        if (value & SpecSymbol)
            strOut.print("Symbol");
        else
            isTop = false;

        if (value & SpecBigInt)
            strOut.print("BigInt");
        else
            isTop = false;
    }
    
    if (value == SpecInt32Only)
        strOut.print("Int32");
    else {
        if (value & SpecBoolInt32)
            strOut.print("BoolInt32");
        else
            isTop = false;
        
        if (value & SpecNonBoolInt32)
            strOut.print("NonBoolInt32");
        else
            isTop = false;
    }

    if ((value & SpecBytecodeDouble) == SpecBytecodeDouble)
        strOut.print("BytecodeDouble");
    else {
        if (value & SpecAnyIntAsDouble)
            strOut.print("AnyIntAsDouble");
        else
            isTop = false;
        
        if (value & SpecNonIntAsDouble)
            strOut.print("NonIntAsDouble");
        else
            isTop = false;
        
        if (value & SpecDoublePureNaN)
            strOut.print("DoublePureNaN");
        else
            isTop = false;
    }
    
    if (value & SpecDoubleImpureNaN)
        strOut.print("DoubleImpureNaN");
    
    if (value & SpecBoolean)
        strOut.print("Bool");
    else
        isTop = false;
    
    if (value & SpecOther)
        strOut.print("Other");
    else
        isTop = false;
    
    if (value & SpecEmpty)
        strOut.print("Empty");
    else
        isTop = false;

    if (value & SpecInt52Any) {
        if ((value & SpecInt52Any) == SpecInt52Any)
            strOut.print("Int52Any");
        else if (value & SpecInt32AsInt52)
            strOut.print("Int32AsInt52");
        else if (value & SpecNonInt32AsInt52)
            strOut.print("NonInt32AsInt52");
    } else
        isTop = false;
    
    if (value == SpecBytecodeTop)
        out.print("BytecodeTop");
    else if (value == SpecHeapTop)
        out.print("HeapTop");
    else if (value == SpecFullTop)
        out.print("FullTop");
    else if (isTop)
        out.print("Top");
    else
        out.print(strStream.toCString());
}

// We don't expose this because we don't want anyone relying on the fact that this method currently
// just returns string constants.
static const char* speculationToAbbreviatedString(SpeculatedType prediction)
{
    if (isFinalObjectSpeculation(prediction))
        return "<Final>";
    if (isArraySpeculation(prediction))
        return "<Array>";
    if (isStringIdentSpeculation(prediction))
        return "<StringIdent>";
    if (isStringSpeculation(prediction))
        return "<String>";
    if (isFunctionSpeculation(prediction))
        return "<Function>";
    if (isInt8ArraySpeculation(prediction))
        return "<Int8array>";
    if (isInt16ArraySpeculation(prediction))
        return "<Int16array>";
    if (isInt32ArraySpeculation(prediction))
        return "<Int32array>";
    if (isUint8ArraySpeculation(prediction))
        return "<Uint8array>";
    if (isUint16ArraySpeculation(prediction))
        return "<Uint16array>";
    if (isUint32ArraySpeculation(prediction))
        return "<Uint32array>";
    if (isFloat32ArraySpeculation(prediction))
        return "<Float32array>";
    if (isFloat64ArraySpeculation(prediction))
        return "<Float64array>";
    if (isDirectArgumentsSpeculation(prediction))
        return "<DirectArguments>";
    if (isScopedArgumentsSpeculation(prediction))
        return "<ScopedArguments>";
    if (isStringObjectSpeculation(prediction))
        return "<StringObject>";
    if (isRegExpObjectSpeculation(prediction))
        return "<RegExpObject>";
    if (isStringOrStringObjectSpeculation(prediction))
        return "<StringOrStringObject>";
    if (isObjectSpeculation(prediction))
        return "<Object>";
    if (isCellSpeculation(prediction))
        return "<Cell>";
    if (isBoolInt32Speculation(prediction))
        return "<BoolInt32>";
    if (isInt32Speculation(prediction))
        return "<Int32>";
    if (isAnyIntAsDoubleSpeculation(prediction))
        return "<AnyIntAsDouble>";
    if (prediction == SpecNonInt32AsInt52)
        return "<NonInt32AsInt52>";
    if (prediction == SpecInt32AsInt52)
        return "<Int32AsInt52>";
    if (isAnyInt52Speculation(prediction))
        return "<Int52Any>";
    if (isDoubleSpeculation(prediction))
        return "<Double>";
    if (isFullNumberSpeculation(prediction))
        return "<Number>";
    if (isBooleanSpeculation(prediction))
        return "<Boolean>";
    if (isOtherSpeculation(prediction))
        return "<Other>";
    if (isMiscSpeculation(prediction))
        return "<Misc>";
    return "";
}

void dumpSpeculationAbbreviated(PrintStream& out, SpeculatedType value)
{
    out.print(speculationToAbbreviatedString(value));
}

SpeculatedType speculationFromTypedArrayType(TypedArrayType type)
{
    switch (type) {
    case TypeInt8:
        return SpecInt8Array;
    case TypeInt16:
        return SpecInt16Array;
    case TypeInt32:
        return SpecInt32Array;
    case TypeUint8:
        return SpecUint8Array;
    case TypeUint8Clamped:
        return SpecUint8ClampedArray;
    case TypeUint16:
        return SpecUint16Array;
    case TypeUint32:
        return SpecUint32Array;
    case TypeFloat32:
        return SpecFloat32Array;
    case TypeFloat64:
        return SpecFloat64Array;
    case NotTypedArray:
    case TypeDataView:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return SpecNone;
}

SpeculatedType speculationFromClassInfo(const ClassInfo* classInfo)
{
    if (classInfo == JSString::info())
        return SpecString;

    if (classInfo == Symbol::info())
        return SpecSymbol;
    
    if (classInfo == JSBigInt::info())
        return SpecBigInt;

    if (classInfo == JSFinalObject::info())
        return SpecFinalObject;
    
    if (classInfo == JSArray::info())
        return SpecArray;
    
    if (classInfo == DirectArguments::info())
        return SpecDirectArguments;
    
    if (classInfo == ScopedArguments::info())
        return SpecScopedArguments;
    
    if (classInfo == StringObject::info())
        return SpecStringObject;

    if (classInfo == RegExpObject::info())
        return SpecRegExpObject;

    if (classInfo == DateInstance::info())
        return SpecDateObject;

    if (classInfo == JSMap::info())
        return SpecMapObject;

    if (classInfo == JSSet::info())
        return SpecSetObject;

    if (classInfo == JSWeakMap::info())
        return SpecWeakMapObject;

    if (classInfo == JSWeakSet::info())
        return SpecWeakSetObject;

    if (classInfo == ProxyObject::info())
        return SpecProxyObject;

    if (classInfo == JSDataView::info())
        return SpecDataViewObject;
    
    if (classInfo->isSubClassOf(JSFunction::info())) {
        if (classInfo == JSBoundFunction::info())
            return SpecFunctionWithNonDefaultHasInstance;
        return SpecFunctionWithDefaultHasInstance;
    }

    if (classInfo->isSubClassOf(JSPromise::info()))
        return SpecPromiseObject;
    
    if (isTypedView(classInfo->typedArrayStorageType))
        return speculationFromTypedArrayType(classInfo->typedArrayStorageType);

    if (classInfo->isSubClassOf(JSArray::info()))
        return SpecDerivedArray;
    
    if (classInfo->isSubClassOf(JSObject::info()))
        return SpecObjectOther;
    
    return SpecCellOther;
}

SpeculatedType speculationFromStructure(Structure* structure)
{
    if (structure->typeInfo().type() == StringType)
        return SpecString;
    if (structure->typeInfo().type() == SymbolType)
        return SpecSymbol;
    if (structure->typeInfo().type() == BigIntType)
        return SpecBigInt;
    if (structure->typeInfo().type() == DerivedArrayType)
        return SpecDerivedArray;
    return speculationFromClassInfo(structure->classInfo());
}

SpeculatedType speculationFromCell(JSCell* cell)
{
    if (cell->isString()) {
        JSString* string = jsCast<JSString*>(cell);
        if (const StringImpl* impl = string->tryGetValueImpl()) {
            if (impl->isAtom())
                return SpecStringIdent;
        }
        return SpecString;
    }
    return speculationFromStructure(cell->structure());
}

SpeculatedType speculationFromValue(JSValue value)
{
    if (value.isEmpty())
        return SpecEmpty;
    if (value.isInt32()) {
        if (value.asInt32() & ~1)
            return SpecNonBoolInt32;
        return SpecBoolInt32;
    }
    if (value.isDouble()) {
        double number = value.asNumber();
        if (number != number)
            return SpecDoublePureNaN;
        if (value.isAnyInt())
            return SpecAnyIntAsDouble;
        return SpecNonIntAsDouble;
    }
    if (value.isCell())
        return speculationFromCell(value.asCell());
    if (value.isBoolean())
        return SpecBoolean;
    ASSERT(value.isUndefinedOrNull());
    return SpecOther;
}

SpeculatedType int52AwareSpeculationFromValue(JSValue value)
{
    if (!value.isAnyInt())
        return speculationFromValue(value);

    int64_t intValue = value.asAnyInt();
    bool isI32 = static_cast<int64_t>(static_cast<int32_t>(intValue)) == intValue;
    if (isI32)
        return SpecInt32AsInt52;
    return SpecNonInt32AsInt52;
}

TypedArrayType typedArrayTypeFromSpeculation(SpeculatedType type)
{
    if (isInt8ArraySpeculation(type))
        return TypeInt8;
        
    if (isInt16ArraySpeculation(type))
        return TypeInt16;
        
    if (isInt32ArraySpeculation(type))
        return TypeInt32;
        
    if (isUint8ArraySpeculation(type))
        return TypeUint8;
        
    if (isUint8ClampedArraySpeculation(type))
        return TypeUint8Clamped;
        
    if (isUint16ArraySpeculation(type))
        return TypeUint16;
        
    if (isUint32ArraySpeculation(type))
        return TypeUint32;
        
    if (isFloat32ArraySpeculation(type))
        return TypeFloat32;
        
    if (isFloat64ArraySpeculation(type))
        return TypeFloat64;
    
    return NotTypedArray;
}

Optional<SpeculatedType> speculationFromJSType(JSType type)
{
    switch (type) {
    case StringType:
        return SpecString;
    case SymbolType:
        return SpecSymbol;
    case BigIntType:
        return SpecBigInt;
    case ArrayType:
        return SpecArray;
    case DerivedArrayType:
        return SpecDerivedArray;
    case RegExpObjectType:
        return SpecRegExpObject;
    case JSDateType:
        return SpecDateObject;
    case ProxyObjectType:
        return SpecProxyObject;
    case JSPromiseType:
        return SpecPromiseObject;
    case JSMapType:
        return SpecMapObject;
    case JSSetType:
        return SpecSetObject;
    case JSWeakMapType:
        return SpecWeakMapObject;
    case JSWeakSetType:
        return SpecWeakSetObject;
    case DataViewType:
        return SpecDataViewObject;
    default:
        return WTF::nullopt;
    }
}

SpeculatedType leastUpperBoundOfStrictlyEquivalentSpeculations(SpeculatedType type)
{
    // SpecNonIntAsDouble includes negative zero (-0.0), which can be equal to 0 and 0.0 in the context of == and ===.
    if (type & (SpecIntAnyFormat | SpecNonIntAsDouble))
        type |= (SpecIntAnyFormat | SpecNonIntAsDouble);

    if (type & SpecString)
        type |= SpecString;
    return type;
}

static inline SpeculatedType leastUpperBoundOfEquivalentSpeculations(SpeculatedType type)
{
    type = leastUpperBoundOfStrictlyEquivalentSpeculations(type);

    // Boolean or BigInt can be converted to Number when performing non-strict equal.
    if (type & (SpecIntAnyFormat | SpecNonIntAsDouble | SpecBoolean | SpecBigInt))
        type |= (SpecIntAnyFormat | SpecNonIntAsDouble | SpecBoolean | SpecBigInt);

    return type;
}

bool valuesCouldBeEqual(SpeculatedType a, SpeculatedType b)
{
    a = leastUpperBoundOfEquivalentSpeculations(a);
    b = leastUpperBoundOfEquivalentSpeculations(b);
    
    // Anything could be equal to a string.
    if (a & SpecString)
        return true;
    if (b & SpecString)
        return true;
    
    // If both sides are definitely only objects, then equality is fairly sane.
    if (isObjectSpeculation(a) && isObjectSpeculation(b))
        return !!(a & b);
    
    // If either side could be an object or not, then we could call toString or
    // valueOf, which could return anything.
    if (a & SpecObject)
        return true;
    if (b & SpecObject)
        return true;
    
    // Neither side is an object or string, so the world is relatively sane.
    return !!(a & b);
}

static SpeculatedType typeOfDoubleSumOrDifferenceOrProduct(SpeculatedType a, SpeculatedType b)
{
    SpeculatedType result = a | b;

    if (result & SpecNonIntAsDouble) {
        // NaN can be produced by:
        // Infinity - Infinity
        // Infinity + (-Infinity)
        // Infinity * 0
        result |= SpecDoublePureNaN;
    }

    // Impure NaN could become pure NaN during addition because addition may clear bits.
    if (result & SpecDoubleImpureNaN)
        result |= SpecDoublePureNaN;
    // Values could overflow, or fractions could become integers.
    if (result & SpecDoubleReal)
        result |= SpecDoubleReal;
    return result;
}

SpeculatedType typeOfDoubleSum(SpeculatedType a, SpeculatedType b)
{
    return typeOfDoubleSumOrDifferenceOrProduct(a, b);
}

SpeculatedType typeOfDoubleDifference(SpeculatedType a, SpeculatedType b)
{
    return typeOfDoubleSumOrDifferenceOrProduct(a, b);
}

SpeculatedType typeOfDoubleIncOrDec(SpeculatedType t)
{
    // Impure NaN could become pure NaN during addition because addition may clear bits.
    if (t & SpecDoubleImpureNaN)
        t |= SpecDoublePureNaN;
    // Values could overflow, or fractions could become integers.
    if (t & SpecDoubleReal)
        t |= SpecDoubleReal;
    return t;
}

SpeculatedType typeOfDoubleProduct(SpeculatedType a, SpeculatedType b)
{
    return typeOfDoubleSumOrDifferenceOrProduct(a, b);
}

static SpeculatedType polluteDouble(SpeculatedType value)
{
    // Impure NaN could become pure NaN because the operation could clear some bits.
    if (value & SpecDoubleImpureNaN)
        value |= SpecDoubleNaN;
    // Values could overflow, fractions could become integers, or an error could produce
    // PureNaN.
    if (value & SpecDoubleReal)
        value |= SpecDoubleReal | SpecDoublePureNaN;
    return value;
}

SpeculatedType typeOfDoubleQuotient(SpeculatedType a, SpeculatedType b)
{
    return polluteDouble(a | b);
}

SpeculatedType typeOfDoubleMinMax(SpeculatedType a, SpeculatedType b)
{
    SpeculatedType result = a | b;
    // Impure NaN could become pure NaN during addition because addition may clear bits.
    if (result & SpecDoubleImpureNaN)
        result |= SpecDoublePureNaN;
    return result;
}

SpeculatedType typeOfDoubleNegation(SpeculatedType value)
{
    // Changing bits can make pure NaN impure and vice versa:
    // 0xefff000000000000 (pure) - 0xffff000000000000 (impure)
    if (value & SpecDoubleNaN)
        value |= SpecDoubleNaN;
    // We could get negative zero, which mixes SpecAnyIntAsDouble and SpecNotIntAsDouble.
    // We could also overflow a large negative int into something that is no longer
    // representable as an int.
    if (value & SpecDoubleReal)
        value |= SpecDoubleReal;
    return value;
}

SpeculatedType typeOfDoubleAbs(SpeculatedType value)
{
    return typeOfDoubleNegation(value);
}

SpeculatedType typeOfDoubleRounding(SpeculatedType value)
{
    // Double Pure NaN can becomes impure when converted back from Float.
    // and vice versa.
    if (value & SpecDoubleNaN)
        value |= SpecDoubleNaN;
    // We might lose bits, which leads to a value becoming integer-representable.
    if (value & SpecNonIntAsDouble)
        value |= SpecAnyIntAsDouble;
    return value;
}

SpeculatedType typeOfDoublePow(SpeculatedType xValue, SpeculatedType yValue)
{
    // Math.pow() always return NaN if the exponent is NaN, unlike std::pow().
    // We always set a pure NaN in that case.
    if (yValue & SpecDoubleNaN)
        xValue |= SpecDoublePureNaN;
    // Handle the wierd case of NaN ^ 0, which returns 1. See https://tc39.github.io/ecma262/#sec-applying-the-exp-operator
    if (xValue & SpecDoubleNaN)
        xValue |= SpecFullDouble;
    return polluteDouble(xValue);
}

SpeculatedType typeOfDoubleBinaryOp(SpeculatedType a, SpeculatedType b)
{
    return polluteDouble(a | b);
}

SpeculatedType typeOfDoubleUnaryOp(SpeculatedType value)
{
    return polluteDouble(value);
}

SpeculatedType speculationFromString(const char* speculation)
{
    if (!strncmp(speculation, "SpecNone", strlen("SpecNone")))
        return SpecNone;
    if (!strncmp(speculation, "SpecFinalObject", strlen("SpecFinalObject")))
        return SpecFinalObject;
    if (!strncmp(speculation, "SpecArray", strlen("SpecArray")))
        return SpecArray;
    if (!strncmp(speculation, "SpecFunction", strlen("SpecFunction")))
        return SpecFunction;
    if (!strncmp(speculation, "SpecInt8Array", strlen("SpecInt8Array")))
        return SpecInt8Array;
    if (!strncmp(speculation, "SpecInt16Array", strlen("SpecInt16Array")))
        return SpecInt16Array;
    if (!strncmp(speculation, "SpecInt32Array", strlen("SpecInt32Array")))
        return SpecInt32Array;
    if (!strncmp(speculation, "SpecUint8Array", strlen("SpecUint8Array")))
        return SpecUint8Array;
    if (!strncmp(speculation, "SpecUint8ClampedArray", strlen("SpecUint8ClampedArray")))
        return SpecUint8ClampedArray;
    if (!strncmp(speculation, "SpecUint16Array", strlen("SpecUint16Array")))
        return SpecUint16Array;
    if (!strncmp(speculation, "SpecUint32Array", strlen("SpecUint32Array")))
        return SpecUint32Array;
    if (!strncmp(speculation, "SpecFloat32Array", strlen("SpecFloat32Array")))
        return SpecFloat32Array;
    if (!strncmp(speculation, "SpecFloat64Array", strlen("SpecFloat64Array")))
        return SpecFloat64Array;
    if (!strncmp(speculation, "SpecTypedArrayView", strlen("SpecTypedArrayView")))
        return SpecTypedArrayView;
    if (!strncmp(speculation, "SpecDirectArguments", strlen("SpecDirectArguments")))
        return SpecDirectArguments;
    if (!strncmp(speculation, "SpecScopedArguments", strlen("SpecScopedArguments")))
        return SpecScopedArguments;
    if (!strncmp(speculation, "SpecStringObject", strlen("SpecStringObject")))
        return SpecStringObject;
    if (!strncmp(speculation, "SpecRegExpObject", strlen("SpecRegExpObject")))
        return SpecRegExpObject;
    if (!strncmp(speculation, "SpecDateObject", strlen("SpecDateObject")))
        return SpecDateObject;
    if (!strncmp(speculation, "SpecPromiseObject", strlen("SpecPromiseObject")))
        return SpecPromiseObject;
    if (!strncmp(speculation, "SpecMapObject", strlen("SpecMapObject")))
        return SpecMapObject;
    if (!strncmp(speculation, "SpecSetObject", strlen("SpecSetObject")))
        return SpecSetObject;
    if (!strncmp(speculation, "SpecWeakMapObject", strlen("SpecWeakMapObject")))
        return SpecWeakMapObject;
    if (!strncmp(speculation, "SpecWeakSetObject", strlen("SpecWeakSetObject")))
        return SpecWeakSetObject;
    if (!strncmp(speculation, "SpecProxyObject", strlen("SpecProxyObject")))
        return SpecProxyObject;
    if (!strncmp(speculation, "SpecDerivedArray", strlen("SpecDerivedArray")))
        return SpecDerivedArray;
    if (!strncmp(speculation, "SpecDataViewObject", strlen("SpecDataViewObject")))
        return SpecDataViewObject;
    if (!strncmp(speculation, "SpecObjectOther", strlen("SpecObjectOther")))
        return SpecObjectOther;
    if (!strncmp(speculation, "SpecObject", strlen("SpecObject")))
        return SpecObject;
    if (!strncmp(speculation, "SpecStringIdent", strlen("SpecStringIdent")))
        return SpecStringIdent;
    if (!strncmp(speculation, "SpecStringVar", strlen("SpecStringVar")))
        return SpecStringVar;
    if (!strncmp(speculation, "SpecString", strlen("SpecString")))
        return SpecString;
    if (!strncmp(speculation, "SpecSymbol", strlen("SpecSymbol")))
        return SpecSymbol;
    if (!strncmp(speculation, "SpecBigInt", strlen("SpecBigInt")))
        return SpecBigInt;
    if (!strncmp(speculation, "SpecCellOther", strlen("SpecCellOther")))
        return SpecCellOther;
    if (!strncmp(speculation, "SpecCell", strlen("SpecCell")))
        return SpecCell;
    if (!strncmp(speculation, "SpecBoolInt32", strlen("SpecBoolInt32")))
        return SpecBoolInt32;
    if (!strncmp(speculation, "SpecNonBoolInt32", strlen("SpecNonBoolInt32")))
        return SpecNonBoolInt32;
    if (!strncmp(speculation, "SpecInt32Only", strlen("SpecInt32Only")))
        return SpecInt32Only;
    if (!strncmp(speculation, "SpecInt32AsInt52", strlen("SpecInt32AsInt52")))
        return SpecInt32AsInt52;
    if (!strncmp(speculation, "SpecNonInt32AsInt52", strlen("SpecNonInt32AsInt52")))
        return SpecNonInt32AsInt52;
    if (!strncmp(speculation, "SpecInt52Any", strlen("SpecInt52Any")))
        return SpecInt52Any;
    if (!strncmp(speculation, "SpecIntAnyFormat", strlen("SpecIntAnyFormat")))
        return SpecIntAnyFormat;
    if (!strncmp(speculation, "SpecAnyIntAsDouble", strlen("SpecAnyIntAsDouble")))
        return SpecAnyIntAsDouble;
    if (!strncmp(speculation, "SpecNonIntAsDouble", strlen("SpecNonIntAsDouble")))
        return SpecNonIntAsDouble;
    if (!strncmp(speculation, "SpecDoubleReal", strlen("SpecDoubleReal")))
        return SpecDoubleReal;
    if (!strncmp(speculation, "SpecDoublePureNaN", strlen("SpecDoublePureNaN")))
        return SpecDoublePureNaN;
    if (!strncmp(speculation, "SpecDoubleImpureNaN", strlen("SpecDoubleImpureNaN")))
        return SpecDoubleImpureNaN;
    if (!strncmp(speculation, "SpecDoubleNaN", strlen("SpecDoubleNaN")))
        return SpecDoubleNaN;
    if (!strncmp(speculation, "SpecBytecodeDouble", strlen("SpecBytecodeDouble")))
        return SpecBytecodeDouble;
    if (!strncmp(speculation, "SpecFullDouble", strlen("SpecFullDouble")))
        return SpecFullDouble;
    if (!strncmp(speculation, "SpecBytecodeRealNumber", strlen("SpecBytecodeRealNumber")))
        return SpecBytecodeRealNumber;
    if (!strncmp(speculation, "SpecFullRealNumber", strlen("SpecFullRealNumber")))
        return SpecFullRealNumber;
    if (!strncmp(speculation, "SpecBytecodeNumber", strlen("SpecBytecodeNumber")))
        return SpecBytecodeNumber;
    if (!strncmp(speculation, "SpecFullNumber", strlen("SpecFullNumber")))
        return SpecFullNumber;
    if (!strncmp(speculation, "SpecBoolean", strlen("SpecBoolean")))
        return SpecBoolean;
    if (!strncmp(speculation, "SpecOther", strlen("SpecOther")))
        return SpecOther;
    if (!strncmp(speculation, "SpecMisc", strlen("SpecMisc")))
        return SpecMisc;
    if (!strncmp(speculation, "SpecHeapTop", strlen("SpecHeapTop")))
        return SpecHeapTop;
    if (!strncmp(speculation, "SpecPrimitive", strlen("SpecPrimitive")))
        return SpecPrimitive;
    if (!strncmp(speculation, "SpecEmpty", strlen("SpecEmpty")))
        return SpecEmpty;
    if (!strncmp(speculation, "SpecBytecodeTop", strlen("SpecBytecodeTop")))
        return SpecBytecodeTop;
    if (!strncmp(speculation, "SpecFullTop", strlen("SpecFullTop")))
        return SpecFullTop;
    if (!strncmp(speculation, "SpecCellCheck", strlen("SpecCellCheck")))
        return SpecCellCheck;
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

