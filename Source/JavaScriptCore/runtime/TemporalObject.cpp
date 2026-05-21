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

#include "FractionToDouble.h"
#include "FunctionPrototype.h"
#include "IntlObjectInlines.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "ObjectPrototype.h"
#include "Rounding.h"
#include "TemporalCalendar.h"
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
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainMonthDayConstructor.h"
#include "TemporalPlainMonthDayPrototype.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainTimeConstructor.h"
#include "TemporalPlainTimePrototype.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalPlainYearMonthConstructor.h"
#include "TemporalPlainYearMonthPrototype.h"
#include "TemporalZonedDateTime.h"
#include "TemporalZonedDateTimeConstructor.h"
#include "TemporalZonedDateTimePrototype.h"
#include <wtf/Int128.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/CharacterNames.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalObject);

static JSValue createNowObject(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    JSGlobalObject* globalObject = temporalObject->realm();
    return TemporalNow::create(vm, TemporalNow::createStructure(vm, globalObject));
}

static JSValue createDurationConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    JSGlobalObject* globalObject = temporalObject->realm();
    return TemporalDurationConstructor::create(vm, TemporalDurationConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), uncheckedDowncast<TemporalDurationPrototype>(globalObject->durationStructure()->storedPrototypeObject()));
}

static JSValue createInstantConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    JSGlobalObject* globalObject = temporalObject->realm();
    return TemporalInstantConstructor::create(vm, TemporalInstantConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), uncheckedDowncast<TemporalInstantPrototype>(globalObject->instantStructure()->storedPrototypeObject()));
}

static JSValue createPlainDateConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    auto* globalObject = temporalObject->realm();
    return TemporalPlainDateConstructor::create(vm, TemporalPlainDateConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), uncheckedDowncast<TemporalPlainDatePrototype>(globalObject->plainDateStructure()->storedPrototypeObject()));
}

static JSValue createPlainDateTimeConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    auto* globalObject = temporalObject->realm();
    return TemporalPlainDateTimeConstructor::create(vm, TemporalPlainDateTimeConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), uncheckedDowncast<TemporalPlainDateTimePrototype>(globalObject->plainDateTimeStructure()->storedPrototypeObject()));
}

static JSValue createPlainMonthDayConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    auto* globalObject = temporalObject->realm();
    return TemporalPlainMonthDayConstructor::create(vm, TemporalPlainMonthDayConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), uncheckedDowncast<TemporalPlainMonthDayPrototype>(globalObject->plainMonthDayStructure()->storedPrototypeObject()));
}

static JSValue createPlainTimeConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    auto* globalObject = temporalObject->realm();
    return TemporalPlainTimeConstructor::create(vm, TemporalPlainTimeConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), uncheckedDowncast<TemporalPlainTimePrototype>(globalObject->plainTimeStructure()->storedPrototypeObject()));
}

static JSValue createPlainYearMonthConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    auto* globalObject = temporalObject->realm();
    return TemporalPlainYearMonthConstructor::create(vm, TemporalPlainYearMonthConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), uncheckedDowncast<TemporalPlainYearMonthPrototype>(globalObject->plainYearMonthStructure()->storedPrototypeObject()));
}

static JSValue createZonedDateTimeConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = uncheckedDowncast<TemporalObject>(object);
    auto* globalObject = temporalObject->realm();
    return TemporalZonedDateTimeConstructor::create(vm, TemporalZonedDateTimeConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), uncheckedDowncast<TemporalZonedDateTimePrototype>(globalObject->zonedDateTimeStructure()->storedPrototypeObject()));
}

} // namespace JSC

#include "TemporalObject.lut.h"

namespace JSC {

/* Source for TemporalObject.lut.h
@begin temporalObjectTable
  Duration       createDurationConstructor       DontEnum|PropertyCallback
  Instant        createInstantConstructor        DontEnum|PropertyCallback
  Now            createNowObject                 DontEnum|PropertyCallback
  PlainDate      createPlainDateConstructor      DontEnum|PropertyCallback
  PlainDateTime  createPlainDateTimeConstructor  DontEnum|PropertyCallback
  PlainTime      createPlainTimeConstructor      DontEnum|PropertyCallback
  PlainMonthDay  createPlainMonthDayConstructor  DontEnum|PropertyCallback
  PlainYearMonth createPlainYearMonthConstructor DontEnum|PropertyCallback
  ZonedDateTime  createZonedDateTimeConstructor  DontEnum|PropertyCallback
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

static StringView NODELETE singularUnit(StringView unit)
{
    // Plurals are allowed, but thankfully they're all just a simple -s.
    return unit.endsWith('s') ? unit.left(unit.length() - 1) : unit;
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
#define JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME(name, capitalizedName) \
    case TemporalUnit::capitalizedName:                               \
        return vm.propertyNames->name##s;
        JSC_TEMPORAL_UNITS(JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME)
#undef JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME
    }

    RELEASE_ASSERT_NOT_REACHED();
}

PropertyName temporalUnitSingularPropertyName(VM& vm, TemporalUnit unit)
{
    switch (unit) {
#define JSC_TEMPORAL_UNIT_SINGULAR_PROPERTY_NAME(name, capitalizedName) \
    case TemporalUnit::capitalizedName:                                 \
        return vm.propertyNames->name;
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

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalunitvaluedoption
Variant<TemporalAuto, std::optional<TemporalUnit>> getTemporalUnitValuedOption(JSGlobalObject* globalObject, JSObject* options, PropertyName key)
{

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String unit = intlStringOption(globalObject, options, key, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    if (!unit)
        return std::nullopt;

    if (unit == "auto"_s)
        return TemporalAuto::Auto;

    auto unitType = temporalUnitType(unit);
    if (!unitType) [[unlikely]] {
        throwRangeError(globalObject, scope, "invalid Temporal unit"_s);
        return std::nullopt;
    }

    return unitType;
}

// https://tc39.es/proposal-temporal/#sec-temporal-validatetemporalunitvaluedoption
void validateTemporalUnitValue(JSGlobalObject* globalObject, Variant<TemporalAuto, std::optional<TemporalUnit>> unit, UnitGroup unitGroup, AllowedUnit extraValue, StringView valueName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isAbsentUnit(unit))
        return;
    if (extraValue == AllowedUnit::Auto && std::holds_alternative<TemporalAuto>(unit))
        return;
    TemporalUnit actualUnit = std::get<std::optional<TemporalUnit>>(unit).value();
    if (extraValue == AllowedUnit::Day && actualUnit == TemporalUnit::Day)
        return;
    if (actualUnit <= TemporalUnit::Day && ((unitGroup == UnitGroup::Date) || (unitGroup == UnitGroup::DateTime)))
        return;
    if (actualUnit > TemporalUnit::Day && ((unitGroup == UnitGroup::Time) || (unitGroup == UnitGroup::DateTime)))
        return;
    throwRangeError(globalObject, scope, makeString(valueName, " is a disallowed unit"_s));
}

// dividend must be a double because the maximum rounding increment for nanoseconds
// is greater than UINT32_MAX
// Therefore, rounding increment must be a double as well
void validateTemporalRoundingIncrement(JSGlobalObject* globalObject, double increment, std::optional<double> dividend, Inclusivity isInclusive)
{
    auto result = TemporalCore::validateTemporalRoundingIncrement(increment, dividend, isInclusive);
    if (!result) {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwRangeError(globalObject, scope, result.error().message);
    }
}

// https://tc39.es/proposal-temporal/#sec-temporal-getdifferencesettings
std::tuple<TemporalUnit, TemporalUnit, RoundingMode, double> extractDifferenceOptions(JSGlobalObject* globalObject, JSValue optionsValue, UnitGroup unitGroup, TemporalUnit fallbackSmallestUnit, TemporalUnit smallestLargestDefaultUnit)
{
    static const std::initializer_list<TemporalUnit> disallowedUnits[] = {
        { },
        { TemporalUnit::Hour, TemporalUnit::Minute, TemporalUnit::Second, TemporalUnit::Millisecond, TemporalUnit::Microsecond, TemporalUnit::Nanosecond },
        { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day }
    };

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto largestUnitMaybeAuto = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->largestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundingIncrement = temporalRoundingIncrement(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });
    Variant<TemporalAuto, std::optional<TemporalUnit>> smallestUnitMaybeAuto = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(std::holds_alternative<std::optional<TemporalUnit>>(smallestUnitMaybeAuto));
    auto smallestUnitOptional = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);

    validateTemporalUnitValue(globalObject, largestUnitMaybeAuto, unitGroup, AllowedUnit::Auto, "largestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });

    if (isAbsentUnit(largestUnitMaybeAuto))
        largestUnitMaybeAuto = TemporalAuto::Auto;

    auto disallowedUnitsList = disallowedUnits[static_cast<uint8_t>(unitGroup)];

    if (std::holds_alternative<std::optional<TemporalUnit>>(largestUnitMaybeAuto)) {
        auto largestUnitOptional = std::get<std::optional<TemporalUnit>>(largestUnitMaybeAuto);
        if (largestUnitOptional) {
            if (disallowedUnitsList.size() && std::ranges::find(disallowedUnitsList, largestUnitOptional.value()) != disallowedUnitsList.end()) [[unlikely]] {
                throwRangeError(globalObject, scope, "largestUnit is a disallowed unit"_s);
                return { };
            }
        }
    }

    validateTemporalUnitValue(globalObject, smallestUnitMaybeAuto, unitGroup, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });

    auto smallestUnit = smallestUnitOptional.value_or(fallbackSmallestUnit);

    if (disallowedUnitsList.size() && std::ranges::find(disallowedUnitsList, smallestUnit) != disallowedUnitsList.end()) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
        return { };
    }

    auto defaultLargestUnit = std::min(smallestLargestDefaultUnit, smallestUnit);
    auto largestUnit = defaultLargestUnit;
    if (std::holds_alternative<std::optional<TemporalUnit>>(largestUnitMaybeAuto)) {
        auto largestUnitOptional = std::get<std::optional<TemporalUnit>>(largestUnitMaybeAuto);
        ASSERT(largestUnitOptional);
        largestUnit = largestUnitOptional.value();
    }

    if (smallestUnit < largestUnit) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit must be smaller than largestUnit"_s);
        return { };
    }

    auto maximum = TemporalCore::maximumRoundingIncrement(smallestUnit);
    validateTemporalRoundingIncrement(globalObject, roundingIncrement, maximum, Inclusivity::Exclusive);
    RETURN_IF_EXCEPTION(scope, { });

    return { smallestUnit, largestUnit, roundingMode, roundingIncrement };
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
        double doubleValue = std::floor(value.asNumber());
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

    auto smallestUnitMaybeAuto = getTemporalUnitValuedOption(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(std::holds_alternative<std::optional<TemporalUnit>>(smallestUnitMaybeAuto));
    auto smallestUnit = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);

    auto disallowedUnits = { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day, TemporalUnit::Hour };
    if (disallowedUnits.size() && std::ranges::find(disallowedUnits, smallestUnit) != disallowedUnits.end()) {
        throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
        return { };
    }

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
    return intlOption<RoundingMode>(globalObject, options, globalObject->vm().propertyNames->roundingMode, { { "ceil"_s, RoundingMode::Ceil }, { "floor"_s, RoundingMode::Floor }, { "expand"_s, RoundingMode::Expand }, { "trunc"_s, RoundingMode::Trunc }, { "halfCeil"_s, RoundingMode::HalfCeil }, { "halfFloor"_s, RoundingMode::HalfFloor }, { "halfExpand"_s, RoundingMode::HalfExpand }, { "halfTrunc"_s, RoundingMode::HalfTrunc }, { "halfEven"_s, RoundingMode::HalfEven } }, "roundingMode must be \"ceil\", \"floor\", \"expand\", \"trunc\", \"halfCeil\", \"halfFloor\", \"halfExpand\", \"halfTrunc\", or \"halfEven\""_s, fallback);
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
        throwRangeError(globalObject, scope, makeString(property.publicName(), " is NaN"_s));
        return 0;
    }

    return doubleValue;
}

// ToTemporalRoundingIncrement ( normalizedOptions, dividend, inclusive )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalroundingincrement
double temporalRoundingIncrement(JSGlobalObject* globalObject, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double increment = doubleNumberOption(globalObject, options, vm.propertyNames->roundingIncrement, 1);
    RETURN_IF_EXCEPTION(scope, 0);

    return std::trunc(increment);
}

TemporalOverflow toTemporalOverflow(JSGlobalObject* globalObject, JSObject* options)
{
    return intlOption<TemporalOverflow>(globalObject, options, globalObject->vm().propertyNames->overflow,
        { { "constrain"_s, TemporalOverflow::Constrain }, { "reject"_s, TemporalOverflow::Reject } },
        "overflow must be either \"constrain\" or \"reject\""_s, TemporalOverflow::Constrain);
}

TemporalOverflow toTemporalOverflow(JSGlobalObject* globalObject, JSValue val)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, val);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, toTemporalOverflow(globalObject, options));
}

String toTemporalCalendarName(JSGlobalObject* globalObject, JSObject* options)
{
    return intlOption<String>(globalObject, options, globalObject->vm().propertyNames->calendarName,
        { { "auto"_s, "auto"_s }, { "always"_s, "always"_s }, { "never"_s, "never"_s }, { "critical"_s, "critical"_s } },
        "calendarName must be \"auto\", \"always\", \"never\", or \"critical\""_s, "auto"_s);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalcalendar
// Converts a JSValue (calendar ID string or Temporal date-like object) to a
// canonical calendar identifier string (e.g., "iso8601", "gregory").
// Accepts temporal date strings with [u-ca=...] annotations.
// Throws TypeError for non-string non-Temporal-object inputs.
// Throws RangeError for unrecognized calendar identifiers.
String toTemporalCalendarIdentifier(JSGlobalObject* globalObject, JSValue calendarLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (calendarLike.isString()) {
        auto calendarString = calendarLike.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        // Fast path: direct calendar ID (case-insensitive).
        if (auto calendarId = isBuiltinCalendar(calendarString))
            return intlAvailableCalendars()[*calendarId];

        // Try parsing as a temporal date string to extract the [u-ca=...] annotation.
        // Order: Date, then YearMonth, then MonthDay. YearMonth before MonthDay to
        // avoid the MonthDay parser asserting on year-looking strings like "2020-01".
        auto parsed = ISO8601::parseCalendarDateTime(calendarString, TemporalDateFormat::Date);
        if (!parsed)
            parsed = ISO8601::parseCalendarDateTime(calendarString, TemporalDateFormat::YearMonth);
        if (!parsed)
            parsed = ISO8601::parseCalendarDateTime(calendarString, TemporalDateFormat::MonthDay);
        if (!parsed) {
            // Per spec ParseTemporalCalendarString, also try TemporalTimeString.
            // A time string with no calendar annotation returns "iso8601".
            auto parsedTime = ISO8601::parseCalendarTime(calendarString);
            if (!parsedTime) {
                throwRangeError(globalObject, scope, makeString("invalid calendar identifier: "_s, calendarString));
                return { };
            }
            auto& calAnnotation = std::get<2>(parsedTime.value());
            if (!calAnnotation)
                return "iso8601"_s;
            if (auto calendarId = isBuiltinCalendar(StringView(*calAnnotation)))
                return intlAvailableCalendars()[*calendarId];
            throwRangeError(globalObject, scope, makeString("invalid calendar identifier: "_s, StringView(*calAnnotation)));
            return { };
        }
        auto& calendarAnnotation = std::get<3>(parsed.value());
        if (!calendarAnnotation)
            return "iso8601"_s;
        if (auto calendarId = isBuiltinCalendar(StringView(*calendarAnnotation)))
            return intlAvailableCalendars()[*calendarId];
        throwRangeError(globalObject, scope, makeString("invalid calendar identifier: "_s, StringView(*calendarAnnotation)));
        return { };
    }

    // Fast path: read [[Calendar]] from Temporal date-like objects without invoking
    // the "calendar" property getter (per spec ToTemporalCalendar step 1.b).
    if (calendarLike.isObject()) {
        JSObject* obj = asObject(calendarLike);
        if (obj->inherits<TemporalPlainDate>())
            return uncheckedDowncast<TemporalPlainDate>(obj)->calendarId();
        if (obj->inherits<TemporalPlainDateTime>())
            return uncheckedDowncast<TemporalPlainDateTime>(obj)->calendarId();
        if (obj->inherits<TemporalPlainYearMonth>())
            return uncheckedDowncast<TemporalPlainYearMonth>(obj)->calendarId();
        if (obj->inherits<TemporalPlainMonthDay>())
            return uncheckedDowncast<TemporalPlainMonthDay>(obj)->calendarId();
        if (obj->inherits<TemporalZonedDateTime>())
            return uncheckedDowncast<TemporalZonedDateTime>(obj)->calendarId();
    }

    throwTypeError(globalObject, scope, "calendar argument must be a string or a Temporal date-like object"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldisambiguation
TemporalDisambiguation toTemporalDisambiguation(JSGlobalObject* globalObject, JSObject* options)
{
    return intlOption<TemporalDisambiguation>(globalObject, options, globalObject->vm().propertyNames->disambiguation,
        { { "compatible"_s, TemporalDisambiguation::Compatible }, { "earlier"_s, TemporalDisambiguation::Earlier },
            { "later"_s, TemporalDisambiguation::Later }, { "reject"_s, TemporalDisambiguation::Reject } },
        "disambiguation must be one of \"compatible\", \"earlier\", \"later\", or \"reject\""_s,
        TemporalDisambiguation::Compatible);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaloffset
TemporalOffsetDisambiguation toTemporalOffset(JSGlobalObject* globalObject, JSObject* options, TemporalOffsetDisambiguation fallback)
{
    return intlOption<TemporalOffsetDisambiguation>(globalObject, options, globalObject->vm().propertyNames->offset,
        { { "use"_s, TemporalOffsetDisambiguation::Use }, { "prefer"_s, TemporalOffsetDisambiguation::Prefer },
            { "ignore"_s, TemporalOffsetDisambiguation::Ignore }, { "reject"_s, TemporalOffsetDisambiguation::Reject } },
        "offset must be one of \"use\", \"prefer\", \"ignore\", or \"reject\""_s,
        fallback);
}

// https://tc39.es/proposal-temporal/#sec-temporal-rejectobjectwithcalendarortimezone
void rejectObjectWithCalendarOrTimeZone(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (object->inherits<TemporalPlainDate>()
        || object->inherits<TemporalPlainDateTime>()
        || object->inherits<TemporalPlainTime>()
        || object->inherits<TemporalPlainMonthDay>()
        || object->inherits<TemporalPlainYearMonth>()
        || object->inherits<TemporalZonedDateTime>()) {
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

void throwTemporalError(JSGlobalObject* globalObject, ThrowScope& scope, const TemporalError& error)
{
    if (error.kind == TemporalErrorKind::RangeError)
        throwRangeError(globalObject, scope, error.message);
    else
        throwTypeError(globalObject, scope, error.message);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
