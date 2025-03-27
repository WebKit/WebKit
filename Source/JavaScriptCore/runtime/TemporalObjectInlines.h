/*
 *  Copyright (C) 2025 Igalia, S.L. All rights reserved.
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

#include "IntlDateTimeFormat.h"
#include "JSGlobalObject.h"
#include "TemporalDuration.h"
#include "TemporalInstant.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalTimeZone.h"
#include "TemporalZonedDateTime.h"

namespace JSC {

// https://tc39.es/proposal-temporal/#sec-temporal-istemporalobject
static inline bool isTemporalObject(JSValue value)
{
    if (!value.isObject())
        return false;
    return (jsDynamicCast<TemporalPlainDate*>(value)
        || jsDynamicCast<TemporalPlainTime*>(value)
        || jsDynamicCast<TemporalPlainDateTime*>(value)
        || jsDynamicCast<TemporalZonedDateTime*>(value)
        || jsDynamicCast<TemporalPlainYearMonth*>(value)
        || jsDynamicCast<TemporalPlainMonthDay*>(value)
        || jsDynamicCast<TemporalInstant*>(value));
}

static inline JSValue callIntlDateTimeFormat(JSGlobalObject* globalObject, JSObject* temporalObject,
    JSValue locales, JSValue options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(isTemporalObject(temporalObject));

    IntlDateTimeFormat* dateTimeFormat = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
    ASSERT(dateTimeFormat);
    dateTimeFormat->initializeDateTimeFormat(globalObject, locales, options,
        IntlDateTimeFormat::RequiredComponent::Any, IntlDateTimeFormat::Defaults::Date);

    auto [value, optionalDateTimeFormat, optionalTimeZone] =
        dateTimeFormat->IntlDateTimeFormat::handleDateTimeValue(globalObject, temporalObject, true);
    RETURN_IF_EXCEPTION(scope, { });
    
    RELEASE_AND_RETURN(scope, dateTimeFormat->format(globalObject, value,
        optionalDateTimeFormat, optionalTimeZone));
}

} // namespace JSC
