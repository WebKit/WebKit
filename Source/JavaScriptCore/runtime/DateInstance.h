/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

class DateInstance final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell* cell)
    {
        static_cast<DateInstance*>(cell)->DateInstance::~DateInstance();
    }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.dateInstanceSpace();
    }

    static DateInstance* create(VM& vm, Structure* structure, double date)
    {
        DateInstance* instance = new (NotNull, allocateCell<DateInstance>(vm)) DateInstance(vm, structure);
        instance->finishCreation(vm, date);
        return instance;
    }

    static DateInstance* create(VM& vm, Structure* structure)
    {
        DateInstance* instance = new (NotNull, allocateCell<DateInstance>(vm)) DateInstance(vm, structure);
        instance->finishCreation(vm);
        return instance;
    }

    double internalNumber() const { return m_internalNumber; }
    void setInternalNumber(double value)
    {
        ASSERT(!std::isnan(value) && std::isfinite(value));
        m_internalNumber = value;
    }

    DECLARE_EXPORT_INFO;

    const GregorianDateTime* gregorianDateTime(DateCache& cache) const
    {
        if (m_data && m_data->m_gregorianDateTimeCachedForMS == internalNumber())
            return &m_data->m_cachedGregorianDateTime;
        return calculateGregorianDateTime(cache);
    }

    const GregorianDateTime* gregorianDateTimeUTC(DateCache& cache) const
    {
        if (m_data && m_data->m_gregorianDateTimeUTCCachedForMS == internalNumber())
            return &m_data->m_cachedGregorianDateTimeUTC;
        return calculateGregorianDateTimeUTC(cache);
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static constexpr ptrdiff_t offsetOfInternalNumber() { return OBJECT_OFFSETOF(DateInstance, m_internalNumber); }
    static constexpr ptrdiff_t offsetOfData() { return OBJECT_OFFSETOF(DateInstance, m_data); }
    static constexpr ptrdiff_t offsetOfYear() { return OBJECT_OFFSETOF(DateInstance, m_year); }
    static constexpr ptrdiff_t offsetOfMonth() { return OBJECT_OFFSETOF(DateInstance, m_month); }
    static constexpr ptrdiff_t offsetOfMonthDay() { return OBJECT_OFFSETOF(DateInstance, m_monthDay); }
    static constexpr ptrdiff_t offsetOfWeekDay() { return OBJECT_OFFSETOF(DateInstance, m_weekDay); }
    static constexpr ptrdiff_t offsetOfHour() { return OBJECT_OFFSETOF(DateInstance, m_hour); }
    static constexpr ptrdiff_t offsetOfMinute() { return OBJECT_OFFSETOF(DateInstance, m_minute); }
    static constexpr ptrdiff_t offsetOfSecond() { return OBJECT_OFFSETOF(DateInstance, m_second); }

    inline void setYear(int year) { m_year = jsNumber(year); }
    inline void setMonth(int month) { m_month = jsNumber(month); }
    inline void setMonthDay(int monthDay) { m_monthDay = jsNumber(monthDay); }
    inline void setWeekDay(int weekDay) { m_weekDay = jsNumber(weekDay); }
    inline void setHour(int hour) { m_hour = jsNumber(hour); }
    inline void setMinute(int minute) { m_minute = jsNumber(minute); }
    inline void setSecond(int second) { m_second = jsNumber(second); }

    inline JSValue year() { return m_year; }
    inline JSValue month() { return m_month; }
    inline JSValue monthDay() { return m_monthDay; }
    inline JSValue weekDay() { return m_weekDay; }
    inline JSValue hour() { return m_hour; }
    inline JSValue minute() { return m_minute; }
    inline JSValue second() { return m_second; }

    JSValue setNaN();
    double setAndCache(VM&, double);

private:
    JS_EXPORT_PRIVATE DateInstance(VM&, Structure*);

    DECLARE_DEFAULT_FINISH_CREATION;
    JS_EXPORT_PRIVATE void finishCreation(VM&, double);
    JS_EXPORT_PRIVATE const GregorianDateTime* calculateGregorianDateTime(DateCache&) const;
    JS_EXPORT_PRIVATE const GregorianDateTime* calculateGregorianDateTimeUTC(DateCache&) const;

    double m_internalNumber { PNaN };
    mutable RefPtr<DateInstanceData> m_data;

    JSValue m_year { };
    JSValue m_month { };
    JSValue m_monthDay { };
    JSValue m_weekDay { };
    JSValue m_hour { };
    JSValue m_minute { };
    JSValue m_second { };
};

} // namespace JSC
