/*
 *  Copyright (C) 2021 Igalia, S.L. All rights reserved.
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

#pragma once

#include "JSObject.h"

namespace JSC {

#define JSC_TEMPORAL_UNITS(macro) \
    macro(year, Year) \
    macro(month, Month) \
    macro(week, Week) \
    macro(day, Day) \
    macro(hour, Hour) \
    macro(minute, Minute) \
    macro(second, Second) \
    macro(millisecond, Millisecond) \
    macro(microsecond, Microsecond) \
    macro(nanosecond, Nanosecond) \

enum class TemporalUnit : uint8_t {
#define JSC_DEFINE_TEMPORAL_UNIT_ENUM(name, capitalizedName) capitalizedName,
    JSC_TEMPORAL_UNITS(JSC_DEFINE_TEMPORAL_UNIT_ENUM)
#undef JSC_DEFINE_TEMPORAL_UNIT_ENUM
};
#define JSC_COUNT_TEMPORAL_UNITS(name, capitalizedName) + 1
static constexpr unsigned numberOfTemporalUnits = 0 JSC_TEMPORAL_UNITS(JSC_COUNT_TEMPORAL_UNITS);
#undef JSC_COUNT_TEMPORAL_UNITS

class TemporalObject final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(TemporalObject, Base);
        return &vm.plainObjectSpace;
    }

    static TemporalObject* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*);

    DECLARE_INFO;

private:
    TemporalObject(VM&, Structure*);
    void finishCreation(VM&);
};

enum class RoundingMode : uint8_t {
    Ceil,
    Floor,
    Trunc,
    HalfExpand
};

struct PrecisionData {
    std::optional<unsigned> precision;
    TemporalUnit unit;
    double increment;
};

std::optional<TemporalUnit> temporalUnitType(StringView);
std::optional<TemporalUnit> temporalLargestUnit(JSGlobalObject*, JSObject* options, std::initializer_list<TemporalUnit> disallowedUnits, TemporalUnit autoValue);
std::optional<TemporalUnit> temporalSmallestUnit(JSGlobalObject*, JSObject* options, std::initializer_list<TemporalUnit> disallowedUnits);
std::optional<unsigned> temporalFractionalSecondDigits(JSGlobalObject*, JSObject* options);
PrecisionData secondsStringPrecision(JSGlobalObject*, JSObject* options);
std::optional<double> maximumRoundingIncrement(TemporalUnit);
double temporalRoundingIncrement(JSGlobalObject*, JSObject* options, std::optional<double> dividend, bool inclusive);
double roundNumberToIncrement(double, double increment, RoundingMode);

} // namespace JSC
