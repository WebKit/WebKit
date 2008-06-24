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
#include "date_object.h"

#include "DateMath.h"
#include "JSString.h"
#include "error_object.h"
#include "operations.h"
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/UnusedParam.h>

#if HAVE(ERRNO_H)
#include <errno.h>
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

#if PLATFORM(MAC)
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace WTF;

namespace KJS {

static JSValue* dateProtoFuncGetDate(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetDay(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetFullYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetHours(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetMilliSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetMinutes(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetMonth(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetTime(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetTimezoneOffset(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCDate(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCDay(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCFullYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCHours(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCMilliseconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCMinutes(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCMonth(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetDate(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetFullYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetHours(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetMilliSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetMinutes(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetMonth(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetTime(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCDate(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCFullYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCHours(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCMilliseconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCMinutes(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCMonth(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToDateString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToGMTString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToLocaleDateString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToLocaleString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToLocaleTimeString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToTimeString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToUTCString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncValueOf(ExecState*, JSObject*, JSValue*, const ArgList&);

}

#include "date_object.lut.h"

namespace KJS {

static double parseDate(const UString&);
static double timeClip(double);

static inline int gmtoffset(const GregorianDateTime& t)
{
    return t.utcOffset;
}

struct DateInstance::Cache {
    double m_gregorianDateTimeCachedForMS;
    GregorianDateTime m_cachedGregorianDateTime;
    double m_gregorianDateTimeUTCCachedForMS;
    GregorianDateTime m_cachedGregorianDateTimeUTC;
};

#if PLATFORM(MAC)

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

static UString formatLocaleDate(ExecState *exec, double time, bool includeDate, bool includeTime, const ArgList& args)
{
    CFDateFormatterStyle dateStyle = (includeDate ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);
    CFDateFormatterStyle timeStyle = (includeTime ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);

    bool useCustomFormat = false;
    UString customFormatString;

    UString arg0String = args[0]->toString(exec);
    if (arg0String == "custom" && !args[1]->isUndefined()) {
        useCustomFormat = true;
        customFormatString = args[1]->toString(exec);
    } else if (includeDate && includeTime && !args[1]->isUndefined()) {
        dateStyle = styleFromArgString(arg0String, dateStyle);
        timeStyle = styleFromArgString(args[1]->toString(exec), timeStyle);
    } else if (includeDate && !args[0]->isUndefined()) {
        dateStyle = styleFromArgString(arg0String, dateStyle);
    } else if (includeTime && !args[0]->isUndefined()) {
        timeStyle = styleFromArgString(arg0String, timeStyle);
    }

    CFLocaleRef locale = CFLocaleCopyCurrent();
    CFDateFormatterRef formatter = CFDateFormatterCreate(0, locale, dateStyle, timeStyle);
    CFRelease(locale);

    if (useCustomFormat) {
        CFStringRef customFormatCFString = CFStringCreateWithCharacters(0, (UniChar *)customFormatString.data(), customFormatString.size());
        CFDateFormatterSetFormat(formatter, customFormatCFString);
        CFRelease(customFormatCFString);
    }

    CFStringRef string = CFDateFormatterCreateStringWithAbsoluteTime(0, formatter, time - kCFAbsoluteTimeIntervalSince1970);

    CFRelease(formatter);

    // We truncate the string returned from CFDateFormatter if it's absurdly long (> 200 characters).
    // That's not great error handling, but it just won't happen so it doesn't matter.
    UChar buffer[200];
    const size_t bufferLength = sizeof(buffer) / sizeof(buffer[0]);
    size_t length = CFStringGetLength(string);
    ASSERT(length <= bufferLength);
    if (length > bufferLength)
        length = bufferLength;
    CFStringGetCharacters(string, CFRangeMake(0, length), reinterpret_cast<UniChar *>(buffer));

    CFRelease(string);

    return UString(buffer, length);
}

#else

enum LocaleDateTimeFormat { LocaleDateAndTime, LocaleDate, LocaleTime };
 
static JSCell* formatLocaleDate(ExecState* exec, const GregorianDateTime& gdt, const LocaleDateTimeFormat format)
{
    static const char* formatStrings[] = {"%#c", "%#x", "%X"};
 
    // Offset year if needed
    struct tm localTM = gdt;
    int year = gdt.year + 1900;
    bool yearNeedsOffset = year < 1900 || year > 2038;
    if (yearNeedsOffset) {
        localTM.tm_year = equivalentYearForDST(year) - 1900;
     }
 
    // Do the formatting
    const int bufsize=128;
    char timebuffer[bufsize];
    size_t ret = strftime(timebuffer, bufsize, formatStrings[format], &localTM);
 
    if ( ret == 0 )
        return jsString(exec, "");
 
    // Copy original into the buffer
    if (yearNeedsOffset && format != LocaleTime) {
        static const int yearLen = 5;   // FIXME will be a problem in the year 10,000
        char yearString[yearLen];
 
        snprintf(yearString, yearLen, "%d", localTM.tm_year + 1900);
        char* yearLocation = strstr(timebuffer, yearString);
        snprintf(yearString, yearLen, "%d", year);
 
        strncpy(yearLocation, yearString, yearLen - 1);
    }
 
    return jsString(exec, timebuffer);
}

#endif // PLATFORM(WIN_OS)

static UString formatDate(const GregorianDateTime &t)
{
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s %s %02d %04d",
        weekdayName[(t.weekDay + 6) % 7],
        monthName[t.month], t.monthDay, t.year + 1900);
    return buffer;
}

static UString formatDateUTCVariant(const GregorianDateTime &t)
{
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s, %02d %s %04d",
        weekdayName[(t.weekDay + 6) % 7],
        t.monthDay, monthName[t.month], t.year + 1900);
    return buffer;
}

static UString formatTime(const GregorianDateTime &t, bool utc)
{
    char buffer[100];
    if (utc) {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT", t.hour, t.minute, t.second);
    } else {
        int offset = abs(gmtoffset(t));
        char tzname[70];
        struct tm gtm = t;
        strftime(tzname, sizeof(tzname), "%Z", &gtm);

        if (tzname[0]) {
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT%c%02d%02d (%s)",
                t.hour, t.minute, t.second,
                gmtoffset(t) < 0 ? '-' : '+', offset / (60*60), (offset / 60) % 60, tzname);
        } else {
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT%c%02d%02d",
                t.hour, t.minute, t.second,
                gmtoffset(t) < 0 ? '-' : '+', offset / (60*60), (offset / 60) % 60);
        }
    }
    return UString(buffer);
}

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
        milliseconds += args[idx++]->toInt32(exec, ok) * msPerHour;
    }

    // minutes
    if (maxArgs >= 3 && idx < numArgs && ok) {
        t->minute = 0;
        milliseconds += args[idx++]->toInt32(exec, ok) * msPerMinute;
    }
    
    // seconds
    if (maxArgs >= 2 && idx < numArgs && ok) {
        t->second = 0;
        milliseconds += args[idx++]->toInt32(exec, ok) * msPerSecond;
    }
    
    if (!ok)
        return false;
        
    // milliseconds
    if (idx < numArgs) {
        double millis = args[idx]->toNumber(exec);
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
        t->year = args[idx++]->toInt32(exec, ok) - 1900;
    
    // months
    if (maxArgs >= 2 && idx < numArgs && ok)   
        t->month = args[idx++]->toInt32(exec, ok);
    
    // days
    if (idx < numArgs && ok) {   
        t->monthDay = 0;
        *ms += args[idx]->toInt32(exec, ok) * msPerDay;
    }
    
    return ok;
}

// ------------------------------ DateInstance ------------------------------

const ClassInfo DateInstance::info = {"Date", 0, 0, 0};

DateInstance::DateInstance(JSObject *proto)
  : JSWrapperObject(proto)
  , m_cache(0)
{
}

DateInstance::~DateInstance()
{
    delete m_cache;
}

void DateInstance::msToGregorianDateTime(double milli, bool outputIsUTC, GregorianDateTime& t) const
{
    if (!m_cache) {
        m_cache = new Cache;
        m_cache->m_gregorianDateTimeCachedForMS = NaN;
        m_cache->m_gregorianDateTimeUTCCachedForMS = NaN;
    }

    if (outputIsUTC) {
        if (m_cache->m_gregorianDateTimeUTCCachedForMS != milli) {
            KJS::msToGregorianDateTime(milli, true, m_cache->m_cachedGregorianDateTimeUTC);
            m_cache->m_gregorianDateTimeUTCCachedForMS = milli;
        }
        t.copyFrom(m_cache->m_cachedGregorianDateTimeUTC);
    } else {
        if (m_cache->m_gregorianDateTimeCachedForMS != milli) {
            KJS::msToGregorianDateTime(milli, false, m_cache->m_cachedGregorianDateTime);
            m_cache->m_gregorianDateTimeCachedForMS = milli;
        }
        t.copyFrom(m_cache->m_cachedGregorianDateTime);
    }
}

bool DateInstance::getTime(GregorianDateTime &t, int &offset) const
{
    double milli = internalNumber();
    if (isnan(milli))
        return false;
    
    msToGregorianDateTime(milli, false, t);
    offset = gmtoffset(t);
    return true;
}

bool DateInstance::getUTCTime(GregorianDateTime &t) const
{
    double milli = internalNumber();
    if (isnan(milli))
        return false;
    
    msToGregorianDateTime(milli, true, t);
    return true;
}

bool DateInstance::getTime(double &milli, int &offset) const
{
    milli = internalNumber();
    if (isnan(milli))
        return false;
    
    GregorianDateTime t;
    msToGregorianDateTime(milli, false, t);
    offset = gmtoffset(t);
    return true;
}

bool DateInstance::getUTCTime(double &milli) const
{
    milli = internalNumber();
    if (isnan(milli))
        return false;
    
    return true;
}

static inline bool isTime_tSigned()
{
    time_t minusOne = (time_t)(-1);
    return minusOne < 0;
}

// ------------------------------ DatePrototype -----------------------------

const ClassInfo DatePrototype::info = {"Date", &DateInstance::info, 0, ExecState::dateTable};

/* Source for date_object.lut.h
   FIXME: We could use templates to simplify the UTC variants.
@begin dateTable
  toString              dateProtoFuncToString                DontEnum|Function       0
  toUTCString           dateProtoFuncToUTCString             DontEnum|Function       0
  toDateString          dateProtoFuncToDateString            DontEnum|Function       0
  toTimeString          dateProtoFuncToTimeString            DontEnum|Function       0
  toLocaleString        dateProtoFuncToLocaleString          DontEnum|Function       0
  toLocaleDateString    dateProtoFuncToLocaleDateString      DontEnum|Function       0
  toLocaleTimeString    dateProtoFuncToLocaleTimeString      DontEnum|Function       0
  valueOf               dateProtoFuncValueOf                 DontEnum|Function       0
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

DatePrototype::DatePrototype(ExecState* exec, ObjectPrototype* objectProto)
    : DateInstance(objectProto)
{
    setInternalValue(jsNaN(exec));
    // The constructor will be added later, after DateConstructor has been built.
}

bool DatePrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, ExecState::dateTable(exec), this, propertyName, slot);
}

// ------------------------------ DateConstructor --------------------------------

// TODO: MakeTime (15.9.11.1) etc. ?

static JSValue* dateParse(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateNow(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateUTC(ExecState*, JSObject*, JSValue*, const ArgList&);

DateConstructor::DateConstructor(ExecState* exec, FunctionPrototype* funcProto, DatePrototype* dateProto)
  : InternalFunction(funcProto, Identifier(exec, dateProto->classInfo()->className))
{
  putDirect(exec->propertyNames().prototype, dateProto, DontEnum|DontDelete|ReadOnly);
  putDirectFunction(new (exec) PrototypeFunction(exec, funcProto, 1, exec->propertyNames().parse, dateParse), DontEnum);
  putDirectFunction(new (exec) PrototypeFunction(exec, funcProto, 7, exec->propertyNames().UTC, dateUTC), DontEnum);
  putDirectFunction(new (exec) PrototypeFunction(exec, funcProto, 0, exec->propertyNames().now, dateNow), DontEnum);
  putDirect(exec, exec->propertyNames().length, 7, ReadOnly | DontEnum | DontDelete);
}

// ECMA 15.9.3
static JSObject* constructDate(ExecState* exec, JSObject*, const ArgList& args)
{
  int numArgs = args.size();

  double value;

  if (numArgs == 0) { // new Date() ECMA 15.9.3.3
    value = getCurrentUTCTime();
  } else if (numArgs == 1) {
    if (args[0]->isObject(&DateInstance::info))
      value = static_cast<DateInstance*>(args[0])->internalNumber();
    else {
      JSValue* primitive = args[0]->toPrimitive(exec);
      if (primitive->isString())
        value = parseDate(primitive->getString());
      else
        value = primitive->toNumber(exec);
    }
  } else {
    if (isnan(args[0]->toNumber(exec))
        || isnan(args[1]->toNumber(exec))
        || (numArgs >= 3 && isnan(args[2]->toNumber(exec)))
        || (numArgs >= 4 && isnan(args[3]->toNumber(exec)))
        || (numArgs >= 5 && isnan(args[4]->toNumber(exec)))
        || (numArgs >= 6 && isnan(args[5]->toNumber(exec)))
        || (numArgs >= 7 && isnan(args[6]->toNumber(exec)))) {
      value = NaN;
    } else {
      GregorianDateTime t;
      int year = args[0]->toInt32(exec);
      t.year = (year >= 0 && year <= 99) ? year : year - 1900;
      t.month = args[1]->toInt32(exec);
      t.monthDay = (numArgs >= 3) ? args[2]->toInt32(exec) : 1;
      t.hour = args[3]->toInt32(exec);
      t.minute = args[4]->toInt32(exec);
      t.second = args[5]->toInt32(exec);
      t.isDST = -1;
      double ms = (numArgs >= 7) ? args[6]->toNumber(exec) : 0;
      value = gregorianDateTimeToMS(t, ms, false);
    }
  }
  
  DateInstance* ret = new (exec) DateInstance(exec->lexicalGlobalObject()->datePrototype());
  ret->setInternalValue(jsNumber(exec, timeClip(value)));
  return ret;
}

ConstructType DateConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructDate;
    return ConstructTypeNative;
}

// ECMA 15.9.2
static JSValue* callDate(ExecState* exec, JSObject*, JSValue*, const ArgList&)
{
    time_t localTime = time(0);
    tm localTM;
    getLocalTime(&localTime, &localTM);
    GregorianDateTime ts(localTM);
    return jsString(exec, formatDate(ts) + " " + formatTime(ts, false));
}

CallType DateConstructor::getCallData(CallData& callData)
{
    callData.native.function = callDate;
    return CallTypeNative;
}

static JSValue* dateParse(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, parseDate(args[0]->toString(exec)));
}

static JSValue* dateNow(ExecState* exec, JSObject*, JSValue*, const ArgList&)
{
    return jsNumber(exec, getCurrentUTCTime());
}

static JSValue* dateUTC(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    int n = args.size();
    if (isnan(args[0]->toNumber(exec))
            || isnan(args[1]->toNumber(exec))
            || (n >= 3 && isnan(args[2]->toNumber(exec)))
            || (n >= 4 && isnan(args[3]->toNumber(exec)))
            || (n >= 5 && isnan(args[4]->toNumber(exec)))
            || (n >= 6 && isnan(args[5]->toNumber(exec)))
            || (n >= 7 && isnan(args[6]->toNumber(exec)))) {
        return jsNaN(exec);
    }

    GregorianDateTime t;
    int year = args[0]->toInt32(exec);
    t.year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.month = args[1]->toInt32(exec);
    t.monthDay = (n >= 3) ? args[2]->toInt32(exec) : 1;
    t.hour = args[3]->toInt32(exec);
    t.minute = args[4]->toInt32(exec);
    t.second = args[5]->toInt32(exec);
    double ms = (n >= 7) ? args[6]->toNumber(exec) : 0;
    return jsNumber(exec, gregorianDateTimeToMS(t, ms, true));
}

// -----------------------------------------------------------------------------

// Code originally from krfcdate.cpp, but we don't want to use kdecore, and we want double range.

static inline double ymdhmsToSeconds(long year, int mon, int day, int hour, int minute, int second)
{
    double days = (day - 32075)
        + floor(1461 * (year + 4800.0 + (mon - 14) / 12) / 4)
        + 367 * (mon - 2 - (mon - 14) / 12 * 12) / 12
        - floor(3 * ((year + 4900.0 + (mon - 14) / 12) / 100) / 4)
        - 2440588;
    return ((days * hoursPerDay + hour) * minutesPerHour + minute) * secondsPerMinute + second;
}

// We follow the recommendation of RFC 2822 to consider all
// obsolete time zones not listed here equivalent to "-0000".
static const struct KnownZone {
#if !PLATFORM(WIN_OS)
    const
#endif
        char tzName[4];
    int tzOffset;
} known_zones[] = {
    { "UT", 0 },
    { "GMT", 0 },
    { "EST", -300 },
    { "EDT", -240 },
    { "CST", -360 },
    { "CDT", -300 },
    { "MST", -420 },
    { "MDT", -360 },
    { "PST", -480 },
    { "PDT", -420 }
};

inline static void skipSpacesAndComments(const char*& s)
{
    int nesting = 0;
    char ch;
    while ((ch = *s)) {
        if (!isASCIISpace(ch)) {
            if (ch == '(')
                nesting++;
            else if (ch == ')' && nesting > 0)
                nesting--;
            else if (nesting == 0)
                break;
        }
        s++;
    }
}

// returns 0-11 (Jan-Dec); -1 on failure
static int findMonth(const char* monthStr)
{
    ASSERT(monthStr);
    char needle[4];
    for (int i = 0; i < 3; ++i) {
        if (!*monthStr)
            return -1;
        needle[i] = static_cast<char>(toASCIILower(*monthStr++));
    }
    needle[3] = '\0';
    const char *haystack = "janfebmaraprmayjunjulaugsepoctnovdec";
    const char *str = strstr(haystack, needle);
    if (str) {
        int position = static_cast<int>(str - haystack);
        if (position % 3 == 0)
            return position / 3;
    }
    return -1;
}

static double parseDate(const UString &date)
{
    // This parses a date in the form:
    //     Tuesday, 09-Nov-99 23:12:40 GMT
    // or
    //     Sat, 01-Jan-2000 08:00:00 GMT
    // or
    //     Sat, 01 Jan 2000 08:00:00 GMT
    // or
    //     01 Jan 99 22:00 +0100    (exceptions in rfc822/rfc2822)
    // ### non RFC formats, added for Javascript:
    //     [Wednesday] January 09 1999 23:12:40 GMT
    //     [Wednesday] January 09 23:12:40 GMT 1999
    //
    // We ignore the weekday.

    CString dateCString = date.UTF8String();
    const char *dateString = dateCString.c_str();
     
    // Skip leading space
    skipSpacesAndComments(dateString);

    long month = -1;
    const char *wordStart = dateString;
    // Check contents of first words if not number
    while (*dateString && !isASCIIDigit(*dateString)) {
        if (isASCIISpace(*dateString) || *dateString == '(') {
            if (dateString - wordStart >= 3)
                month = findMonth(wordStart);
            skipSpacesAndComments(dateString);
            wordStart = dateString;
        } else
           dateString++;
    }

    // Missing delimiter between month and day (like "January29")?
    if (month == -1 && wordStart != dateString)
        month = findMonth(wordStart);

    skipSpacesAndComments(dateString);

    if (!*dateString)
        return NaN;

    // ' 09-Nov-99 23:12:40 GMT'
    char *newPosStr;
    errno = 0;
    long day = strtol(dateString, &newPosStr, 10);
    if (errno)
        return NaN;
    dateString = newPosStr;

    if (!*dateString)
        return NaN;

    if (day < 0)
        return NaN;

    long year = 0;
    if (day > 31) {
        // ### where is the boundary and what happens below?
        if (*dateString != '/')
            return NaN;
        // looks like a YYYY/MM/DD date
        if (!*++dateString)
            return NaN;
        year = day;
        month = strtol(dateString, &newPosStr, 10) - 1;
        if (errno)
            return NaN;
        dateString = newPosStr;
        if (*dateString++ != '/' || !*dateString)
            return NaN;
        day = strtol(dateString, &newPosStr, 10);
        if (errno)
            return NaN;
        dateString = newPosStr;
    } else if (*dateString == '/' && month == -1) {
        dateString++;
        // This looks like a MM/DD/YYYY date, not an RFC date.
        month = day - 1; // 0-based
        day = strtol(dateString, &newPosStr, 10);
        if (errno)
            return NaN;
        if (day < 1 || day > 31)
            return NaN;
        dateString = newPosStr;
        if (*dateString == '/')
            dateString++;
        if (!*dateString)
            return NaN;
     } else {
        if (*dateString == '-')
            dateString++;

        skipSpacesAndComments(dateString);

        if (*dateString == ',')
            dateString++;

        if (month == -1) { // not found yet
            month = findMonth(dateString);
            if (month == -1)
                return NaN;

            while (*dateString && *dateString != '-' && *dateString != ',' && !isASCIISpace(*dateString))
                dateString++;

            if (!*dateString)
                return NaN;

            // '-99 23:12:40 GMT'
            if (*dateString != '-' && *dateString != '/' && *dateString != ',' && !isASCIISpace(*dateString))
                return NaN;
            dateString++;
        }
    }

    if (month < 0 || month > 11)
        return NaN;

    // '99 23:12:40 GMT'
    if (year <= 0 && *dateString) {
        year = strtol(dateString, &newPosStr, 10);
        if (errno)
            return NaN;
    }
    
    // Don't fail if the time is missing.
    long hour = 0;
    long minute = 0;
    long second = 0;
    if (!*newPosStr)
        dateString = newPosStr;
    else {
        // ' 23:12:40 GMT'
        if (!(isASCIISpace(*newPosStr) || *newPosStr == ',')) {
            if (*newPosStr != ':')
                return NaN;
            // There was no year; the number was the hour.
            year = -1;
        } else {
            // in the normal case (we parsed the year), advance to the next number
            dateString = ++newPosStr;
            skipSpacesAndComments(dateString);
        }

        hour = strtol(dateString, &newPosStr, 10);
        // Do not check for errno here since we want to continue
        // even if errno was set becasue we are still looking
        // for the timezone!

        // Read a number? If not, this might be a timezone name.
        if (newPosStr != dateString) {
            dateString = newPosStr;

            if (hour < 0 || hour > 23)
                return NaN;

            if (!*dateString)
                return NaN;

            // ':12:40 GMT'
            if (*dateString++ != ':')
                return NaN;

            minute = strtol(dateString, &newPosStr, 10);
            if (errno)
                return NaN;
            dateString = newPosStr;

            if (minute < 0 || minute > 59)
                return NaN;

            // ':40 GMT'
            if (*dateString && *dateString != ':' && !isASCIISpace(*dateString))
                return NaN;

            // seconds are optional in rfc822 + rfc2822
            if (*dateString ==':') {
                dateString++;

                second = strtol(dateString, &newPosStr, 10);
                if (errno)
                    return NaN;
                dateString = newPosStr;
            
                if (second < 0 || second > 59)
                    return NaN;
            }

            skipSpacesAndComments(dateString);

            if (strncasecmp(dateString, "AM", 2) == 0) {
                if (hour > 12)
                    return NaN;
                if (hour == 12)
                    hour = 0;
                dateString += 2;
                skipSpacesAndComments(dateString);
            } else if (strncasecmp(dateString, "PM", 2) == 0) {
                if (hour > 12)
                    return NaN;
                if (hour != 12)
                    hour += 12;
                dateString += 2;
                skipSpacesAndComments(dateString);
            }
        }
    }

    bool haveTZ = false;
    int offset = 0;

    // Don't fail if the time zone is missing. 
    // Some websites omit the time zone (4275206).
    if (*dateString) {
        if (strncasecmp(dateString, "GMT", 3) == 0 || strncasecmp(dateString, "UTC", 3) == 0) {
            dateString += 3;
            haveTZ = true;
        }

        if (*dateString == '+' || *dateString == '-') {
            long o = strtol(dateString, &newPosStr, 10);
            if (errno)
                return NaN;
            dateString = newPosStr;

            if (o < -9959 || o > 9959)
                return NaN;

            int sgn = (o < 0) ? -1 : 1;
            o = abs(o);
            if (*dateString != ':') {
                offset = ((o / 100) * 60 + (o % 100)) * sgn;
            } else { // GMT+05:00
                long o2 = strtol(dateString, &newPosStr, 10);
                if (errno)
                    return NaN;
                dateString = newPosStr;
                offset = (o * 60 + o2) * sgn;
            }
            haveTZ = true;
        } else {
            for (int i = 0; i < int(sizeof(known_zones) / sizeof(KnownZone)); i++) {
                if (0 == strncasecmp(dateString, known_zones[i].tzName, strlen(known_zones[i].tzName))) {
                    offset = known_zones[i].tzOffset;
                    dateString += strlen(known_zones[i].tzName);
                    haveTZ = true;
                    break;
                }
            }
        }
    }

    skipSpacesAndComments(dateString);

    if (*dateString && year == -1) {
        year = strtol(dateString, &newPosStr, 10);
        if (errno)
            return NaN;
        dateString = newPosStr;
    }
     
    skipSpacesAndComments(dateString);
     
    // Trailing garbage
    if (*dateString)
        return NaN;

    // Y2K: Handle 2 digit years.
    if (year >= 0 && year < 100) {
        if (year < 50)
            year += 2000;
        else
            year += 1900;
    }

    // fall back to local timezone
    if (!haveTZ) {
        GregorianDateTime t;
        t.monthDay = day;
        t.month = month;
        t.year = year - 1900;
        t.isDST = -1;
        t.second = second;
        t.minute = minute;
        t.hour = hour;

        // Use our gregorianDateTimeToMS() rather than mktime() as the latter can't handle the full year range.
        return gregorianDateTimeToMS(t, 0, false);
    }

    return (ymdhmsToSeconds(year, month + 1, day, hour, minute, second) - (offset * 60.0)) * msPerSecond;
}

double timeClip(double t)
{
    if (!isfinite(t))
        return NaN;
    if (fabs(t) > 8.64E15)
        return NaN;
    return trunc(t);
}

// Functions

JSValue* dateProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatDate(t) + " " + formatTime(t, utc));
}

JSValue* dateProtoFuncToUTCString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatDateUTCVariant(t) + " " + formatTime(t, utc));
}

JSValue* dateProtoFuncToDateString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatDate(t));
}

JSValue* dateProtoFuncToTimeString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatTime(t, utc));
}

JSValue* dateProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

#if PLATFORM(MAC)
    double secs = floor(milli / msPerSecond);
    return jsString(exec, formatLocaleDate(exec, secs, true, true, args));
#else
    UNUSED_PARAM(args);

    const bool utc = false;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return formatLocaleDate(exec, t, LocaleDateAndTime);
#endif
}

JSValue* dateProtoFuncToLocaleDateString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

#if PLATFORM(MAC)
    double secs = floor(milli / msPerSecond);
    return jsString(exec, formatLocaleDate(exec, secs, true, false, args));
#else
    UNUSED_PARAM(args);

    const bool utc = false;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return formatLocaleDate(exec, t, LocaleDate);
#endif
}

JSValue* dateProtoFuncToLocaleTimeString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

#if PLATFORM(MAC)
    double secs = floor(milli / msPerSecond);
    return jsString(exec, formatLocaleDate(exec, secs, false, true, args));
#else
    UNUSED_PARAM(args);

    const bool utc = false;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return formatLocaleDate(exec, t, LocaleTime);
#endif
}

JSValue* dateProtoFuncValueOf(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    return jsNumber(exec, milli);
}

JSValue* dateProtoFuncGetTime(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    return jsNumber(exec, milli);
}

JSValue* dateProtoFuncGetFullYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, 1900 + t.year);
}

JSValue* dateProtoFuncGetUTCFullYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, 1900 + t.year);
}

JSValue* dateProtoFuncToGMTString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatDateUTCVariant(t) + " " + formatTime(t, utc));
}

JSValue* dateProtoFuncGetMonth(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.month);
}

JSValue* dateProtoFuncGetUTCMonth(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.month);
}

JSValue* dateProtoFuncGetDate(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.monthDay);
}

JSValue* dateProtoFuncGetUTCDate(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.monthDay);
}

JSValue* dateProtoFuncGetDay(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.weekDay);
}

JSValue* dateProtoFuncGetUTCDay(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.weekDay);
}

JSValue* dateProtoFuncGetHours(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.hour);
}

JSValue* dateProtoFuncGetUTCHours(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.hour);
}

JSValue* dateProtoFuncGetMinutes(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.minute);
}

JSValue* dateProtoFuncGetUTCMinutes(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.minute);
}

JSValue* dateProtoFuncGetSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.second);
}

JSValue* dateProtoFuncGetUTCSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.second);
}

JSValue* dateProtoFuncGetMilliSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    return jsNumber(exec, ms);
}

JSValue* dateProtoFuncGetUTCMilliseconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    return jsNumber(exec, ms);
}

JSValue* dateProtoFuncGetTimezoneOffset(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, -gmtoffset(t) / minutesPerHour);
}

JSValue* dateProtoFuncSetTime(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 

    double milli = timeClip(args[0]->toNumber(exec));
    JSValue* result = jsNumber(exec, milli);
    thisDateObj->setInternalValue(result);
    return result;
}

static JSValue* setNewValueFromTimeArgs(ExecState* exec, JSValue* thisValue, const ArgList& args, int numArgsToUse, bool inputIsUTC)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue);
    double milli = thisDateObj->internalNumber();
    
    if (args.isEmpty() || isnan(milli)) {
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
     
    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, inputIsUTC, t);

    if (!fillStructuresUsingTimeArgs(exec, args, numArgsToUse, &ms, &t)) {
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    } 
    
    JSValue* result = jsNumber(exec, gregorianDateTimeToMS(t, ms, inputIsUTC));
    thisDateObj->setInternalValue(result);
    return result;
}

static JSValue* setNewValueFromDateArgs(ExecState* exec, JSValue* thisValue, const ArgList& args, int numArgsToUse, bool inputIsUTC)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue);
    if (args.isEmpty()) {
        JSValue* result = jsNaN(exec);
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
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    } 
           
    JSValue* result = jsNumber(exec, gregorianDateTimeToMS(t, ms, inputIsUTC));
    thisDateObj->setInternalValue(result);
    return result;
}

JSValue* dateProtoFuncSetMilliSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCMilliseconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue* dateProtoFuncSetSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue* dateProtoFuncSetMinutes(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCMinutes(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue* dateProtoFuncSetHours(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 4, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCHours(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 4, inputIsUTC);
}

JSValue* dateProtoFuncSetDate(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCDate(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue* dateProtoFuncSetMonth(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCMonth(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue* dateProtoFuncSetFullYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCFullYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue* dateProtoFuncSetYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue);     
    if (args.isEmpty()) { 
        JSValue* result = jsNaN(exec);
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
    int32_t year = args[0]->toInt32(exec, ok);
    if (!ok) {
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
            
    t.year = (year > 99 || year < 0) ? year - 1900 : year;
    JSValue* result = jsNumber(exec, gregorianDateTimeToMS(t, ms, utc));
    thisDateObj->setInternalValue(result);
    return result;
}

JSValue* dateProtoFuncGetYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);

    // NOTE: IE returns the full year even in getYear.
    return jsNumber(exec, t.year);
}

} // namespace KJS
