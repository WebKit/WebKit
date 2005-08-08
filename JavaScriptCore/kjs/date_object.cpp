// -*- c-basic-offset: 2 -*-
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef HAVE_SYS_TIMEB_H
#define HAVE_SYS_TIMEB_H 0
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#  include <time.h>
# endif
#endif
#if HAVE_SYS_TIMEB_H
#include <sys/timeb.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif // HAVE_SYS_PARAM_H

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <ctype.h>

#include "date_object.h"
#include "error_object.h"
#include "operations.h"

#include "date_object.lut.h"

// some constants
const time_t invalidDate = LONG_MIN;
const double hoursPerDay = 24;
const double minutesPerHour = 60;
const double secondsPerMinute = 60;
const double msPerSecond = 1000;
const double msPerMinute = msPerSecond * secondsPerMinute;
const double msPerHour = msPerMinute * minutesPerHour;
const double msPerDay = msPerHour * hoursPerDay;

#if APPLE_CHANGES

// Originally, we wrote our own implementation that uses Core Foundation because of a performance problem in Mac OS X 10.2.
// But we need to keep using this rather than the standard library functions because this handles a larger range of dates.

#include <notify.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

using KJS::UChar;
using KJS::UString;

#define gmtime(x) gmtimeUsingCF(x)
#define localtime(x) localtimeUsingCF(x)
#define mktime(x) mktimeUsingCF(x)
#define time(x) timeUsingCF(x)

#define ctime(x) NotAllowedToCallThis()
#define strftime(a, b, c, d) NotAllowedToCallThis()

static const char * const weekdayName[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static const char * const monthName[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    
static struct tm *tmUsingCF(time_t clock, CFTimeZoneRef timeZone)
{
    static struct tm result;
    static char timeZoneCString[128];
    
    CFAbsoluteTime absoluteTime = clock - kCFAbsoluteTimeIntervalSince1970;
    CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(absoluteTime, timeZone);

    CFStringRef abbreviation = CFTimeZoneCopyAbbreviation(timeZone, absoluteTime);
    CFStringGetCString(abbreviation, timeZoneCString, sizeof(timeZoneCString), kCFStringEncodingASCII);
    CFRelease(abbreviation);

    result.tm_sec = (int)date.second;
    result.tm_min = date.minute;
    result.tm_hour = date.hour;
    result.tm_mday = date.day;
    result.tm_mon = date.month - 1;
    result.tm_year = date.year - 1900;
    result.tm_wday = CFAbsoluteTimeGetDayOfWeek(absoluteTime, timeZone) % 7;
    result.tm_yday = CFAbsoluteTimeGetDayOfYear(absoluteTime, timeZone) - 1;
    result.tm_isdst = CFTimeZoneIsDaylightSavingTime(timeZone, absoluteTime);
    result.tm_gmtoff = (int)CFTimeZoneGetSecondsFromGMT(timeZone, absoluteTime);
    result.tm_zone = timeZoneCString;
    
    return &result;
}

static CFTimeZoneRef UTCTimeZone()
{
    static CFTimeZoneRef zone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0.0);
    return zone;
}

static CFTimeZoneRef CopyLocalTimeZone()
{
    // Check for a time zone notification, and tell CoreFoundation to re-get the time zone if it happened.
    // Some day, CoreFoundation may do this itself, but for now it needs our help.
    static bool registered = false;
    static int notificationToken;
    if (!registered) {
        uint32_t status = notify_register_check("com.apple.system.timezone", &notificationToken);
        if (status == NOTIFY_STATUS_OK) {
            registered = true;
        }
    }
    if (registered) {
        int notified;
        uint32_t status = notify_check(notificationToken, &notified);
        if (status == NOTIFY_STATUS_OK && notified) {
            CFTimeZoneResetSystem();
        }
    }

    CFTimeZoneRef zone = CFTimeZoneCopyDefault();
    if (zone) {
        return zone;
    }
    zone = UTCTimeZone();
    CFRetain(zone);
    return zone;
}

static struct tm *gmtimeUsingCF(const time_t *clock)
{
    return tmUsingCF(*clock, UTCTimeZone());
}

static struct tm *localtimeUsingCF(const time_t *clock)
{
    CFTimeZoneRef timeZone = CopyLocalTimeZone();
    struct tm *result = tmUsingCF(*clock, timeZone);
    CFRelease(timeZone);
    return result;
}

static time_t timetUsingCF(struct tm *tm, CFTimeZoneRef timeZone)
{
    CFGregorianDate date;
    date.second = tm->tm_sec;
    date.minute = tm->tm_min;
    date.hour = tm->tm_hour;
    date.day = tm->tm_mday;
    date.month = tm->tm_mon + 1;
    date.year = tm->tm_year + 1900;

    // CFGregorianDateGetAbsoluteTime will go nuts if the year is too large or small,
    // so we pick an arbitrary cutoff.
    if (date.year < -2500 || date.year > 2500) {
        return invalidDate;
    }

    CFAbsoluteTime absoluteTime = CFGregorianDateGetAbsoluteTime(date, timeZone);
    CFTimeInterval interval = absoluteTime + kCFAbsoluteTimeIntervalSince1970;
    if (interval > LONG_MAX) {
        interval = LONG_MAX;
    }

    return (time_t) interval;
}

static time_t mktimeUsingCF(struct tm *tm)
{
    CFTimeZoneRef timeZone = CopyLocalTimeZone();
    time_t result = timetUsingCF(tm, timeZone);
    CFRelease(timeZone);
    return result;
}

static time_t timeUsingCF(time_t *clock)
{
    time_t result = (time_t)(CFAbsoluteTimeGetCurrent() + kCFAbsoluteTimeIntervalSince1970);
    if (clock) {
        *clock = result;
    }
    return result;
}

static UString formatDate(struct tm &tm)
{
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s %s %02d %04d",
        weekdayName[(tm.tm_wday + 6) % 7],
        monthName[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900);
    return buffer;
}

static UString formatDateUTCVariant(struct tm &tm)
{
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s, %02d %s %04d",
        weekdayName[(tm.tm_wday + 6) % 7],
        tm.tm_mday, monthName[tm.tm_mon], tm.tm_year + 1900);
    return buffer;
}

static UString formatTime(struct tm &tm)
{
    char buffer[100];
    if (tm.tm_gmtoff == 0) {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT", tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else {
        int offset = tm.tm_gmtoff;
        if (offset < 0) {
            offset = -offset;
        }
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d GMT%c%02d%02d",
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            tm.tm_gmtoff < 0 ? '-' : '+', offset / (60*60), (offset / 60) % 60);
    }
    return UString(buffer);
}

static CFDateFormatterStyle styleFromArgString(const UString& string,CFDateFormatterStyle defaultStyle)
{
    CFDateFormatterStyle retVal = defaultStyle;
    if (string == "short")
	retVal = kCFDateFormatterShortStyle;
    else if (string == "medium")
	retVal = kCFDateFormatterMediumStyle;
    else if (string == "long")
	retVal = kCFDateFormatterLongStyle;
    else if (string == "full")
	retVal = kCFDateFormatterFullStyle;
    return retVal;
}

static UString formatLocaleDate(KJS::ExecState *exec, double time, bool includeDate, bool includeTime, const KJS::List &args)
{
    CFLocaleRef locale = CFLocaleCopyCurrent();
    int argCount = args.size();
    
    CFDateFormatterStyle    dateStyle = (includeDate ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);
    CFDateFormatterStyle    timeStyle = (includeTime ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);

    UString	arg0String;
    UString	arg1String;
    bool	useCustomFormat = false;
    UString	customFormatString;
    arg0String = args[0]->toString(exec);
    if ((arg0String == "custom") && (argCount >= 2)) {
	useCustomFormat = true;
	customFormatString = args[1]->toString(exec);
    } else if (includeDate && includeTime && (argCount >= 2)) {
	arg1String = args[1]->toString(exec);
	dateStyle = styleFromArgString(arg0String,dateStyle);
	timeStyle = styleFromArgString(arg1String,timeStyle);
    } else if (includeDate && (argCount >= 1)) {
	dateStyle = styleFromArgString(arg0String,dateStyle);
    } else if (includeTime && (argCount >= 1)) {
	timeStyle = styleFromArgString(arg0String,timeStyle);
    }
    CFDateFormatterRef formatter = CFDateFormatterCreate(NULL, locale, dateStyle, timeStyle);
    if (useCustomFormat) {
	CFStringRef	customFormatCFString = CFStringCreateWithCharacters(NULL,(UniChar*)customFormatString.data(),customFormatString.size());
	CFDateFormatterSetFormat(formatter,customFormatCFString);
	CFRelease(customFormatCFString);
    }
    CFStringRef string = CFDateFormatterCreateStringWithAbsoluteTime(NULL, formatter, time - kCFAbsoluteTimeIntervalSince1970);
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
    CFRelease(formatter);
    CFRelease(locale);
    
    return UString(buffer, length);
}

#endif // APPLE_CHANGES

namespace KJS {

static int day(double t)
{
  return int(floor(t / msPerDay));
}

static double dayFromYear(int year)
{
  return 365.0 * (year - 1970)
    + floor((year - 1969) / 4.0)
    - floor((year - 1901) / 100.0)
    + floor((year - 1601) / 400.0);
}

// depending on whether it's a leap year or not
static int daysInYear(int year)
{
  if (year % 4 != 0)
    return 365;
  else if (year % 400 == 0)
    return 366;
  else if (year % 100 == 0)
    return 365;
  else
    return 366;
}

// time value of the start of a year
static double timeFromYear(int year)
{
  return msPerDay * dayFromYear(year);
}

// year determined by time value
static int yearFromTime(double t)
{
  // ### there must be an easier way
  // initial guess
  int y = 1970 + int(t / (365.25 * msPerDay));
  // adjustment
  if (timeFromYear(y) > t) {
    do {
      --y;
    } while (timeFromYear(y) > t);
  } else {
    while (timeFromYear(y + 1) < t)
      ++y;
  }

  return y;
}

// 0: Sunday, 1: Monday, etc.
static int weekDay(double t)
{
  int wd = (day(t) + 4) % 7;
  if (wd < 0)
    wd += 7;
  return wd;
}

static double timeZoneOffset(const struct tm *t)
{
#if defined BSD || defined(__linux__) || defined(__APPLE__)
  return -(t->tm_gmtoff / 60);
#else
#  if defined(__BORLANDC__) || defined(__CYGWIN__)
// FIXME consider non one-hour DST change
#if !defined(__CYGWIN__)
#error please add daylight savings offset here!
#endif
  return _timezone / 60 - (t->tm_isdst > 0 ? 60 : 0);
#  else
  return timezone / 60 - (t->tm_isdst > 0 ? 60 : 0 );
#  endif
#endif
}

static double timeFromArgs(ExecState *exec, const List &args, int maxArgs, double ms, struct tm *t)
{
    double result = 0;
    int idx = 0;
    int numArgs = args.size();
    
    // process up to max_args arguments
    if (numArgs > maxArgs)
        numArgs = maxArgs;
    // hours
    if (maxArgs >= 4 && idx < numArgs) {
        t->tm_hour = 0;
        result = args[idx++]->toInt32(exec) * msPerHour;
    }
    // minutes
    if (maxArgs >= 3 && idx < numArgs) {
        t->tm_min = 0;
        result += args[idx++]->toInt32(exec) * msPerMinute;
    }
    // seconds
    if (maxArgs >= 2 && idx < numArgs) {
        t->tm_sec = 0;
        result += args[idx++]->toInt32(exec) * msPerSecond;
    }
    // read ms from args if present or add the old value
    result += idx < numArgs ? roundValue(exec, args[idx]) : ms;
            
    return result;
}

// ------------------------------ DateInstanceImp ------------------------------

const ClassInfo DateInstanceImp::info = {"Date", 0, 0, 0};

DateInstanceImp::DateInstanceImp(ObjectImp *proto)
  : ObjectImp(proto)
{
}

// ------------------------------ DatePrototypeImp -----------------------------

const ClassInfo DatePrototypeImp::info = {"Date", &DateInstanceImp::info, &dateTable, 0};

/* Source for date_object.lut.h
   We use a negative ID to denote the "UTC" variant.
@begin dateTable 61
  toString		DateProtoFuncImp::ToString		DontEnum|Function	0
  toUTCString		-DateProtoFuncImp::ToUTCString		DontEnum|Function	0
  toDateString		DateProtoFuncImp::ToDateString		DontEnum|Function	0
  toTimeString		DateProtoFuncImp::ToTimeString		DontEnum|Function	0
  toLocaleString	DateProtoFuncImp::ToLocaleString	DontEnum|Function	0
  toLocaleDateString	DateProtoFuncImp::ToLocaleDateString	DontEnum|Function	0
  toLocaleTimeString	DateProtoFuncImp::ToLocaleTimeString	DontEnum|Function	0
  valueOf		DateProtoFuncImp::ValueOf		DontEnum|Function	0
  getTime		DateProtoFuncImp::GetTime		DontEnum|Function	0
  getFullYear		DateProtoFuncImp::GetFullYear		DontEnum|Function	0
  getUTCFullYear	-DateProtoFuncImp::GetFullYear		DontEnum|Function	0
  toGMTString		-DateProtoFuncImp::ToGMTString		DontEnum|Function	0
  getMonth		DateProtoFuncImp::GetMonth		DontEnum|Function	0
  getUTCMonth		-DateProtoFuncImp::GetMonth		DontEnum|Function	0
  getDate		DateProtoFuncImp::GetDate		DontEnum|Function	0
  getUTCDate		-DateProtoFuncImp::GetDate		DontEnum|Function	0
  getDay		DateProtoFuncImp::GetDay		DontEnum|Function	0
  getUTCDay		-DateProtoFuncImp::GetDay		DontEnum|Function	0
  getHours		DateProtoFuncImp::GetHours		DontEnum|Function	0
  getUTCHours		-DateProtoFuncImp::GetHours		DontEnum|Function	0
  getMinutes		DateProtoFuncImp::GetMinutes		DontEnum|Function	0
  getUTCMinutes		-DateProtoFuncImp::GetMinutes		DontEnum|Function	0
  getSeconds		DateProtoFuncImp::GetSeconds		DontEnum|Function	0
  getUTCSeconds		-DateProtoFuncImp::GetSeconds		DontEnum|Function	0
  getMilliseconds	DateProtoFuncImp::GetMilliSeconds	DontEnum|Function	0
  getUTCMilliseconds	-DateProtoFuncImp::GetMilliSeconds	DontEnum|Function	0
  getTimezoneOffset	DateProtoFuncImp::GetTimezoneOffset	DontEnum|Function	0
  setTime		DateProtoFuncImp::SetTime		DontEnum|Function	1
  setMilliseconds	DateProtoFuncImp::SetMilliSeconds	DontEnum|Function	1
  setUTCMilliseconds	-DateProtoFuncImp::SetMilliSeconds	DontEnum|Function	1
  setSeconds		DateProtoFuncImp::SetSeconds		DontEnum|Function	2
  setUTCSeconds		-DateProtoFuncImp::SetSeconds		DontEnum|Function	2
  setMinutes		DateProtoFuncImp::SetMinutes		DontEnum|Function	3
  setUTCMinutes		-DateProtoFuncImp::SetMinutes		DontEnum|Function	3
  setHours		DateProtoFuncImp::SetHours		DontEnum|Function	4
  setUTCHours		-DateProtoFuncImp::SetHours		DontEnum|Function	4
  setDate		DateProtoFuncImp::SetDate		DontEnum|Function	1
  setUTCDate		-DateProtoFuncImp::SetDate		DontEnum|Function	1
  setMonth		DateProtoFuncImp::SetMonth		DontEnum|Function	2
  setUTCMonth		-DateProtoFuncImp::SetMonth		DontEnum|Function	2
  setFullYear		DateProtoFuncImp::SetFullYear		DontEnum|Function	3
  setUTCFullYear	-DateProtoFuncImp::SetFullYear		DontEnum|Function	3
  setYear		DateProtoFuncImp::SetYear		DontEnum|Function	1
  getYear		DateProtoFuncImp::GetYear		DontEnum|Function	0
@end
*/
// ECMA 15.9.4

DatePrototypeImp::DatePrototypeImp(ExecState *,
                                   ObjectPrototypeImp *objectProto)
  : DateInstanceImp(objectProto)
{
  setInternalValue(jsNaN());
  // The constructor will be added later, after DateObjectImp has been built
}

bool DatePrototypeImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticFunctionSlot<DateProtoFuncImp, ObjectImp>(exec, &dateTable, this, propertyName, slot);
}

// ------------------------------ DateProtoFuncImp -----------------------------

DateProtoFuncImp::DateProtoFuncImp(ExecState *exec, int i, int len)
  : InternalFunctionImp(
    static_cast<FunctionPrototypeImp*>(exec->lexicalInterpreter()->builtinFunctionPrototype())
    ), id(abs(i)), utc(i<0)
  // We use a negative ID to denote the "UTC" variant.
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

bool DateProtoFuncImp::implementsCall() const
{
  return true;
}

ValueImp *DateProtoFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if ((id == ToString || id == ValueOf || id == GetTime || id == SetTime) &&
      !thisObj->inherits(&DateInstanceImp::info)) {
    // non-generic function called on non-date object

    // ToString and ValueOf are generic according to the spec, but the mozilla
    // tests suggest otherwise...
    ObjectImp *err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }


  ValueImp *result = NULL;
  UString s;
#if !APPLE_CHANGES
  const int bufsize=100;
  char timebuffer[bufsize];
  CString oldlocale = setlocale(LC_TIME,NULL);
  if (!oldlocale.c_str())
    oldlocale = setlocale(LC_ALL, NULL);
#endif
  ValueImp *v = thisObj->internalValue();
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
        return String("Invalid Date");
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
  
  // check whether time value is outside time_t's usual range
  // make the necessary transformations if necessary
  int realYearOffset = 0;
  double milliOffset = 0.0;
  double secs = floor(milli / 1000.0);

  if (milli < 0 || milli >= timeFromYear(2038)) {
    // ### ugly and probably not very precise
    int realYear = yearFromTime(milli);
    int base = daysInYear(realYear) == 365 ? 2001 : 2000;
    milliOffset = timeFromYear(base) - timeFromYear(realYear);
    milli += milliOffset;
    realYearOffset = realYear - base;
  }

  time_t tv = (time_t) floor(milli / 1000.0);
  double ms = milli - tv * 1000.0;

  struct tm *t = utc ? gmtime(&tv) : localtime(&tv);
  // we had an out of range year. use that one (plus/minus offset
  // found by calculating tm_year) and fix the week day calculation
  if (realYearOffset != 0) {
    t->tm_year += realYearOffset;
    milli -= milliOffset;
    // our own weekday calculation. beware of need for local time.
    double m = milli;
    if (!utc)
      m -= timeZoneOffset(t) * msPerMinute;
    t->tm_wday = weekDay(m);
  }
  
  switch (id) {
#if APPLE_CHANGES
  case ToString:
    result = String(formatDate(*t) + " " + formatTime(*t));
    break;
  case ToDateString:
    result = String(formatDate(*t));
    break;
  case ToTimeString:
    result = String(formatTime(*t));
    break;
  case ToGMTString:
  case ToUTCString:
    result = String(formatDateUTCVariant(*t) + " " + formatTime(*t));
    break;
  case ToLocaleString:
    result = String(formatLocaleDate(exec, secs, true, true, args));
    break;
  case ToLocaleDateString:
    result = String(formatLocaleDate(exec, secs, true, false, args));
    break;
  case ToLocaleTimeString:
    result = String(formatLocaleDate(exec, secs, false, true, args));
    break;
#else
  case ToString:
    s = ctime(&tv);
    result = String(s.substr(0, s.size() - 1));
    break;
  case ToDateString:
  case ToTimeString:
  case ToGMTString:
  case ToUTCString:
    setlocale(LC_TIME,"C");
    if (id == DateProtoFuncImp::ToDateString) {
      strftime(timebuffer, bufsize, "%x",t);
    } else if (id == DateProtoFuncImp::ToTimeString) {
      strftime(timebuffer, bufsize, "%X",t);
    } else { // toGMTString & toUTCString
      strftime(timebuffer, bufsize, "%a, %d %b %Y %H:%M:%S %Z", t);
    }
    setlocale(LC_TIME,oldlocale.c_str());
    result = String(timebuffer);
    break;
  case ToLocaleString:
    strftime(timebuffer, bufsize, "%c", t);
    result = String(timebuffer);
    break;
  case ToLocaleDateString:
    strftime(timebuffer, bufsize, "%x", t);
    result = String(timebuffer);
    break;
  case ToLocaleTimeString:
    strftime(timebuffer, bufsize, "%X", t);
    result = String(timebuffer);
    break;
#endif
  case ValueOf:
    result = Number(milli);
    break;
  case GetTime:
    result = Number(milli);
    break;
  case GetYear:
    // IE returns the full year even in getYear.
    if ( exec->dynamicInterpreter()->compatMode() == Interpreter::IECompat )
      result = Number(1900 + t->tm_year);
    else
      result = Number(t->tm_year);
    break;
  case GetFullYear:
    result = Number(1900 + t->tm_year);
    break;
  case GetMonth:
    result = Number(t->tm_mon);
    break;
  case GetDate:
    result = Number(t->tm_mday);
    break;
  case GetDay:
    result = Number(t->tm_wday);
    break;
  case GetHours:
    result = Number(t->tm_hour);
    break;
  case GetMinutes:
    result = Number(t->tm_min);
    break;
  case GetSeconds:
    result = Number(t->tm_sec);
    break;
  case GetMilliSeconds:
    result = Number(ms);
    break;
  case GetTimezoneOffset:
#if defined BSD || defined(__APPLE__)
    result = Number(-t->tm_gmtoff / 60);
#else
#  if defined(__BORLANDC__)
#error please add daylight savings offset here!
    // FIXME: Using the daylight value was wrong for BSD, maybe wrong here too.
    result = Number(_timezone / 60 - (_daylight ? 60 : 0));
#  else
    // FIXME: Using the daylight value was wrong for BSD, maybe wrong here too.
    result = Number(( timezone / 60 - ( daylight ? 60 : 0 )));
#  endif
#endif
    break;
  case SetTime:
    milli = roundValue(exec, args[0]);
    result = Number(milli);
    thisObj->setInternalValue(result);
    break;
  case SetMilliSeconds:
    ms = roundValue(exec, args[0]);
    break;
  case SetSeconds:
    ms = timeFromArgs(exec, args, 2, ms, t);
    break;
  case SetMinutes:
    ms = timeFromArgs(exec, args, 3, ms, t);
    break;
  case SetHours:
    ms = timeFromArgs(exec, args, 4, ms, t);
    break;
  case SetDate:
      t->tm_mday = 0;
      ms += args[0]->toInt32(exec) * msPerDay;
      break;
  case SetMonth:
    t->tm_mon = args[0]->toInt32(exec);
    if (args.size() >= 2)
      t->tm_mday = args[1]->toInt32(exec);
    break;
  case SetFullYear:
    t->tm_year = args[0]->toInt32(exec) - 1900;
    if (args.size() >= 2)
      t->tm_mon = args[1]->toInt32(exec);
    if (args.size() >= 3)
      t->tm_mday = args[2]->toInt32(exec);
    break;
  case SetYear:
    t->tm_year = args[0]->toInt32(exec) >= 1900 ? args[0]->toInt32(exec) - 1900 : args[0]->toInt32(exec);
    break;
  }

  if (id == SetYear || id == SetMilliSeconds || id == SetSeconds ||
      id == SetMinutes || id == SetHours || id == SetDate ||
      id == SetMonth || id == SetFullYear ) {
    result = Number(makeTime(t, ms, utc));
    thisObj->setInternalValue(result);
  }
  
  return result;
}

// ------------------------------ DateObjectImp --------------------------------

// TODO: MakeTime (15.9.11.1) etc. ?

DateObjectImp::DateObjectImp(ExecState *exec,
                             FunctionPrototypeImp *funcProto,
                             DatePrototypeImp *dateProto)
  : InternalFunctionImp(funcProto)
{
  // ECMA 15.9.4.1 Date.prototype
  putDirect(prototypePropertyName, dateProto, DontEnum|DontDelete|ReadOnly);

  static const Identifier parsePropertyName("parse");
  putDirect(parsePropertyName, new DateObjectFuncImp(exec,funcProto,DateObjectFuncImp::Parse, 1), DontEnum);
  static const Identifier UTCPropertyName("UTC");
  putDirect(UTCPropertyName,   new DateObjectFuncImp(exec,funcProto,DateObjectFuncImp::UTC,   7),   DontEnum);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, 7, ReadOnly|DontDelete|DontEnum);
}

bool DateObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.9.3
ObjectImp *DateObjectImp::construct(ExecState *exec, const List &args)
{
  int numArgs = args.size();

#ifdef KJS_VERBOSE
  fprintf(stderr,"DateObjectImp::construct - %d args\n", numArgs);
#endif
  double value;

  if (numArgs == 0) { // new Date() ECMA 15.9.3.3
#if HAVE_SYS_TIMEB_H
#  if defined(__BORLANDC__)
    struct timeb timebuffer;
    ftime(&timebuffer);
#  else
    struct _timeb timebuffer;
    _ftime(&timebuffer);
#  endif
    double utc = floor((double)timebuffer.time * 1000.0 + (double)timebuffer.millitm);
#else
    struct timeval tv;
    gettimeofday(&tv, 0L);
    double utc = floor((double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0);
#endif
    value = utc;
  } else if (numArgs == 1) {
      if (args[0]->isString())
          value = parseDate(args[0]->toString(exec));
      else
          value = args[0]->toPrimitive(exec)->toNumber(exec);
  } else {
    struct tm t;
    memset(&t, 0, sizeof(t));
    if (isNaN(args[0]->toNumber(exec))
        || isNaN(args[1]->toNumber(exec))
        || (numArgs >= 3 && isNaN(args[2]->toNumber(exec)))
        || (numArgs >= 4 && isNaN(args[3]->toNumber(exec)))
        || (numArgs >= 5 && isNaN(args[4]->toNumber(exec)))
        || (numArgs >= 6 && isNaN(args[5]->toNumber(exec)))
        || (numArgs >= 7 && isNaN(args[6]->toNumber(exec)))) {
      value = NaN;
    } else {
      int year = args[0]->toInt32(exec);
      t.tm_year = (year >= 0 && year <= 99) ? year : year - 1900;
      t.tm_mon = args[1]->toInt32(exec);
      t.tm_mday = (numArgs >= 3) ? args[2]->toInt32(exec) : 1;
      t.tm_hour = (numArgs >= 4) ? args[3]->toInt32(exec) : 0;
      t.tm_min = (numArgs >= 5) ? args[4]->toInt32(exec) : 0;
      t.tm_sec = (numArgs >= 6) ? args[5]->toInt32(exec) : 0;
      t.tm_isdst = -1;
      double ms = (numArgs >= 7) ? roundValue(exec, args[6]) : 0;
      value = makeTime(&t, ms, false);
    }
  }
  
  DateInstanceImp *ret = new DateInstanceImp(exec->lexicalInterpreter()->builtinDatePrototype());
  ret->setInternalValue(Number(timeClip(value)));
  return ret;
}

bool DateObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.9.2
ValueImp *DateObjectImp::callAsFunction(ExecState */*exec*/, ObjectImp */*thisObj*/, const List &/*args*/)
{
  time_t t = time(0L);
#if APPLE_CHANGES
  struct tm *tm = localtime(&t);
  return String(formatDate(*tm) + " " + formatTime(*tm));
#else
  UString s(ctime(&t));

  // return formatted string minus trailing \n
  return String(s.substr(0, s.size() - 1));
#endif
}

// ------------------------------ DateObjectFuncImp ----------------------------

DateObjectFuncImp::DateObjectFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                                     int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

bool DateObjectFuncImp::implementsCall() const
{
  return true;
}

// ECMA 15.9.4.2 - 3
ValueImp *DateObjectFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (id == Parse) {
    return Number(parseDate(args[0]->toString(exec)));
  }
  else { // UTC
    struct tm t;
    memset(&t, 0, sizeof(t));
    int n = args.size();
    if (isNaN(args[0]->toNumber(exec))
        || isNaN(args[1]->toNumber(exec))
        || (n >= 3 && isNaN(args[2]->toNumber(exec)))
        || (n >= 4 && isNaN(args[3]->toNumber(exec)))
        || (n >= 5 && isNaN(args[4]->toNumber(exec)))
        || (n >= 6 && isNaN(args[5]->toNumber(exec)))
        || (n >= 7 && isNaN(args[6]->toNumber(exec)))) {
      return Number(NaN);
    }
    int year = args[0]->toInt32(exec);
    t.tm_year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.tm_mon = args[1]->toInt32(exec);
    t.tm_mday = (n >= 3) ? args[2]->toInt32(exec) : 1;
    t.tm_hour = (n >= 4) ? args[3]->toInt32(exec) : 0;
    t.tm_min = (n >= 5) ? args[4]->toInt32(exec) : 0;
    t.tm_sec = (n >= 6) ? args[5]->toInt32(exec) : 0;
    double ms = (n >= 7) ? roundValue(exec, args[6]) : 0;
    return Number(makeTime(&t, ms, true));
  }
}

// -----------------------------------------------------------------------------


double parseDate(const UString &u)
{
#ifdef KJS_VERBOSE
  fprintf(stderr,"KJS::parseDate %s\n",u.ascii());
#endif
  double /*time_t*/ seconds = KRFCDate_parseDate( u );

  return seconds == invalidDate ? NaN : seconds * 1000.0;
}

///// Awful duplication from krfcdate.cpp - we don't link to kdecore

static double ymdhms_to_seconds(int year, int mon, int day, int hour, int minute, int second)
{
    double ret = (day - 32075)       /* days */
            + 1461L * (year + 4800L + (mon - 14) / 12) / 4
            + 367 * (mon - 2 - (mon - 14) / 12 * 12) / 12
            - 3 * ((year + 4900L + (mon - 14) / 12) / 100) / 4
            - 2440588;
    ret = 24*ret + hour;     /* hours   */
    ret = 60*ret + minute;   /* minutes */
    ret = 60*ret + second;   /* seconds */

    return ret;
}

static const char haystack[37]="janfebmaraprmayjunjulaugsepoctnovdec";

// we follow the recommendation of rfc2822 to consider all
// obsolete time zones not listed here equivalent to "-0000"
static const struct KnownZone {
#ifdef _WIN32
    char tzName[4];
#else
    const char tzName[4];
#endif
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

double makeTime(struct tm *t, double ms, bool utc)
{
    int utcOffset;
    if (utc) {
	time_t zero = 0;
#if defined BSD || defined(__linux__) || defined(__APPLE__)
        struct tm t3;
        localtime_r(&zero, &t3);
        utcOffset = t3.tm_gmtoff;
        t->tm_isdst = t3.tm_isdst;
#else
        (void)localtime(&zero);
#  if defined(__BORLANDC__) || defined(__CYGWIN__)
        utcOffset = - _timezone;
#  else
        utcOffset = - timezone;
#  endif
        t->tm_isdst = 0;
#endif
    } else {
	utcOffset = 0;
	t->tm_isdst = -1;
    }

    double yearOffset = 0.0;
    if (t->tm_year < (1970 - 1900) || t->tm_year > (2038 - 1900)) {
      // we'll fool mktime() into believing that this year is within
      // its normal, portable range (1970-2038) by setting tm_year to
      // 2000 or 2001 and adding the difference in milliseconds later.
      // choice between offset will depend on whether the year is a
      // leap year or not.
      int y = t->tm_year + 1900;
      int baseYear = daysInYear(y) == 365 ? 2001 : 2000;
      const double baseTime = timeFromYear(baseYear);
      yearOffset = timeFromYear(y) - baseTime;
      t->tm_year = baseYear - 1900;
    }

    return (mktime(t) + utcOffset) * 1000.0 + ms + yearOffset;
}

// returns 0-11 (Jan-Dec); -1 on failure
static int findMonth(const char *monthStr)
{
  assert(monthStr);
  static const char haystack[37] = "janfebmaraprmayjunjulaugsepoctnovdec";
  char needle[4];
  for (int i = 0; i < 3; ++i) {
    if (!*monthStr)
      return -1;
    needle[i] = tolower(*monthStr++);
  }
  needle[3] = '\0';
  const char *str = strstr(haystack, needle);
  if (str) {
    int position = str - haystack;
    if (position % 3 == 0) {
      return position / 3;
    }
  }
  return -1;
}

double KRFCDate_parseDate(const UString &_date)
{
     // This parse a date in the form:
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
     // We ignore the weekday
     //
     double result = -1;
     int offset = 0;
     bool have_tz = false;
     char *newPosStr;
     const char *dateString = _date.ascii();
     int day = 0;
     int month = -1; // not set yet
     int year = 0;
     int hour = 0;
     int minute = 0;
     int second = 0;
     bool have_time = false;
     
     // for strtol error checking
     errno = 0;

     // Skip leading space
     while(isspace(*dateString))
     	dateString++;

     const char *wordStart = dateString;
     // Check contents of first words if not number
     while(*dateString && !isdigit(*dateString))
     {
        if ( isspace(*dateString) && dateString - wordStart >= 3 )
        {
          month = findMonth(wordStart);
          while(isspace(*dateString))
             dateString++;
          wordStart = dateString;
        }
        else
           dateString++;
     }
     // missing delimiter between month and day (like "January29")?
     if (month == -1 && dateString && wordStart != dateString) {
       month = findMonth(wordStart);
       // TODO: emit warning about dubious format found
     }

     while(isspace(*dateString))
     	dateString++;

     if (!*dateString)
     	return invalidDate;

     // ' 09-Nov-99 23:12:40 GMT'
     day = strtol(dateString, &newPosStr, 10);
     if (errno)
       return invalidDate;
     dateString = newPosStr;

     if (!*dateString)
     	return invalidDate;

     if (day < 1)
       return invalidDate;
     if (day > 31) {
       // ### where is the boundary and what happens below?
       if (*dateString == '/' && day >= 1000) {
         // looks like a YYYY/MM/DD date
         if (!*++dateString)
           return invalidDate;
         year = day;
         month = strtol(dateString, &newPosStr, 10) - 1;
         if (errno)
           return invalidDate;
         dateString = newPosStr;
         if (*dateString++ != '/' || !*dateString)
           return invalidDate;
         day = strtol(dateString, &newPosStr, 10);
         if (errno)
           return invalidDate;
         dateString = newPosStr;
       } else {
         return invalidDate;
       }
     } else if (*dateString == '/' && day <= 12 && month == -1) {
     	dateString++;
        // This looks like a MM/DD/YYYY date, not an RFC date.....
        month = day - 1; // 0-based
        day = strtol(dateString, &newPosStr, 10);
        if (errno)
          return invalidDate;
        dateString = newPosStr;
        if (*dateString == '/')
          dateString++;
        if (!*dateString)
          return invalidDate;
     }
     else
     {
       if (*dateString == '-')
         dateString++;

       while(isspace(*dateString))
         dateString++;

       if (*dateString == ',')
         dateString++;

       if ( month == -1 ) // not found yet
       {
         month = findMonth(dateString);
         if (month == -1)
           return invalidDate;

         while(*dateString && (*dateString != '-') && !isspace(*dateString))
           dateString++;

         if (!*dateString)
           return invalidDate;

         // '-99 23:12:40 GMT'
         if ((*dateString != '-') && (*dateString != '/') && !isspace(*dateString))
           return invalidDate;
         dateString++;
       }

       if ((month < 0) || (month > 11))
         return invalidDate;
     }

     // '99 23:12:40 GMT'
     if (year <= 0 && *dateString) {
       year = strtol(dateString, &newPosStr, 10);
       if (errno)
         return invalidDate;
    }
    
     // Don't fail if the time is missing.
     if (*newPosStr)
     {
        // ' 23:12:40 GMT'
        if (!isspace(*newPosStr)) {
           if ( *newPosStr == ':' ) // Ah, so there was no year, but the number was the hour
               year = -1;
           else
               return invalidDate;
        } else // in the normal case (we parsed the year), advance to the next number
            dateString = ++newPosStr;

        hour = strtol(dateString, &newPosStr, 10);

        // Do not check for errno here since we want to continue
        // even if errno was set becasue we are still looking
        // for the timezone!
        // read a number? if not this might be a timezone name
        if (newPosStr != dateString) {
          have_time = true;
          dateString = newPosStr;

          if ((hour < 0) || (hour > 23))
            return invalidDate;

          if (!*dateString)
            return invalidDate;

          // ':12:40 GMT'
          if (*dateString++ != ':')
            return invalidDate;

          minute = strtol(dateString, &newPosStr, 10);
          if (errno)
            return invalidDate;
          dateString = newPosStr;

          if ((minute < 0) || (minute > 59))
            return invalidDate;

          // ':40 GMT'
          if (*dateString && *dateString != ':' && !isspace(*dateString))
            return invalidDate;

          // seconds are optional in rfc822 + rfc2822
          if (*dateString ==':') {
            dateString++;

            second = strtol(dateString, &newPosStr, 10);
            if (errno)
              return invalidDate;
            dateString = newPosStr;
            
            if ((second < 0) || (second > 59))
              return invalidDate;
          }

          while(isspace(*dateString))
            dateString++;

	  if (strncasecmp(dateString, "AM", 2) == 0) {
	    if (hour > 12)
	      return invalidDate;
	    if (hour == 12)
	      hour = 0;
	    dateString += 2;
	    while (isspace(*dateString))
	      dateString++;
	  } else if (strncasecmp(dateString, "PM", 2) == 0) {
	    if (hour > 12)
	      return invalidDate;
	    if (hour != 12)
	      hour += 12;
	    dateString += 2;
	    while (isspace(*dateString))
	      dateString++;
	  }
        }
     } else {
       dateString = newPosStr;
     }

     // don't fail if the time zone is missing, some
     // broken mail-/news-clients omit the time zone
     if (*dateString) {
       if (strncasecmp(dateString, "GMT", 3) == 0 ||
	   strncasecmp(dateString, "UTC", 3) == 0) {
         dateString += 3;
         have_tz = true;
       }

       while (isspace(*dateString))
         ++dateString;

       if (strncasecmp(dateString, "GMT", 3) == 0) {
         dateString += 3;
       }
       if ((*dateString == '+') || (*dateString == '-')) {
         offset = strtol(dateString, &newPosStr, 10);
         if (errno)
           return invalidDate;
         dateString = newPosStr;

         if ((offset < -9959) || (offset > 9959))
            return invalidDate;

         int sgn = (offset < 0)? -1:1;
         offset = abs(offset);
         if ( *dateString == ':' ) { // GMT+05:00
           int offset2 = strtol(dateString, &newPosStr, 10);
           if (errno)
             return invalidDate;
           dateString = newPosStr;
           offset = (offset*60 + offset2)*sgn;
         }
         else
           offset = ((offset / 100)*60 + (offset % 100))*sgn;
         have_tz = true;
       } else {
         for (int i=0; i < int(sizeof(known_zones)/sizeof(KnownZone)); i++) {
           if (0 == strncasecmp(dateString, known_zones[i].tzName, strlen(known_zones[i].tzName))) {
             offset = known_zones[i].tzOffset;
             have_tz = true;
             break;
           }
         }
         // Bail out if we found an unknown timezone
         if (!have_tz)
             return invalidDate;
       }
     }

     while(isspace(*dateString))
        dateString++;

     if ( *dateString && year == -1 ) {
       year = strtol(dateString, &newPosStr, 10);
       if (errno)
         return invalidDate;
     }

     // Y2K: Solve 2 digit years
     if ((year >= 0) && (year < 50))
         year += 2000;

     if ((year >= 50) && (year < 100))
         year += 1900;  // Y2K

     if (!have_tz) {
       // fall back to midnight, local timezone
       struct tm t;
       memset(&t, 0, sizeof(tm));
       t.tm_mday = day;
       t.tm_mon = month;
       t.tm_year = year - 1900;
       t.tm_isdst = -1;
       if (have_time) {
         t.tm_sec = second;
         t.tm_min = minute;
         t.tm_hour = hour;
       }

       // better not use mktime() as it can't handle the full year range
       return makeTime(&t, 0, false) / 1000.0;
     }
     
     result = ymdhms_to_seconds(year, month+1, day, hour, minute, second) - (offset*60);
     return result;
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
