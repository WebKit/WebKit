/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "Arguments.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSCInlines.h"
#include "StringObject.h"
#include "ValueProfile.h"
#include <wtf/BoundsCheckedPointer.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

void dumpSpeculation(PrintStream& out, SpeculatedType value)
{
    if (value == SpecNone) {
        out.print("None");
        return;
    }
    
    StringPrintStream myOut;
    
    bool isTop = true;
    
    if ((value & SpecCell) == SpecCell)
        myOut.print("Cell");
    else {
        if ((value & SpecObject) == SpecObject)
            myOut.print("Object");
        else {
            if (value & SpecCellOther)
                myOut.print("Othercell");
            else
                isTop = false;
    
            if (value & SpecObjectOther)
                myOut.print("Otherobj");
            else
                isTop = false;
    
            if (value & SpecFinalObject)
                myOut.print("Final");
            else
                isTop = false;

            if (value & SpecArray)
                myOut.print("Array");
            else
                isTop = false;
    
            if (value & SpecInt8Array)
                myOut.print("Int8array");
            else
                isTop = false;
    
            if (value & SpecInt16Array)
                myOut.print("Int16array");
            else
                isTop = false;
    
            if (value & SpecInt32Array)
                myOut.print("Int32array");
            else
                isTop = false;
    
            if (value & SpecUint8Array)
                myOut.print("Uint8array");
            else
                isTop = false;

            if (value & SpecUint8ClampedArray)
                myOut.print("Uint8clampedarray");
            else
                isTop = false;
    
            if (value & SpecUint16Array)
                myOut.print("Uint16array");
            else
                isTop = false;
    
            if (value & SpecUint32Array)
                myOut.print("Uint32array");
            else
                isTop = false;
    
            if (value & SpecFloat32Array)
                myOut.print("Float32array");
            else
                isTop = false;
    
            if (value & SpecFloat64Array)
                myOut.print("Float64array");
            else
                isTop = false;
    
            if (value & SpecFunction)
                myOut.print("Function");
            else
                isTop = false;
    
            if (value & SpecArguments)
                myOut.print("Arguments");
            else
                isTop = false;
    
            if (value & SpecStringObject)
                myOut.print("Stringobject");
            else
                isTop = false;
        }

        if ((value & SpecString) == SpecString)
            myOut.print("String");
        else {
            if (value & SpecStringIdent)
                myOut.print("Stringident");
            else
                isTop = false;
            
            if (value & SpecStringVar)
                myOut.print("Stringvar");
            else
                isTop = false;
        }
    }
    
    if (value & SpecInt32)
        myOut.print("Int32");
    else
        isTop = false;
    
    if (value & SpecInt52)
        myOut.print("Int52");
        
    if ((value & SpecBytecodeDouble) == SpecBytecodeDouble)
        myOut.print("Bytecodedouble");
    else {
        if (value & SpecInt52AsDouble)
            myOut.print("Int52asdouble");
        else
            isTop = false;
        
        if (value & SpecNonIntAsDouble)
            myOut.print("Nonintasdouble");
        else
            isTop = false;
        
        if (value & SpecDoublePureNaN)
            myOut.print("Doublepurenan");
        else
            isTop = false;
    }
    
    if (value & SpecDoubleImpureNaN)
        out.print("Doubleimpurenan");
    
    if (value & SpecBoolean)
        myOut.print("Bool");
    else
        isTop = false;
    
    if (value & SpecOther)
        myOut.print("Other");
    else
        isTop = false;
    
    if (isTop)
        out.print("Top");
    else
        out.print(myOut.toCString());
    
    if (value & SpecEmpty)
        out.print("Empty");
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
    if (isArgumentsSpeculation(prediction))
        return "<Arguments>";
    if (isStringObjectSpeculation(prediction))
        return "<StringObject>";
    if (isStringOrStringObjectSpeculation(prediction))
        return "<StringOrStringObject>";
    if (isObjectSpeculation(prediction))
        return "<Object>";
    if (isCellSpeculation(prediction))
        return "<Cell>";
    if (isInt32Speculation(prediction))
        return "<Int32>";
    if (isInt52AsDoubleSpeculation(prediction))
        return "<Int52AsDouble>";
    if (isInt52Speculation(prediction))
        return "<Int52>";
    if (isMachineIntSpeculation(prediction))
        return "<MachineInt>";
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
    if (classInfo == JSFinalObject::info())
        return SpecFinalObject;
    
    if (classInfo == JSArray::info())
        return SpecArray;
    
    if (classInfo == Arguments::info())
        return SpecArguments;
    
    if (classInfo == StringObject::info())
        return SpecStringObject;
    
    if (classInfo->isSubClassOf(JSFunction::info()))
        return SpecFunction;
    
    if (isTypedView(classInfo->typedArrayStorageType))
        return speculationFromTypedArrayType(classInfo->typedArrayStorageType);
    
    if (classInfo->isSubClassOf(JSObject::info()))
        return SpecObjectOther;
    
    return SpecCellOther;
}

SpeculatedType speculationFromStructure(Structure* structure)
{
    if (structure->typeInfo().type() == StringType)
        return SpecString;
    return speculationFromClassInfo(structure->classInfo());
}

SpeculatedType speculationFromCell(JSCell* cell)
{
    if (JSString* string = jsDynamicCast<JSString*>(cell)) {
        if (const StringImpl* impl = string->tryGetValueImpl()) {
            if (impl->isAtomic())
                return SpecStringIdent;
        }
        return SpecStringVar;
    }
    return speculationFromStructure(cell->structure());
}

SpeculatedType speculationFromValue(JSValue value)
{
    if (value.isEmpty())
        return SpecEmpty;
    if (value.isInt32())
        return SpecInt32;
    if (value.isDouble()) {
        double number = value.asNumber();
        if (number != number)
            return SpecDoublePureNaN;
        if (value.isMachineInt())
            return SpecInt52AsDouble;
        return SpecNonIntAsDouble;
    }
    if (value.isCell())
        return speculationFromCell(value.asCell());
    if (value.isBoolean())
        return SpecBoolean;
    ASSERT(value.isUndefinedOrNull());
    return SpecOther;
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

SpeculatedType leastUpperBoundOfStrictlyEquivalentSpeculations(SpeculatedType type)
{
    if (type & SpecInteger)
        type |= SpecInteger;
    if (type & SpecString)
        type |= SpecString;
    return type;
}

bool valuesCouldBeEqual(SpeculatedType a, SpeculatedType b)
{
    a = leastUpperBoundOfStrictlyEquivalentSpeculations(a);
    b = leastUpperBoundOfStrictlyEquivalentSpeculations(b);
    
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

SpeculatedType typeOfDoubleSum(SpeculatedType a, SpeculatedType b)
{
    SpeculatedType result = a | b;
    // Impure NaN could become pure NaN during addition because addition may clear bits.
    if (result & SpecDoubleImpureNaN)
        result |= SpecDoublePureNaN;
    // Values could overflow, or fractions could become integers.
    if (result & SpecDoubleReal)
        result |= SpecDoubleReal;
    return result;
}

SpeculatedType typeOfDoubleDifference(SpeculatedType a, SpeculatedType b)
{
    return typeOfDoubleSum(a, b);
}

SpeculatedType typeOfDoubleProduct(SpeculatedType a, SpeculatedType b)
{
    return typeOfDoubleSum(a, b);
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
    // Impure NaN could become pure NaN because bits might get cleared.
    if (value & SpecDoubleImpureNaN)
        value |= SpecDoublePureNaN;
    // We could get negative zero, which mixes SpecInt52AsDouble and SpecNotIntAsDouble.
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

SpeculatedType typeOfDoubleFRound(SpeculatedType value)
{
    // We might lose bits, which leads to a NaN being purified.
    if (value & SpecDoubleImpureNaN)
        value |= SpecDoublePureNaN;
    // We might lose bits, which leads to a value becoming integer-representable.
    if (value & SpecNonIntAsDouble)
        value |= SpecInt52AsDouble;
    return value;
}

SpeculatedType typeOfDoubleBinaryOp(SpeculatedType a, SpeculatedType b)
{
    return polluteDouble(a | b);
}

SpeculatedType typeOfDoubleUnaryOp(SpeculatedType value)
{
    return polluteDouble(value);
}

} // namespace JSC

