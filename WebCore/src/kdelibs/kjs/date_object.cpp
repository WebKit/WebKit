/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
#include <locale.h>

#include "kjs.h"
#include "date_object.h"

namespace KJS {

  class DateObjectFunc : public InternalFunctionImp {
  public:
    DateObjectFunc(int i) : id(i) { };
    Completion execute(const List &);
    enum { Parse, UTC };
  private:
    int id;
  };

  class DateProtoFunc : public InternalFunctionImp {
  public:
    DateProtoFunc(int i, bool u);
    Completion execute(const List &);
    enum { ToString, ToDateString, ToTimeString, ToLocaleString,
	   ToLocaleDateString, ToLocaleTimeString, ValueOf, GetTime,
	   GetFullYear, GetMonth, GetDate, GetDay, GetHours, GetMinutes,
	   GetSeconds, GetMilliSeconds, GetTimezoneOffset, SetTime,
	   SetMilliSeconds, SetSeconds, SetMinutes, SetHours, SetDate,
	   SetMonth, SetFullYear, ToUTCString,
	   // non-normative properties (Appendix B)
	   GetYear, SetYear, ToGMTString };
  private:
    int id;
    bool utc;
  };

  // helper functions
  KJSO parseDate(const String &s);
  KJSO timeClip(const KJSO &t);
};

using namespace KJS;

KJSO KJS::parseDate(const String &s)
{
  UString u = s.value();
  int firstSlash = u.find('/');
  if ( firstSlash == -1 )
  {
    /* TODO parse dates like "December 25, 1995 23:15:00"*/
    fprintf(stderr,"KJS::parseDate parsing for this format isn't implemented\n%s", u.ascii());
    return Number(0);
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
      return Number(0);
    }
    int day = u.substr(firstSlash+1,secondSlash-firstSlash-1).toULong();
    int year = u.substr(secondSlash+1).toULong();
    //fprintf(stdout,"KJS::parseDate day=%d, month=%d, year=%d\n", day, month, year);
    struct tm t;
    memset( &t, 0, sizeof(t) );
    year = (year > 2037) ? 2037 : year; // mktime is limited to 2037 !!!
    t.tm_year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.tm_mon = month-1; // mktime wants 0-11 for some reason
    t.tm_mday = day;
    time_t seconds = mktime(&t);
    if ( seconds == -1 )
    {
      fprintf(stderr,"KJS::parseDate mktime returned -1.\n%s", u.ascii());
      return Undefined();
    }
    else
      return Number(seconds * 1000.0);
  }
}

KJSO KJS::timeClip(const KJSO &t)
{
  /* TODO */
  return t;
}

DateObject::DateObject(const Object& funcProto, const Object &dateProto)
    : ConstructorImp(funcProto, 7)
{
  // ECMA 15.9.4.1 Date.prototype
  setPrototypeProperty(dateProto);
}

// ECMA 15.9.2
Completion DateObject::execute(const List &)
{
  time_t t = time(0L);
  UString s(ctime(&t));

  // return formatted string minus trailing \n
  return Completion(ReturnValue, String(s.substr(0, s.size() - 1)));
}

// ECMA 15.9.3
Object DateObject::construct(const List &args)
{
  KJSO value;

  int numArgs = args.size();

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
    value = Number(utc);
  } else if (numArgs == 1) {
    KJSO p = args[0].toPrimitive();
    if (p.isA(StringType))
      value = parseDate(p.toString());
    else
      value = p.toNumber();
  } else {
    struct tm t;
    memset(&t, 0, sizeof(t));
    Number y = args[0].toNumber();
    /* TODO: check for NaN */
    int year = y.toInt32();
    t.tm_year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.tm_mon = args[1].toInt32();
    t.tm_mday = (numArgs >= 3) ? args[2].toInt32() : 1;
    t.tm_hour = (numArgs >= 4) ? args[3].toInt32() : 0;
    t.tm_min = (numArgs >= 5) ? args[4].toInt32() : 0;
    t.tm_sec = (numArgs >= 6) ? args[5].toInt32() : 0;
    t.tm_isdst = -1;
    int ms = (numArgs >= 7) ? args[6].toInt32() : 0;
    value = Number(mktime(&t) * 1000.0 + ms);
  }

  return Object::create(DateClass, timeClip(value));
}

KJSO DateObject::get(const UString &p) const
{
  int id;

  if (p == "parse")
    id = DateObjectFunc::Parse;
  else if (p == "UTC")
    id = DateObjectFunc::UTC;
  else
    return Imp::get(p);

  return Function(new DateObjectFunc(id));
}

// ECMA 15.9.4.2 - 3
Completion DateObjectFunc::execute(const List &args)
{
  KJSO result;

  if (id == Parse)
    if (args[0].isA(StringType))
      result = parseDate(args[0].toString());
    else
      result = Undefined();
  else {
    struct tm t;
    memset(&t, 0, sizeof(t));
    int n = args.size();
    Number y = args[0].toNumber();
    /* TODO: check for NaN */
    int year = y.toInt32();
    t.tm_year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.tm_mon = args[1].toInt32();
    t.tm_mday = (n >= 3) ? args[2].toInt32() : 1;
    t.tm_hour = (n >= 4) ? args[3].toInt32() : 0;
    t.tm_min = (n >= 5) ? args[4].toInt32() : 0;
    t.tm_sec = (n >= 6) ? args[5].toInt32() : 0;
    int ms = (n >= 7) ? args[6].toInt32() : 0;
    result = Number(mktime(&t) * 1000.0 + ms);
  }

  return Completion(ReturnValue, result);
}

// ECMA 15.9.4
DatePrototype::DatePrototype(const Object& proto)
  : ObjectImp(DateClass, Number(NaN), proto)
{
  // The constructor will be added later in DateObject's constructor
}

KJSO DatePrototype::get(const UString &p) const
{
  int id;

  if (p == "toString" || p == "toUTCString")
    id = DateProtoFunc::ToString;
  else if (p == "toDateString")
    id = DateProtoFunc::ToDateString;
  else if (p == "toTimeString")
    id = DateProtoFunc::ToTimeString;
  else if (p == "toLocaleString")
    id = DateProtoFunc::ToLocaleString;
  else if (p == "toLocaleDateString")
    id = DateProtoFunc::ToLocaleDateString;
  else if (p == "toLocaleTimeString")
    id = DateProtoFunc::ToLocaleTimeString;
  else if (p == "valueOf")
    id = DateProtoFunc::ValueOf;
  else if (p == "getTime")
    id = DateProtoFunc::GetTime;
  else if (p == "getFullYear" || p == "getUTCFullYear")
    id = DateProtoFunc::GetFullYear;
  else if (p == "toGMTString")
    id = DateProtoFunc::ToGMTString;
  else if (p == "getMonth" || p == "getUTCMonth")
    id = DateProtoFunc::GetMonth;
  else if (p == "getDate" || p == "getUTCDate")
    id = DateProtoFunc::GetDate;
  else if (p == "getDay" || p == "getUTCDay")
    id = DateProtoFunc::GetDay;
  else if (p == "getHours" || p == "getUTCHours")
    id = DateProtoFunc::GetHours;
  else if (p == "getMinutes" || p == "getUTCMinutes")
    id = DateProtoFunc::GetMinutes;
  else if (p == "getSeconds" || p == "getUTCSeconds")
    id = DateProtoFunc::GetSeconds;
  else if (p == "getMilliseconds" || p == "getUTCMilliseconds")
    id = DateProtoFunc::GetMilliSeconds;
  else if (p == "getTimezoneOffset")
    id = DateProtoFunc::GetTimezoneOffset;
  else if (p == "setTime")
    id = DateProtoFunc::SetTime;
  else if (p == "setMilliseconds" || p == "setUTCMilliseconds")
    id = DateProtoFunc::SetMilliSeconds;
  else if (p == "setSeconds" || p == "setUTCSeconds")
    id = DateProtoFunc::SetSeconds;
  else if (p == "setMinutes" || p == "setUTCMinutes")
    id = DateProtoFunc::SetMinutes;
  else if (p == "setHours" || p == "setUTCHours")
    id = DateProtoFunc::SetHours;
  else if (p == "setDate" || p == "setUTCDate")
    id = DateProtoFunc::SetDate;
  else if (p == "setMonth" || p == "setUTCMonth")
    id = DateProtoFunc::SetMonth;
  else if (p == "setFullYear" || p == "setUTCFullYear")
    id = DateProtoFunc::SetFullYear;
  else if (p == "setYear" )
    id = DateProtoFunc::SetYear;
  // non-normative
  else if (p == "getYear")
    id = DateProtoFunc::GetYear;
  else if (p == "toGMTString")
    id = DateProtoFunc::ToGMTString;
  else
    return Undefined();

  bool utc = (p.find("UTC") >= 0) ? true : false;
  return Function(new DateProtoFunc(id, utc));
}

DateProtoFunc::DateProtoFunc(int i, bool u) : id(i), utc(u)
{
}

Completion DateProtoFunc::execute(const List &args)
{
  KJSO result;
  UString s;
  const int bufsize=100;
  char timebuffer[bufsize];
  char *oldlocale = setlocale(LC_TIME,NULL);
  if (!oldlocale)
    oldlocale = setlocale(LC_ALL, NULL);
  Object thisObj = Object::dynamicCast(thisValue());
  KJSO v = thisObj.internalValue();
  double milli = v.toNumber().value();
  time_t tv = (time_t) floor(milli / 1000.0);
  int ms = int(milli - tv * 1000.0);

  struct tm *t;
  if (utc)
    t = gmtime(&tv);
  else
    t = localtime(&tv);

  switch (id) {
  case ToString:
    s = ctime(&tv);
    result = String(s.substr(0, s.size() - 1));
    break;
  case ToDateString:
  case ToTimeString:
  case ToGMTString:
    setlocale(LC_TIME,"C");
    if (id == DateProtoFunc::ToDateString) {
      strftime(timebuffer, bufsize, "%x",t);
    } else if (id == DateProtoFunc::ToTimeString) {
      strftime(timebuffer, bufsize, "%X",t);
    } else {
      t = gmtime(&tv);
      strftime(timebuffer, bufsize, "%a, %d-%b-%y %H:%M:%S %Z", t);
    }
    setlocale(LC_TIME,oldlocale);
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
  case ValueOf:
    result = Number(milli);
    break;
  case GetTime:
    if (thisObj.getClass() == DateClass)
      result = Number(milli);
    else
      result = Error::create(TypeError);
    break;
  case GetYear:
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
    result = Undefined();
    break;
  case GetTimezoneOffset:
#if defined BSD || defined(__APPLE__)
    result = Number(-( t->tm_gmtoff / 60 ) + ( t->tm_isdst ? 60 : 0 ));
#else
#  if defined(__BORLANDC__)
#error please add daylight savings offset here!
    result = Number(_timezone / 60 - (_daylight ? 60 : 0));
#  else
    result = Number(( timezone / 60 - ( daylight ? 60 : 0 )));
#  endif
#endif
    break;
  case SetTime:
    milli = args[0].round();
    result = Number(milli);
    thisObj.setInternalValue(result);
    break;
  case SetMilliSeconds:
    ms = args[0].toInt32();
    break;
  case SetSeconds:
    t->tm_sec = args[0].toInt32();
    break;
  case SetMinutes:
    t->tm_min = args[0].toInt32();
    break;
  case SetHours:
    t->tm_hour = args[0].toInt32();
    break;
  case SetDate:
    t->tm_mday = args[0].toInt32();
    break;
  case SetMonth:
    t->tm_mon = args[0].toInt32();
    break;
  case SetFullYear:
    t->tm_year = args[0].toInt32() - 1900;
    break;
  case SetYear:
    t->tm_year = args[0].toInt32() >= 1900 ? args[0].toInt32() - 1900 : args[0].toInt32();
    break;
  }

  if (id == SetYear || id == SetMilliSeconds || id == SetSeconds ||
      id == SetMinutes || id == SetHours || id == SetDate ||
      id == SetMonth || id == SetFullYear ) {
    result = Number(mktime(t) * 1000.0 + ms);
    thisObj.setInternalValue(result);
  }

  return Completion(ReturnValue, result);
}
