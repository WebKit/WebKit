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
Variant<TemporalAuto, std::optional<TemporalUnit>> temporalUnitValued(JSGlobalObject* globalObject, JSObject* options, PropertyName key, TemporalUnitDefault defaultValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-3: Build allowedStrings (all singular + plural unit names + "auto") and call GetOption.
    // NOTE: intlStringOption with empty allowed list accepts any string; validation against
    // the unit table is deferred to temporalUnitType() below, preserving the same observable behavior.
    String unit = intlStringOption(globalObject, options, key, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    // Step 4: If value is undefined:
    if (!unit) {
        if (defaultValue == TemporalUnitDefault::Required) [[unlikely]] {
            throwRangeError(globalObject, scope, makeString('\'', key.publicName(), "' option is required"_s));
            return std::nullopt;
        }
        return std::nullopt;
    }

    // Step 5: If value is "auto", return ~auto~.
    if (unit == "auto"_s)
        return TemporalAuto::Auto;

    // Step 6: Return the Temporal unit for the string (singular or plural).
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

    // Step 1: If value is ~unset~, return ~unused~.
    if (isAbsentUnit(unit))
        return;
    // Step 2: If extraValues contains value, return ~unused~.
    if (extraValue == AllowedUnit::Auto && std::holds_alternative<TemporalAuto>(unit))
        return;
    if (extraValue == AllowedUnit::Day && std::holds_alternative<std::optional<TemporalUnit>>(unit)
        && std::get<std::optional<TemporalUnit>>(unit) == TemporalUnit::Day)
        return;
    // Step 3: Let category be the "Category" column for value in the units table.
    //         If there is no such row (e.g. ~auto~), throw a RangeError exception.
    if (std::holds_alternative<TemporalAuto>(unit)) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString(valueName, " cannot be \"auto\""_s));
        return;
    }
    TemporalUnit actualUnit = std::get<std::optional<TemporalUnit>>(unit).value();
    // Step 4: If category is ~date~ and unitGroup is ~date~ or ~datetime~, return ~unused~.
    if (actualUnit <= TemporalUnit::Day && ((unitGroup == UnitGroup::Date) || (unitGroup == UnitGroup::DateTime)))
        return;
    // Step 5: If category is ~time~ and unitGroup is ~time~ or ~datetime~, return ~unused~.
    if (actualUnit > TemporalUnit::Day && ((unitGroup == UnitGroup::Time) || (unitGroup == UnitGroup::DateTime)))
        return;
    // Step 6: Throw a RangeError exception.
    throwRangeError(globalObject, scope, makeString(valueName, " is a disallowed unit"_s));
}

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
std::tuple<TemporalUnit, TemporalUnit, RoundingMode, double> extractDifferenceOptions(JSGlobalObject* globalObject, JSValue optionsValue, UnitGroup unitGroup, TemporalUnit fallbackSmallestUnit, TemporalUnit smallestLargestDefaultUnit, DifferenceOperation operation)
{
    // disallowedUnits corresponds to the spec's _disallowedUnits_ parameter,
    // derived here from unitGroup rather than passed by the caller.
    static const std::initializer_list<TemporalUnit> disallowedUnits[] = {
        { },
        { TemporalUnit::Hour, TemporalUnit::Minute, TemporalUnit::Second, TemporalUnit::Millisecond, TemporalUnit::Microsecond, TemporalUnit::Nanosecond },
        { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day }
    };

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 1: NOTE: Read options in alphabetical order: largestUnit, roundingIncrement, roundingMode, smallestUnit.
    // Step 2: Let largestUnit be ? GetTemporalUnitValuedOption(options, "largestUnit", ~unset~).
    auto largestUnitMaybeAuto = temporalUnitValued(globalObject, options, vm.propertyNames->largestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 3: Let roundingIncrement be ? GetRoundingIncrementOption(options).
    auto roundingIncrement = temporalRoundingIncrement(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 4: Let roundingMode be ? GetRoundingModeOption(options, ~trunc~).
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 5: Let smallestUnit be ? GetTemporalUnitValuedOption(options, "smallestUnit", ~unset~).
    auto smallestUnitMaybeAuto = temporalUnitValued(globalObject, options, vm.propertyNames->smallestUnit);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: Perform ? ValidateTemporalUnitValue(largestUnit, unitGroup, « ~auto~ »).
    validateTemporalUnitValue(globalObject, largestUnitMaybeAuto, unitGroup, AllowedUnit::Auto, "largestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    // Step 7: If largestUnit is ~unset~, set largestUnit to ~auto~.
    if (isAbsentUnit(largestUnitMaybeAuto))
        largestUnitMaybeAuto = TemporalAuto::Auto;
    // Step 8: If disallowedUnits contains largestUnit, throw a RangeError exception.
    auto disallowedUnitsList = disallowedUnits[static_cast<uint8_t>(unitGroup)];
    if (std::holds_alternative<std::optional<TemporalUnit>>(largestUnitMaybeAuto)) {
        auto largestUnitOptional = std::get<std::optional<TemporalUnit>>(largestUnitMaybeAuto);
        if (largestUnitOptional && disallowedUnitsList.size()
            && std::ranges::find(disallowedUnitsList, largestUnitOptional.value()) != disallowedUnitsList.end()) [[unlikely]] {
            throwRangeError(globalObject, scope, "largestUnit is a disallowed unit"_s);
            return { };
        }
    }

    // Step 9: Perform ? ValidateTemporalUnitValue(smallestUnit, unitGroup).
    validateTemporalUnitValue(globalObject, smallestUnitMaybeAuto, unitGroup, AllowedUnit::None, "smallestUnit"_s);
    RETURN_IF_EXCEPTION(scope, { });
    auto smallestUnitOptional = std::get<std::optional<TemporalUnit>>(smallestUnitMaybeAuto);
    // Step 10: If smallestUnit is ~unset~, set smallestUnit to fallbackSmallestUnit.
    auto smallestUnit = smallestUnitOptional.value_or(fallbackSmallestUnit);
    // Step 11: If disallowedUnits contains smallestUnit, throw a RangeError exception.
    if (disallowedUnitsList.size() && std::ranges::find(disallowedUnitsList, smallestUnit) != disallowedUnitsList.end()) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
        return { };
    }

    // Step 12: Let defaultLargestUnit be LargerOfTwoTemporalUnits(smallestLargestDefaultUnit, smallestUnit).
    auto defaultLargestUnit = std::min(smallestLargestDefaultUnit, smallestUnit);
    // Step 13: If largestUnit is ~auto~, set largestUnit to defaultLargestUnit.
    auto largestUnit = defaultLargestUnit;
    if (std::holds_alternative<std::optional<TemporalUnit>>(largestUnitMaybeAuto)) {
        auto largestUnitOptional = std::get<std::optional<TemporalUnit>>(largestUnitMaybeAuto);
        ASSERT(largestUnitOptional);
        largestUnit = largestUnitOptional.value();
    }

    // Step 14: If LargerOfTwoTemporalUnits(largestUnit, smallestUnit) is not largestUnit, throw a RangeError exception.
    if (smallestUnit < largestUnit) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit must be smaller than largestUnit"_s);
        return { };
    }

    // Step 15: Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
    // Step 16: If maximum is not ~unset~, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
    auto maximum = TemporalCore::maximumRoundingIncrement(smallestUnit);
    if (maximum) {
        validateTemporalRoundingIncrement(globalObject, roundingIncrement, static_cast<double>(*maximum), Inclusivity::Exclusive);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 17: If operation is ~since~, set roundingMode to NegateRoundingMode(roundingMode).
    if (operation == DifferenceOperation::Since)
        roundingMode = TemporalCore::negateTemporalRoundingMode(roundingMode);
    // Step 18: Return the Record.
    return { smallestUnit, largestUnit, roundingMode, roundingIncrement };
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalfractionalseconddigitsoption
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
        if (!(doubleValue >= 0 && doubleValue <= 9)) [[unlikely]] {
            throwRangeError(globalObject, scope, makeString("fractionalSecondDigits must be 'auto' or 0 through 9, not "_s, doubleValue));
            return std::nullopt;
        }

        return static_cast<unsigned>(doubleValue);
    }

    String stringValue = value.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    if (stringValue != "auto"_s) [[unlikely]]
        throwRangeError(globalObject, scope, makeString("fractionalSecondDigits must be 'auto' or 0 through 9, not "_s, ellipsizeAt(100, stringValue)));

    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal-tosecondsstringprecisionrecord
PrecisionData toSecondsStringPrecisionRecord(std::optional<TemporalUnit> smallestUnit, std::optional<unsigned> fractionalDigitCount)
{
    // Steps 1-5: If smallestUnit is a specific time unit, return the corresponding fixed precision.
    if (smallestUnit == TemporalUnit::Minute)
        return { { Precision::Minute, 0 }, TemporalUnit::Minute, 1 };
    if (smallestUnit == TemporalUnit::Second)
        return { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 };
    if (smallestUnit == TemporalUnit::Millisecond)
        return { { Precision::Fixed, 3 }, TemporalUnit::Millisecond, 1 };
    if (smallestUnit == TemporalUnit::Microsecond)
        return { { Precision::Fixed, 6 }, TemporalUnit::Microsecond, 1 };
    if (smallestUnit == TemporalUnit::Nanosecond)
        return { { Precision::Fixed, 9 }, TemporalUnit::Nanosecond, 1 };

    // Step 6: Assert: smallestUnit is ~unset~.
    ASSERT(!smallestUnit);

    // Step 7: If fractionalDigitCount is ~auto~, return auto/nanosecond/1.
    if (!fractionalDigitCount)
        return { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 };

    auto pow10 = [](unsigned n) -> unsigned {
        unsigned r = 1;
        for (unsigned i = 0; i < n; i++)
            r *= 10;
        return r;
    };

    // Steps 8-10: Map digit count to precision record.
    unsigned d = *fractionalDigitCount;
    if (!d)
        return { { Precision::Fixed, 0 }, TemporalUnit::Second, 1 };
    if (d <= 3)
        return { { Precision::Fixed, d }, TemporalUnit::Millisecond, pow10(3 - d) };
    if (d <= 6)
        return { { Precision::Fixed, d }, TemporalUnit::Microsecond, pow10(6 - d) };
    return { { Precision::Fixed, d }, TemporalUnit::Nanosecond, pow10(9 - d) };
}

// https://tc39.es/proposal-temporal/#sec-temporal-getroundingmodeoption
RoundingMode temporalRoundingMode(JSGlobalObject* globalObject, JSObject* options, RoundingMode fallback)
{
    return intlOption<RoundingMode>(globalObject, options, globalObject->vm().propertyNames->roundingMode, {
        { "ceil"_s, RoundingMode::Ceil }, { "floor"_s, RoundingMode::Floor }, { "expand"_s, RoundingMode::Expand }, { "trunc"_s, RoundingMode::Trunc },
        { "halfCeil"_s, RoundingMode::HalfCeil }, { "halfFloor"_s, RoundingMode::HalfFloor }, { "halfExpand"_s, RoundingMode::HalfExpand }, { "halfTrunc"_s, RoundingMode::HalfTrunc }, { "halfEven"_s, RoundingMode::HalfEven }
        }, "roundingMode must be \"ceil\", \"floor\", \"expand\", \"trunc\", \"halfCeil\", \"halfFloor\", \"halfExpand\", \"halfTrunc\", or \"halfEven\""_s, fallback);
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

    if (std::isnan(doubleValue)) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString(property.publicName(), " is NaN"_s));
        return 0;
    }

    return doubleValue;
}

// https://tc39.es/proposal-temporal/#sec-temporal-getroundingincrementoption
double temporalRoundingIncrement(JSGlobalObject* globalObject, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1-2: Get "roundingIncrement"; if undefined return 1.
    double increment = doubleNumberOption(globalObject, options, vm.propertyNames->roundingIncrement, 1);
    RETURN_IF_EXCEPTION(scope, 0);

    // Step 3: ToIntegerWithTruncation — if not finite, throw a RangeError.
    if (!std::isfinite(increment)) [[unlikely]] {
        throwRangeError(globalObject, scope, "roundingIncrement must be a finite integer"_s);
        return 0;
    }

    // Step 3 (cont): truncate.
    double integerIncrement = std::trunc(increment);

    // Step 4: If integerIncrement < 1 or integerIncrement > 10^9, throw a RangeError.
    if (integerIncrement < 1 || integerIncrement > 1e9) [[unlikely]] {
        throwRangeError(globalObject, scope, "roundingIncrement must be in the range 1 to 10^9 inclusive"_s);
        return 0;
    }

    // Step 5: Return integerIncrement.
    return integerIncrement;
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaloverflow
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

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalshowcalendarnameoption
String temporalShowCalendarName(JSGlobalObject* globalObject, JSObject* options)
{
    return intlOption<String>(globalObject, options, globalObject->vm().propertyNames->calendarName,
        { { "auto"_s, "auto"_s }, { "always"_s, "always"_s }, { "never"_s, "never"_s }, { "critical"_s, "critical"_s } },
        "calendarName must be \"auto\", \"always\", \"never\", or \"critical\""_s, "auto"_s);
}

// ToTemporalCalendarIdentifier ( temporalCalendarLike )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalcalendaridentifier
CalendarID toTemporalCalendarIdentifier(JSGlobalObject* globalObject, JSValue calendarLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If calendarLike is an Object with a Temporal-date-like internal slot, return its [[Calendar]].
    if (calendarLike.isObject()) {
        JSObject* obj = asObject(calendarLike);
        if (obj->inherits<TemporalPlainDate>())
            return uncheckedDowncast<TemporalPlainDate>(obj)->calendarID();
        if (obj->inherits<TemporalPlainDateTime>())
            return uncheckedDowncast<TemporalPlainDateTime>(obj)->calendarID();
        if (obj->inherits<TemporalPlainYearMonth>())
            return uncheckedDowncast<TemporalPlainYearMonth>(obj)->calendarID();
        if (obj->inherits<TemporalPlainMonthDay>())
            return uncheckedDowncast<TemporalPlainMonthDay>(obj)->calendarID();
        if (obj->inherits<TemporalZonedDateTime>())
            return uncheckedDowncast<TemporalZonedDateTime>(obj)->calendarID();
    }

    // Step 2: If calendarLike is not a String, throw TypeError.
    if (!calendarLike.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "calendar must be a string or Temporal object"_s);
        return iso8601CalendarID();
    }

    auto calendarString = calendarLike.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, iso8601CalendarID());

    // Fast path: bare builtin calendar id ("iso8601", "hebrew", ...). Skips
    // ParseTemporalCalendarString entirely since both that op and CanonicalizeCalendar
    // would just round-trip the string through the same isBuiltinCalendar lookup.
    if (auto calendarId = isBuiltinCalendar(calendarString))
        return *calendarId;

    // Step 3: identifier = ? ParseTemporalCalendarString(string).
    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporalcalendarstring
    //   PTCS Step 1: parseResult = Completion(ParseISODateTime(string,
    //     « TDS[+Zoned], TDS[~Zoned], TIS, TimeString, MonthDayString, YearMonthString »)).
    //   PTCS Step 2: If normal completion → calendar (default "iso8601").
    //   PTCS Steps 3-5: Otherwise, ParseText(string, AnnotationValue) fallback. We
    //     collapse 3-5 with CanonicalizeCalendar below: any input that's not a builtin
    //     name (fast path failed above) and isn't a Temporal date string can never pass
    //     CanonicalizeCalendar, so throw immediately.
    auto parsed = ISO8601::parseISODateTime(calendarString, {
        ISO8601::TemporalProduction::DateTimeZoned,
        ISO8601::TemporalProduction::DateTimeUnzoned,
        ISO8601::TemporalProduction::Instant,
        ISO8601::TemporalProduction::YearMonth,
        ISO8601::TemporalProduction::MonthDay,
        ISO8601::TemporalProduction::Time,
    });
    if (!parsed) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("invalid calendar identifier: "_s, calendarString));
        return iso8601CalendarID();
    }
    // PTCS Step 2.a-c: extract calendar (or default to "iso8601").
    String identifier = parsed->calendar
        ? StringView(*parsed->calendar).convertToASCIILowercase()
        : "iso8601"_s;

    // Step 4: Return ? CanonicalizeCalendar(identifier).
    if (auto calendarId = isBuiltinCalendar(identifier))
        return *calendarId;
    throwRangeError(globalObject, scope, makeString("invalid calendar identifier: "_s, identifier));
    return iso8601CalendarID();
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
    if (!calendar.isUndefined()) [[unlikely]] {
        throwTypeError(globalObject, scope, "argument object must not have calendar property"_s);
        return;
    }

    auto timeZone = object->get(globalObject, vm.propertyNames->timeZone);
    RETURN_IF_EXCEPTION(scope, void());
    if (!timeZone.isUndefined()) [[unlikely]] {
        throwTypeError(globalObject, scope, "argument object must not have timeZone property"_s);
        return;
    }
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltimezoneidentifier
std::optional<TimeZone> toTemporalTimeZoneIdentifier(JSGlobalObject* globalObject, JSValue item)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If item has [[InitializedTemporalZonedDateTime]], return its [[TimeZone]].
    if (auto* zdt = dynamicDowncast<TemporalZonedDateTime>(item))
        return zdt->timeZone();

    // Step 2: If item is not a String, throw TypeError.
    if (!item.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "time zone must be a string or ZonedDateTime"_s);
        return std::nullopt;
    }
    String tzString = asString(item)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    // Steps 3-5: ParseTimeZoneIdentifier; the resulting TimeZone already carries the
    // case-normalized, alias-preserving identifier (named) or canonical offset.
    auto parsed = ISO8601::parseTemporalTimeZoneIdentifier(tzString);
    if (!parsed) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));
        return std::nullopt;
    }

    return *parsed;
}

void throwTemporalError(JSGlobalObject* globalObject, ThrowScope& scope, const TemporalError& error)
{
    throwError(globalObject, scope, error.kind == TemporalErrorKind::RangeError ? ErrorType::RangeError : ErrorType::TypeError, error.message);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
