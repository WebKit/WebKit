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
#include "TemporalInstantConstructor.h"
#include "TemporalInstantPrototype.h"
#include "TemporalNow.h"
#include "TemporalPlainDateConstructor.h"
#include "TemporalPlainDatePrototype.h"
#include "TemporalPlainTimeConstructor.h"
#include "TemporalPlainTimePrototype.h"
#include "TemporalTimeZoneConstructor.h"
#include "TemporalTimeZonePrototype.h"
#include <wtf/Int128.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/unicode/CharacterNames.h>

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

static JSValue createInstantConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject(vm);
    return TemporalInstantConstructor::create(vm, TemporalInstantConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalInstantPrototype*>(globalObject->instantStructure()->storedPrototypeObject()));
}

static JSValue createPlainDateConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    auto* globalObject = temporalObject->globalObject(vm);
    return TemporalPlainDateConstructor::create(vm, TemporalPlainDateConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalPlainDatePrototype*>(globalObject->plainDateStructure()->storedPrototypeObject()));
}

static JSValue createPlainTimeConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    auto* globalObject = temporalObject->globalObject(vm);
    return TemporalPlainTimeConstructor::create(vm, TemporalPlainTimeConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalPlainTimePrototype*>(globalObject->plainTimeStructure()->storedPrototypeObject()));
}

static JSValue createTimeZoneConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject(vm);
    return TemporalTimeZoneConstructor::create(vm, TemporalTimeZoneConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalTimeZonePrototype*>(globalObject->timeZoneStructure()->storedPrototypeObject()));
}

} // namespace JSC

#include "TemporalObject.lut.h"

namespace JSC {

/* Source for TemporalObject.lut.h
@begin temporalObjectTable
  Calendar       createCalendarConstructor       DontEnum|PropertyCallback
  Duration       createDurationConstructor       DontEnum|PropertyCallback
  Instant        createInstantConstructor        DontEnum|PropertyCallback
  Now            createNowObject                 DontEnum|PropertyCallback
  PlainDate      createPlainDateConstructor      DontEnum|PropertyCallback
  PlainTime      createPlainTimeConstructor      DontEnum|PropertyCallback
  TimeZone       createTimeZoneConstructor       DontEnum|PropertyCallback
@end
*/

const ClassInfo TemporalObject::s_info = { "Temporal", &Base::s_info, &temporalObjectTable, nullptr, CREATE_METHOD_TABLE(TemporalObject) };

TemporalObject::TemporalObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

TemporalObject* TemporalObject::create(VM& vm, Structure* structure)
{
    TemporalObject* object = new (NotNull, allocateCell<TemporalObject>(vm)) TemporalObject(vm, structure);
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

// For use in error messages where a string value is potentially unbounded
WTF::String ellipsizeAt(unsigned maxLength, const WTF::String& string)
{
    WTF::String copy { string };
    if (string.length() > maxLength) {
        copy.truncate(maxLength - 1);
        copy.append(horizontalEllipsis);
    }
    return copy;
}

PropertyName temporalUnitPluralPropertyName(VM& vm, TemporalUnit unit)
{
    switch (unit) {
#define JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME(name, capitalizedName) case TemporalUnit::capitalizedName: return vm.propertyNames->name##s;
        JSC_TEMPORAL_UNITS(JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME)
#undef JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME
    }

    RELEASE_ASSERT_NOT_REACHED();
}

PropertyName temporalUnitSingularPropertyName(VM& vm, TemporalUnit unit)
{
    switch (unit) {
#define JSC_TEMPORAL_UNIT_SINGULAR_PROPERTY_NAME(name, capitalizedName) case TemporalUnit::capitalizedName: return vm.propertyNames->name;
        JSC_TEMPORAL_UNITS(JSC_TEMPORAL_UNIT_SINGULAR_PROPERTY_NAME)
#undef JSC_TEMPORAL_UNIT_SINGULAR_PROPERTY_NAME
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// https://tc39.es/proposal-temporal/#table-temporal-temporaldurationlike-properties
const TemporalUnit temporalUnitsInTableOrder[numberOfTemporalUnits] = {
    TemporalUnit::Day,
    TemporalUnit::Hour,
    TemporalUnit::Microsecond,
    TemporalUnit::Millisecond,
    TemporalUnit::Minute,
    TemporalUnit::Month,
    TemporalUnit::Nanosecond,
    TemporalUnit::Second,
    TemporalUnit::Week,
    TemporalUnit::Year,
};

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
        if (!(doubleValue >= 0 && doubleValue <= 9)) {
            throwRangeError(globalObject, scope, makeString("fractionalSecondDigits must be 'auto' or 0 through 9, not "_s, doubleValue));
            return std::nullopt;
        }

        return static_cast<unsigned>(doubleValue);
    }

    String stringValue = value.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    if (stringValue != "auto")
        throwRangeError(globalObject, scope, makeString("fractionalSecondDigits must be 'auto' or 0 through 9, not "_s, ellipsizeAt(100, stringValue)));

    return std::nullopt;
}

// ToSecondsStringPrecision ( normalizedOptions )
// https://tc39.es/proposal-temporal/#sec-temporal-tosecondsstringprecision
PrecisionData secondsStringPrecision(JSGlobalObject* globalObject, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto smallestUnit = temporalSmallestUnit(globalObject, options, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day, TemporalUnit::Hour });
    RETURN_IF_EXCEPTION(scope, { });

    if (smallestUnit) {
        switch (smallestUnit.value()) {
        case TemporalUnit::Minute:
            return { { Precision::Minute, 0 }, TemporalUnit::Minute, 1 };
        case TemporalUnit::Second:
            return { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 };
        case TemporalUnit::Millisecond:
            return { { Precision::Fixed, 3 }, TemporalUnit::Millisecond, 1 };
        case TemporalUnit::Microsecond:
            return { { Precision::Fixed, 6 }, TemporalUnit::Microsecond, 1 };
        case TemporalUnit::Nanosecond:
            return { { Precision::Fixed, 9 }, TemporalUnit::Nanosecond, 1 };
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return { };
        }
    }

    auto precision = temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    if (!precision)
        return { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 };

    auto pow10Unsigned = [](unsigned n) -> unsigned {
        unsigned result = 1;
        for (unsigned i = 0; i < n; ++i)
            result *= 10;
        return result;
    };

    unsigned digits = precision.value();
    if (!digits)
        return { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 };

    if (digits <= 3)
        return { { Precision::Fixed, digits }, TemporalUnit::Millisecond, pow10Unsigned(3 - digits) };

    if (digits <= 6)
        return { { Precision::Fixed, digits }, TemporalUnit::Microsecond, pow10Unsigned(6 - digits) };

    ASSERT(digits <= 9);
    return { { Precision::Fixed, digits }, TemporalUnit::Nanosecond, pow10Unsigned(9 - digits) };
}

// ToTemporalRoundingMode ( normalizedOptions, fallback )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalroundingmode
RoundingMode temporalRoundingMode(JSGlobalObject* globalObject, JSObject* options, RoundingMode fallback)
{
    return intlOption<RoundingMode>(globalObject, options, globalObject->vm().propertyNames->roundingMode,
        { { "ceil"_s, RoundingMode::Ceil }, { "floor"_s, RoundingMode::Floor }, { "trunc"_s, RoundingMode::Trunc }, { "halfExpand"_s, RoundingMode::HalfExpand } },
        "roundingMode must be either \"ceil\", \"floor\", \"trunc\", or \"halfExpand\""_s, fallback);
}

void formatSecondsStringFraction(StringBuilder& builder, unsigned fraction, std::tuple<Precision, unsigned> precision)
{
    auto [precisionType, precisionValue] = precision;
    if ((precisionType == Precision::Auto && fraction) || (precisionType == Precision::Fixed && precisionValue)) {
        auto padded = makeString('.', pad('0', 9, fraction));
        if (precisionType == Precision::Fixed)
            builder.append(StringView(padded).left(padded.length() - (9 - precisionValue)));
        else {
            auto lengthWithoutTrailingZeroes = padded.length();
            while (padded[lengthWithoutTrailingZeroes - 1] == '0')
                lengthWithoutTrailingZeroes--;
            builder.append(StringView(padded).left(lengthWithoutTrailingZeroes));
        }
    }
}

// FormatSecondsStringPart ( second, millisecond, microsecond, nanosecond, precision )
// https://tc39.es/proposal-temporal/#sec-temporal-formatsecondsstringpart
void formatSecondsStringPart(StringBuilder& builder, unsigned second, unsigned fraction, PrecisionData precision)
{
    if (precision.unit == TemporalUnit::Minute)
        return;

    builder.append(':', pad('0', 2, second));
    formatSecondsStringFraction(builder, fraction, precision.precision);
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

    // They are not supported in Temporal right now.
    case RoundingMode::Expand:
    case RoundingMode::HalfCeil:
    case RoundingMode::HalfFloor:
    case RoundingMode::HalfTrunc:
    case RoundingMode::HalfEven:
        return std::trunc(quotient) * increment;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static Int128 abs(Int128 x)
{
    return x < 0 ? -x : x;
}

// Same as above, but with Int128
Int128 roundNumberToIncrement(Int128 x, Int128 increment, RoundingMode mode)
{
    ASSERT(increment);

    if (increment == 1)
        return x;

    Int128 quotient = x / increment;
    Int128 remainder = x % increment;
    if (!remainder)
        return x;

    bool sign = remainder < 0;
    switch (mode) {
    case RoundingMode::Ceil:
        if (!sign)
            quotient++;
        break;
    case RoundingMode::Floor:
        if (sign)
            quotient--;
        break;
    case RoundingMode::Trunc:
        break;
    case RoundingMode::HalfExpand:
        // "half up away from zero"
        if (abs(remainder * 2) >= increment)
            quotient += sign ? -1 : 1;
        break;
    // They are not supported in Temporal right now.
    case RoundingMode::Expand:
    case RoundingMode::HalfCeil:
    case RoundingMode::HalfFloor:
    case RoundingMode::HalfTrunc:
    case RoundingMode::HalfEven:
        break;
    }
    return quotient * increment;
}

TemporalOverflow toTemporalOverflow(JSGlobalObject* globalObject, JSObject* options)
{
    return intlOption<TemporalOverflow>(globalObject, options, globalObject->vm().propertyNames->overflow,
        { { "constrain"_s, TemporalOverflow::Constrain }, { "reject"_s, TemporalOverflow::Reject } },
        "overflow must be either \"constrain\" or \"reject\""_s, TemporalOverflow::Constrain);
}

} // namespace JSC
