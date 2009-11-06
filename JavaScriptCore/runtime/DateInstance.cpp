/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "JSGlobalObject.h"

#include <math.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>

using namespace WTF;

namespace JSC {

const ClassInfo DateInstance::info = {"Date", 0, 0, 0};

DateInstance::DateInstance(ExecState* exec, NonNullPassRefPtr<Structure> structure)
    : JSWrapperObject(structure)
{
    setInternalValue(jsNaN(exec));
}

DateInstance::DateInstance(ExecState* exec, double time)
    : JSWrapperObject(exec->lexicalGlobalObject()->dateStructure())
{
    setInternalValue(jsNumber(exec, timeClip(time)));
}

const GregorianDateTime* DateInstance::gregorianDateTime(ExecState* exec, bool outputIsUTC) const
{
    double milli = internalNumber();
    if (isnan(milli))
        return 0;

    if (!m_data)
        m_data = exec->globalData().dateInstanceCache.add(milli);

    if (outputIsUTC) {
        if (m_data->m_gregorianDateTimeUTCCachedForMS != milli) {
            msToGregorianDateTime(internalNumber(), true, m_data->m_cachedGregorianDateTimeUTC);
            m_data->m_gregorianDateTimeUTCCachedForMS = milli;
        }
        return &m_data->m_cachedGregorianDateTimeUTC;
    }

    if (m_data->m_gregorianDateTimeCachedForMS != milli) {
        msToGregorianDateTime(internalNumber(), false, m_data->m_cachedGregorianDateTime);
        m_data->m_gregorianDateTimeCachedForMS = milli;
    }
    return &m_data->m_cachedGregorianDateTime;
}

} // namespace JSC
