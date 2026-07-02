/*
 *  Copyright (C) 2021 Igalia, S.L. All rights reserved.
 *  Copyright (C) 2021 Sony Interactive Entertainment Inc.
 *  Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/IntlObject.h>
#include <JavaScriptCore/JSCTimeZone.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalEnums.h>

namespace JSC {

class TemporalObject final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(TemporalObject, Base);
        return &vm.plainObjectSpace();
    }

    static TemporalObject* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*);

    DECLARE_INFO;

private:
    TemporalObject(VM&, Structure*);
    void finishCreation(VM&);
};

enum class TemporalAuto : bool {
    Auto
};

enum class Precision : uint8_t {
    Minute,
    Fixed,
    Auto,
};

struct PrecisionData {
    std::tuple<Precision, unsigned> precision;
    TemporalUnit unit;
    unsigned increment;
};

enum class UnitGroup : uint8_t {
    DateTime,
    Date,
    Time,
};

enum class AllowedUnit : uint8_t {
    Auto,
    Day,
    None
};

enum class AddOrSubtract : bool {
    Add,
    Subtract
};

struct ParsedMonthCode {
    uint8_t monthNumber;
    bool isLeapMonth;
};

static inline bool isAbsentUnit(Variant<TemporalAuto, std::optional<TemporalUnit>> unit)
{
    return std::holds_alternative<std::optional<TemporalUnit>>(unit) && !std::get<std::optional<TemporalUnit>>(unit);
}

WTF::String ellipsizeAt(unsigned maxLength, const WTF::String&);
PropertyName NODELETE temporalUnitPluralPropertyName(VM&, TemporalUnit);
PropertyName NODELETE temporalUnitSingularPropertyName(VM&, TemporalUnit);
std::optional<TemporalUnit> temporalUnitType(StringView);

enum class TemporalUnitDefault : uint8_t { Unset, Required };
Variant<TemporalAuto, std::optional<TemporalUnit>>
temporalUnitValued(JSGlobalObject*, JSObject*, PropertyName, TemporalUnitDefault = TemporalUnitDefault::Unset);
void validateTemporalUnitValue(JSGlobalObject*, Variant<TemporalAuto, std::optional<TemporalUnit>>, UnitGroup, AllowedUnit, StringView);
void validateTemporalRoundingIncrement(JSGlobalObject*, double, std::optional<double>, Inclusivity);
std::tuple<TemporalUnit, TemporalUnit, RoundingMode, double> extractDifferenceOptions(JSGlobalObject*, JSValue, UnitGroup, TemporalUnit, TemporalUnit, DifferenceOperation = DifferenceOperation::Until);
std::optional<unsigned> temporalFractionalSecondDigits(JSGlobalObject*, JSObject* options);
// https://tc39.es/proposal-temporal/#sec-temporal-tosecondsstringprecisionrecord
PrecisionData toSecondsStringPrecisionRecord(std::optional<TemporalUnit> smallestUnit, std::optional<unsigned> fractionalDigitCount);
RoundingMode temporalRoundingMode(JSGlobalObject*, JSObject*, RoundingMode);
void formatSecondsStringFraction(StringBuilder&, unsigned fraction, std::tuple<Precision, unsigned>);
void formatSecondsStringPart(StringBuilder&, unsigned second, unsigned fraction, PrecisionData);
double temporalRoundingIncrement(JSGlobalObject*, JSObject* options);
double roundNumberToIncrement(double, double increment, RoundingMode);
void rejectObjectWithCalendarOrTimeZone(JSGlobalObject*, JSObject*);

TemporalOverflow toTemporalOverflow(JSGlobalObject*, JSObject*);
TemporalOverflow toTemporalOverflow(JSGlobalObject*, JSValue);
String temporalShowCalendarName(JSGlobalObject*, JSObject*);
CalendarID toTemporalCalendarIdentifier(JSGlobalObject*, JSValue);
TemporalDisambiguation toTemporalDisambiguation(JSGlobalObject*, JSObject*);
TemporalOffsetDisambiguation toTemporalOffset(JSGlobalObject*, JSObject*, TemporalOffsetDisambiguation fallback);

enum class TemporalDateFormat : uint8_t {
    Date,
    YearMonth,
    MonthDay
};

enum class TemporalAnyProperties : bool {
    None,
    Some,
};

void throwTemporalError(JSGlobalObject*, ThrowScope&, const TemporalError&);

std::optional<TimeZone> toTemporalTimeZoneIdentifier(JSGlobalObject*, JSValue);

} // namespace JSC
