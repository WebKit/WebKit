/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "DateInstance.h"

#include "JSCInlines.h"
#include "JSDateMath.h"

namespace JSC {

const ClassInfo DateInstance::s_info = {"Date", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DateInstance)};

DateInstance::DateInstance(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void DateInstance::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void DateInstance::finishCreation(VM& vm, double time)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    m_internalNumber = timeClip(time);
}

const GregorianDateTime* DateInstance::calculateGregorianDateTime(DateCache& cache) const
{
    double milli = internalNumber();
    if (std::isnan(milli))
        return nullptr;

    if (!m_data)
        m_data = cache.cachedDateInstanceData(milli);

    if (m_data->m_gregorianDateTimeCachedForMS != milli) {
        cache.msToGregorianDateTime(milli, WTF::LocalTime, m_data->m_cachedGregorianDateTime);
        m_data->m_gregorianDateTimeCachedForMS = milli;
    }
    return &m_data->m_cachedGregorianDateTime;
}

const GregorianDateTime* DateInstance::calculateGregorianDateTimeUTC(DateCache& cache) const
{
    double milli = internalNumber();
    if (std::isnan(milli))
        return nullptr;

    if (!m_data)
        m_data = cache.cachedDateInstanceData(milli);

    if (m_data->m_gregorianDateTimeUTCCachedForMS != milli) {
        cache.msToGregorianDateTime(milli, WTF::UTCTime, m_data->m_cachedGregorianDateTimeUTC);
        m_data->m_gregorianDateTimeUTCCachedForMS = milli;
    }
    return &m_data->m_cachedGregorianDateTimeUTC;
}

} // namespace JSC
