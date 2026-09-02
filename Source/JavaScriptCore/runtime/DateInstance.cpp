/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

const ClassInfo DateInstance::s_info = { "Date"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DateInstance) };

DateInstance::DateInstance(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void DateInstance::finishCreation(VM& vm, double time)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_internalNumber = timeClip(time);
}

// An instance that has never decomposed anything still holds the time value it was built with, so
// some other Date may well have decomposed the same value already. Once it has been walked forward
// its values are its own and the shared memo can only miss.
static DateCache::UseSharedCache useSharedCacheFor(PlainGregorianDateTime cached)
{
    return cached.hasNeverBeenComputed() ? DateCache::UseSharedCache::Yes : DateCache::UseSharedCache::No;
}

PlainGregorianDateTime DateInstance::calculateGregorianDateTime(DateCache& cache) const
{
    double milli = internalNumber();
    if (std::isnan(milli))
        return { };

    m_cachedGregorianDateTime = cache.msToGregorianDateTime(milli, TimeType::LocalTime, useSharedCacheFor(m_cachedGregorianDateTime));
    cache.noteCachedLocalGregorianDateTime();
    return m_cachedGregorianDateTime;
}

PlainGregorianDateTime DateInstance::calculateGregorianDateTimeUTC(DateCache& cache) const
{
    double milli = internalNumber();
    if (std::isnan(milli))
        return { };

    m_cachedGregorianDateTimeUTC = cache.msToGregorianDateTime(milli, TimeType::UTCTime, useSharedCacheFor(m_cachedGregorianDateTimeUTC));
    return m_cachedGregorianDateTimeUTC;
}

} // namespace JSC
