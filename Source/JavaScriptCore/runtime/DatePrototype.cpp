/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 *  Copyright (C) 2008, 2009 Torch Mobile, Inc. All rights reserved.
 *  Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "DatePrototype.h"

#include "DateConversion.h"
#include "DateInstance.h"
#include "Error.h"
#include "JSCBuiltins.h"
#include "JSDateMath.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "Lookup.h"
#include "ObjectPrototype.h"
#include "JSCInlines.h"
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

#if HAVE(LANGINFO_H)
#include <langinfo.h>
#endif

#if HAVE(SYS_PARAM_H)
#include <sys/param.h>
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

#if !(OS(DARWIN) && USE(CF))
#include <unicode/udat.h>
#endif

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace JSC {

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetDate(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetDay(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetFullYear(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetHours(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetMilliSeconds(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetMinutes(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetMonth(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetSeconds(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetTimezoneOffset(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCDate(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCDay(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCFullYear(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCHours(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCMilliseconds(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCMinutes(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCMonth(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCSeconds(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncGetYear(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetDate(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetFullYear(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetHours(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetMilliSeconds(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetMinutes(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetMonth(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetSeconds(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetTime(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCDate(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCFullYear(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCHours(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCMilliseconds(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCMinutes(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCMonth(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCSeconds(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncSetYear(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToDateString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToLocaleDateString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToLocaleString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToLocaleTimeString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToPrimitiveSymbol(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToTimeString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToUTCString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToISOString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dateProtoFuncToJSON(JSGlobalObject*, CallFrame*);

}

#include "DatePrototype.lut.h"

namespace JSC {

enum LocaleDateTimeFormat { LocaleDateAndTime, LocaleDate, LocaleTime };
 
#if OS(DARWIN) && USE(CF)

// FIXME: Since this is superior to the strftime-based version, why limit this to OS(DARWIN)?
// Instead we should consider using this whenever USE(CF) is true.

static CFDateFormatterStyle styleFromArgString(const String& string, CFDateFormatterStyle defaultStyle)
{
    if (string == "short")
        return kCFDateFormatterShortStyle;
    if (string == "medium")
        return kCFDateFormatterMediumStyle;
    if (string == "long")
        return kCFDateFormatterLongStyle;
    if (string == "full")
        return kCFDateFormatterFullStyle;
    return defaultStyle;
}

static JSCell* formatLocaleDate(JSGlobalObject* globalObject, CallFrame* callFrame, DateInstance*, double timeInMilliseconds, LocaleDateTimeFormat format)
{
    VM& vm = globalObject->vm();
    CFDateFormatterStyle dateStyle = (format != LocaleTime ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);
    CFDateFormatterStyle timeStyle = (format != LocaleDate ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);

    bool useCustomFormat = false;
    String customFormatString;

    String arg0String = callFrame->argument(0).toWTFString(globalObject);
    if (arg0String == "custom" && !callFrame->argument(1).isUndefined()) {
        useCustomFormat = true;
        customFormatString = callFrame->argument(1).toWTFString(globalObject);
    } else if (format == LocaleDateAndTime && !callFrame->argument(1).isUndefined()) {
        dateStyle = styleFromArgString(arg0String, dateStyle);
        timeStyle = styleFromArgString(callFrame->argument(1).toWTFString(globalObject), timeStyle);
    } else if (format != LocaleTime && !callFrame->argument(0).isUndefined())
        dateStyle = styleFromArgString(arg0String, dateStyle);
    else if (format != LocaleDate && !callFrame->argument(0).isUndefined())
        timeStyle = styleFromArgString(arg0String, timeStyle);

    CFAbsoluteTime absoluteTime = floor(timeInMilliseconds / msPerSecond) - kCFAbsoluteTimeIntervalSince1970;

    auto formatter = adoptCF(CFDateFormatterCreate(kCFAllocatorDefault, adoptCF(CFLocaleCopyCurrent()).get(), dateStyle, timeStyle));
    if (useCustomFormat)
        CFDateFormatterSetFormat(formatter.get(), customFormatString.createCFString().get());
    return jsNontrivialString(vm, adoptCF(CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, formatter.get(), absoluteTime)).get());
}

#elif !UCONFIG_NO_FORMATTING

static JSCell* formatLocaleDate(JSGlobalObject* globalObject, CallFrame*, DateInstance*, double timeInMilliseconds, LocaleDateTimeFormat format)
{
    VM& vm = globalObject->vm();
    UDateFormatStyle timeStyle = (format != LocaleDate ? UDAT_LONG : UDAT_NONE);
    UDateFormatStyle dateStyle = (format != LocaleTime ? UDAT_LONG : UDAT_NONE);

    UErrorCode status = U_ZERO_ERROR;
    UDateFormat* df = udat_open(timeStyle, dateStyle, 0, 0, -1, 0, 0, &status);
    if (!df)
        return jsEmptyString(vm);

    UChar buffer[128];
    int32_t length;
    length = udat_format(df, timeInMilliseconds, buffer, 128, 0, &status);
    udat_close(df);
    if (status != U_ZERO_ERROR)
        return jsEmptyString(vm);

    return jsNontrivialString(vm, String(buffer, length));
}

#else

static JSCell* formatLocaleDate(JSGlobalObject* globalObject, CallFrame* callFrame, const GregorianDateTime& gdt, LocaleDateTimeFormat format)
{
    VM& vm = globalObject->vm();
#if OS(WINDOWS)
    SYSTEMTIME systemTime;
    memset(&systemTime, 0, sizeof(systemTime));
    systemTime.wYear = gdt.year();
    systemTime.wMonth = gdt.month() + 1;
    systemTime.wDay = gdt.monthDay();
    systemTime.wDayOfWeek = gdt.weekDay();
    systemTime.wHour = gdt.hour();
    systemTime.wMinute = gdt.minute();
    systemTime.wSecond = gdt.second();

    Vector<UChar, 128> buffer;
    size_t length = 0;

    if (format == LocaleDate) {
        buffer.resize(GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &systemTime, 0, 0, 0));
        length = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &systemTime, 0, buffer.data(), buffer.size());
    } else if (format == LocaleTime) {
        buffer.resize(GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &systemTime, 0, 0, 0));
        length = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &systemTime, 0, buffer.data(), buffer.size());
    } else if (format == LocaleDateAndTime) {
        buffer.resize(GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &systemTime, 0, 0, 0) + GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &systemTime, 0, 0, 0));
        length = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &systemTime, 0, buffer.data(), buffer.size());
        if (length) {
            buffer[length - 1] = ' ';
            length += GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &systemTime, 0, buffer.data() + length, buffer.size() - length);
        }
    } else
        RELEASE_ASSERT_NOT_REACHED();

    //  Remove terminating null character.
    if (length)
        length--;

    return jsNontrivialString(vm, String(buffer.data(), length));

#else // OS(WINDOWS)

#if HAVE(LANGINFO_H)
    static const nl_item formats[] = { D_T_FMT, D_FMT, T_FMT };
#else
    static const char* const formatStrings[] = { "%#c", "%#x", "%X" };
#endif

    // Offset year if needed
    struct tm localTM = gdt;
    int year = gdt.year();
    bool yearNeedsOffset = year < 1900 || year > 2038;
    if (yearNeedsOffset)
        localTM.tm_year = equivalentYearForDST(year) - 1900;

#if HAVE(LANGINFO_H)
    // We do not allow strftime to generate dates with 2-digits years,
    // both to avoid ambiguity, and a crash in strncpy, for years that
    // need offset.
    char* formatString = strdup(nl_langinfo(formats[format]));
    char* yPos = strchr(formatString, 'y');
    if (yPos)
        *yPos = 'Y';
#endif

    // Do the formatting
    const int bufsize = 128;
    char timebuffer[bufsize];

#if HAVE(LANGINFO_H)
    size_t ret = strftime(timebuffer, bufsize, formatString, &localTM);
    free(formatString);
#else
    size_t ret = strftime(timebuffer, bufsize, formatStrings[format], &localTM);
#endif

    if (ret == 0)
        return jsEmptyString(vm);

    // Copy original into the buffer
    if (yearNeedsOffset && format != LocaleTime) {
        static const int yearLen = 5;   // FIXME will be a problem in the year 10,000
        char yearString[yearLen];

        snprintf(yearString, yearLen, "%d", localTM.tm_year + 1900);
        char* yearLocation = strstr(timebuffer, yearString);
        snprintf(yearString, yearLen, "%d", year);

        strncpy(yearLocation, yearString, yearLen - 1);
    }

    // Convert multi-byte result to UNICODE.
    // If __STDC_ISO_10646__ is defined, wide character represents
    // UTF-16 (or UTF-32) code point. In most modern Unix like system
    // (e.g. Linux with glibc 2.2 and above) the macro is defined,
    // and wide character represents UTF-32 code point.
    // Here we static_cast potential UTF-32 to UTF-16, it should be
    // safe because date and (or) time related characters in different languages
    // should be in UNICODE BMP. If mbstowcs fails, we just fall
    // back on using multi-byte result as-is.
#ifdef __STDC_ISO_10646__
    UChar buffer[bufsize];
    wchar_t tempbuffer[bufsize];
    size_t length = mbstowcs(tempbuffer, timebuffer, bufsize - 1);
    if (length != static_cast<size_t>(-1)) {
        for (size_t i = 0; i < length; ++i)
            buffer[i] = static_cast<UChar>(tempbuffer[i]);
        return jsNontrivialString(vm, String(buffer, length));
    }
#endif

    return jsNontrivialString(vm, timebuffer);
#endif // OS(WINDOWS)
}

static JSCell* formatLocaleDate(JSGlobalObject* globalObject, CallFrame* callFrame, DateInstance* dateObject, double, LocaleDateTimeFormat format)
{
    VM& vm = globalObject->vm();
    const GregorianDateTime* gregorianDateTime = dateObject->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return jsNontrivialString(vm, "Invalid Date"_s);
    return formatLocaleDate(globalObject, callFrame, *gregorianDateTime, format);
}

#endif // OS(DARWIN) && USE(CF)

static EncodedJSValue formateDateInstance(JSGlobalObject* globalObject, CallFrame* callFrame, DateTimeFormat format, bool asUTCVariant)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = asUTCVariant
        ? thisDateObj->gregorianDateTimeUTC(vm)
        : thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNontrivialString(vm, String("Invalid Date"_s)));

    return JSValue::encode(jsNontrivialString(vm, formatDateTime(*gregorianDateTime, format, asUTCVariant)));
}

// Converts a list of arguments sent to a Date member function into milliseconds, updating
// ms (representing milliseconds) and t (representing the rest of the date structure) appropriately.
//
// Format of member function: f([hour,] [min,] [sec,] [ms])
static bool fillStructuresUsingTimeArgs(JSGlobalObject* globalObject, CallFrame* callFrame, int maxArgs, double* ms, GregorianDateTime* t)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double milliseconds = 0;
    bool ok = true;
    int idx = 0;
    int numArgs = callFrame->argumentCount();
    
    // JS allows extra trailing arguments -- ignore them
    if (numArgs > maxArgs)
        numArgs = maxArgs;

    // hours
    if (maxArgs >= 4 && idx < numArgs) {
        t->setHour(0);
        double hours = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = std::isfinite(hours);
        milliseconds += hours * msPerHour;
    }

    // minutes
    if (maxArgs >= 3 && idx < numArgs && ok) {
        t->setMinute(0);
        double minutes = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = std::isfinite(minutes);
        milliseconds += minutes * msPerMinute;
    }
    
    // seconds
    if (maxArgs >= 2 && idx < numArgs && ok) {
        t->setSecond(0);
        double seconds = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = std::isfinite(seconds);
        milliseconds += seconds * msPerSecond;
    }
    
    if (!ok)
        return false;
        
    // milliseconds
    if (idx < numArgs) {
        double millis = callFrame->uncheckedArgument(idx).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = std::isfinite(millis);
        milliseconds += millis;
    } else
        milliseconds += *ms;
    
    *ms = milliseconds;
    return ok;
}

// Converts a list of arguments sent to a Date member function into years, months, and milliseconds, updating
// ms (representing milliseconds) and t (representing the rest of the date structure) appropriately.
//
// Format of member function: f([years,] [months,] [days])
static bool fillStructuresUsingDateArgs(JSGlobalObject* globalObject, CallFrame* callFrame, int maxArgs, double *ms, GregorianDateTime *t)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    int idx = 0;
    bool ok = true;
    int numArgs = callFrame->argumentCount();
  
    // JS allows extra trailing arguments -- ignore them
    if (numArgs > maxArgs)
        numArgs = maxArgs;
  
    // years
    if (maxArgs >= 3 && idx < numArgs) {
        double years = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = std::isfinite(years);
        t->setYear(toInt32(years));
    }
    // months
    if (maxArgs >= 2 && idx < numArgs && ok) {
        double months = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = std::isfinite(months);
        t->setMonth(toInt32(months));
    }
    // days
    if (idx < numArgs && ok) {
        double days = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = std::isfinite(days);
        t->setMonthDay(0);
        *ms += days * msPerDay;
    }
    
    return ok;
}

const ClassInfo DatePrototype::s_info = {"Object", &JSNonFinalObject::s_info, &dateTable, nullptr, CREATE_METHOD_TABLE(DatePrototype)};

/* Source for DatePrototype.lut.h
@begin dateTable
  toString              dateProtoFuncToString                DontEnum|Function       0
  toISOString           dateProtoFuncToISOString             DontEnum|Function       0
  toDateString          dateProtoFuncToDateString            DontEnum|Function       0
  toTimeString          dateProtoFuncToTimeString            DontEnum|Function       0
  toLocaleString        dateProtoFuncToLocaleString          DontEnum|Function       0
  toLocaleDateString    dateProtoFuncToLocaleDateString      DontEnum|Function       0
  toLocaleTimeString    dateProtoFuncToLocaleTimeString      DontEnum|Function       0
  valueOf               dateProtoFuncGetTime                 DontEnum|Function       0  DatePrototypeGetTimeIntrinsic
  getTime               dateProtoFuncGetTime                 DontEnum|Function       0  DatePrototypeGetTimeIntrinsic
  getFullYear           dateProtoFuncGetFullYear             DontEnum|Function       0  DatePrototypeGetFullYearIntrinsic
  getUTCFullYear        dateProtoFuncGetUTCFullYear          DontEnum|Function       0  DatePrototypeGetUTCFullYearIntrinsic
  getMonth              dateProtoFuncGetMonth                DontEnum|Function       0  DatePrototypeGetMonthIntrinsic
  getUTCMonth           dateProtoFuncGetUTCMonth             DontEnum|Function       0  DatePrototypeGetUTCMonthIntrinsic
  getDate               dateProtoFuncGetDate                 DontEnum|Function       0  DatePrototypeGetDateIntrinsic
  getUTCDate            dateProtoFuncGetUTCDate              DontEnum|Function       0  DatePrototypeGetUTCDateIntrinsic
  getDay                dateProtoFuncGetDay                  DontEnum|Function       0  DatePrototypeGetDayIntrinsic
  getUTCDay             dateProtoFuncGetUTCDay               DontEnum|Function       0  DatePrototypeGetUTCDayIntrinsic
  getHours              dateProtoFuncGetHours                DontEnum|Function       0  DatePrototypeGetHoursIntrinsic
  getUTCHours           dateProtoFuncGetUTCHours             DontEnum|Function       0  DatePrototypeGetUTCHoursIntrinsic
  getMinutes            dateProtoFuncGetMinutes              DontEnum|Function       0  DatePrototypeGetMinutesIntrinsic
  getUTCMinutes         dateProtoFuncGetUTCMinutes           DontEnum|Function       0  DatePrototypeGetUTCMinutesIntrinsic
  getSeconds            dateProtoFuncGetSeconds              DontEnum|Function       0  DatePrototypeGetSecondsIntrinsic
  getUTCSeconds         dateProtoFuncGetUTCSeconds           DontEnum|Function       0  DatePrototypeGetUTCSecondsIntrinsic
  getMilliseconds       dateProtoFuncGetMilliSeconds         DontEnum|Function       0  DatePrototypeGetMillisecondsIntrinsic
  getUTCMilliseconds    dateProtoFuncGetUTCMilliseconds      DontEnum|Function       0  DatePrototypeGetUTCMillisecondsIntrinsic
  getTimezoneOffset     dateProtoFuncGetTimezoneOffset       DontEnum|Function       0  DatePrototypeGetTimezoneOffsetIntrinsic
  getYear               dateProtoFuncGetYear                 DontEnum|Function       0  DatePrototypeGetYearIntrinsic
  setTime               dateProtoFuncSetTime                 DontEnum|Function       1
  setMilliseconds       dateProtoFuncSetMilliSeconds         DontEnum|Function       1
  setUTCMilliseconds    dateProtoFuncSetUTCMilliseconds      DontEnum|Function       1
  setSeconds            dateProtoFuncSetSeconds              DontEnum|Function       2
  setUTCSeconds         dateProtoFuncSetUTCSeconds           DontEnum|Function       2
  setMinutes            dateProtoFuncSetMinutes              DontEnum|Function       3
  setUTCMinutes         dateProtoFuncSetUTCMinutes           DontEnum|Function       3
  setHours              dateProtoFuncSetHours                DontEnum|Function       4
  setUTCHours           dateProtoFuncSetUTCHours             DontEnum|Function       4
  setDate               dateProtoFuncSetDate                 DontEnum|Function       1
  setUTCDate            dateProtoFuncSetUTCDate              DontEnum|Function       1
  setMonth              dateProtoFuncSetMonth                DontEnum|Function       2
  setUTCMonth           dateProtoFuncSetUTCMonth             DontEnum|Function       2
  setFullYear           dateProtoFuncSetFullYear             DontEnum|Function       3
  setUTCFullYear        dateProtoFuncSetUTCFullYear          DontEnum|Function       3
  setYear               dateProtoFuncSetYear                 DontEnum|Function       1
  toJSON                dateProtoFuncToJSON                  DontEnum|Function       1
@end
*/

// ECMA 15.9.4

DatePrototype::DatePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void DatePrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    Identifier toUTCStringName = Identifier::fromString(vm, "toUTCString"_s);
    JSFunction* toUTCStringFunction = JSFunction::create(vm, globalObject, 0, toUTCStringName.string(), dateProtoFuncToUTCString);
    putDirectWithoutTransition(vm, toUTCStringName, toUTCStringFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "toGMTString"_s), toUTCStringFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

#if ENABLE(INTL)
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("toLocaleString", datePrototypeToLocaleStringCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("toLocaleDateString", datePrototypeToLocaleDateStringCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("toLocaleTimeString", datePrototypeToLocaleTimeStringCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
#endif

    JSFunction* toPrimitiveFunction = JSFunction::create(vm, globalObject, 1, "[Symbol.toPrimitive]"_s, dateProtoFuncToPrimitiveSymbol, NoIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->toPrimitiveSymbol, toPrimitiveFunction, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    // The constructor will be added later, after DateConstructor has been built.
}

// Functions

EncodedJSValue JSC_HOST_CALL dateProtoFuncToString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    const bool asUTCVariant = false;
    return formateDateInstance(globalObject, callFrame, DateTimeFormatDateAndTime, asUTCVariant);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToUTCString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    const bool asUTCVariant = true;
    return formateDateInstance(globalObject, callFrame, DateTimeFormatDateAndTime, asUTCVariant);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToISOString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);
    
    if (!std::isfinite(thisDateObj->internalNumber()))
        return throwVMError(globalObject, scope, createRangeError(globalObject, "Invalid Date"_s));

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNontrivialString(vm, String("Invalid Date"_s)));
    // Maximum amount of space we need in buffer: 7 (max. digits in year) + 2 * 5 (2 characters each for month, day, hour, minute, second) + 4 (. + 3 digits for milliseconds)
    // 6 for formatting and one for null termination = 28. We add one extra character to allow us to force null termination.
    char buffer[28];
    // If the year is outside the bounds of 0 and 9999 inclusive we want to use the extended year format (ES 15.9.1.15.1).
    int ms = static_cast<int>(fmod(thisDateObj->internalNumber(), msPerSecond));
    if (ms < 0)
        ms += msPerSecond;

    int charactersWritten;
    if (gregorianDateTime->year() > 9999 || gregorianDateTime->year() < 0)
        charactersWritten = snprintf(buffer, sizeof(buffer), "%+07d-%02d-%02dT%02d:%02d:%02d.%03dZ", gregorianDateTime->year(), gregorianDateTime->month() + 1, gregorianDateTime->monthDay(), gregorianDateTime->hour(), gregorianDateTime->minute(), gregorianDateTime->second(), ms);
    else
        charactersWritten = snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", gregorianDateTime->year(), gregorianDateTime->month() + 1, gregorianDateTime->monthDay(), gregorianDateTime->hour(), gregorianDateTime->minute(), gregorianDateTime->second(), ms);

    ASSERT(charactersWritten > 0 && static_cast<unsigned>(charactersWritten) < sizeof(buffer));
    if (static_cast<unsigned>(charactersWritten) >= sizeof(buffer))
        return JSValue::encode(jsEmptyString(vm));

    return JSValue::encode(jsNontrivialString(vm, String(buffer, charactersWritten)));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToDateString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    const bool asUTCVariant = false;
    return formateDateInstance(globalObject, callFrame, DateTimeFormatDate, asUTCVariant);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToTimeString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    const bool asUTCVariant = false;
    return formateDateInstance(globalObject, callFrame, DateTimeFormatTime, asUTCVariant);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToLocaleString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(formatLocaleDate(globalObject, callFrame, thisDateObj, thisDateObj->internalNumber(), LocaleDateAndTime));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToLocaleDateString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(formatLocaleDate(globalObject, callFrame, thisDateObj, thisDateObj->internalNumber(), LocaleDate));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToLocaleTimeString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(formatLocaleDate(globalObject, callFrame, thisDateObj, thisDateObj->internalNumber(), LocaleTime));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToPrimitiveSymbol(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Date.prototype[Symbol.toPrimitive] expected |this| to be an object.");
    JSObject* thisObject = jsCast<JSObject*>(thisValue);

    if (!callFrame->argumentCount())
        return throwVMTypeError(globalObject, scope, "Date.prototype[Symbol.toPrimitive] expected a first argument.");

    JSValue hintValue = callFrame->uncheckedArgument(0);
    PreferredPrimitiveType type = toPreferredPrimitiveType(globalObject, hintValue);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (type == NoPreference)
        type = PreferString;

    RELEASE_AND_RETURN(scope, JSValue::encode(thisObject->ordinaryToPrimitive(globalObject, type)));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetTime(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(jsNumber(thisDateObj->internalNumber()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetFullYear(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->year()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCFullYear(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->year()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetMonth(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->month()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCMonth(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->month()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetDate(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->monthDay()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCDate(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->monthDay()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetDay(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->weekDay()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCDay(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->weekDay()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetHours(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->hour()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCHours(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->hour()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetMinutes(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->minute()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCMinutes(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->minute()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetSeconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->second()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCSeconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->second()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetMilliSeconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double milli = thisDateObj->internalNumber();
    if (std::isnan(milli))
        return JSValue::encode(jsNaN());

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    // Since timeClip makes internalNumber integer milliseconds, this result is always int32_t.
    return JSValue::encode(jsNumber(static_cast<int32_t>(ms)));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetUTCMilliseconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double milli = thisDateObj->internalNumber();
    if (std::isnan(milli))
        return JSValue::encode(jsNaN());

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    // Since timeClip makes internalNumber integer milliseconds, this result is always int32_t.
    return JSValue::encode(jsNumber(static_cast<int32_t>(ms)));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetTimezoneOffset(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(-gregorianDateTime->utcOffsetInMinute()));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetTime(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double milli = timeClip(callFrame->argument(0).toNumber(globalObject));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    thisDateObj->setInternalNumber(milli);
    return JSValue::encode(jsNumber(milli));
}

static EncodedJSValue setNewValueFromTimeArgs(JSGlobalObject* globalObject, CallFrame* callFrame, int numArgsToUse, WTF::TimeType inputTimeType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double milli = thisDateObj->internalNumber();

    if (!callFrame->argumentCount() || std::isnan(milli)) {
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    }
     
    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;

    const GregorianDateTime* other = inputTimeType == WTF::UTCTime
        ? thisDateObj->gregorianDateTimeUTC(vm)
        : thisDateObj->gregorianDateTime(vm);
    if (!other)
        return JSValue::encode(jsNaN());

    GregorianDateTime gregorianDateTime(*other);
    bool success = fillStructuresUsingTimeArgs(globalObject, callFrame, numArgsToUse, &ms, &gregorianDateTime);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!success) {
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    } 

    double newUTCDate = gregorianDateTimeToMS(vm, gregorianDateTime, ms, inputTimeType);
    double result = timeClip(newUTCDate);
    thisDateObj->setInternalNumber(result);
    return JSValue::encode(jsNumber(result));
}

static EncodedJSValue setNewValueFromDateArgs(JSGlobalObject* globalObject, CallFrame* callFrame, int numArgsToUse, WTF::TimeType inputTimeType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    if (!callFrame->argumentCount()) {
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    }

    double milli = thisDateObj->internalNumber();
    double ms = 0; 

    GregorianDateTime gregorianDateTime; 
    if (numArgsToUse == 3 && std::isnan(milli)) 
        msToGregorianDateTime(vm, 0, WTF::UTCTime, gregorianDateTime);
    else { 
        ms = milli - floor(milli / msPerSecond) * msPerSecond; 
        const GregorianDateTime* other = inputTimeType == WTF::UTCTime
            ? thisDateObj->gregorianDateTimeUTC(vm)
            : thisDateObj->gregorianDateTime(vm);
        if (!other)
            return JSValue::encode(jsNaN());
        gregorianDateTime = *other;
    }
    
    bool success = fillStructuresUsingDateArgs(globalObject, callFrame, numArgsToUse, &ms, &gregorianDateTime);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!success) {
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    } 

    double newUTCDate = gregorianDateTimeToMS(vm, gregorianDateTime, ms, inputTimeType);
    double result = timeClip(newUTCDate);
    thisDateObj->setInternalNumber(result);
    return JSValue::encode(jsNumber(result));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetMilliSeconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 1, WTF::LocalTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCMilliseconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 1, WTF::UTCTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetSeconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 2, WTF::LocalTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCSeconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 2, WTF::UTCTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetMinutes(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 3, WTF::LocalTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCMinutes(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 3, WTF::UTCTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetHours(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 4, WTF::LocalTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCHours(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 4, WTF::UTCTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetDate(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromDateArgs(globalObject, callFrame, 1, WTF::LocalTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCDate(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromDateArgs(globalObject, callFrame, 1, WTF::UTCTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetMonth(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromDateArgs(globalObject, callFrame, 2, WTF::LocalTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCMonth(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromDateArgs(globalObject, callFrame, 2, WTF::UTCTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetFullYear(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromDateArgs(globalObject, callFrame, 3, WTF::LocalTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetUTCFullYear(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setNewValueFromDateArgs(globalObject, callFrame, 3, WTF::UTCTime);
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncSetYear(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    if (!callFrame->argumentCount()) { 
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    }

    double milli = thisDateObj->internalNumber();
    double ms = 0;

    GregorianDateTime gregorianDateTime;
    if (std::isnan(milli))
        // Based on ECMA 262 B.2.5 (setYear)
        // the time must be reset to +0 if it is NaN.
        msToGregorianDateTime(vm, 0, WTF::UTCTime, gregorianDateTime);
    else {
        double secs = floor(milli / msPerSecond);
        ms = milli - secs * msPerSecond;
        if (const GregorianDateTime* other = thisDateObj->gregorianDateTime(vm))
            gregorianDateTime = *other;
    }

    double year = callFrame->argument(0).toIntegerPreserveNaN(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!std::isfinite(year)) {
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    }

    gregorianDateTime.setYear(toInt32((year >= 0 && year <= 99) ? (year + 1900) : year));
    double timeInMilliseconds = gregorianDateTimeToMS(vm, gregorianDateTime, ms, WTF::LocalTime);
    double result = timeClip(timeInMilliseconds);
    thisDateObj->setInternalNumber(result);
    return JSValue::encode(jsNumber(result));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncGetYear(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(vm, thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());

    // NOTE: IE returns the full year even in getYear.
    return JSValue::encode(jsNumber(gregorianDateTime->year() - 1900));
}

EncodedJSValue JSC_HOST_CALL dateProtoFuncToJSON(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    JSObject* object = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue timeValue = object->toPrimitive(globalObject, PreferNumber);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (timeValue.isNumber() && !std::isfinite(timeValue.asNumber()))
        return JSValue::encode(jsNull());

    JSValue toISOValue = object->get(globalObject, vm.propertyNames->toISOString);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    CallData callData;
    CallType callType = getCallData(vm, toISOValue, callData);
    if (callType == CallType::None)
        return throwVMTypeError(globalObject, scope, "toISOString is not a function"_s);

    JSValue result = call(globalObject, asObject(toISOValue), callType, callData, object, *vm.emptyList);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(result);
}

} // namespace JSC
