/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IntlDateTimeFormatPrototype.h"

#include "BuiltinNames.h"
#include "DateConstructor.h"
#include "IntlDateTimeFormatInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "TemporalInstant.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalZonedDateTime.h"
#include <wtf/DateMath.h>

namespace JSC {

static JSC_DECLARE_CUSTOM_GETTER(intlDateTimeFormatPrototypeGetterFormat);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRange);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRangeToParts);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatToParts);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncResolvedOptions);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatFuncFormatDateTime);

}

#include "IntlDateTimeFormatPrototype.lut.h"

namespace JSC {

const ClassInfo IntlDateTimeFormatPrototype::s_info = { "Intl.DateTimeFormat"_s, &Base::s_info, &dateTimeFormatPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormatPrototype) };

/* Source for IntlDateTimeFormatPrototype.lut.h
@begin dateTimeFormatPrototypeTable
  format                intlDateTimeFormatPrototypeGetterFormat              DontEnum|ReadOnly|CustomAccessor
  formatRange           intlDateTimeFormatPrototypeFuncFormatRange           DontEnum|Function 2
  formatRangeToParts    intlDateTimeFormatPrototypeFuncFormatRangeToParts    DontEnum|Function 2
  formatToParts         intlDateTimeFormatPrototypeFuncFormatToParts         DontEnum|Function 1
  resolvedOptions       intlDateTimeFormatPrototypeFuncResolvedOptions       DontEnum|Function 0
@end
*/

IntlDateTimeFormatPrototype* IntlDateTimeFormatPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    IntlDateTimeFormatPrototype* object = new (NotNull, allocateCell<IntlDateTimeFormatPrototype>(vm)) IntlDateTimeFormatPrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* IntlDateTimeFormatPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDateTimeFormatPrototype::IntlDateTimeFormatPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlDateTimeFormatPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    UNUSED_PARAM(globalObject);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

using ExactTime = ISO8601::ExactTime;

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

void IntlDateTimeFormat::checkTimeOptions(JSGlobalObject* globalObject, StringView error)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_timeStyle != DateTimeStyle::None)
        return;

    if (m_userSpecifiedHour) {
        throwTypeError(globalObject, scope, makeString(error, " cannot be formatted with hour option"_s));
        return;
    }
    if (m_userSpecifiedMinute) {
        throwTypeError(globalObject, scope, makeString(error, " cannot be formatted with minute option"_s));
        return;
    }
    if (m_userSpecifiedSecond) {
        throwTypeError(globalObject, scope, makeString(error, " cannot be formatted with second option"_s));
        return;
    }
}

void IntlDateTimeFormat::checkDateOptions(JSGlobalObject* globalObject, StringView error)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_dateStyle != DateTimeStyle::None)
        return;

    if (m_userSpecifiedYear) {
        throwTypeError(globalObject, scope, makeString(error, " cannot be formatted with year option"_s));
        return;
    }
    if (m_userSpecifiedMonth) {
        throwTypeError(globalObject, scope, makeString(error, " cannot be formatted with month option"_s));
        return;
    }
    if (m_userSpecifiedDay) {
        throwTypeError(globalObject, scope, makeString(error, " cannot be formatted with day option"_s));
        return;
    }
    if (m_userSpecifiedWeekday) {
        throwTypeError(globalObject, scope, makeString(error, " cannot be formatted with weekday option"_s));
        return;
    }
}

void IntlDateTimeFormat::checkOptionsCompatibility(JSGlobalObject* globalObject, JSValue x)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(isTemporalObject(x));

    bool dateStyleExists = m_dateStyle != DateTimeStyle::None;
    bool timeStyleExists = m_timeStyle != DateTimeStyle::None;

    TemporalInstant* instant = jsDynamicCast<TemporalInstant*>(x);
    if (instant)
        return;

    TemporalPlainDate* plainDate = jsDynamicCast<TemporalPlainDate*>(x);
    if (plainDate) {
        if (timeStyleExists && !dateStyleExists) {
            throwTypeError(globalObject, scope, "PlainDate cannot be formatted with timeStyle option and no dateStyle option"_s);
            return;
        }
        if (!(m_userSpecifiedYear || m_userSpecifiedMonth || m_userSpecifiedDay || m_userSpecifiedWeekday)) {
            checkTimeOptions(globalObject, "PlainDate"_s);
            RETURN_IF_EXCEPTION(scope, void());
        }
        return;
    }

    TemporalPlainDateTime* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(x);
    if (plainDateTime)
        return;

    TemporalPlainMonthDay* plainMonthDay = jsDynamicCast<TemporalPlainMonthDay*>(x);
    if (plainMonthDay) {
        if (timeStyleExists && !dateStyleExists) {
            throwTypeError(globalObject, scope, "PlainMonthDay cannot be formatted with timeStyle option and no dateStyle option"_s);
            return;
        }

        bool hasMonthDayOptions = m_userSpecifiedMonth || m_userSpecifiedDay;

        if (!hasMonthDayOptions) {
            checkTimeOptions(globalObject, "PlainMonthDay"_s);
            RETURN_IF_EXCEPTION(scope, void());

            if (m_userSpecifiedYear) {
                throwTypeError(globalObject, scope, "PlainMonthDay cannot be formatted with year option"_s);
                return;
            }
        }
        return;
    }

    TemporalPlainYearMonth* plainYearMonth = jsDynamicCast<TemporalPlainYearMonth*>(x);
    if (plainYearMonth) {
        if (timeStyleExists && !dateStyleExists) {
            throwTypeError(globalObject, scope, "PlainYearMonth cannot be formatted with timeStyle option and no dateStyle option"_s);
            return;
        }

        bool hasYearMonthOptions = m_userSpecifiedYear || m_userSpecifiedMonth;

        if (!hasYearMonthOptions) {
            checkTimeOptions(globalObject, "PlainYearMonth"_s);
            RETURN_IF_EXCEPTION(scope, void());

            if (m_userSpecifiedWeekday || m_userSpecifiedDay) {
                String error = m_userSpecifiedWeekday ? "weekday"_s : "day"_s;
                throwTypeError(globalObject, scope, makeString("PlainYearMonth cannot be formatted with "_s, error, " option"_s));
                return;
            }
        }
        return;
    }

    TemporalPlainTime* plainTime = jsDynamicCast<TemporalPlainTime*>(x);
    if (plainTime) {
        if (dateStyleExists && !timeStyleExists) {
            throwTypeError(globalObject, scope, "PlainTime cannot be formatted with dateStyle option and no timeStyle option"_s);
            return;
        }

        if (!(m_userSpecifiedHour || m_userSpecifiedMinute || m_userSpecifiedSecond)) {
            checkDateOptions(globalObject, "PlainTime"_s);
            RETURN_IF_EXCEPTION(scope, void());
        }
        return;
    }

    TemporalZonedDateTime* zonedDateTime = jsDynamicCast<TemporalZonedDateTime*>(x);
    if (zonedDateTime)
        return;

    RELEASE_ASSERT_NOT_REACHED();
    
}

// HandleDateTimeValue ( dateTimeFormat, x )
// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimevalue
std::tuple<ExactTime, std::optional<TemporalDateTimeFormat>>
IntlDateTimeFormat::handleDateTimeValue(JSGlobalObject* globalObject, JSValue x)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (x.isUndefined())
        RELEASE_AND_RETURN(scope, std::tuple(ISO8601::ExactTime::now(), std::nullopt));

    // In the spec, these checks are in the PartitionDateTimeRangePattern AO;
    // but since in the implementation, a single date-time formatter can be re-used
    // for multiple arguments, the checks have to be done when the argument is known
    if (isTemporalObject(x)) {
        checkOptionsCompatibility(globalObject, x);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalinstant
    TemporalInstant* instant = jsDynamicCast<TemporalInstant*>(x);
    if (instant)
        return std::tuple(instant->exactTime(), std::nullopt);

    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
    TemporalPlainDate* plainDate = jsDynamicCast<TemporalPlainDate*>(x);
    if (plainDate) {
        ISO8601::PlainDate date = plainDate->plainDate();
        ISO8601::PlainDateTime isoDateTime = TemporalPlainDateTime::combineISODateAndTimeRecord(date,
            ISO8601::PlainTime(12, 0, 0, 0, 0, 0));
        ExactTime result = TemporalTimeZone::getEpochNanosecondsFor(globalObject,
            m_timeZone, isoDateTime, TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, std::tuple(result, TemporalDateTimeFormat::PlainDate));
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldatetime
    TemporalPlainDateTime* plainDateTime = jsDynamicCast<TemporalPlainDateTime*>(x);
    if (plainDateTime) {
        ISO8601::PlainDateTime isoDateTime =
            TemporalPlainDateTime::combineISODateAndTimeRecord(plainDateTime->plainDate(),
                plainDateTime->plainTime());
        ExactTime result = TemporalTimeZone::getEpochNanosecondsFor(globalObject,
            m_timeZone, isoDateTime, TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });
        RELEASE_AND_RETURN(scope, std::tuple(result, TemporalDateTimeFormat::PlainDateTime));
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalmonthday
    TemporalPlainMonthDay* plainMonthDay = jsDynamicCast<TemporalPlainMonthDay*>(x);
    if (plainMonthDay) {
        auto isoDateTime = TemporalPlainDateTime::combineISODateAndTimeRecord(
            plainMonthDay->plainMonthDay().isoPlainDate(),
            ISO8601::PlainTime(12, 0, 0, 0, 0, 0));
        ExactTime epochNs = TemporalTimeZone::getEpochNanosecondsFor(globalObject,
            m_timeZone, isoDateTime, TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, std::tuple(epochNs, TemporalDateTimeFormat::PlainMonthDay));
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalyearmonth
    TemporalPlainYearMonth* plainYearMonth = jsDynamicCast<TemporalPlainYearMonth*>(x);
    if (plainYearMonth) {
        auto isoDateTime = TemporalPlainDateTime::combineISODateAndTimeRecord(
            plainYearMonth->plainYearMonth().isoPlainDate(),
            ISO8601::PlainTime(12, 0, 0, 0, 0, 0));
        ExactTime epochNs = TemporalTimeZone::getEpochNanosecondsFor(globalObject,
            m_timeZone, isoDateTime, TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, std::tuple(epochNs, TemporalDateTimeFormat::PlainYearMonth));
    }

    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaltime
    TemporalPlainTime* plainTime = jsDynamicCast<TemporalPlainTime*>(x);
    if (plainTime) {
        auto isoDate = ISO8601::PlainDate(1970, 1, 1);
        auto isoDateTime = TemporalPlainDateTime::combineISODateAndTimeRecord(isoDate,
            plainTime->plainTime());
        ExactTime epochNs = TemporalTimeZone::getEpochNanosecondsFor(globalObject,
            m_timeZone, isoDateTime, TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, std::tuple(epochNs, TemporalDateTimeFormat::PlainTime));
    }

    double x1 = WTF::timeClip(x.toNumber(globalObject));
    if (std::isnan(x1)) {
        throwRangeError(globalObject, scope, "handleDateTimeValue: not a number"_s);
        return { };
    }
    auto epochNanoseconds = ExactTime(1'000'000 * static_cast<Int128>(x1));
    RELEASE_AND_RETURN(scope, std::tuple(epochNanoseconds, std::nullopt));
}

JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatFuncFormatDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 12.1.7 DateTime Format Functions (ECMA-402)
    // https://tc39.github.io/ecma402/#sec-formatdatetime

    IntlDateTimeFormat* format = jsDynamicCast<IntlDateTimeFormat*>(callFrame->thisValue());
    if (UNLIKELY(!format))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.format called on value that's not a DateTimeFormat"_s));

    JSValue date = callFrame->argument(0);
    auto [value, optionalDateTimeFormat] =
        format->IntlDateTimeFormat::handleDateTimeValue(globalObject, date);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(format->format(globalObject, value, optionalDateTimeFormat)));
}

JSC_DEFINE_CUSTOM_GETTER(intlDateTimeFormatPrototypeGetterFormat, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.3.3 Intl.DateTimeFormat.prototype.format (ECMA-402 2.0)
    // 1. Let dtf be this DateTimeFormat object.
    auto* dtf = IntlDateTimeFormat::unwrapForOldFunctions(globalObject, JSValue::decode(thisValue));
    RETURN_IF_EXCEPTION(scope, { });
    // 2. ReturnIfAbrupt(dtf).
    if (UNLIKELY(!dtf))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.format called on value that's not a DateTimeFormat"_s));

    JSBoundFunction* boundFormat = dtf->boundFormat();
    // 3. If the [[boundFormat]] internal slot of this DateTimeFormat object is undefined,
    if (!boundFormat) {
        JSGlobalObject* globalObject = dtf->globalObject();
        // a. Let F be a new built-in function object as defined in 12.3.4.
        // b. The value of F’s length property is 1. (Note: F’s length property was 0 in ECMA-402 1.0)
        JSFunction* targetObject = JSFunction::create(vm, globalObject, 1, "format"_s, intlDateTimeFormatFuncFormatDateTime, ImplementationVisibility::Public);
        // c. Let bf be BoundFunctionCreate(F, «this value»).
        boundFormat = JSBoundFunction::create(vm, globalObject, targetObject, dtf, { }, 1, jsEmptyString(vm));
        RETURN_IF_EXCEPTION(scope, { });
        boundFormat->reifyLazyPropertyIfNeeded<>(vm, globalObject, vm.propertyNames->name);
        RETURN_IF_EXCEPTION(scope, { });
        boundFormat->putDirect(vm, vm.propertyNames->name, jsEmptyString(vm), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
        // d. Set dtf.[[boundFormat]] to bf.
        dtf->setBoundFormat(vm, boundFormat);
    }
    // 4. Return dtf.[[boundFormat]].
    return JSValue::encode(boundFormat);
}

JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4 Intl.DateTimeFormat.prototype.formatToParts (ECMA-402 4.0)
    // https://tc39.github.io/ecma402/#sec-Intl.DateTimeFormat.prototype.formatToParts

    // Do not use unwrapForOldFunctions.
    auto* dateTimeFormat = jsDynamicCast<IntlDateTimeFormat*>(callFrame->thisValue());
    if (UNLIKELY(!dateTimeFormat))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatToParts called on value that's not a DateTimeFormat"_s));

    JSValue date = callFrame->argument(0);
    auto [value, optionalDateTimeFormat] = dateTimeFormat->handleDateTimeValue(globalObject, date);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatToParts(globalObject,
        value, optionalDateTimeFormat)));
}

// https://tc39.es/proposal-temporal/#sec-temporal-sametemporaltype
static inline bool sameTemporalType(JSValue x, JSValue y)
{
    if (!isTemporalObject(x) || !isTemporalObject(y))
        return false;
    if ((jsDynamicCast<TemporalPlainDate*>(x) && !jsDynamicCast<TemporalPlainDate*>(y))
        || (jsDynamicCast<TemporalPlainTime*>(x) && !jsDynamicCast<TemporalPlainTime*>(y))
        || (jsDynamicCast<TemporalPlainDateTime*>(x) && !jsDynamicCast<TemporalPlainDateTime*>(y))
        || (jsDynamicCast<TemporalZonedDateTime*>(x) && !jsDynamicCast<TemporalZonedDateTime*>(y))
        || (jsDynamicCast<TemporalPlainYearMonth*>(x) && !jsDynamicCast<TemporalPlainYearMonth*>(y))
        || (jsDynamicCast<TemporalPlainMonthDay*>(x) && !jsDynamicCast<TemporalPlainMonthDay*>(y))
        || (jsDynamicCast<TemporalInstant*>(x) && !jsDynamicCast<TemporalInstant*>(y)))
        return false;
    return true;
}

// https://tc39.es/proposal-temporal/#sec-todatetimeformattable
static JSValue toDateTimeFormattable(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isTemporalObject(value))
        return value;
    RELEASE_AND_RETURN(scope, jsNumber(value.toNumber(globalObject)));
}

// http://tc39.es/proposal-intl-DateTimeFormat-formatRange/#sec-intl.datetimeformat.prototype.formatRange
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRange, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Do not use unwrapForOldFunctions.
    auto* dateTimeFormat = jsDynamicCast<IntlDateTimeFormat*>(callFrame->thisValue());
    if (UNLIKELY(!dateTimeFormat))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatRange called on value that's not a DateTimeFormat"_s));

    JSValue startDateValue = callFrame->argument(0);
    JSValue endDateValue = callFrame->argument(1);

    if (startDateValue.isUndefined() || endDateValue.isUndefined())
        return throwVMTypeError(globalObject, scope, "startDate or endDate is undefined"_s);

    auto x = toDateTimeFormattable(globalObject, startDateValue);
    RETURN_IF_EXCEPTION(scope, { });
    auto y = toDateTimeFormattable(globalObject, endDateValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (isTemporalObject(x) || isTemporalObject(y)) {
        if (!sameTemporalType(x, y)) {
            throwTypeError(globalObject, scope,
                "formatRange: Temporal objects have different types"_s);
            return { };
        }
    }

    auto [startDate, startFormat] = dateTimeFormat->handleDateTimeValue(globalObject, x);
    RETURN_IF_EXCEPTION(scope, { });
    // Note: since sameTemporalType(x, y) is true, startFormat == endFormat
    auto [endDate, endFormat] = dateTimeFormat->handleDateTimeValue(globalObject, y);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatRange(globalObject,
        startDate, endDate, startFormat)));
}

// http://tc39.es/proposal-intl-DateTimeFormat-formatRange/#sec-intl.datetimeformat.prototype.formatRangeToParts
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRangeToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Do not use unwrapForOldFunctions.
    auto* dateTimeFormat = jsDynamicCast<IntlDateTimeFormat*>(callFrame->thisValue());
    if (UNLIKELY(!dateTimeFormat))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatRangeToParts called on value that's not a DateTimeFormat"_s));

    JSValue startDateValue = callFrame->argument(0);
    JSValue endDateValue = callFrame->argument(1);

    if (startDateValue.isUndefined() || endDateValue.isUndefined())
        return throwVMTypeError(globalObject, scope, "startDate or endDate is undefined"_s);

    auto x = toDateTimeFormattable(globalObject, startDateValue);
    RETURN_IF_EXCEPTION(scope, { });
    auto y = toDateTimeFormattable(globalObject, endDateValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (isTemporalObject(x) || isTemporalObject(y)) {
        if (!sameTemporalType(x, y)) {
            throwTypeError(globalObject, scope,
                "formatRangeToParts: Temporal objects have different types"_s);
            return { };
        }
    }

    auto [startDate, startFormat] = dateTimeFormat->handleDateTimeValue(globalObject, x);
    RETURN_IF_EXCEPTION(scope, { });
    // Note: since sameTemporalType(x, y) is true, startFormat == endFormat
    auto [endDate, endFormat] = dateTimeFormat->handleDateTimeValue(globalObject, y);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatRangeToParts(globalObject, startDate, endDate, startFormat)));
}

JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.3.5 Intl.DateTimeFormat.prototype.resolvedOptions() (ECMA-402 2.0)

    auto* dateTimeFormat = IntlDateTimeFormat::unwrapForOldFunctions(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });
    if (UNLIKELY(!dateTimeFormat))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.resolvedOptions called on value that's not a DateTimeFormat"_s));

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->resolvedOptions(globalObject)));
}

} // namespace JSC
