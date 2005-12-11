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

#include "config.h"
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "number_object.h"
#include "error_object.h"
#include "dtoa.h"

#include "number_object.lut.h"

#include <assert.h>
#include <math.h>
using namespace KJS;


// ------------------------------ NumberInstanceImp ----------------------------

const ClassInfo NumberInstanceImp::info = {"Number", 0, 0, 0};

NumberInstanceImp::NumberInstanceImp(ObjectImp *proto)
  : ObjectImp(proto)
{
}
// ------------------------------ NumberPrototypeImp ---------------------------

// ECMA 15.7.4

NumberPrototypeImp::NumberPrototypeImp(ExecState *exec,
                                       ObjectPrototypeImp *objProto,
                                       FunctionPrototypeImp *funcProto)
  : NumberInstanceImp(objProto)
{
  setInternalValue(jsNumber(0));

  // The constructor will be added later, after NumberObjectImp has been constructed

  putDirect(toStringPropertyName,       new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ToString,       1), DontEnum);
  putDirect(toLocaleStringPropertyName, new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ToLocaleString, 0), DontEnum);
  putDirect(valueOfPropertyName,        new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ValueOf,        0), DontEnum);
  putDirect(toFixedPropertyName,        new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ToFixed,        1), DontEnum);
  putDirect(toExponentialPropertyName,  new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ToExponential,  1), DontEnum);
  putDirect(toPrecisionPropertyName,    new NumberProtoFuncImp(exec,funcProto,NumberProtoFuncImp::ToPrecision,    1), DontEnum);
}


// ------------------------------ NumberProtoFuncImp ---------------------------

NumberProtoFuncImp::NumberProtoFuncImp(ExecState *exec,
                                       FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}


bool NumberProtoFuncImp::implementsCall() const
{
  return true;
}

static UString integer_part_noexp(double d)
{
    int decimalPoint;
    int sign;
    char *result = kjs_dtoa(d, 0, 0, &decimalPoint, &sign, NULL);
    int length = strlen(result);
    
    UString str = sign ? "-" : "";
    if (decimalPoint == 9999) {
        str += UString(result);
    } else if (decimalPoint <= 0) {
        str += UString("0");
    } else {
        char *buf;
        
        if (length <= decimalPoint) {
            buf = (char*)fastMalloc(decimalPoint+1);
            strcpy(buf,result);
            memset(buf+length,'0',decimalPoint-length);
        } else {
            buf = (char*)fastMalloc(decimalPoint+1);
            strncpy(buf,result,decimalPoint);
        }
        
        buf[decimalPoint] = '\0';
        str += UString(buf);
        fastFree(buf);
    }
    
    kjs_freedtoa(result);
    
    return str;
}

static UString char_sequence(char c, int count)
{
    char *buf = (char*)fastMalloc(count+1);
    memset(buf,c,count);
    buf[count] = '\0';
    UString s(buf);
    fastFree(buf);
    return s;
}

// ECMA 15.7.4.2 - 15.7.4.7
ValueImp *NumberProtoFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  // no generic function. "this" has to be a Number object
  if (!thisObj->inherits(&NumberInstanceImp::info))
    return throwError(exec, TypeError);

  ValueImp *v = thisObj->internalValue();
  switch (id) {
  case ToString: {
    double dradix = 10;
    if (!args.isEmpty())
      dradix = args[0]->toInteger(exec);
    if (dradix >= 2 && dradix <= 36 && dradix != 10) { // false for NaN
      int radix = static_cast<int>(dradix);
      unsigned i = v->toUInt32(exec);
      char s[33];
      char *p = s + sizeof(s);
      *--p = '\0';
      do {
        *--p = "0123456789abcdefghijklmnopqrstuvwxyz"[i % radix];
        i /= radix;
      } while (i);
      return jsString(p);
    } else
      return jsString(v->toString(exec));
  }
  case ToLocaleString: /* TODO */
    return jsString(v->toString(exec));
  case ValueOf:
    return jsNumber(v->toNumber(exec));
  case ToFixed: 
  {
      ValueImp *fractionDigits = args[0];
      double df = fractionDigits->toInteger(exec);
      if (!(df >= 0 && df <= 20)) // true for NaN
          return throwError(exec, RangeError, "toFixed() digits argument must be between 0 and 20");
      int f = (int)df;
      
      double x = v->toNumber(exec);
      if (isNaN(x))
          return jsString("NaN");
      
      UString s = "";
      if (x < 0) {
          s += "-";
          x = -x;
      }
      
      if (x >= pow(10.0, 21.0))
          return jsString(s+UString::from(x));
      
      double n = floor(x*pow(10.0, f));
      if (fabs(n / pow(10.0, f) - x) > fabs((n + 1) / pow(10.0, f) - x))
          n++;
      
      UString m = integer_part_noexp(n);
      
      int k = m.size();
      if (m.size() < f) {
          UString z = "";
          for (int i = 0; i < f+1-k; i++)
              z += "0";
          m = z + m;
          k = f + 1;
          assert(k == m.size());
      }
      if (k-f < m.size())
          return jsString(s+m.substr(0,k-f)+"."+m.substr(k-f));
      else
          return jsString(s+m.substr(0,k-f));
  }
  case ToExponential: {
      double x = v->toNumber(exec);
      
      if (isNaN(x) || isInf(x))
          return jsString(UString::from(x));
      
      ValueImp *fractionDigits = args[0];
      double df = fractionDigits->toInteger(exec);
      if (!(df >= 0 && df <= 20)) // true for NaN
          return throwError(exec, RangeError, "toExponential() argument must between 0 and 20");
      int f = (int)df;
      
      int decimalAdjust = 0;
      if (!fractionDigits->isUndefined()) {
          double logx = floor(log10(x));
          x /= pow(10.0, logx);
          double fx = floor(x * pow(10.0, f)) / pow(10.0,f);
          double cx = ceil(x * pow(10.0, f)) / pow(10.0, f);
          
          if (fabs(fx-x) < fabs(cx-x))
              x = fx;
          else
              x = cx;
          
          decimalAdjust = int(logx);
      }
      
      char buf[80];
      int decimalPoint;
      int sign;
      
      if (isNaN(x))
          return jsString("NaN");
      
      char *result = kjs_dtoa(x, 0, 0, &decimalPoint, &sign, NULL);
      int length = strlen(result);
      decimalPoint += decimalAdjust;
      
      int i = 0;
      if (sign) {
          buf[i++] = '-';
      }
      
      if (decimalPoint == 999) {
          strcpy(buf + i, result);
      } else {
          buf[i++] = result[0];
          
          if (fractionDigits->isUndefined())
              f = length-1;
          
          if (length > 1 && f > 0) {
              buf[i++] = '.';
              int haveFDigits = length-1;
              if (f < haveFDigits) {
                  strncpy(buf+i,result+1, f);
                  i += f;
              }
              else {
                  strcpy(buf+i,result+1);
                  i += length-1;
                  for (int j = 0; j < f-haveFDigits; j++)
                      buf[i++] = '0';
              }
          }
          
          buf[i++] = 'e';
          buf[i++] = (decimalPoint >= 0) ? '+' : '-';
          // decimalPoint can't be more than 3 digits decimal given the
          // nature of float representation
          int exponential = decimalPoint - 1;
          if (exponential < 0) {
              exponential = exponential * -1;
          }
          if (exponential >= 100) {
              buf[i++] = '0' + exponential / 100;
          }
          if (exponential >= 10) {
              buf[i++] = '0' + (exponential % 100) / 10;
          }
          buf[i++] = '0' + exponential % 10;
          buf[i++] = '\0';
      }
      
      assert(i <= 80);
      
      kjs_freedtoa(result);
      
      return jsString(buf);
  }
  case ToPrecision:
  {
      int e = 0;
      UString m;
      
      double dp = args[0]->toInteger(exec);
      double x = v->toNumber(exec);
      if (isNaN(dp) || isNaN(x) || isInf(x))
          return jsString(v->toString(exec));
      
      UString s = "";
      if (x < 0) {
          s = "-";
          x = -x;
      }
      
      if (!(dp >= 1 && dp <= 21)) // true for NaN
          return throwError(exec, RangeError, "toPrecision() argument must be between 1 and 21");
      int p = (int)dp;
      
      if (x != 0) {
          e = int(log10(x));
          double n = floor(x / pow(10.0, e - p + 1));
          if (n < pow(10.0, p - 1)) {
              e = e - 1;
              n = floor(x / pow(10.0, e - p + 1));
          }
          
          if (fabs((n + 1) * pow(10.0, e - p + 1) - x) < fabs(n * pow(10.0, e - p + 1) - x))
              n++;
          assert(pow(10.0, p - 1) <= n);
          assert(n < pow(10.0, p));
          
          m = integer_part_noexp(n);
          if (e < -6 || e >= p) {
              if (m.size() > 1)
                  m = m.substr(0,1)+"."+m.substr(1);
              if (e >= 0)
                  return jsString(s+m+"e+"+UString::from(e));
              else
                  return jsString(s+m+"e-"+UString::from(-e));
          }
      }
      else {
          m = char_sequence('0',p);
          e = 0;
      }
      
      if (e == p-1) {
          return jsString(s+m);
      }
      else if (e >= 0) {
          if (e+1 < m.size())
              return jsString(s+m.substr(0,e+1)+"."+m.substr(e+1));
          else
              return jsString(s+m.substr(0,e+1));
      }
      else {
          return jsString(s+"0."+char_sequence('0',-(e+1))+m);
      }
   }
      
 }
  return NULL;
}

// ------------------------------ NumberObjectImp ------------------------------

const ClassInfo NumberObjectImp::info = {"Number", &InternalFunctionImp::info, &numberTable, 0};

/* Source for number_object.lut.h
@begin numberTable 5
  NaN			NumberObjectImp::NaNValue	DontEnum|DontDelete|ReadOnly
  NEGATIVE_INFINITY	NumberObjectImp::NegInfinity	DontEnum|DontDelete|ReadOnly
  POSITIVE_INFINITY	NumberObjectImp::PosInfinity	DontEnum|DontDelete|ReadOnly
  MAX_VALUE		NumberObjectImp::MaxValue	DontEnum|DontDelete|ReadOnly
  MIN_VALUE		NumberObjectImp::MinValue	DontEnum|DontDelete|ReadOnly
@end
*/
NumberObjectImp::NumberObjectImp(ExecState *exec,
                                 FunctionPrototypeImp *funcProto,
                                 NumberPrototypeImp *numberProto)
  : InternalFunctionImp(funcProto)
{
  // Number.Prototype
  putDirect(prototypePropertyName, numberProto,DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}

bool NumberObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<NumberObjectImp, InternalFunctionImp>(exec, &numberTable, this, propertyName, slot);
}

ValueImp *NumberObjectImp::getValueProperty(ExecState *, int token) const
{
  // ECMA 15.7.3
  switch(token) {
  case NaNValue:
    return jsNaN();
  case NegInfinity:
    return jsNumber(-Inf);
  case PosInfinity:
    return jsNumber(Inf);
  case MaxValue:
    return jsNumber(1.7976931348623157E+308);
  case MinValue:
    return jsNumber(5E-324);
  }
  return jsNull();
}

bool NumberObjectImp::implementsConstruct() const
{
  return true;
}


// ECMA 15.7.1
ObjectImp *NumberObjectImp::construct(ExecState *exec, const List &args)
{
  ObjectImp *proto = exec->lexicalInterpreter()->builtinNumberPrototype();
  ObjectImp *obj(new NumberInstanceImp(proto));

  double n;
  if (args.isEmpty())
    n = 0;
  else
    n = args[0]->toNumber(exec);

  obj->setInternalValue(jsNumber(n));

  return obj;
}

bool NumberObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.7.2
ValueImp *NumberObjectImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  if (args.isEmpty())
    return jsNumber(0);
  else
    return jsNumber(args[0]->toNumber(exec));
}
