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
#include "DateConstructor.h"

#include "DateMath.h"
#include "DatePrototype.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "date_object.h"
#include <math.h>
#include <time.h>
#include <wtf/MathExtras.h>

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

namespace KJS {

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

} // namespace KJS
