/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "JSCJSValue.h"

#include "BigIntObject.h"
#include "BooleanConstructor.h"
#include "CustomGetterSetter.h"
#include "GetterSetter.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "NumberObject.h"
#include "TypeError.h"

namespace JSC {

// ECMA 9.4
double JSValue::toInteger(JSGlobalObject* globalObject) const
{
    if (isInt32())
        return asInt32();
    double d = toNumber(globalObject);
    return std::isnan(d) ? 0.0 : trunc(d);
}

double JSValue::toIntegerPreserveNaN(JSGlobalObject* globalObject) const
{
    if (isInt32())
        return asInt32();
    return trunc(toNumber(globalObject));
}

double JSValue::toLength(JSGlobalObject* globalObject) const
{
    // ECMA 7.1.15
    // http://www.ecma-international.org/ecma-262/6.0/#sec-tolength
    double d = toInteger(globalObject);
    if (d <= 0)
        return 0.0;
    if (std::isinf(d))
        return maxSafeInteger();
    return std::min(d, maxSafeInteger());
}

double JSValue::toNumberSlowCase(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!isInt32() && !isDouble());
    if (isCell())
        RELEASE_AND_RETURN(scope, asCell()->toNumber(globalObject));
#if USE(BIGINT32)
    if (isBigInt32()) {
        throwTypeError(globalObject, scope, "Conversion from 'BigInt' to 'number' is not allowed."_s);
        return 0.0;
    }
#endif
    if (isTrue())
        return 1.0;
    return isUndefined() ? PNaN : 0; // null and false both convert to 0.
}

Optional<double> JSValue::toNumberFromPrimitive() const
{
    if (isEmpty())
        return WTF::nullopt;
    if (isNumber())
        return asNumber();
    if (isBoolean())
        return asBoolean();
    if (isUndefined())
        return PNaN;
    if (isNull())
        return 0;
    return WTF::nullopt;
}

JSObject* JSValue::toObjectSlowCase(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(!isCell());

    if (isInt32() || isDouble())
        return constructNumber(globalObject, asValue());
    if (isTrue() || isFalse())
        return constructBooleanFromImmediateBoolean(globalObject, asValue());
#if USE(BIGINT32)
    if (isBigInt32())
        return BigIntObject::create(vm, globalObject, *this);
#endif

    ASSERT(isUndefinedOrNull());
    throwException(globalObject, scope, createNotAnObjectError(globalObject, *this));
    return nullptr;
}

JSValue JSValue::toThisSlowCase(JSGlobalObject* globalObject, ECMAMode ecmaMode) const
{
    ASSERT(!isCell());

    if (ecmaMode.isStrict())
        return *this;

    if (isInt32() || isDouble())
        return constructNumber(globalObject, asValue());
    if (isTrue() || isFalse())
        return constructBooleanFromImmediateBoolean(globalObject, asValue());
#if USE(BIGINT32)
    if (isBigInt32())
        return BigIntObject::create(globalObject->vm(), globalObject, *this);
#endif

    ASSERT(isUndefinedOrNull());
    return globalObject->globalThis();
}

JSObject* JSValue::synthesizePrototype(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isCell()) {
        if (isString())
            return globalObject->stringPrototype();
        if (isHeapBigInt())
            return globalObject->bigIntPrototype();
        ASSERT(isSymbol());
        return globalObject->symbolPrototype();
    }

    if (isNumber())
        return globalObject->numberPrototype();
    if (isBoolean())
        return globalObject->booleanPrototype();
#if USE(BIGINT32)
    if (isBigInt32())
        return globalObject->bigIntPrototype();
#endif

    ASSERT(isUndefinedOrNull());
    throwException(globalObject, scope, createNotAnObjectError(globalObject, *this));
    return nullptr;
}

// ECMA 8.7.2
bool JSValue::putToPrimitive(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (Optional<uint32_t> index = parseIndex(propertyName))
        RELEASE_AND_RETURN(scope, putToPrimitiveByIndex(globalObject, index.value(), value, slot.isStrictMode()));

    // Check if there are any setters or getters in the prototype chain
    JSObject* obj = synthesizePrototype(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !obj);
    if (UNLIKELY(!obj))
        return false;
    JSValue prototype;
    if (propertyName != vm.propertyNames->underscoreProto) {
        while (true) {
            Structure* structure = obj->structure(vm);
            if (structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || structure->typeInfo().hasPutPropertySecurityCheck())
                break;
            prototype = obj->getPrototype(vm, globalObject);
            RETURN_IF_EXCEPTION(scope, false);

            if (prototype.isNull())
                return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);
            obj = asObject(prototype);
        }
    }

    for (; ; obj = asObject(prototype)) {
        Structure* structure = obj->structure(vm);
        if (UNLIKELY(structure->typeInfo().hasPutPropertySecurityCheck())) {
            obj->methodTable(vm)->doPutPropertySecurityCheck(obj, globalObject, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, false);
        }
        unsigned attributes;
        PropertyOffset offset = structure->get(vm, propertyName, attributes);
        if (offset != invalidOffset) {
            if (attributes & PropertyAttribute::ReadOnly)
                return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);

            JSValue gs = obj->getDirect(offset);
            if (gs.isGetterSetter())
                RELEASE_AND_RETURN(scope, callSetter(globalObject, *this, gs, value, slot.isStrictMode() ? ECMAMode::strict() : ECMAMode::sloppy()));

            if (gs.isCustomGetterSetter())
                return callCustomSetter(globalObject, gs, attributes & PropertyAttribute::CustomAccessor, obj, slot.thisValue(), value);

            // If there's an existing property on the object or one of its 
            // prototypes it should be replaced, so break here.
            break;
        }

        prototype = obj->getPrototype(vm, globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (prototype.isNull())
            break;
    }
    
    return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);
}

bool JSValue::putToPrimitiveByIndex(JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (propertyName > MAX_ARRAY_INDEX) {
        PutPropertySlot slot(*this, shouldThrow);
        return putToPrimitive(globalObject, Identifier::from(vm, propertyName), value, slot);
    }
    
    JSObject* prototype = synthesizePrototype(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !prototype);
    if (UNLIKELY(!prototype))
        return false;
    bool putResult = false;
    bool success = prototype->attemptToInterceptPutByIndexOnHoleForPrototype(globalObject, *this, propertyName, value, shouldThrow, putResult);
    RETURN_IF_EXCEPTION(scope, false);
    if (success)
        return putResult;
    
    return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);
}

void JSValue::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

void JSValue::dumpInContext(PrintStream& out, DumpContext* context) const
{
    dumpInContextAssumingStructure(
        out, context, (!!*this && isCell()) ? asCell()->structure() : nullptr);
}

void JSValue::dumpInContextAssumingStructure(
    PrintStream& out, DumpContext* context, Structure* structure) const
{
    if (!*this)
        out.print("<JSValue()>");
    else if (isInt32())
        out.printf("Int32: %d", asInt32());
    else if (isDouble()) {
#if USE(JSVALUE64)
        out.printf("Double: %lld, %lf", (long long)reinterpretDoubleToInt64(asDouble()), asDouble());
#else
        union {
            double asDouble;
            uint32_t asTwoInt32s[2];
        } u;
        u.asDouble = asDouble();
        out.printf("Double: %08x:%08x, %lf", u.asTwoInt32s[1], u.asTwoInt32s[0], asDouble());
#endif
    } else if (isCell()) {
        if (structure->classInfo()->isSubClassOf(JSString::info())) {
            JSString* string = asString(asCell());
            out.print("String");
            if (string->isRope())
                out.print(" (rope)");
            const StringImpl* impl = string->tryGetValueImpl();
            if (impl) {
                if (impl->isAtom())
                    out.print(" (atomic)");
                if (impl->isSymbol())
                    out.print(" (symbol)");
            } else
                out.print(" (unresolved)");
            if (string->is8Bit())
                out.print(",8Bit:(1)");
            else
                out.print(",8Bit:(0)");
            out.print(",length:(", string->length(), ")");
            out.print(": ", impl);
        } else if (structure->classInfo()->isSubClassOf(RegExp::info()))
            out.print("RegExp: ", *jsCast<RegExp*>(asCell()));
        else if (structure->classInfo()->isSubClassOf(Symbol::info()))
            out.print("Symbol: ", RawPointer(asCell()));
        else if (structure->classInfo()->isSubClassOf(Structure::info()))
            out.print("Structure: ", inContext(*jsCast<Structure*>(asCell()), context));
        else if (isHeapBigInt())
            out.print("BigInt[heap-allocated]: addr=", RawPointer(asCell()), ", length=", jsCast<JSBigInt*>(asCell())->length(), ", sign=", jsCast<JSBigInt*>(asCell())->sign());
        else if (structure->classInfo()->isSubClassOf(JSObject::info())) {
            out.print("Object: ", RawPointer(asCell()));
            out.print(" with butterfly ", RawPointer(asObject(asCell())->butterfly()));
            out.print(" (Structure ", inContext(*structure, context), ")");
        } else {
            out.print("Cell: ", RawPointer(asCell()));
            out.print(" (", inContext(*structure, context), ")");
        }
#if USE(JSVALUE64)
        out.print(", StructureID: ", asCell()->structureID());
#endif
    } else if (isTrue())
        out.print("True");
    else if (isFalse())
        out.print("False");
    else if (isNull())
        out.print("Null");
    else if (isUndefined())
        out.print("Undefined");
#if USE(BIGINT32)
    else if (isBigInt32())
        out.printf("BigInt[inline]: %d", bigInt32AsInt32());
#endif
    else
        out.print("INVALID");
}

void JSValue::dumpForBacktrace(PrintStream& out) const
{
    if (!*this)
        out.print("<JSValue()>");
    else if (isInt32())
        out.printf("%d", asInt32());
    else if (isDouble())
        out.printf("%lf", asDouble());
    else if (isCell()) {
        VM& vm = asCell()->vm();
        if (asCell()->inherits<JSString>(vm)) {
            JSString* string = asString(asCell());
            const StringImpl* impl = string->tryGetValueImpl();
            if (impl)
                out.print("\"", impl, "\"");
            else
                out.print("(unresolved string)");
        } else if (asCell()->inherits<Structure>(vm)) {
            out.print("Structure[ ", asCell()->structure()->classInfo()->className);
#if USE(JSVALUE64)
            out.print(" ID: ", asCell()->structureID());
#endif
            out.print("]: ", RawPointer(asCell()));
        } else {
            out.print("Cell[", asCell()->structure()->classInfo()->className);
#if USE(JSVALUE64)
            out.print(" ID: ", asCell()->structureID());
#endif
            out.print("]: ", RawPointer(asCell()));
        }
    } else if (isTrue())
        out.print("True");
    else if (isFalse())
        out.print("False");
    else if (isNull())
        out.print("Null");
    else if (isUndefined())
        out.print("Undefined");
#if USE(BIGINT32)
    else if (isBigInt32())
        out.printf("BigInt[inline]: %d", bigInt32AsInt32());
#endif
    else
        out.print("INVALID");
}

bool JSValue::isValidCallee()
{
    return asObject(asCell())->globalObject();
}

JSString* JSValue::toStringSlowCase(JSGlobalObject* globalObject, bool returnEmptyStringOnError) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto errorValue = [&] () -> JSString* {
        if (returnEmptyStringOnError)
            return jsEmptyString(vm);
        return nullptr;
    };
    
    ASSERT(!isString());
    if (isInt32()) {
        auto integer = asInt32();
        if (static_cast<unsigned>(integer) <= 9)
            return vm.smallStrings.singleCharacterString(integer + '0');
        return jsNontrivialString(vm, vm.numericStrings.add(integer));
    }
    if (isDouble())
        return jsString(vm, vm.numericStrings.add(asDouble()));
    if (isTrue())
        return vm.smallStrings.trueString();
    if (isFalse())
        return vm.smallStrings.falseString();
    if (isNull())
        return vm.smallStrings.nullString();
    if (isUndefined())
        return vm.smallStrings.undefinedString();
#if USE(BIGINT32)
    if (isBigInt32()) {
        auto integer = bigInt32AsInt32();
        if (static_cast<unsigned>(integer) <= 9)
            return vm.smallStrings.singleCharacterString(integer + '0');
        return jsNontrivialString(vm, vm.numericStrings.add(integer));
    }
#endif
    if (isHeapBigInt()) {
        JSBigInt* bigInt = asHeapBigInt();
        // FIXME: we should rather have two cases here: one-character string vs jsNonTrivialString for everything else.
        auto string = bigInt->toString(globalObject, 10);
        RETURN_IF_EXCEPTION(scope, errorValue());
        JSString* returnString = JSString::create(vm, string.releaseImpl().releaseNonNull());
        RETURN_IF_EXCEPTION(scope, errorValue());
        return returnString;
    }
    if (isSymbol()) {
        throwTypeError(globalObject, scope, "Cannot convert a symbol to a string"_s);
        return errorValue();
    }

    ASSERT(isCell());
    JSValue value = asCell()->toPrimitive(globalObject, PreferString);
    RETURN_IF_EXCEPTION(scope, errorValue());
    ASSERT(!value.isObject());
    JSString* result = value.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, errorValue());
    return result;
}

String JSValue::toWTFStringSlowCase(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (isInt32())
        return vm.numericStrings.add(asInt32());
    if (isDouble())
        return vm.numericStrings.add(asDouble());
    if (isTrue())
        return vm.propertyNames->trueKeyword.string();
    if (isFalse())
        return vm.propertyNames->falseKeyword.string();
    if (isNull())
        return vm.propertyNames->nullKeyword.string();
    if (isUndefined())
        return vm.propertyNames->undefinedKeyword.string();
    JSString* string = toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, string->value(globalObject));
}

#if !COMPILER(GCC_COMPATIBLE)
// This makes the argument opaque from the compiler.
NEVER_INLINE void ensureStillAliveHere(JSValue)
{
}
#endif

} // namespace JSC
