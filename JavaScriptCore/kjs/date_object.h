// -*- c-basic-offset: 2 -*-
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _DATE_OBJECT_H_
#define _DATE_OBJECT_H_

#include "internal.h"
#include "function_object.h"

#include <sys/time.h>

namespace KJS {

  class DateInstanceImp : public ObjectImp {
  public:
    DateInstanceImp(ObjectImp *proto);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * The initial value of Date.prototype (and thus all objects created
   * with the Date constructor
   */
  class DatePrototypeImp : public DateInstanceImp {
  public:
    DatePrototypeImp(ExecState *exec, ObjectPrototypeImp *objectProto);
    bool getOwnPropertySlot(ExecState *, const Identifier &, PropertySlot&);
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Date.prototype object
   */
  class DateProtoFuncImp : public InternalFunctionImp {
  public:
    DateProtoFuncImp(ExecState *exec, int i, int len);

    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);


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

  /**
   * @internal
   *
   * The initial value of the the global variable's "Date" property
   */
  class DateObjectImp : public InternalFunctionImp {
  public:
    DateObjectImp(ExecState *exec,
                  FunctionPrototypeImp *funcProto,
                  DatePrototypeImp *dateProto);

    virtual bool implementsConstruct() const;
    virtual ObjectImp *construct(ExecState *exec, const List &args);
    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);

    Completion execute(const List &);
    ObjectImp *construct(const List &);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Date object
   */
  class DateObjectFuncImp : public InternalFunctionImp {
  public:
    DateObjectFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                      int i, int len);

    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);

    enum { Parse, UTC };
  private:
    int id;
  };

  // helper functions
  double parseDate(const UString &u);
  double KRFCDate_parseDate(const UString &_date);
  double timeClip(double t);
  double makeTime(struct tm *t, double milli, bool utc);

} // namespace

#endif
