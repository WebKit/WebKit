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
#include "TemporalPlainDate.h"
#include "TemporalPlainDateConstructor.h"
#include "TemporalPlainDatePrototype.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainDateTimeConstructor.h"
#include "TemporalPlainDateTimePrototype.h"
#include "TemporalPlainTime.h"
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
    JSGlobalObject* globalObject = temporalObject->globalObject();
    return TemporalCalendarConstructor::create(vm, TemporalCalendarConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalCalendarPrototype*>(globalObject->calendarStructure()->storedPrototypeObject()));
}

static JSValue createNowObject(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject();
    return TemporalNow::create(vm, TemporalNow::createStructure(vm, globalObject));
}

static JSValue createDurationConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject();
    return TemporalDurationConstructor::create(vm, TemporalDurationConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalDurationPrototype*>(globalObject->durationStructure()->storedPrototypeObject()));
}

static JSValue createInstantConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject();
    return TemporalInstantConstructor::create(vm, TemporalInstantConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalInstantPrototype*>(globalObject->instantStructure()->storedPrototypeObject()));
}

static JSValue createPlainDateConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    auto* globalObject = temporalObject->globalObject();
    return TemporalPlainDateConstructor::create(vm, TemporalPlainDateConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalPlainDatePrototype*>(globalObject->plainDateStructure()->storedPrototypeObject()));
}

static JSValue createPlainDateTimeConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    auto* globalObject = temporalObject->globalObject();
    return TemporalPlainDateTimeConstructor::create(vm, TemporalPlainDateTimeConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalPlainDateTimePrototype*>(globalObject->plainDateTimeStructure()->storedPrototypeObject()));
}

static JSValue createPlainTimeConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    auto* globalObject = temporalObject->globalObject();
    return TemporalPlainTimeConstructor::create(vm, TemporalPlainTimeConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalPlainTimePrototype*>(globalObject->plainTimeStructure()->storedPrototypeObject()));
}

static JSValue createTimeZoneConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject();
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
  PlainDateTime  createPlainDateTimeConstructor  DontEnum|PropertyCallback
  PlainTime      createPlainTimeConstructor      DontEnum|PropertyCallback
  TimeZone       createTimeZoneConstructor       DontEnum|PropertyCallback
@end
*/

const ClassInfo TemporalObject::s_info = { "Temporal"_s, &Base::s_info, &temporalObjectTable, nullptr, CREATE_METHOD_TABLE(TemporalObject) };

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
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

static StringView singularUnit(StringView unit)
{
    // Plurals are allowed, but thankfully they're all just a simple -s.
    return unit.endsWith('s') ? unit.left(unit.length() - 1) : unit;
}

double nonNegativeModulo(double x, double y)
{
    double result = std::fmod(x, y);
    if (!result)
        return 0;
    if (result < 0)
        result += y;
    return result;
}

// For use in error messages where a string value is potentially unbounded
WTF::String ellipsizeAt(unsigned maxLength, const WTF::String& string)
{
    if (string.length() <= maxLength)
        return string;
    return makeString(StringView(string).left(maxLength - 1), horizontalEllipsis);
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

    if (singular == "year"_s)
        return TemporalUnit::Year;
    if (singular == "month"_s)
        return TemporalUnit::Month;
    if (singular == "week"_s)
        return TemporalUnit::Week;
    if (singular == "day"_s)
        return TemporalUnit::Day;
    if (singular == "hour"_s)
        return TemporalUnit::Hour;
    if (singular == "minute"_s)
        return TemporalUnit::Minute;
    if (singular == "second"_s)
        return TemporalUnit::Second;
    if (singular == "millisecond"_s)
        return TemporalUnit::Millisecond;
    if (singular == "microsecond"_s)
        return TemporalUnit::Microsecond;
    if (singular == "nanosecond"_s)
        return TemporalUnit::Nanosecond;
    
    return std::nullopt;
}

// ToLargestTemporalUnit ( normalizedOptions, disallowedUnits, fallback [ , autoValue ] )
// https://tc39.es/proposal-temporal/#sec-temporal-tolargesttemporalunit
std::optional<TemporalUnit> temporalLargestUnit(JSGlobalObject* globalObject, JSObject* options, std::initializer_list<TemporalUnit> disallowedUnits, TemporalUnit autoValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String largestUnit = intlStringOption(globalObject, options, vm.propertyNames->largestUnit, { }, { }, { });
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

    String smallestUnit = intlStringOption(globalObject, options, vm.propertyNames->smallestUnit, { }, { }, { });
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

static constexpr std::initializer_list<TemporalUnit> disallowedUnits[] = {
    { },
    { TemporalUnit::Hour, TemporalUnit::Minute, TemporalUnit::Second, TemporalUnit::Millisecond, TemporalUnit::Microsecond, TemporalUnit::Nanosecond },
    { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day }
};

// https://tc39.es/proposal-temporal/#sec-temporal-getdifferencesettings
std::tuple<TemporalUnit, TemporalUnit, RoundingMode, double> extractDifferenceOptions(JSGlobalObject* globalObject, JSValue optionsValue, UnitGroup unitGroup, TemporalUnit defaultSmallestUnit, TemporalUnit defaultLargestUnit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto smallest = temporalSmallestUnit(globalObject, options, disallowedUnits[static_cast<uint8_t>(unitGroup)]);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalUnit smallestUnit = smallest.value_or(defaultSmallestUnit);
    defaultLargestUnit = std::min(defaultLargestUnit, smallestUnit);

    auto largest = temporalLargestUnit(globalObject, options, disallowedUnits[static_cast<uint8_t>(unitGroup)], defaultLargestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalUnit largestUnit = largest.value_or(defaultLargestUnit);

    if (smallestUnit < largestUnit) {
        throwRangeError(globalObject, scope, "smallestUnit must be smaller than largestUnit"_s);
        return { };
    }

    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    auto increment = temporalRoundingIncrement(globalObject, options, maximumRoundingIncrement(smallestUnit), false);
    RETURN_IF_EXCEPTION(scope, { });

    return { smallestUnit, largestUnit, roundingMode, increment };
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
        double doubleValue = std::trunc(value.asNumber());
        if (!(doubleValue >= 0 && doubleValue <= 9)) {
            throwRangeError(globalObject, scope, makeString("fractionalSecondDigits must be 'auto' or 0 through 9, not "_s, doubleValue));
            return std::nullopt;
        }

        return static_cast<unsigned>(doubleValue);
    }

    String stringValue = value.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    if (stringValue != "auto"_s)
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
    return intlOption<RoundingMode>(globalObject, options, globalObject->vm().propertyNames->roundingMode, {
        { "ceil"_s, RoundingMode::Ceil }, { "floor"_s, RoundingMode::Floor }, { "expand"_s, RoundingMode::Expand }, { "trunc"_s, RoundingMode::Trunc },
        { "halfCeil"_s, RoundingMode::HalfCeil }, { "halfFloor"_s, RoundingMode::HalfFloor }, { "halfExpand"_s, RoundingMode::HalfExpand }, { "halfTrunc"_s, RoundingMode::HalfTrunc }, { "halfEven"_s, RoundingMode::HalfEven }
        }, "roundingMode must be \"ceil\", \"floor\", \"expand\", \"trunc\", \"halfCeil\", \"halfFloor\", \"halfExpand\", \"halfTrunc\", or \"halfEven\""_s, fallback);
}

// https://tc39.es/proposal-temporal/#sec-temporal-negatetemporalroundingmode
RoundingMode negateTemporalRoundingMode(RoundingMode roundingMode)
{
    switch (roundingMode) {
    case RoundingMode::Ceil:
        return RoundingMode::Floor;
    case RoundingMode::Floor:
        return RoundingMode::Ceil;
    case RoundingMode::HalfCeil:
        return RoundingMode::HalfFloor;
    case RoundingMode::HalfFloor:
        return RoundingMode::HalfCeil;
    default:
        return roundingMode;
    }
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

static double doubleNumberOption(JSGlobalObject* globalObject, JSObject* options, PropertyName property, double defaultValue)
{
    // https://tc39.es/proposal-temporal/#sec-getoption
    // 'number' case.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!options)
        return defaultValue;

    JSValue value = options->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, 0);

    if (value.isUndefined())
        return defaultValue;

    double doubleValue = value.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, 0);

    if (std::isnan(doubleValue)) {
        throwRangeError(globalObject, scope, *property.publicName() + " is NaN"_s);
        return 0;
    }

    return doubleValue;
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

    double increment = doubleNumberOption(globalObject, options, vm.propertyNames->roundingIncrement, 1);
    RETURN_IF_EXCEPTION(scope, 0);

    if (increment < 1 || increment > maximum) {
        throwRangeError(globalObject, scope, "roundingIncrement is out of range"_s);
        return 0;
    }

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
    auto truncatedQuotient = std::trunc(quotient);
    if (truncatedQuotient == quotient)
        return truncatedQuotient * increment;

    auto isNegative = quotient < 0;
    auto expandedQuotient = isNegative ? truncatedQuotient - 1 : truncatedQuotient + 1;

    if (mode >= RoundingMode::HalfCeil) {
        auto unsignedFractionalPart = std::abs(quotient - truncatedQuotient);
        if (unsignedFractionalPart < 0.5)
            return truncatedQuotient * increment;
        if (unsignedFractionalPart > 0.5)
            return expandedQuotient * increment;
    }

    switch (mode) {
    case RoundingMode::Ceil:
    case RoundingMode::HalfCeil:
        return (isNegative ? truncatedQuotient : expandedQuotient) * increment;
    case RoundingMode::Floor:
    case RoundingMode::HalfFloor:
        return (isNegative ? expandedQuotient : truncatedQuotient) * increment;
    case RoundingMode::Expand:
    case RoundingMode::HalfExpand:
        return expandedQuotient * increment;
    case RoundingMode::Trunc:
    case RoundingMode::HalfTrunc:
        return truncatedQuotient * increment;
    case RoundingMode::HalfEven:
        return (!std::fmod(truncatedQuotient, 2) ? truncatedQuotient : expandedQuotient) * increment;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// RoundNumberToIncrementAsIfPositive ( x, increment, roundingMode )
// https://tc39.es/proposal-temporal/#sec-temporal-roundnumbertoincrementasifpositive
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
    case RoundingMode::Expand:
        if (!sign)
            quotient++;
        break;
    case RoundingMode::Floor:
    case RoundingMode::Trunc:
        if (sign)
            quotient--;
        break;
    case RoundingMode::HalfCeil:
    case RoundingMode::HalfExpand:
        // "half toward infinity"
        if (!sign && remainder * 2 >= increment)
            quotient++;
        else if (sign && -remainder * 2 > increment)
            quotient--;
        break;
    case RoundingMode::HalfFloor:
    case RoundingMode::HalfTrunc:
        // "half toward zero"
        if (!sign && remainder * 2 > increment)
            quotient++;
        else if (sign && -remainder * 2 >= increment)
            quotient--;
        break;
    case RoundingMode::HalfEven:
        // "half toward even multiple of increment"
        if (!sign && (remainder * 2 > increment || (remainder * 2 == increment && quotient % 2 == 1)))
            quotient++;
        else if (sign && (-remainder * 2 > increment || (-remainder * 2 == increment && -quotient % 2 == 1)))
            quotient--;
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

// https://tc39.es/proposal-temporal/#sec-temporal-rejectobjectwithcalendarortimezone
void rejectObjectWithCalendarOrTimeZone(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: Also support PlainMonthDay, PlainYearMonth, ZonedDateTime.
    if (object->inherits<TemporalPlainDate>()
        || object->inherits<TemporalPlainDateTime>()
        || object->inherits<TemporalPlainTime>()) {
        throwTypeError(globalObject, scope, "argument object must not have calendar or timeZone property"_s);
        return;
    }

    auto calendar = object->get(globalObject, vm.propertyNames->calendar);    
    RETURN_IF_EXCEPTION(scope, void());
    if (!calendar.isUndefined()) {
        throwTypeError(globalObject, scope, "argument object must not have calendar property"_s);
        return;
    }

    auto timeZone = object->get(globalObject, vm.propertyNames->timeZone);    
    RETURN_IF_EXCEPTION(scope, void());
    if (!timeZone.isUndefined()) {
        throwTypeError(globalObject, scope, "argument object must not have timeZone property"_s);
        return;
    }
}

} // namespace JSC
