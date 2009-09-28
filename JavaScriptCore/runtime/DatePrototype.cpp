/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2008, 2009 Torch Mobile, Inc. All rights reserved.
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
#include "Error.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "DateInstance.h"
#include <float.h>

#if !PLATFORM(MAC) && HAVE(LANGINFO_H)
#include <langinfo.h>
#endif

#include <limits.h>
#include <locale.h>
#include <math.h>
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/UnusedParam.h>

#if HAVE(SYS_PARAM_H)
#include <sys/param.h>
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

#if PLATFORM(MAC)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if PLATFORM(WINCE) && !PLATFORM(QT)
extern "C" size_t strftime(char * const s, const size_t maxsize, const char * const format, const struct tm * const t); //provided by libce
#endif

using namespace WTF;

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(DatePrototype);

static JSValue JSC_HOST_CALL dateProtoFuncGetDate(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetDay(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetFullYear(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetHours(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetMilliSeconds(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetMinutes(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetMonth(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetSeconds(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetTime(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetTimezoneOffset(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetUTCDate(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetUTCDay(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetUTCFullYear(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetUTCHours(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetUTCMilliseconds(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetUTCMinutes(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetUTCMonth(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetUTCSeconds(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncGetYear(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetDate(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetFullYear(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetHours(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetMilliSeconds(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetMinutes(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetMonth(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetSeconds(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetTime(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetUTCDate(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetUTCFullYear(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetUTCHours(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetUTCMilliseconds(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetUTCMinutes(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetUTCMonth(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetUTCSeconds(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncSetYear(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToDateString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToGMTString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToLocaleDateString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToLocaleString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToLocaleTimeString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToTimeString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToUTCString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL dateProtoFuncToISOString(ExecState*, JSObject*, JSValue, const ArgList&);

static JSValue JSC_HOST_CALL dateProtoFuncToJSON(ExecState*, JSObject*, JSValue, const ArgList&);

}

#include "DatePrototype.lut.h"

namespace JSC {

enum LocaleDateTimeFormat { LocaleDateAndTime, LocaleDate, LocaleTime };
 
#if PLATFORM(MAC)

// FIXME: Since this is superior to the strftime-based version, why limit this to PLATFORM(MAC)?
// Instead we should consider using this whenever PLATFORM(CF) is true.

static CFDateFormatterStyle styleFromArgString(const UString& string, CFDateFormatterStyle defaultStyle)
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

static JSCell* formatLocaleDate(ExecState* exec, DateInstance*, double timeInMilliseconds, LocaleDateTimeFormat format, const ArgList& args)
{
    CFDateFormatterStyle dateStyle = (format != LocaleTime ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);
    CFDateFormatterStyle timeStyle = (format != LocaleDate ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);

    bool useCustomFormat = false;
    UString customFormatString;

    UString arg0String = args.at(0).toString(exec);
    if (arg0String == "custom" && !args.at(1).isUndefined()) {
        useCustomFormat = true;
        customFormatString = args.at(1).toString(exec);
    } else if (format == LocaleDateAndTime && !args.at(1).isUndefined()) {
        dateStyle = styleFromArgString(arg0String, dateStyle);
        timeStyle = styleFromArgString(args.at(1).toString(exec), timeStyle);
    } else if (format != LocaleTime && !args.at(0).isUndefined())
        dateStyle = styleFromArgString(arg0String, dateStyle);
    else if (format != LocaleDate && !args.at(0).isUndefined())
        timeStyle = styleFromArgString(arg0String, timeStyle);

    CFLocaleRef locale = CFLocaleCopyCurrent();
    CFDateFormatterRef formatter = CFDateFormatterCreate(0, locale, dateStyle, timeStyle);
    CFRelease(locale);

    if (useCustomFormat) {
        CFStringRef customFormatCFString = CFStringCreateWithCharacters(0, customFormatString.data(), customFormatString.size());
        CFDateFormatterSetFormat(formatter, customFormatCFString);
        CFRelease(customFormatCFString);
    }

    CFStringRef string = CFDateFormatterCreateStringWithAbsoluteTime(0, formatter, floor(timeInMilliseconds / msPerSecond) - kCFAbsoluteTimeIntervalSince1970);

    CFRelease(formatter);

    // We truncate the string returned from CFDateFormatter if it's absurdly long (> 200 characters).
    // That's not great error handling, but it just won't happen so it doesn't matter.
    UChar buffer[200];
    const size_t bufferLength = sizeof(buffer) / sizeof(buffer[0]);
    size_t length = CFStringGetLength(string);
    ASSERT(length <= bufferLength);
    if (length > bufferLength)
        length = bufferLength;
    CFStringGetCharacters(string, CFRangeMake(0, length), buffer);

    CFRelease(string);

    return jsNontrivialString(exec, UString(buffer, length));
}

#else // !PLATFORM(MAC)

static JSCell* formatLocaleDate(ExecState* exec, const GregorianDateTime& gdt, LocaleDateTimeFormat format)
{
#if HAVE(LANGINFO_H)
    static const nl_item formats[] = { D_T_FMT, D_FMT, T_FMT };
#elif (PLATFORM(WINCE) && !PLATFORM(QT)) || PLATFORM(SYMBIAN)
     // strftime() does not support '#' on WinCE or Symbian
    static const char* const formatStrings[] = { "%c", "%x", "%X" };
#else
    static const char* const formatStrings[] = { "%#c", "%#x", "%X" };
#endif
 
    // Offset year if needed
    struct tm localTM = gdt;
    int year = gdt.year + 1900;
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
        return jsEmptyString(exec);
 
    // Copy original into the buffer
    if (yearNeedsOffset && format != LocaleTime) {
        static const int yearLen = 5;   // FIXME will be a problem in the year 10,000
        char yearString[yearLen];
 
        snprintf(yearString, yearLen, "%d", localTM.tm_year + 1900);
        char* yearLocation = strstr(timebuffer, yearString);
        snprintf(yearString, yearLen, "%d", year);
 
        strncpy(yearLocation, yearString, yearLen - 1);
    }
 
    return jsNontrivialString(exec, timebuffer);
}

static JSCell* formatLocaleDate(ExecState* exec, DateInstance* dateObject, double timeInMilliseconds, LocaleDateTimeFormat format, const ArgList&)
{
    GregorianDateTime gregorianDateTime;
    const bool notUTC = false;
    dateObject->msToGregorianDateTime(timeInMilliseconds, notUTC, gregorianDateTime);
    return formatLocaleDate(exec, gregorianDateTime, format);
}

#endif // !PLATFORM(MAC)

// Converts a list of arguments sent to a Date member function into milliseconds, updating
// ms (representing milliseconds) and t (representing the rest of the date structure) appropriately.
//
// Format of member function: f([hour,] [min,] [sec,] [ms])
static bool fillStructuresUsingTimeArgs(ExecState* exec, const ArgList& args, int maxArgs, double* ms, GregorianDateTime* t)
{
    double milliseconds = 0;
    bool ok = true;
    int idx = 0;
    int numArgs = args.size();
    
    // JS allows extra trailing arguments -- ignore them
    if (numArgs > maxArgs)
        numArgs = maxArgs;

    // hours
    if (maxArgs >= 4 && idx < numArgs) {
        t->hour = 0;
        milliseconds += args.at(idx++).toInt32(exec, ok) * msPerHour;
    }

    // minutes
    if (maxArgs >= 3 && idx < numArgs && ok) {
        t->minute = 0;
        milliseconds += args.at(idx++).toInt32(exec, ok) * msPerMinute;
    }
    
    // seconds
    if (maxArgs >= 2 && idx < numArgs && ok) {
        t->second = 0;
        milliseconds += args.at(idx++).toInt32(exec, ok) * msPerSecond;
    }
    
    if (!ok)
        return false;
        
    // milliseconds
    if (idx < numArgs) {
        double millis = args.at(idx).toNumber(exec);
        ok = isfinite(millis);
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
static bool fillStructuresUsingDateArgs(ExecState *exec, const ArgList& args, int maxArgs, double *ms, GregorianDateTime *t)
{
    int idx = 0;
    bool ok = true;
    int numArgs = args.size();
  
    // JS allows extra trailing arguments -- ignore them
    if (numArgs > maxArgs)
        numArgs = maxArgs;
  
    // years
    if (maxArgs >= 3 && idx < numArgs)
        t->year = args.at(idx++).toInt32(exec, ok) - 1900;
    
    // months
    if (maxArgs >= 2 && idx < numArgs && ok)   
        t->month = args.at(idx++).toInt32(exec, ok);
    
    // days
    if (idx < numArgs && ok) {   
        t->monthDay = 0;
        *ms += args.at(idx).toInt32(exec, ok) * msPerDay;
    }
    
    return ok;
}

const ClassInfo DatePrototype::info = {"Date", &DateInstance::info, 0, ExecState::dateTable};

/* Source for DatePrototype.lut.h
@begin dateTable
  toString              dateProtoFuncToString                DontEnum|Function       0
  toISOString           dateProtoFuncToISOString             DontEnum|Function       0
  toUTCString           dateProtoFuncToUTCString             DontEnum|Function       0
  toDateString          dateProtoFuncToDateString            DontEnum|Function       0
  toTimeString          dateProtoFuncToTimeString            DontEnum|Function       0
  toLocaleString        dateProtoFuncToLocaleString          DontEnum|Function       0
  toLocaleDateString    dateProtoFuncToLocaleDateString      DontEnum|Function       0
  toLocaleTimeString    dateProtoFuncToLocaleTimeString      DontEnum|Function       0
  valueOf               dateProtoFuncGetTime                 DontEnum|Function       0
  getTime               dateProtoFuncGetTime                 DontEnum|Function       0
  getFullYear           dateProtoFuncGetFullYear             DontEnum|Function       0
  getUTCFullYear        dateProtoFuncGetUTCFullYear          DontEnum|Function       0
  toGMTString           dateProtoFuncToGMTString             DontEnum|Function       0
  getMonth              dateProtoFuncGetMonth                DontEnum|Function       0
  getUTCMonth           dateProtoFuncGetUTCMonth             DontEnum|Function       0
  getDate               dateProtoFuncGetDate                 DontEnum|Function       0
  getUTCDate            dateProtoFuncGetUTCDate              DontEnum|Function       0
  getDay                dateProtoFuncGetDay                  DontEnum|Function       0
  getUTCDay             dateProtoFuncGetUTCDay               DontEnum|Function       0
  getHours              dateProtoFuncGetHours                DontEnum|Function       0
  getUTCHours           dateProtoFuncGetUTCHours             DontEnum|Function       0
  getMinutes            dateProtoFuncGetMinutes              DontEnum|Function       0
  getUTCMinutes         dateProtoFuncGetUTCMinutes           DontEnum|Function       0
  getSeconds            dateProtoFuncGetSeconds              DontEnum|Function       0
  getUTCSeconds         dateProtoFuncGetUTCSeconds           DontEnum|Function       0
  getMilliseconds       dateProtoFuncGetMilliSeconds         DontEnum|Function       0
  getUTCMilliseconds    dateProtoFuncGetUTCMilliseconds      DontEnum|Function       0
  getTimezoneOffset     dateProtoFuncGetTimezoneOffset       DontEnum|Function       0
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
  getYear               dateProtoFuncGetYear                 DontEnum|Function       0
  toJSON                dateProtoFuncToJSON                  DontEnum|Function       0
@end
*/

// ECMA 15.9.4

DatePrototype::DatePrototype(ExecState* exec, NonNullPassRefPtr<Structure> structure)
    : DateInstance(structure)
{
    setInternalValue(jsNaN(exec));
    // The constructor will be added later, after DateConstructor has been built.
}

bool DatePrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, ExecState::dateTable(exec), this, propertyName, slot);
}


bool DatePrototype::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticFunctionDescriptor<JSObject>(exec, ExecState::dateTable(exec), this, propertyName, descriptor);
}

// Functions

JSValue JSC_HOST_CALL dateProtoFuncToString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNontrivialString(exec, formatDate(t) + " " + formatTime(t, utc));
}

JSValue JSC_HOST_CALL dateProtoFuncToUTCString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNontrivialString(exec, formatDateUTCVariant(t) + " " + formatTime(t, utc));
}

JSValue JSC_HOST_CALL dateProtoFuncToISOString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);
    
    const bool utc = true;
    
    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (!isfinite(milli))
        return jsNontrivialString(exec, "Invalid Date");
    
    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    // Maximum amount of space we need in buffer: 6 (max. digits in year) + 2 * 5 (2 characters each for month, day, hour, minute, second) + 4 (. + 3 digits for milliseconds)
    // 6 for formatting and one for null termination = 27.  We add one extra character to allow us to force null termination.
    char buffer[28];
    snprintf(buffer, sizeof(buffer) - 1, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + t.year, t.month + 1, t.monthDay, t.hour, t.minute, t.second, static_cast<int>(fmod(milli, 1000)));
    buffer[sizeof(buffer) - 1] = 0;
    return jsNontrivialString(exec, buffer);
}

JSValue JSC_HOST_CALL dateProtoFuncToDateString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNontrivialString(exec, formatDate(t));
}

JSValue JSC_HOST_CALL dateProtoFuncToTimeString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNontrivialString(exec, formatTime(t, utc));
}

JSValue JSC_HOST_CALL dateProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    return formatLocaleDate(exec, thisDateObj, milli, LocaleDateAndTime, args);
}

JSValue JSC_HOST_CALL dateProtoFuncToLocaleDateString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    return formatLocaleDate(exec, thisDateObj, milli, LocaleDate, args);
}

JSValue JSC_HOST_CALL dateProtoFuncToLocaleTimeString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    return formatLocaleDate(exec, thisDateObj, milli, LocaleTime, args);
}

JSValue JSC_HOST_CALL dateProtoFuncGetTime(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    return jsNumber(exec, milli);
}

JSValue JSC_HOST_CALL dateProtoFuncGetFullYear(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, 1900 + t.year);
}

JSValue JSC_HOST_CALL dateProtoFuncGetUTCFullYear(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, 1900 + t.year);
}

JSValue JSC_HOST_CALL dateProtoFuncToGMTString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNontrivialString(exec, formatDateUTCVariant(t) + " " + formatTime(t, utc));
}

JSValue JSC_HOST_CALL dateProtoFuncGetMonth(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.month);
}

JSValue JSC_HOST_CALL dateProtoFuncGetUTCMonth(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.month);
}

JSValue JSC_HOST_CALL dateProtoFuncGetDate(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.monthDay);
}

JSValue JSC_HOST_CALL dateProtoFuncGetUTCDate(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.monthDay);
}

JSValue JSC_HOST_CALL dateProtoFuncGetDay(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.weekDay);
}

JSValue JSC_HOST_CALL dateProtoFuncGetUTCDay(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.weekDay);
}

JSValue JSC_HOST_CALL dateProtoFuncGetHours(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.hour);
}

JSValue JSC_HOST_CALL dateProtoFuncGetUTCHours(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.hour);
}

JSValue JSC_HOST_CALL dateProtoFuncGetMinutes(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.minute);
}

JSValue JSC_HOST_CALL dateProtoFuncGetUTCMinutes(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.minute);
}

JSValue JSC_HOST_CALL dateProtoFuncGetSeconds(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.second);
}

JSValue JSC_HOST_CALL dateProtoFuncGetUTCSeconds(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.second);
}

JSValue JSC_HOST_CALL dateProtoFuncGetMilliSeconds(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    return jsNumber(exec, ms);
}

JSValue JSC_HOST_CALL dateProtoFuncGetUTCMilliseconds(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    return jsNumber(exec, ms);
}

JSValue JSC_HOST_CALL dateProtoFuncGetTimezoneOffset(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, -gmtoffset(t) / minutesPerHour);
}

JSValue JSC_HOST_CALL dateProtoFuncSetTime(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 

    double milli = timeClip(args.at(0).toNumber(exec));
    JSValue result = jsNumber(exec, milli);
    thisDateObj->setInternalValue(result);
    return result;
}

static JSValue setNewValueFromTimeArgs(ExecState* exec, JSValue thisValue, const ArgList& args, int numArgsToUse, bool inputIsUTC)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue);
    double milli = thisDateObj->internalNumber();
    
    if (args.isEmpty() || isnan(milli)) {
        JSValue result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
     
    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, inputIsUTC, t);

    if (!fillStructuresUsingTimeArgs(exec, args, numArgsToUse, &ms, &t)) {
        JSValue result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    } 
    
    JSValue result = jsNumber(exec, gregorianDateTimeToMS(t, ms, inputIsUTC));
    thisDateObj->setInternalValue(result);
    return result;
}

static JSValue setNewValueFromDateArgs(ExecState* exec, JSValue thisValue, const ArgList& args, int numArgsToUse, bool inputIsUTC)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue);
    if (args.isEmpty()) {
        JSValue result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }      
    
    double milli = thisDateObj->internalNumber();
    double ms = 0;

    GregorianDateTime t;
    if (numArgsToUse == 3 && isnan(milli))
        // Based on ECMA 262 15.9.5.40 - .41 (set[UTC]FullYear)
        // the time must be reset to +0 if it is NaN. 
        thisDateObj->msToGregorianDateTime(0, true, t);
    else {
        double secs = floor(milli / msPerSecond);
        ms = milli - secs * msPerSecond;
        thisDateObj->msToGregorianDateTime(milli, inputIsUTC, t);
    }
    
    if (!fillStructuresUsingDateArgs(exec, args, numArgsToUse, &ms, &t)) {
        JSValue result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    } 
           
    JSValue result = jsNumber(exec, gregorianDateTimeToMS(t, ms, inputIsUTC));
    thisDateObj->setInternalValue(result);
    return result;
}

JSValue JSC_HOST_CALL dateProtoFuncSetMilliSeconds(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetUTCMilliseconds(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetSeconds(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetUTCSeconds(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetMinutes(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetUTCMinutes(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetHours(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 4, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetUTCHours(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 4, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetDate(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetUTCDate(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetMonth(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetUTCMonth(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetFullYear(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetUTCFullYear(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue JSC_HOST_CALL dateProtoFuncSetYear(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue);     
    if (args.isEmpty()) { 
        JSValue result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
    
    double milli = thisDateObj->internalNumber();
    double ms = 0;

    GregorianDateTime t;
    if (isnan(milli))
        // Based on ECMA 262 B.2.5 (setYear)
        // the time must be reset to +0 if it is NaN. 
        thisDateObj->msToGregorianDateTime(0, true, t);
    else {   
        double secs = floor(milli / msPerSecond);
        ms = milli - secs * msPerSecond;
        thisDateObj->msToGregorianDateTime(milli, utc, t);
    }
    
    bool ok = true;
    int32_t year = args.at(0).toInt32(exec, ok);
    if (!ok) {
        JSValue result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
            
    t.year = (year > 99 || year < 0) ? year - 1900 : year;
    JSValue result = jsNumber(exec, gregorianDateTimeToMS(t, ms, utc));
    thisDateObj->setInternalValue(result);
    return result;
}

JSValue JSC_HOST_CALL dateProtoFuncGetYear(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    if (!thisValue.inherits(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);

    // NOTE: IE returns the full year even in getYear.
    return jsNumber(exec, t.year);
}

JSValue JSC_HOST_CALL dateProtoFuncToJSON(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    JSObject* object = thisValue.toThisObject(exec);
    if (exec->hadException())
        return jsNull();
    
    JSValue toISOValue = object->get(exec, exec->globalData().propertyNames->toISOString);
    if (exec->hadException())
        return jsNull();

    CallData callData;
    CallType callType = toISOValue.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError, "toISOString is not a function");

    JSValue result = call(exec, asObject(toISOValue), callType, callData, object, exec->emptyList());
    if (exec->hadException())
        return jsNull();
    if (result.isObject())
        return throwError(exec, TypeError, "toISOString did not return a primitive value");
    return result;
}

} // namespace JSC
