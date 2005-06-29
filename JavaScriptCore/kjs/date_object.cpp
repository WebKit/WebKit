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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

const time_t invalidDate = -1;

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
#define timegm(x) timegmUsingCF(x)
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

static time_t timegmUsingCF(struct tm *tm)
{
    return timetUsingCF(tm, UTCTimeZone());
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

static UString formatLocaleDate(KJS::ExecState *exec,time_t tv, bool includeDate, bool includeTime, const KJS::List &args)
{
    LongDateTime longDateTime;
    UCConvertCFAbsoluteTimeToLongDateTime(tv - kCFAbsoluteTimeIntervalSince1970, &longDateTime);

    CFLocaleRef locale = CFLocaleCopyCurrent();
    
    int argCount = args.size();
    
    CFDateFormatterStyle    dateStyle = (includeDate ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);
    CFDateFormatterStyle    timeStyle = (includeTime ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);

    UString	arg0String;
    UString	arg1String;
    bool	useCustomFormat = false;
    UString	customFormatString;
    arg0String = args[0].toString(exec);
    if ((arg0String == "custom") && (argCount >= 2)) {
	useCustomFormat = true;
	customFormatString = args[1].toString(exec);
    } else if (includeDate && includeTime && (argCount >= 2)) {
	arg1String = args[1].toString(exec);
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
    CFStringRef string = CFDateFormatterCreateStringWithAbsoluteTime(NULL, formatter, tv - kCFAbsoluteTimeIntervalSince1970);

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

using namespace KJS;

// ------------------------------ DateInstanceImp ------------------------------

const ClassInfo DateInstanceImp::info = {"Date", 0, 0, 0};

DateInstanceImp::DateInstanceImp(ObjectImp *proto)
  : ObjectImp(proto)
{
}

// ------------------------------ DatePrototypeImp -----------------------------

const ClassInfo DatePrototypeImp::info = {"Date", 0, &dateTable, 0};

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
  Value protect(this);
  setInternalValue(NumberImp::create(NaN));
  // The constructor will be added later, after DateObjectImp has been built
}

Value DatePrototypeImp::get(ExecState *exec, const Identifier &propertyName) const
{
  return lookupGetFunction<DateProtoFuncImp, ObjectImp>( exec, propertyName, &dateTable, this );
}

// ------------------------------ DateProtoFuncImp -----------------------------

DateProtoFuncImp::DateProtoFuncImp(ExecState *exec, int i, int len)
  : InternalFunctionImp(
    static_cast<FunctionPrototypeImp*>(exec->lexicalInterpreter()->builtinFunctionPrototype().imp())
    ), id(abs(i)), utc(i<0)
  // We use a negative ID to denote the "UTC" variant.
{
  Value protect(this);
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

bool DateProtoFuncImp::implementsCall() const
{
  return true;
}

Value DateProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &args)
{
  if ((id == ToString || id == ValueOf || id == GetTime || id == SetTime) &&
      !thisObj.inherits(&DateInstanceImp::info)) {
    // non-generic function called on non-date object

    // ToString and ValueOf are generic according to the spec, but the mozilla
    // tests suggest otherwise...
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }


  Value result;
  UString s;
#if !APPLE_CHANGES
  const int bufsize=100;
  char timebuffer[bufsize];
  CString oldlocale = setlocale(LC_TIME,NULL);
  if (!oldlocale.c_str())
    oldlocale = setlocale(LC_ALL, NULL);
#endif
  Value v = thisObj.internalValue();
  double milli = v.toNumber(exec);
  
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
        return Number(NaN);
    }
  }
  
  time_t tv = (time_t)(milli / 1000.0);
  int ms = int(milli - tv * 1000.0);

  struct tm *t;
  if (utc)
    t = gmtime(&tv);
  else
    t = localtime(&tv);

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
    result = String(formatLocaleDate(exec,tv, true, true,args));
    break;
  case ToLocaleDateString:
    result = String(formatLocaleDate(exec,tv, true, false,args));
    break;
  case ToLocaleTimeString:
    result = String(formatLocaleDate(exec,tv, false, true,args));
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
    milli = roundValue(exec,args[0]);
    result = Number(milli);
    thisObj.setInternalValue(result);
    break;
  case SetMilliSeconds:
    ms = args[0].toInt32(exec);
    break;
  case SetSeconds:
    t->tm_sec = args[0].toInt32(exec);
    if (args.size() >= 2)
      ms = args[1].toInt32(exec);
    break;
  case SetMinutes:
    t->tm_min = args[0].toInt32(exec);
    if (args.size() >= 2)
      t->tm_sec = args[1].toInt32(exec);
    if (args.size() >= 3)
      ms = args[2].toInt32(exec);
    break;
  case SetHours:
    t->tm_hour = args[0].toInt32(exec);
    if (args.size() >= 2)
      t->tm_min = args[1].toInt32(exec);
    if (args.size() >= 3)
      t->tm_sec = args[2].toInt32(exec);
    if (args.size() >= 4)
      ms = args[3].toInt32(exec);
    break;
  case SetDate:
    t->tm_mday = args[0].toInt32(exec);
    break;
  case SetMonth:
    t->tm_mon = args[0].toInt32(exec);
    if (args.size() >= 2)
      t->tm_mday = args[1].toInt32(exec);
    break;
  case SetFullYear:
    t->tm_year = args[0].toInt32(exec) - 1900;
    if (args.size() >= 2)
      t->tm_mon = args[1].toInt32(exec);
    if (args.size() >= 3)
      t->tm_mday = args[2].toInt32(exec);
    break;
  case SetYear:
    t->tm_year = args[0].toInt32(exec) >= 1900 ? args[0].toInt32(exec) - 1900 : args[0].toInt32(exec);
    break;
  }

  if (id == SetYear || id == SetMilliSeconds || id == SetSeconds ||
      id == SetMinutes || id == SetHours || id == SetDate ||
      id == SetMonth || id == SetFullYear ) {
    time_t mktimeResult = utc ? timegm(t) : mktime(t);
    if (mktimeResult == invalidDate)
      result = Number(NaN);
    else
      result = Number(mktimeResult * 1000.0 + ms);
    thisObj.setInternalValue(result);
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
  Value protect(this);
  
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
Object DateObjectImp::construct(ExecState *exec, const List &args)
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
    UString s = args[0].toString(exec);
    double d = s.toDouble();
    if (isNaN(d))
      value = parseDate(s);
    else
      value = d;
  } else {
    struct tm t;
    memset(&t, 0, sizeof(t));
    if (isNaN(args[0].toNumber(exec))
        || isNaN(args[1].toNumber(exec))
        || (numArgs >= 3 && isNaN(args[2].toNumber(exec)))
        || (numArgs >= 4 && isNaN(args[3].toNumber(exec)))
        || (numArgs >= 5 && isNaN(args[4].toNumber(exec)))
        || (numArgs >= 6 && isNaN(args[5].toNumber(exec)))
        || (numArgs >= 7 && isNaN(args[6].toNumber(exec)))) {
      value = NaN;
    } else {
      int year = args[0].toInt32(exec);
      t.tm_year = (year >= 0 && year <= 99) ? year : year - 1900;
      t.tm_mon = args[1].toInt32(exec);
      t.tm_mday = (numArgs >= 3) ? args[2].toInt32(exec) : 1;
      t.tm_hour = (numArgs >= 4) ? args[3].toInt32(exec) : 0;
      t.tm_min = (numArgs >= 5) ? args[4].toInt32(exec) : 0;
      t.tm_sec = (numArgs >= 6) ? args[5].toInt32(exec) : 0;
      t.tm_isdst = -1;
      int ms = (numArgs >= 7) ? args[6].toInt32(exec) : 0;
      time_t mktimeResult = mktime(&t);
      if (mktimeResult == invalidDate)
        value = NaN;
      else
        value = mktimeResult * 1000.0 + ms;
    }
  }

  Object proto = exec->lexicalInterpreter()->builtinDatePrototype();
  Object ret(new DateInstanceImp(proto.imp()));
  ret.setInternalValue(Number(timeClip(value)));
  return ret;
}

bool DateObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.9.2
Value DateObjectImp::call(ExecState */*exec*/, Object &/*thisObj*/, const List &/*args*/)
{
#ifdef KJS_VERBOSE
  fprintf(stderr,"DateObjectImp::call - current time\n");
#endif
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
  Value protect(this);
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

bool DateObjectFuncImp::implementsCall() const
{
  return true;
}

// ECMA 15.9.4.2 - 3
Value DateObjectFuncImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  if (id == Parse) {
    return Number(parseDate(args[0].toString(exec)));
  }
  else { // UTC
    struct tm t;
    memset(&t, 0, sizeof(t));
    int n = args.size();
    if (isNaN(args[0].toNumber(exec))
        || isNaN(args[1].toNumber(exec))
        || (n >= 3 && isNaN(args[2].toNumber(exec)))
        || (n >= 4 && isNaN(args[3].toNumber(exec)))
        || (n >= 5 && isNaN(args[4].toNumber(exec)))
        || (n >= 6 && isNaN(args[5].toNumber(exec)))
        || (n >= 7 && isNaN(args[6].toNumber(exec)))) {
      return Number(NaN);
    }
    int year = args[0].toInt32(exec);
    t.tm_year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.tm_mon = args[1].toInt32(exec);
    t.tm_mday = (n >= 3) ? args[2].toInt32(exec) : 1;
    t.tm_hour = (n >= 4) ? args[3].toInt32(exec) : 0;
    t.tm_min = (n >= 5) ? args[4].toInt32(exec) : 0;
    t.tm_sec = (n >= 6) ? args[5].toInt32(exec) : 0;
    int ms = (n >= 7) ? args[6].toInt32(exec) : 0;
    time_t mktimeResult = timegm(&t);
    if (mktimeResult == invalidDate)
      return Number(NaN);
    return Number(mktimeResult * 1000.0 + ms);
  }
}

// -----------------------------------------------------------------------------


double KJS::parseDate(const UString &u)
{
#ifdef KJS_VERBOSE
  fprintf(stderr,"KJS::parseDate %s\n",u.ascii());
#endif
  int firstSlash = u.find('/');
  if ( firstSlash == -1 )
  {
    time_t seconds = KRFCDate_parseDate( u );
#ifdef KJS_VERBOSE
    fprintf(stderr,"KRFCDate_parseDate returned seconds=%d\n",seconds);
#endif
    if ( seconds == invalidDate )
      return NaN;
    else
      return seconds * 1000.0;
  }
  else
  {
    // Found 12/31/2099 on some website -> obviously MM/DD/YYYY
    int month = u.substr(0,firstSlash).toULong();
    int secondSlash = u.find('/',firstSlash+1);
    //fprintf(stdout,"KJS::parseDate firstSlash=%d, secondSlash=%d\n", firstSlash, secondSlash);
    if ( secondSlash == -1 )
    {
      fprintf(stderr,"KJS::parseDate parsing for this format isn't implemented\n%s", u.ascii());
      return NaN;
    }
    int day = u.substr(firstSlash+1,secondSlash-firstSlash-1).toULong();
    int year = u.substr(secondSlash+1).toULong();
    //fprintf(stdout,"KJS::parseDate day=%d, month=%d, year=%d\n", day, month, year);
    struct tm t;
    memset( &t, 0, sizeof(t) );
#if !APPLE_CHANGES
    year = (year > 2037) ? 2037 : year; // mktime is limited to 2037 !!!
#endif
    t.tm_year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.tm_mon = month-1; // mktime wants 0-11 for some reason
    t.tm_mday = day;
    time_t seconds = mktime(&t);
    if ( seconds == invalidDate )
    {
#if !APPLE_CHANGES
      fprintf(stderr,"KJS::parseDate mktime returned -1.\n%s", u.ascii());
#endif
      return NaN;
    }
    else
      return seconds * 1000.0;
  }
}

///// Awful duplication from krfcdate.cpp - we don't link to kdecore

static unsigned int ymdhms_to_seconds(int year, int mon, int day, int hour, int minute, int second)
{
    unsigned int ret = (day - 32075)       /* days */
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
static const struct {
    const char *tzName;
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
    { "PDT", -420 },
    { 0, 0 }
};

static inline bool isSpaceOrTab(char c)
{
    return c == ' ' || c == '\t';
}

time_t KJS::KRFCDate_parseDate(const UString &_date)
{
     // This parse a date in the form:
     //     Wednesday, 09-Nov-99 23:12:40 GMT
     // or
     //     Sat, 01-Jan-2000 08:00:00 GMT
     // or
     //     Sat, 01 Jan 2000 08:00:00 GMT
     // or
     //     01 Jan 99 22:00 +0100    (exceptions in rfc822/rfc2822)
     // ### non RFC format, added for Javascript:
     //     [Wednesday] January 09 1999 23:12:40 GMT
     //
     // We ignore the weekday
     //
     int offset = 0;
     char *newPosStr;
     const char *dateString = _date.ascii();
     int day = 0;
     char monthStr[4];
     int month = -1; // not set yet
     int year = 0;
     int hour = 0;
     int minute = 0;
     int second = 0;

     errno = 0;

     // Skip leading space
     while (isSpaceOrTab(*dateString))
     	dateString++;

     const char *wordStart = dateString;
     // Check contents of first words if not number
     while(*dateString && !isdigit(*dateString))
     {
        if ( isSpaceOrTab(*dateString) && dateString - wordStart >= 3 )
        {
          monthStr[0] = tolower(*wordStart++);
          monthStr[1] = tolower(*wordStart++);
          monthStr[2] = tolower(*wordStart++);
          monthStr[3] = '\0';
          //fprintf(stderr,"KJS::parseDate found word starting with '%s'\n", monthStr);
          const char *str = strstr(haystack, monthStr);
          if (str) {
            int position = str - haystack;
            if (position % 3 == 0) {
              month = position / 3; // Jan=00, Feb=01, Mar=02, ..
            }
          }
          while (isSpaceOrTab(*dateString))
             dateString++;
          wordStart = dateString;
        }
        else
           dateString++;
     }

     while (isSpaceOrTab(*dateString))
     	dateString++;

     if (!*dateString)
     	return invalidDate;

     // ' 09-Nov-99 23:12:40 GMT'
     day = strtol(dateString, &newPosStr, 10);
     if (errno)
        return invalidDate;
     dateString = newPosStr;

     if ((day < 1) || (day > 31))
     	return invalidDate;
     if (!*dateString)
     	return invalidDate;

     if (*dateString == '-' || *dateString == ',')
     	dateString++;

     while (isSpaceOrTab(*dateString))
     	dateString++;

     if ( month == -1 ) // not found yet
     {
        for(int i=0; i < 3;i++)
        {
           if (!*dateString || (*dateString == '-') || isSpaceOrTab(*dateString))
              return invalidDate;
           monthStr[i] = tolower(*dateString++);
        }
        monthStr[3] = '\0';

        newPosStr = (char*)strstr(haystack, monthStr);

        if (!newPosStr || (newPosStr - haystack) % 3 != 0)
           return invalidDate;

        month = (newPosStr-haystack)/3; // Jan=00, Feb=01, Mar=02, ..

        if ((month < 0) || (month > 11))
           return invalidDate;

        while (*dateString && *dateString != '-' && !isSpaceOrTab(*dateString))
           dateString++;

        if (!*dateString)
           return invalidDate;

        // '-99 23:12:40 GMT'
        if (*dateString != '-' && !isSpaceOrTab(*dateString))
           return invalidDate;
        dateString++;
     }

     if ((month < 0) || (month > 11))
     	return invalidDate;

     // '99 23:12:40 GMT'
     bool gotYear = true;
     year = strtol(dateString, &newPosStr, 10);
     if (errno)
        return invalidDate;
     dateString = newPosStr;

     // Don't fail if the time is missing.
     if (*dateString == ':' || (isSpaceOrTab(*dateString) && isdigit(dateString[1])))
     {
        if (*dateString == ':') {
          hour = year;
          gotYear = false;
        } else {
          // ' 23:12:40 GMT'
          ++dateString;
        
          hour = strtol(dateString, &newPosStr, 10);
          if (errno)
            return invalidDate;
          dateString = newPosStr;
        }

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
     }
     
     while (isSpaceOrTab(*dateString))
        dateString++;

     if (!gotYear) {
        year = strtol(dateString, &newPosStr, 10);
        if (errno)
          return invalidDate;
        while (isSpaceOrTab(*dateString))
           dateString++;
     }

     // Y2K: Solve 2 digit years
     if ((year >= 0) && (year < 50))
         year += 2000;

     if ((year >= 50) && (year < 100))
         year += 1900;  // Y2K

     if ((year < 1900) || (year > 2500))
     	return invalidDate;

     if (strncasecmp(dateString, "AM", 2) == 0) {
        if (hour < 1 || hour > 12)
            return invalidDate;
        if (hour == 12)
            hour = 0;
        dateString += 2;
        while (isSpaceOrTab(*dateString))
           dateString++;
     } else if (strncasecmp(dateString, "PM", 2) == 0) {
        if (hour < 1 || hour > 12)
            return invalidDate;
        if (hour != 12)
            hour += 12;
        dateString += 2;
        while (isSpaceOrTab(*dateString))
           dateString++;
     }

     // don't fail if the time zone is missing, some
     // broken mail-/news-clients omit the time zone
     bool localTime;
     if (*dateString == 0) {
        // Other web browsers interpret missing time zone as "current time zone".
        localTime = true;
     } else {
        localTime = false;
        if (strncasecmp(dateString, "GMT", 3) == 0) {
            dateString += 3;
        }
        if ((*dateString == '+') || (*dateString == '-')) {
           offset = strtol(dateString, &newPosStr, 10);

           if (errno || (offset < -9959) || (offset > 9959))
              return invalidDate;

           int sgn = (offset < 0)? -1:1;
           offset = abs(offset);
           offset = ((offset / 100)*60 + (offset % 100))*sgn;
        } else {
           for (int i=0; known_zones[i].tzName != 0; i++) {
              if (0 == strncasecmp(dateString, known_zones[i].tzName, strlen(known_zones[i].tzName))) {
                 offset = known_zones[i].tzOffset;
                 break;
              }
           }
        }
     }
     if (sizeof(time_t) == 4)
     {
         if ((time_t)-1 < 0)
         {
            if (year >= 2038)
            {
               year = 2038;
               month = 0;
               day = 1;
               hour = 0;
               minute = 0;
               second = 0;
            }
         }
         else
         {
            if (year >= 2115)
            {
               year = 2115;
               month = 0;
               day = 1;
               hour = 0;
               minute = 0;
               second = 0;
            }
         }
     }

    time_t result;
     
    if (localTime) {
      struct tm tm;
      tm.tm_year = year - 1900;
      tm.tm_mon = month;
      tm.tm_mday = day;
      tm.tm_hour = hour;
      tm.tm_min = minute;
      tm.tm_sec = second;
      tm.tm_isdst = -1;
      result = mktime(&tm);
    } else {
     result = ymdhms_to_seconds(year, month+1, day, hour, minute, second);

     // avoid negative time values
     if ((offset > 0) && (offset > result))
        offset = 0;

     result -= offset*60;
    }

     return result;
}


double KJS::timeClip(double t)
{
    if (!isfinite(t))
        return NaN;
    double at = fabs(t);
    if (at > 8.64E15)
        return NaN;
    return copysign(floor(at), t);
}
