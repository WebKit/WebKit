/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#include "InternalFunction.h"

namespace JSC {

class DatePrototype;
class GetterSetter;

class DateConstructor final : public InternalFunction {
public:
    typedef InternalFunction Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    static DateConstructor* create(VM& vm, Structure* structure, DatePrototype* datePrototype)
    {
        DateConstructor* constructor = new (NotNull, allocateCell<DateConstructor>(vm)) DateConstructor(vm, structure);
        constructor->finishCreation(vm, datePrototype);
        return constructor;
    }

    DECLARE_INFO;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

private:
    DateConstructor(VM&, Structure*);
    void finishCreation(VM&, DatePrototype*);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(DateConstructor, InternalFunction);

JSObject* constructDate(JSGlobalObject*, JSValue newTarget, const ArgList&);
JSValue dateNowImpl();

// https://tc39.es/ecma262/#sec-makeday
constexpr static inline double makeDay(double year, double month, double date)
{
    double additionalYears = std::floor(month / 12);
    double ym = year + additionalYears;
    if (!std::isfinite(ym))
        return PNaN;
    double mm = month - additionalYears * 12;
    int32_t yearInt32 = toInt32(ym);
    int32_t monthInt32 = toInt32(mm);
    if (yearInt32 != ym || monthInt32 != mm)
        return PNaN;
    double days = dateToDaysFrom1970(yearInt32, monthInt32, 1);
    return days + date - 1;
}

// https://tc39.es/ecma262/#sec-makedate
constexpr static inline double makeDate(double day, double time)
{
#if COMPILER(CLANG)
#pragma STDC FP_CONTRACT OFF
#endif
    return (day * msPerDay) + time;
}

// https://tc39.es/ecma262/#sec-maketime
constexpr static inline double makeTime(double hour, double min, double sec, double ms)
{
#if COMPILER(CLANG)
#pragma STDC FP_CONTRACT OFF
#endif
    return (((hour * msPerHour) + min * msPerMinute) + sec * msPerSecond) + ms;
}
} // namespace JSC
