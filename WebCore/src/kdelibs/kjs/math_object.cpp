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

#include <math.h>
#include <stdlib.h>

#include "kjs.h"
#include "types.h"
#include "operations.h"
#include "math_object.h"
#include "lookup.h"

#include "math_object.lut.h"

using namespace KJS;

KJSO Math::get(const UString &p) const
{
  int token = Lookup::find(&mathTable, p);

  if (token < 0)
    return Imp::get(p);

  double d;
  int len = 1;
  switch (token) {
  case Math::Euler:
    d = exp(1.0);
    break;
  case Math::Ln2:
    d = log(2.0);
    break;
  case Math::Ln10:
    d = log(10.0);
    break;
  case Math::Log2E:
    d = 1.0/log(2.0);
    break;
  case Math::Log10E:
    d = 1.0/log(10.0);
    break;
  case Math::Pi:
    d = 2.0 * asin(1.0);
    break;
  case Math::Sqrt1_2:
    d = sqrt(0.5);
    break;
  case Math::Sqrt2:
    d = sqrt(2.0);
    break;
  default:
    if (token == Math::Min || token == Math::Max || token == Math::Pow)
      len = 2;
    return Function(new MathFunc(token, len));
  };

  return Number(d);
}

bool Math::hasProperty(const UString &p, bool recursive) const
{
  return (Lookup::find(&mathTable, p) >= 0 ||
	  (recursive && Imp::hasProperty(p, recursive)));
}

Completion MathFunc::execute(const List &args)
{
  KJSO v = args[0];
  Number n = v.toNumber();
  double arg = n.value();

  KJSO v2 = args[1];
  Number n2 = v2.toNumber();
  double arg2 = n2.value();
  double result;

  switch (id) {
  case Math::Abs:
    result = ( arg < 0 ) ? (-arg) : arg;
    break;
  case Math::ACos:
    result = ::acos(arg);
    break;
  case Math::ASin:
    result = ::asin(arg);
    break;
  case Math::ATan:
    result = ::atan(arg);
    break;
  case Math::ATan2:
    result = ::atan2(arg, arg2);
    break;
  case Math::Ceil:
    result = ::ceil(arg);
    break;
  case Math::Cos:
    result = ::cos(arg);
    break;
  case Math::Exp:
    result = ::exp(arg);
    break;
  case Math::Floor:
    result = ::floor(arg);
    break;
  case Math::Log:
    result = ::log(arg);
    break;
  case Math::Max:
    result = ( arg > arg2 ) ? arg : arg2;
    break;
  case Math::Min:
    result = ( arg < arg2 ) ? arg : arg2;
    break;
  case Math::Pow:
    result = ::pow(arg, arg2);
    break;
  case Math::Random:
    result = ::rand();
    result = result / RAND_MAX;
    break;
  case Math::Round:
    if (isNaN(arg))
      result = arg;
    if (isInf(arg) || isInf(-arg))
      result = arg;
    else if (arg == -0.5)
      result = 0;
    else
      result = (double)(arg >= 0.0 ? int(arg + 0.5) : int(arg - 0.5));
    break;
  case Math::Sin:
    result = ::sin(arg);
    break;
  case Math::Sqrt:
    result = ::sqrt(arg);
    break;
  case Math::Tan:
    result = ::tan(arg);
    break;

  default:
    result = 0.0;
    assert(0);
  }

  return Completion(ReturnValue, Number(result));
}
