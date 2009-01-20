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
#include "DatePrototype.h"

#include "DateMath.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "DateInstance.h"
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <time.h>
#include <wtf/Assertions.h>
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

using namespace WTF;

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(DatePrototype);

static JSValuePtr dateProtoFuncGetDate(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetDay(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetFullYear(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetHours(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetMilliSeconds(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetMinutes(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetMonth(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetSeconds(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetTime(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetTimezoneOffset(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetUTCDate(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetUTCDay(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetUTCFullYear(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetUTCHours(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetUTCMilliseconds(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetUTCMinutes(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetUTCMonth(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetUTCSeconds(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncGetYear(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetDate(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetFullYear(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetHours(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetMilliSeconds(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetMinutes(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetMonth(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetSeconds(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetTime(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetUTCDate(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetUTCFullYear(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetUTCHours(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetUTCMilliseconds(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetUTCMinutes(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetUTCMonth(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetUTCSeconds(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncSetYear(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncToDateString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncToGMTString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncToLocaleDateString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncToLocaleString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncToLocaleTimeString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncToString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncToTimeString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateProtoFuncToUTCString(ExecState*, JSObject*, JSValuePtr, const ArgList&);

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

    UString arg0String = args.at(exec, 0).toString(exec);
    if (arg0String == "custom" && !args.at(exec, 1).isUndefined()) {
        useCustomFormat = true;
        customFormatString = args.at(exec, 1).toString(exec);
    } else if (format == LocaleDateAndTime && !args.at(exec, 1).isUndefined()) {
        dateStyle = styleFromArgString(arg0String, dateStyle);
        timeStyle = styleFromArgString(args.at(exec, 1).toString(exec), timeStyle);
    } else if (format != LocaleTime && !args.at(exec, 0).isUndefined())
        dateStyle = styleFromArgString(arg0String, dateStyle);
    else if (format != LocaleDate && !args.at(exec, 0).isUndefined())
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
    static const char* const formatStrings[] = { "%#c", "%#x", "%X" };
 
    // Offset year if needed
    struct tm localTM = gdt;
    int year = gdt.year + 1900;
    bool yearNeedsOffset = year < 1900 || year > 2038;
    if (yearNeedsOffset)
        localTM.tm_year = equivalentYearForDST(year) - 1900;
 
    // Do the formatting
    const int bufsize = 128;
    char timebuffer[bufsize];
    size_t ret = strftime(timebuffer, bufsize, formatStrings[format], &localTM);
 
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
        milliseconds += args.at(exec, idx++).toInt32(exec, ok) * msPerHour;
    }

    // minutes
    if (maxArgs >= 3 && idx < numArgs && ok) {
        t->minute = 0;
        milliseconds += args.at(exec, idx++).toInt32(exec, ok) * msPerMinute;
    }
    
    // seconds
    if (maxArgs >= 2 && idx < numArgs && ok) {
        t->second = 0;
        milliseconds += args.at(exec, idx++).toInt32(exec, ok) * msPerSecond;
    }
    
    if (!ok)
        return false;
        
    // milliseconds
    if (idx < numArgs) {
        double millis = args.at(exec, idx).toNumber(exec);
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
        t->year = args.at(exec, idx++).toInt32(exec, ok) - 1900;
    
    // months
    if (maxArgs >= 2 && idx < numArgs && ok)   
        t->month = args.at(exec, idx++).toInt32(exec, ok);
    
    // days
    if (idx < numArgs && ok) {   
        t->monthDay = 0;
        *ms += args.at(exec, idx).toInt32(exec, ok) * msPerDay;
    }
    
    return ok;
}

const ClassInfo DatePrototype::info = {"Date", &DateInstance::info, 0, ExecState::dateTable};

/* Source for DatePrototype.lut.h
@begin dateTable
  toString              dateProtoFuncToString                DontEnum|Function       0
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
@end
*/

// ECMA 15.9.4

DatePrototype::DatePrototype(ExecState* exec, PassRefPtr<Structure> structure)
    : DateInstance(structure)
{
    setInternalValue(jsNaN(exec));
    // The constructor will be added later, after DateConstructor has been built.
}

bool DatePrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, ExecState::dateTable(exec), this, propertyName, slot);
}

// Functions

JSValuePtr dateProtoFuncToString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncToUTCString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncToDateString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncToTimeString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    return formatLocaleDate(exec, thisDateObj, milli, LocaleDateAndTime, args);
}

JSValuePtr dateProtoFuncToLocaleDateString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    return formatLocaleDate(exec, thisDateObj, milli, LocaleDate, args);
}

JSValuePtr dateProtoFuncToLocaleTimeString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNontrivialString(exec, "Invalid Date");

    return formatLocaleDate(exec, thisDateObj, milli, LocaleTime, args);
}

JSValuePtr dateProtoFuncGetTime(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    return jsNumber(exec, milli);
}

JSValuePtr dateProtoFuncGetFullYear(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetUTCFullYear(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncToGMTString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetMonth(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetUTCMonth(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetDate(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetUTCDate(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetDay(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetUTCDay(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetHours(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetUTCHours(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetMinutes(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetUTCMinutes(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetSeconds(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetUTCSeconds(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncGetMilliSeconds(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    return jsNumber(exec, ms);
}

JSValuePtr dateProtoFuncGetUTCMilliseconds(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    return jsNumber(exec, ms);
}

JSValuePtr dateProtoFuncGetTimezoneOffset(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

JSValuePtr dateProtoFuncSetTime(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue); 

    double milli = timeClip(args.at(exec, 0).toNumber(exec));
    JSValuePtr result = jsNumber(exec, milli);
    thisDateObj->setInternalValue(result);
    return result;
}

static JSValuePtr setNewValueFromTimeArgs(ExecState* exec, JSValuePtr thisValue, const ArgList& args, int numArgsToUse, bool inputIsUTC)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue);
    double milli = thisDateObj->internalNumber();
    
    if (args.isEmpty() || isnan(milli)) {
        JSValuePtr result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
     
    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, inputIsUTC, t);

    if (!fillStructuresUsingTimeArgs(exec, args, numArgsToUse, &ms, &t)) {
        JSValuePtr result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    } 
    
    JSValuePtr result = jsNumber(exec, gregorianDateTimeToMS(t, ms, inputIsUTC));
    thisDateObj->setInternalValue(result);
    return result;
}

static JSValuePtr setNewValueFromDateArgs(ExecState* exec, JSValuePtr thisValue, const ArgList& args, int numArgsToUse, bool inputIsUTC)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = asDateInstance(thisValue);
    if (args.isEmpty()) {
        JSValuePtr result = jsNaN(exec);
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
        JSValuePtr result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    } 
           
    JSValuePtr result = jsNumber(exec, gregorianDateTimeToMS(t, ms, inputIsUTC));
    thisDateObj->setInternalValue(result);
    return result;
}

JSValuePtr dateProtoFuncSetMilliSeconds(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValuePtr dateProtoFuncSetUTCMilliseconds(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValuePtr dateProtoFuncSetSeconds(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValuePtr dateProtoFuncSetUTCSeconds(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValuePtr dateProtoFuncSetMinutes(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValuePtr dateProtoFuncSetUTCMinutes(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValuePtr dateProtoFuncSetHours(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 4, inputIsUTC);
}

JSValuePtr dateProtoFuncSetUTCHours(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 4, inputIsUTC);
}

JSValuePtr dateProtoFuncSetDate(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValuePtr dateProtoFuncSetUTCDate(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValuePtr dateProtoFuncSetMonth(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValuePtr dateProtoFuncSetUTCMonth(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValuePtr dateProtoFuncSetFullYear(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValuePtr dateProtoFuncSetUTCFullYear(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValuePtr dateProtoFuncSetYear(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    if (!thisValue.isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = asDateInstance(thisValue);     
    if (args.isEmpty()) { 
        JSValuePtr result = jsNaN(exec);
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
    int32_t year = args.at(exec, 0).toInt32(exec, ok);
    if (!ok) {
        JSValuePtr result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
            
    t.year = (year > 99 || year < 0) ? year - 1900 : year;
    JSValuePtr result = jsNumber(exec, gregorianDateTimeToMS(t, ms, utc));
    thisDateObj->setInternalValue(result);
    return result;
}

JSValuePtr dateProtoFuncGetYear(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&DateInstance::info))
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

} // namespace JSC
