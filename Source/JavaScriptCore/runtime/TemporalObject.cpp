/*
 *  Copyright (C) 2021 Igalia S.L. All rights reserved.
 *  Copyright (C) 2021 Apple Inc. All rights reserved.
 *  Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "TemporalObject.h"

#include "FunctionPrototype.h"
#include "IntlObjectInlines.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "ObjectPrototype.h"
#include "TemporalCalendarConstructor.h"
#include "TemporalCalendarPrototype.h"
#include "TemporalDurationConstructor.h"
#include "TemporalDurationPrototype.h"
#include "TemporalNow.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalObject);

static JSValue createCalendarConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject(vm);
    return TemporalCalendarConstructor::create(vm, TemporalCalendarConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalCalendarPrototype*>(globalObject->calendarStructure()->storedPrototypeObject()));
}

static JSValue createNowObject(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject(vm);
    return TemporalNow::create(vm, TemporalNow::createStructure(vm, globalObject));
}

static JSValue createDurationConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject(vm);
    return TemporalDurationConstructor::create(vm, TemporalDurationConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalDurationPrototype*>(globalObject->durationStructure()->storedPrototypeObject()));
}

} // namespace JSC

#include "TemporalObject.lut.h"

namespace JSC {

/* Source for TemporalObject.lut.h
@begin temporalObjectTable
  Calendar       createCalendarConstructor       DontEnum|PropertyCallback
  Now            createNowObject                 DontEnum|PropertyCallback
  Duration       createDurationConstructor       DontEnum|PropertyCallback
@end
*/

const ClassInfo TemporalObject::s_info = { "Temporal", &Base::s_info, &temporalObjectTable, nullptr, CREATE_METHOD_TABLE(TemporalObject) };

TemporalObject::TemporalObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

TemporalObject* TemporalObject::create(VM& vm, Structure* structure)
{
    TemporalObject* object = new (NotNull, allocateCell<TemporalObject>(vm.heap)) TemporalObject(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* TemporalObject::createStructure(VM& vm, JSGlobalObject* globalObject)
{
    return Structure::create(vm, globalObject, globalObject->objectPrototype(), TypeInfo(ObjectType, StructureFlags), info());
}

void TemporalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

static StringView singularUnit(StringView unit)
{
    // Plurals are allowed, but thankfully they're all just a simple -s.
    return unit.endsWith("s") ? unit.left(unit.length() - 1) : unit;
}

std::optional<TemporalUnit> temporalUnitType(StringView unit)
{
    StringView singular = singularUnit(unit);

    if (singular == "year")
        return TemporalUnit::Year;
    if (singular == "month")
        return TemporalUnit::Month;
    if (singular == "week")
        return TemporalUnit::Week;
    if (singular == "day")
        return TemporalUnit::Day;
    if (singular == "hour")
        return TemporalUnit::Hour;
    if (singular == "minute")
        return TemporalUnit::Minute;
    if (singular == "second")
        return TemporalUnit::Second;
    if (singular == "millisecond")
        return TemporalUnit::Millisecond;
    if (singular == "microsecond")
        return TemporalUnit::Microsecond;
    if (singular == "nanosecond")
        return TemporalUnit::Nanosecond;
    
    return std::nullopt;
}

// ToLargestTemporalUnit ( normalizedOptions, disallowedUnits, fallback [ , autoValue ] )
// https://tc39.es/proposal-temporal/#sec-temporal-tolargesttemporalunit
std::optional<TemporalUnit> temporalLargestUnit(JSGlobalObject* globalObject, JSObject* options, std::initializer_list<TemporalUnit> disallowedUnits, TemporalUnit autoValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String largestUnit = intlStringOption(globalObject, options, vm.propertyNames->largestUnit, { }, nullptr, nullptr);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    if (!largestUnit)
        return std::nullopt;

    if (largestUnit == "auto"_s)
        return autoValue;

    auto unitType = temporalUnitType(largestUnit);
    if (!unitType) {
        throwRangeError(globalObject, scope, "largestUnit is an invalid Temporal unit"_s);
        return std::nullopt;
    }

    if (disallowedUnits.size() && std::find(disallowedUnits.begin(), disallowedUnits.end(), unitType.value()) != disallowedUnits.end()) {
        throwRangeError(globalObject, scope, "largestUnit is a disallowed unit"_s);
        return std::nullopt;
    }

    return unitType;
}

// ToSmallestTemporalUnit ( normalizedOptions, disallowedUnits, fallback )
// https://tc39.es/proposal-temporal/#sec-temporal-tosmallesttemporalunit
std::optional<TemporalUnit> temporalSmallestUnit(JSGlobalObject* globalObject, JSObject* options, std::initializer_list<TemporalUnit> disallowedUnits)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String smallestUnit = intlStringOption(globalObject, options, vm.propertyNames->smallestUnit, { }, nullptr, nullptr);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    if (!smallestUnit)
        return std::nullopt;

    auto unitType = temporalUnitType(smallestUnit);
    if (!unitType) {
        throwRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
        return std::nullopt;
    }

    if (disallowedUnits.size() && std::find(disallowedUnits.begin(), disallowedUnits.end(), unitType.value()) != disallowedUnits.end()) {
        throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
        return std::nullopt;
    }

    return unitType;
}

// GetStringOrNumberOption(normalizedOptions, "fractionalSecondDigits", « "auto" », 0, 9, "auto")
// https://tc39.es/proposal-temporal/#sec-getstringornumberoption
std::optional<unsigned> temporalFractionalSecondDigits(JSGlobalObject* globalObject, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!options)
        return std::nullopt;

    JSValue value = options->get(globalObject, vm.propertyNames->fractionalSecondDigits);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    if (value.isUndefined())
        return std::nullopt;

    if (value.isNumber()) {
        double doubleValue = value.asNumber();
        if (doubleValue < 0 || doubleValue > 9) {
            throwRangeError(globalObject, scope, "fractionalSecondDigits is out of range"_s);
            return std::nullopt;
        }

        return static_cast<unsigned>(doubleValue);
    }

    String stringValue = value.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    if (stringValue != "auto")
        throwRangeError(globalObject, scope, "fractionalSecondDigits is out of range"_s);

    return std::nullopt;
}

// ToSecondsStringPrecision ( normalizedOptions )
// https://tc39.es/proposal-temporal/#sec-temporal-tosecondsstringprecision
PrecisionData secondsStringPrecision(JSGlobalObject* globalObject, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto smallestUnit = temporalSmallestUnit(globalObject, options, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day, TemporalUnit::Hour, TemporalUnit::Minute });
    RETURN_IF_EXCEPTION(scope, { });

    if (smallestUnit) {
        switch (smallestUnit.value()) {
        case TemporalUnit::Second:
            return { 0, TemporalUnit::Second, 1 };
        case TemporalUnit::Millisecond:
            return { 3, TemporalUnit::Millisecond, 1 };
        case TemporalUnit::Microsecond:
            return { 6, TemporalUnit::Microsecond, 1 };
        case TemporalUnit::Nanosecond:
            return { 9, TemporalUnit::Nanosecond, 1 };
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
    }

    auto precision = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    if (!precision)
        return { std::nullopt, TemporalUnit::Nanosecond, 1 };

    unsigned digits = precision.value();
    if (!digits)
        return { 0, TemporalUnit::Second, 1 };

    if (digits <= 3)
        return { digits, TemporalUnit::Millisecond, std::pow(10, 3 - digits) };

    if (digits <= 6)
        return { digits, TemporalUnit::Microsecond, std::pow(10, 6 - digits) };

    ASSERT(digits <= 9);
    return { digits, TemporalUnit::Nanosecond, std::pow(10, 9 - digits) };
}

// MaximumTemporalDurationRoundingIncrement ( unit )
// https://tc39.es/proposal-temporal/#sec-temporal-maximumtemporaldurationroundingincrement
std::optional<double> maximumRoundingIncrement(TemporalUnit unit)
{
    if (unit <= TemporalUnit::Day)
        return std::nullopt;
    if (unit == TemporalUnit::Hour)
        return 24;
    if (unit <= TemporalUnit::Second)
        return 60;
    return 1000;
}

// ToTemporalRoundingIncrement ( normalizedOptions, dividend, inclusive )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalroundingincrement
double temporalRoundingIncrement(JSGlobalObject* globalObject, JSObject* options, std::optional<double> dividend, bool inclusive)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double maximum;
    if (!dividend)
        maximum = std::numeric_limits<double>::infinity();
    else if (inclusive)
        maximum = dividend.value();
    else if (dividend.value() > 1)
        maximum = dividend.value() - 1;
    else
        maximum = 1;

    double increment = intlNumberOption(globalObject, options, vm.propertyNames->roundingIncrement, 1, maximum, 1);
    RETURN_IF_EXCEPTION(scope, 0);

    increment = std::floor(increment);
    if (dividend && std::fmod(dividend.value(), increment)) {
        throwRangeError(globalObject, scope, makeString("roundingIncrement value does not divide "_s, dividend.value(), " evenly"_s));
        return 0;
    }

    return increment;
}

// RoundNumberToIncrement ( x, increment, roundingMode )
// https://tc39.es/proposal-temporal/#sec-temporal-roundnumbertoincrement
double roundNumberToIncrement(double x, double increment, RoundingMode mode)
{
    auto quotient = x / increment;
    switch (mode) {
    case RoundingMode::Ceil:
        return -std::floor(-quotient) * increment;
    case RoundingMode::Floor:
        return std::floor(quotient) * increment;
    case RoundingMode::Trunc:
        return std::trunc(quotient) * increment;
    case RoundingMode::HalfExpand:
        return std::round(quotient) * increment;
    }
}

} // namespace JSC
