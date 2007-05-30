/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#include "config.h"
#include "date_object.h"
#include "date_object.lut.h"
#include "internal.h"

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

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_object.h"
#include "operations.h"
#include "DateMath.h"

#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>

#if PLATFORM(MAC)
    #include <CoreFoundation/CoreFoundation.h>
#endif

namespace KJS {

static double parseDate(const UString&);
static double timeClip(double);

inline int gmtoffset(const GregorianDateTime& t)
{
    return t.utcOffset;
}


/**
 * @internal
 *
 * Class to implement all methods that are properties of the
 * Date object
 */
class DateObjectFuncImp : public InternalFunctionImp {
public:
    DateObjectFuncImp(ExecState *, FunctionPrototype *, int i, int len, const Identifier& );

    virtual JSValue *callAsFunction(ExecState *, JSObject *thisObj, const List &args);

    enum { Parse, UTC };

private:
    int id;
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

static UString formatLocaleDate(ExecState *exec, double time, bool includeDate, bool includeTime, const List &args)
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
    assert(length <= bufferLength);
    if (length > bufferLength)
        length = bufferLength;
    CFStringGetCharacters(string, CFRangeMake(0, length), reinterpret_cast<UniChar *>(buffer));

    CFRelease(string);

    return UString(buffer, length);
}

#else

enum LocaleDateTimeFormat { LocaleDateAndTime, LocaleDate, LocaleTime };
 
static JSCell* formatLocaleDate(const GregorianDateTime& gdt, const LocaleDateTimeFormat format)
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
        return jsString("");
 
    // Copy original into the buffer
    if (yearNeedsOffset && format != LocaleTime) {
        static const int yearLen = 5;   // FIXME will be a problem in the year 10,000
        char yearString[yearLen];
 
        snprintf(yearString, yearLen, "%d", localTM.tm_year + 1900);
        char* yearLocation = strstr(timebuffer, yearString);
        snprintf(yearString, yearLen, "%d", year);
 
        strncpy(yearLocation, yearString, yearLen - 1);
    }
 
    return jsString(timebuffer);
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

        if (tzname) {
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
static void fillStructuresUsingTimeArgs(ExecState *exec, const List &args, int maxArgs, double *ms, GregorianDateTime *t)
{
    double milliseconds = 0;
    int idx = 0;
    int numArgs = args.size();
    
    // JS allows extra trailing arguments -- ignore them
    if (numArgs > maxArgs)
        numArgs = maxArgs;

    // hours
    if (maxArgs >= 4 && idx < numArgs) {
        t->hour = 0;
        milliseconds += args[idx++]->toInt32(exec) * msPerHour;
    }

    // minutes
    if (maxArgs >= 3 && idx < numArgs) {
        t->minute = 0;
        milliseconds += args[idx++]->toInt32(exec) * msPerMinute;
    }
    
    // seconds
    if (maxArgs >= 2 && idx < numArgs) {
        t->second = 0;
        milliseconds += args[idx++]->toInt32(exec) * msPerSecond;
    }
    
    // milliseconds
    if (idx < numArgs) {
        milliseconds += roundValue(exec, args[idx]);
    } else {
        milliseconds += *ms;
    }
    
    *ms = milliseconds;
}

// Converts a list of arguments sent to a Date member function into years, months, and milliseconds, updating
// ms (representing milliseconds) and t (representing the rest of the date structure) appropriately.
//
// Format of member function: f([years,] [months,] [days])
static void fillStructuresUsingDateArgs(ExecState *exec, const List &args, int maxArgs, double *ms, GregorianDateTime *t)
{
    int idx = 0;
    int numArgs = args.size();
  
    // JS allows extra trailing arguments -- ignore them
    if (numArgs > maxArgs)
        numArgs = maxArgs;
  
    // years
    if (maxArgs >= 3 && idx < numArgs)
        t->year = args[idx++]->toInt32(exec) - 1900;
  
    // months
    if (maxArgs >= 2 && idx < numArgs)
        t->month = args[idx++]->toInt32(exec);
  
    // days
    if (idx < numArgs) {
        t->monthDay = 0;
        *ms += args[idx]->toInt32(exec) * msPerDay;
    }
}

// ------------------------------ DateInstance ------------------------------

const ClassInfo DateInstance::info = {"Date", 0, 0, 0};

DateInstance::DateInstance(JSObject *proto)
  : JSWrapperObject(proto)
{
}

bool DateInstance::getTime(GregorianDateTime &t, int &offset) const
{
    double milli = internalValue()->getNumber();
    if (isNaN(milli))
        return false;
    
    msToGregorianDateTime(milli, false, t);
    offset = gmtoffset(t);
    return true;
}

bool DateInstance::getUTCTime(GregorianDateTime &t) const
{
    double milli = internalValue()->getNumber();
    if (isNaN(milli))
        return false;
    
    msToGregorianDateTime(milli, true, t);
    return true;
}

bool DateInstance::getTime(double &milli, int &offset) const
{
    milli = internalValue()->getNumber();
    if (isNaN(milli))
        return false;
    
    GregorianDateTime t;
    msToGregorianDateTime(milli, false, t);
    offset = gmtoffset(t);
    return true;
}

bool DateInstance::getUTCTime(double &milli) const
{
    milli = internalValue()->getNumber();
    if (isNaN(milli))
        return false;
    
    return true;
}

static inline bool isTime_tSigned()
{
    time_t minusOne = (time_t)(-1);
    return minusOne < 0;
}

// ------------------------------ DatePrototype -----------------------------

const ClassInfo DatePrototype::info = {"Date", &DateInstance::info, &dateTable, 0};

/* Source for date_object.lut.h
   We use a negative ID to denote the "UTC" variant.
@begin dateTable 61
  toString              DateProtoFunc::ToString                 DontEnum|Function       0
  toUTCString           -DateProtoFunc::ToUTCString             DontEnum|Function       0
  toDateString          DateProtoFunc::ToDateString             DontEnum|Function       0
  toTimeString          DateProtoFunc::ToTimeString             DontEnum|Function       0
  toLocaleString        DateProtoFunc::ToLocaleString           DontEnum|Function       0
  toLocaleDateString    DateProtoFunc::ToLocaleDateString       DontEnum|Function       0
  toLocaleTimeString    DateProtoFunc::ToLocaleTimeString       DontEnum|Function       0
  valueOf               DateProtoFunc::ValueOf                  DontEnum|Function       0
  getTime               DateProtoFunc::GetTime                  DontEnum|Function       0
  getFullYear           DateProtoFunc::GetFullYear              DontEnum|Function       0
  getUTCFullYear        -DateProtoFunc::GetFullYear             DontEnum|Function       0
  toGMTString           -DateProtoFunc::ToGMTString             DontEnum|Function       0
  getMonth              DateProtoFunc::GetMonth                 DontEnum|Function       0
  getUTCMonth           -DateProtoFunc::GetMonth                DontEnum|Function       0
  getDate               DateProtoFunc::GetDate                  DontEnum|Function       0
  getUTCDate            -DateProtoFunc::GetDate                 DontEnum|Function       0
  getDay                DateProtoFunc::GetDay                   DontEnum|Function       0
  getUTCDay             -DateProtoFunc::GetDay                  DontEnum|Function       0
  getHours              DateProtoFunc::GetHours                 DontEnum|Function       0
  getUTCHours           -DateProtoFunc::GetHours                DontEnum|Function       0
  getMinutes            DateProtoFunc::GetMinutes               DontEnum|Function       0
  getUTCMinutes         -DateProtoFunc::GetMinutes              DontEnum|Function       0
  getSeconds            DateProtoFunc::GetSeconds               DontEnum|Function       0
  getUTCSeconds         -DateProtoFunc::GetSeconds              DontEnum|Function       0
  getMilliseconds       DateProtoFunc::GetMilliSeconds          DontEnum|Function       0
  getUTCMilliseconds    -DateProtoFunc::GetMilliSeconds         DontEnum|Function       0
  getTimezoneOffset     DateProtoFunc::GetTimezoneOffset        DontEnum|Function       0
  setTime               DateProtoFunc::SetTime                  DontEnum|Function       1
  setMilliseconds       DateProtoFunc::SetMilliSeconds          DontEnum|Function       1
  setUTCMilliseconds    -DateProtoFunc::SetMilliSeconds         DontEnum|Function       1
  setSeconds            DateProtoFunc::SetSeconds               DontEnum|Function       2
  setUTCSeconds         -DateProtoFunc::SetSeconds              DontEnum|Function       2
  setMinutes            DateProtoFunc::SetMinutes               DontEnum|Function       3
  setUTCMinutes         -DateProtoFunc::SetMinutes              DontEnum|Function       3
  setHours              DateProtoFunc::SetHours                 DontEnum|Function       4
  setUTCHours           -DateProtoFunc::SetHours                DontEnum|Function       4
  setDate               DateProtoFunc::SetDate                  DontEnum|Function       1
  setUTCDate            -DateProtoFunc::SetDate                 DontEnum|Function       1
  setMonth              DateProtoFunc::SetMonth                 DontEnum|Function       2
  setUTCMonth           -DateProtoFunc::SetMonth                DontEnum|Function       2
  setFullYear           DateProtoFunc::SetFullYear              DontEnum|Function       3
  setUTCFullYear        -DateProtoFunc::SetFullYear             DontEnum|Function       3
  setYear               DateProtoFunc::SetYear                  DontEnum|Function       1
  getYear               DateProtoFunc::GetYear                  DontEnum|Function       0
@end
*/
// ECMA 15.9.4

DatePrototype::DatePrototype(ExecState *, ObjectPrototype *objectProto)
  : DateInstance(objectProto)
{
    setInternalValue(jsNaN());
    // The constructor will be added later, after DateObjectImp has been built.
}

bool DatePrototype::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<DateProtoFunc, JSObject>(exec, &dateTable, this, propertyName, slot);
}

// ------------------------------ DateProtoFunc -----------------------------

DateProtoFunc::DateProtoFunc(ExecState* exec, int i, int len, const Identifier& name)
  : InternalFunctionImp(static_cast<FunctionPrototype*>(exec->lexicalInterpreter()->builtinFunctionPrototype()), name)
  , id(abs(i))
  , utc(i < 0)
  // We use a negative ID to denote the "UTC" variant.
{
    putDirect(exec->propertyNames().length, len, DontDelete|ReadOnly|DontEnum);
}

JSValue *DateProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&DateInstance::info))
    return throwError(exec, TypeError);

  DateInstance* thisDateObj = static_cast<DateInstance*>(thisObj); 

  JSValue *result = 0;
  UString s;
  JSValue *v = thisDateObj->internalValue();
  double milli = v->toNumber(exec);
  if (isNaN(milli)) {
    switch (id) {
      case ToString:
      case ToDateString:
      case ToTimeString:
      case ToGMTString:
      case ToUTCString:
      case ToLocaleString:
      case ToLocaleDateString:
      case ToLocaleTimeString:
        return jsString("Invalid Date");
      case ValueOf:
      case GetTime:
      case GetYear:
      case GetFullYear:
      case GetMonth:
      case GetDate:
      case GetDay:
      case GetHours:
      case GetMinutes:
      case GetSeconds:
      case GetMilliSeconds:
      case GetTimezoneOffset:
        return jsNaN();
    }
  }
  
  double secs = floor(milli / msPerSecond);
  double ms = milli - secs * msPerSecond;

  GregorianDateTime t;
  msToGregorianDateTime(milli, utc, t);

  switch (id) {
  case ToString:
    return jsString(formatDate(t) + " " + formatTime(t, utc));
  case ToDateString:
    return jsString(formatDate(t));
    break;
  case ToTimeString:
    return jsString(formatTime(t, utc));
    break;
  case ToGMTString:
  case ToUTCString:
    return jsString(formatDateUTCVariant(t) + " " + formatTime(t, utc));
    break;
#if PLATFORM(MAC)
  case ToLocaleString:
    return jsString(formatLocaleDate(exec, secs, true, true, args));
    break;
  case ToLocaleDateString:
    return jsString(formatLocaleDate(exec, secs, true, false, args));
    break;
  case ToLocaleTimeString:
    return jsString(formatLocaleDate(exec, secs, false, true, args));
    break;
#else
  case ToLocaleString:
    return formatLocaleDate(t, LocaleDateAndTime);
    break;
  case ToLocaleDateString:
    return formatLocaleDate(t, LocaleDate);
    break;
  case ToLocaleTimeString:
    return formatLocaleDate(t, LocaleTime);
    break;
#endif
  case ValueOf:
  case GetTime:
    return jsNumber(milli);
  case GetYear:
    // IE returns the full year even in getYear.
    if (exec->dynamicInterpreter()->compatMode() == Interpreter::IECompat)
      return jsNumber(1900 + t.year);
    return jsNumber(t.year);
  case GetFullYear:
    return jsNumber(1900 + t.year);
  case GetMonth:
    return jsNumber(t.month);
  case GetDate:
    return jsNumber(t.monthDay);
  case GetDay:
    return jsNumber(t.weekDay);
  case GetHours:
    return jsNumber(t.hour);
  case GetMinutes:
    return jsNumber(t.minute);
  case GetSeconds:
    return jsNumber(t.second);
  case GetMilliSeconds:
    return jsNumber(ms);
  case GetTimezoneOffset:
    return jsNumber(-gmtoffset(t) / minutesPerHour);
  case SetTime:
    milli = roundValue(exec, args[0]);
    result = jsNumber(milli);
    thisDateObj->setInternalValue(result);
    break;
  case SetMilliSeconds:
    fillStructuresUsingTimeArgs(exec, args, 1, &ms, &t);
    break;
  case SetSeconds:
    fillStructuresUsingTimeArgs(exec, args, 2, &ms, &t);
    break;
  case SetMinutes:
    fillStructuresUsingTimeArgs(exec, args, 3, &ms, &t);
    break;
  case SetHours:
    fillStructuresUsingTimeArgs(exec, args, 4, &ms, &t);
    break;
  case SetDate:
    fillStructuresUsingDateArgs(exec, args, 1, &ms, &t);
    break;
  case SetMonth:
    fillStructuresUsingDateArgs(exec, args, 2, &ms, &t);    
    break;
  case SetFullYear:
    fillStructuresUsingDateArgs(exec, args, 3, &ms, &t);
    break;
  case SetYear:
    t.year = (args[0]->toInt32(exec) > 99 || args[0]->toInt32(exec) < 0) ? args[0]->toInt32(exec) - 1900 : args[0]->toInt32(exec);
    break;
  }

  if (id == SetYear || id == SetMilliSeconds || id == SetSeconds ||
      id == SetMinutes || id == SetHours || id == SetDate ||
      id == SetMonth || id == SetFullYear ) {
    result = jsNumber(gregorianDateTimeToMS(t, ms, utc));
    thisDateObj->setInternalValue(result);
  }
  
  return result;
}

// ------------------------------ DateObjectImp --------------------------------

// TODO: MakeTime (15.9.11.1) etc. ?

DateObjectImp::DateObjectImp(ExecState *exec,
                             FunctionPrototype *funcProto,
                             DatePrototype *dateProto)
  : InternalFunctionImp(funcProto)
{
  static const Identifier* parsePropertyName = new Identifier("parse");
  static const Identifier* UTCPropertyName = new Identifier("UTC");

  putDirect(exec->propertyNames().prototype, dateProto, DontEnum|DontDelete|ReadOnly);
  putDirectFunction(new DateObjectFuncImp(exec, funcProto, DateObjectFuncImp::Parse, 1, *parsePropertyName), DontEnum);
  putDirectFunction(new DateObjectFuncImp(exec, funcProto, DateObjectFuncImp::UTC, 7, *UTCPropertyName), DontEnum);
  putDirect(exec->propertyNames().length, 7, ReadOnly|DontDelete|DontEnum);
}

bool DateObjectImp::implementsConstruct() const
{
    return true;
}

// ECMA 15.9.3
JSObject *DateObjectImp::construct(ExecState *exec, const List &args)
{
  int numArgs = args.size();

  double value;

  if (numArgs == 0) { // new Date() ECMA 15.9.3.3
    value = getCurrentUTCTime();
  } else if (numArgs == 1) {
    if (args[0]->isObject(&DateInstance::info))
      value = static_cast<DateInstance*>(args[0])->internalValue()->toNumber(exec);
    else {
      JSValue* primitive = args[0]->toPrimitive(exec);
      if (primitive->isString())
        value = parseDate(primitive->getString());
      else
        value = primitive->toNumber(exec);
    }
  } else {
    if (isNaN(args[0]->toNumber(exec))
        || isNaN(args[1]->toNumber(exec))
        || (numArgs >= 3 && isNaN(args[2]->toNumber(exec)))
        || (numArgs >= 4 && isNaN(args[3]->toNumber(exec)))
        || (numArgs >= 5 && isNaN(args[4]->toNumber(exec)))
        || (numArgs >= 6 && isNaN(args[5]->toNumber(exec)))
        || (numArgs >= 7 && isNaN(args[6]->toNumber(exec)))) {
      value = NaN;
    } else {
      GregorianDateTime t;
      int year = args[0]->toInt32(exec);
      t.year = (year >= 0 && year <= 99) ? year : year - 1900;
      t.month = args[1]->toInt32(exec);
      t.monthDay = (numArgs >= 3) ? args[2]->toInt32(exec) : 1;
      t.hour = (numArgs >= 4) ? args[3]->toInt32(exec) : 0;
      t.minute = (numArgs >= 5) ? args[4]->toInt32(exec) : 0;
      t.second = (numArgs >= 6) ? args[5]->toInt32(exec) : 0;
      t.isDST = -1;
      double ms = (numArgs >= 7) ? roundValue(exec, args[6]) : 0;
      value = gregorianDateTimeToMS(t, ms, false);
    }
  }
  
  DateInstance *ret = new DateInstance(exec->lexicalInterpreter()->builtinDatePrototype());
  ret->setInternalValue(jsNumber(timeClip(value)));
  return ret;
}

// ECMA 15.9.2
JSValue *DateObjectImp::callAsFunction(ExecState * /*exec*/, JSObject * /*thisObj*/, const List &/*args*/)
{
    time_t t = time(0);
    GregorianDateTime ts(*localtime(&t));
    return jsString(formatDate(ts) + " " + formatTime(ts, false));
}

// ------------------------------ DateObjectFuncImp ----------------------------

DateObjectFuncImp::DateObjectFuncImp(ExecState* exec, FunctionPrototype* funcProto, int i, int len, const Identifier& name)
    : InternalFunctionImp(funcProto, name), id(i)
{
    putDirect(exec->propertyNames().length, len, DontDelete|ReadOnly|DontEnum);
}

// ECMA 15.9.4.2 - 3
JSValue *DateObjectFuncImp::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
  if (id == Parse) {
    return jsNumber(parseDate(args[0]->toString(exec)));
  }
  else { // UTC
    int n = args.size();
    if (isNaN(args[0]->toNumber(exec))
        || isNaN(args[1]->toNumber(exec))
        || (n >= 3 && isNaN(args[2]->toNumber(exec)))
        || (n >= 4 && isNaN(args[3]->toNumber(exec)))
        || (n >= 5 && isNaN(args[4]->toNumber(exec)))
        || (n >= 6 && isNaN(args[5]->toNumber(exec)))
        || (n >= 7 && isNaN(args[6]->toNumber(exec)))) {
      return jsNaN();
    }

    GregorianDateTime t;
    memset(&t, 0, sizeof(t));
    int year = args[0]->toInt32(exec);
    t.year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.month = args[1]->toInt32(exec);
    t.monthDay = (n >= 3) ? args[2]->toInt32(exec) : 1;
    t.hour = (n >= 4) ? args[3]->toInt32(exec) : 0;
    t.minute = (n >= 5) ? args[4]->toInt32(exec) : 0;
    t.second = (n >= 6) ? args[5]->toInt32(exec) : 0;
    double ms = (n >= 7) ? roundValue(exec, args[6]) : 0;
    return jsNumber(gregorianDateTimeToMS(t, ms, true));
  }
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

inline static void skipSpacesAndComments(const char *&s)
{
    int nesting = 0;
    char ch;
    while ((ch = *s)) {
        if (!isspace(ch)) {
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
static int findMonth(const char *monthStr)
{
    assert(monthStr);
    char needle[4];
    for (int i = 0; i < 3; ++i) {
        if (!*monthStr)
            return -1;
        needle[i] = static_cast<char>(tolower(*monthStr++));
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
    while (*dateString && !isdigit(*dateString)) {
        if (isspace(*dateString) || *dateString == '(') {
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

            while (*dateString && (*dateString != '-') && !isspace(*dateString))
                dateString++;

            if (!*dateString)
                return NaN;

            // '-99 23:12:40 GMT'
            if (*dateString != '-' && *dateString != '/' && !isspace(*dateString))
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
        if (!isspace(*newPosStr)) {
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
            if (*dateString && *dateString != ':' && !isspace(*dateString))
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
        memset(&t, 0, sizeof(tm));
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
    double at = fabs(t);
    if (at > 8.64E15)
        return NaN;
    return copysign(floor(at), t);
}

}
