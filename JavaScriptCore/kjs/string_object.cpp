// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "regexp.h"
#include "regexp_object.h"
#include "string_object.h"
#include "error_object.h"
#include <stdio.h>
#include "string_object.lut.h"

using namespace KJS;

// ------------------------------ StringInstanceImp ----------------------------

const ClassInfo StringInstanceImp::info = {"String", 0, 0, 0};

StringInstanceImp::StringInstanceImp(const Object &proto)
  : ObjectImp(proto)
{
  setInternalValue(String(""));
}

// ------------------------------ StringPrototypeImp ---------------------------
const ClassInfo StringPrototypeImp::info = {"String", &StringInstanceImp::info, &stringTable, 0};
/* Source for string_object.lut.h
@begin stringTable 26
  toString		StringProtoFuncImp::ToString	DontEnum|Function	0
  valueOf		StringProtoFuncImp::ValueOf	DontEnum|Function	0
  charAt		StringProtoFuncImp::CharAt	DontEnum|Function	1
  charCodeAt		StringProtoFuncImp::CharCodeAt	DontEnum|Function	1
  concat		StringProtoFuncImp::Concat	DontEnum|Function	1
  indexOf		StringProtoFuncImp::IndexOf	DontEnum|Function	2
  lastIndexOf		StringProtoFuncImp::LastIndexOf	DontEnum|Function	2
  match			StringProtoFuncImp::Match	DontEnum|Function	1
  replace		StringProtoFuncImp::Replace	DontEnum|Function	2
  search		StringProtoFuncImp::Search	DontEnum|Function	1
  slice			StringProtoFuncImp::Slice	DontEnum|Function	2
  split			StringProtoFuncImp::Split	DontEnum|Function	2
  substr		StringProtoFuncImp::Substr	DontEnum|Function	2
  substring		StringProtoFuncImp::Substring	DontEnum|Function	2
  toLowerCase		StringProtoFuncImp::ToLowerCase	DontEnum|Function	0
  toUpperCase		StringProtoFuncImp::ToUpperCase	DontEnum|Function	0
#
# Under here: html extension, should only exist if KJS_PURE_ECMA is not defined
# I guess we need to generate two hashtables in the .lut.h file, and use #ifdef
# to select the right one... TODO. #####
  big			StringProtoFuncImp::Big		DontEnum|Function	0
  small			StringProtoFuncImp::Small	DontEnum|Function	0
  blink			StringProtoFuncImp::Blink	DontEnum|Function	0
  bold			StringProtoFuncImp::Bold	DontEnum|Function	0
  fixed			StringProtoFuncImp::Fixed	DontEnum|Function	0
  italics		StringProtoFuncImp::Italics	DontEnum|Function	0
  strike		StringProtoFuncImp::Strike	DontEnum|Function	0
  sub			StringProtoFuncImp::Sub		DontEnum|Function	0
  sup			StringProtoFuncImp::Sup		DontEnum|Function	0
  fontcolor		StringProtoFuncImp::Fontcolor	DontEnum|Function	1
  fontsize		StringProtoFuncImp::Fontsize	DontEnum|Function	1
  anchor		StringProtoFuncImp::Anchor	DontEnum|Function	1
  link			StringProtoFuncImp::Link	DontEnum|Function	1
@end
*/
// ECMA 15.5.4
StringPrototypeImp::StringPrototypeImp(ExecState *exec,
                                       ObjectPrototypeImp *objProto)
  : StringInstanceImp(Object(objProto))
{
  Value protect(this);
  // The constructor will be added later, after StringObjectImp has been built
  put(exec,"length",Number(0),DontDelete|ReadOnly|DontEnum);

}

Value StringPrototypeImp::get(ExecState *exec, const UString &propertyName) const
{
  return lookupGetFunction<StringProtoFuncImp, StringInstanceImp>( exec, propertyName, &stringTable, this );
}

// ------------------------------ StringProtoFuncImp ---------------------------

StringProtoFuncImp::StringProtoFuncImp(ExecState *exec, int i, int len)
  : InternalFunctionImp(
    static_cast<FunctionPrototypeImp*>(exec->interpreter()->builtinFunctionPrototype().imp())
    ), id(i)
{
  Value protect(this);
  put(exec,"length",Number(len),DontDelete|ReadOnly|DontEnum);
}

bool StringProtoFuncImp::implementsCall() const
{
  return true;
}

// ECMA 15.5.4.2 - 15.5.4.20
Value StringProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &args)
{
  Value result;

  // toString and valueOf are no generic function.
  if (id == ToString || id == ValueOf) {
    if (thisObj.isNull() || !thisObj.inherits(&StringInstanceImp::info)) {
      Object err = Error::create(exec,TypeError);
      exec->setException(err);
      return err;
    }

    return String(thisObj.internalValue().toString(exec));
  }

  int n, m;
  UString u, u2, u3;
  int pos, p0, i;
  double d = 0.0;

  UString s = thisObj.toString(exec);

  int len = s.size();
  Value a0 = args[0];
  Value a1 = args[1];

  switch (id) {
  case ToString:
  case ValueOf:
    // handled above
    break;
  case CharAt:
    pos = a0.toInteger(exec);
    if (pos < 0 || pos >= len)
      u = "";
    else
      u = s.substr(pos, 1);
    result = String(u);
    break;
  case CharCodeAt:
    pos = a0.toInteger(exec);
    if (pos < 0 || pos >= len)
      d = NaN;
    else {
      UChar c = s[pos];
      d = (c.high() << 8) + c.low();
    }
    result = Number(d);
    break;
  case Concat: {
    ListIterator it = args.begin();
    for ( ; it != args.end() ; ++it) {
        s += it->toString(exec);
    }
    result = String(s);
    break;
  }
  case IndexOf:
    u2 = a0.toString(exec);
    if (a1.type() == UndefinedType)
      pos = 0;
    else
      pos = a1.toInteger(exec);
    d = s.find(u2, pos);
    result = Number(d);
    break;
  case LastIndexOf:
    u2 = a0.toString(exec);
    d = a1.toNumber(exec);
    if (a1.type() == UndefinedType || KJS::isNaN(d) || KJS::isPosInf(d))
      pos = len;
    else
      pos = a1.toInteger(exec);
    if (pos < 0)
      pos = 0;
    d = s.rfind(u2, pos);
    result = Number(d);
    break;
  case Match:
  case Search: {
    u = s;
    RegExp *reg, *tmpReg = 0;
    RegExpImp *imp = 0;
    if (a0.isA(ObjectType) && a0.toObject(exec).inherits(&RegExpImp::info))
    {
      imp = static_cast<RegExpImp *>( a0.toObject(exec).imp() );
      reg = imp->regExp();
    }
    else
    { /*
       *  ECMA 15.5.4.12 String.prototype.search (regexp)
       *  If regexp is not an object whose [[Class]] property is "RegExp", it is
       *  replaced with the result of the expression new RegExp(regexp).
       */
      reg = tmpReg = new RegExp(a0.toString(exec), RegExp::None);
    }
    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->interpreter()->builtinRegExp().imp());
    int **ovector = regExpObj->registerRegexp(reg, u);
    UString mstr = reg->match(u, -1, &pos, ovector);
    if (id == Search) {
      result = Number(pos);
    } else {
      // Exec
      if ((reg->flags() & RegExp::Global) == 0) {
	// case without 'g' flag is handled like RegExp.prototype.exec
	if (mstr.isNull())
	  return Null(); // no match
	regExpObj->setSubPatterns(reg->subPatterns());
	result = regExpObj->arrayOfMatches(exec,mstr);
      } else {
	// return array of matches
	List list;
	int lastIndex = 0;
	while (pos >= 0) {
	  list.append(String(mstr));
	  lastIndex = pos;
	  pos += mstr.isEmpty() ? 1 : mstr.size();
	  delete [] *ovector;
	  mstr = reg->match(u, pos, &pos, ovector);
	}
	if (imp)
	  imp->put(exec, "lastIndex", Number(lastIndex), DontDelete|DontEnum);
	result = exec->interpreter()->builtinArray().construct(exec, list);
      }
    }
    delete tmpReg;
    break;
  }
  case Replace:
    u = s;
    if (a0.type() == ObjectType && a0.toObject(exec).inherits(&RegExpImp::info)) {
      RegExpImp* imp = static_cast<RegExpImp *>( a0.toObject(exec).imp() );
      RegExp *reg = imp->regExp();
      bool global = false;
      Value tmp = imp->get(exec,"global");
      if (tmp.type() != UndefinedType && tmp.toBoolean(exec) == true)
        global = true;

      RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->interpreter()->builtinRegExp().imp());
      int lastIndex = 0;
      u3 = a1.toString(exec); // replacement string
      // This is either a loop (if global is set) or a one-way (if not).
      do {
        int **ovector = regExpObj->registerRegexp( reg, u );
        UString mstr = reg->match(u, lastIndex, &pos, ovector);
        regExpObj->setSubPatterns(reg->subPatterns());
        if (pos == -1)
          break;
        len = mstr.size();
        // special case of empty match
        if (len == 0 && lastIndex > 0) {
          pos = lastIndex + 1;
          if (pos > u.size())
            break;
        }
        UString rstr(u3);
        bool ok;
        // check if u3 matches $1 or $2 etc
        for (int i = 0; (i = rstr.find(UString("$"), i)) != -1; i++) {
          if (i+1<rstr.size() && rstr[i+1] == '$') {  // "$$" -> "$"
            rstr = rstr.substr(0,i) + "$" + rstr.substr(i+2);
            continue;
          }
          // Assume number part is one char exactly
          unsigned long pos = rstr.substr(i+1,1).toULong(&ok);
          if (ok && pos <= (unsigned)reg->subPatterns()) {
            rstr = rstr.substr(0,i)
                      + u.substr((*ovector)[2*pos],
                                     (*ovector)[2*pos+1]-(*ovector)[2*pos])
                      + rstr.substr(i+2);
            i += (*ovector)[2*pos+1]-(*ovector)[2*pos] - 1; // -1 offsets i++
          }
        }
        lastIndex = pos + rstr.size();
        u = u.substr(0, pos) + rstr + u.substr(pos + len);
        //fprintf(stderr,"pos=%d,len=%d,lastIndex=%d,u=%s\n",pos,len,lastIndex,u.ascii());
      } while (global);

      result = String(u);
    } else { // First arg is a string
      u2 = a0.toString(exec);
      pos = u.find(u2);
      len = u2.size();
      // Do the replacement
      if (pos == -1)
        result = String(s);
      else {
        u3 = u.substr(0, pos) + a1.toString(exec) +
             u.substr(pos + len);
        result = String(u3);
      }
    }
    break;
  case Slice: // http://developer.netscape.com/docs/manuals/js/client/jsref/string.htm#1194366
    {
        // The arg processing is very much like ArrayProtoFunc::Slice
        // We return a new array
        result = exec->interpreter()->builtinArray().construct(exec,List::empty());
        int begin = args[0].toUInt32(exec);
        int end = len;
        if (args[1].type() != UndefinedType)
        {
          end = args[1].toUInt32(exec);
          if ( end < 0 )
            end += len;
        }
        // safety tests
        if ( begin < 0 || end < 0 || begin >= end ) {
            result = String();
            break;
        }
        //printf( "Slicing from %d to %d \n", begin, end );
        result = String(s.substr(begin, end-begin));
        break;
    }
    case Split: {
    Object constructor = exec->interpreter()->builtinArray();
    Object res = Object::dynamicCast(constructor.construct(exec,List::empty()));
    result = res;
    u = s;
    i = p0 = 0;
    d = (a1.type() != UndefinedType) ? a1.toInteger(exec) : -1; // optional max number
    if (a0.type() == ObjectType && Object::dynamicCast(a0).inherits(&RegExpImp::info)) {
      Object obj0 = Object::dynamicCast(a0);
      RegExp reg(obj0.get(exec,"source").toString(exec));
      if (u.isEmpty() && !reg.match(u, 0).isNull()) {
	// empty string matched by regexp -> empty array
	res.put(exec,"length", Number(0));
	break;
      }
      pos = 0;
      while (pos < u.size()) {
	// TODO: back references
        int mpos;
        int *ovector = 0L;
	UString mstr = reg.match(u, pos, &mpos, &ovector);
        delete [] ovector; ovector = 0L;
	if (mpos < 0)
	  break;
	pos = mpos + (mstr.isEmpty() ? 1 : mstr.size());
	if (mpos != p0 || !mstr.isEmpty()) {
	  res.put(exec,UString::from(i), String(u.substr(p0, mpos-p0)));
	  p0 = mpos + mstr.size();
	  i++;
	}
      }
    } else if (a0.type() != UndefinedType) {
      u2 = a0.toString(exec);
      if (u2.isEmpty()) {
	if (u.isEmpty()) {
	  // empty separator matches empty string -> empty array
	  put(exec,"length", Number(0));
	  break;
	} else {
	  while (i != d && i < u.size()-1)
	    res.put(exec,UString::from(i++), String(u.substr(p0++, 1)));
	}
      } else {
	while (i != d && (pos = u.find(u2, p0)) >= 0) {
	  res.put(exec,UString::from(i), String(u.substr(p0, pos-p0)));
	  p0 = pos + u2.size();
	  i++;
	}
      }
    }
    // add remaining string, if any
    if (i != d)
      res.put(exec,UString::from(i++), String(u.substr(p0)));
    res.put(exec,"length", Number(i));
    }
    break;
  case Substr: {
    n = a0.toInteger(exec);
    m = a1.toInteger(exec);
    int d, d2;
    if (n >= 0)
      d = n;
    else
      d = maxInt(len + n, 0);
    if (a1.type() == UndefinedType)
      d2 = len - d;
    else
      d2 = minInt(maxInt(m, 0), len - d);
    result = String(s.substr(d, d2));
    break;
  }
  case Substring: {
    double start = a0.toNumber(exec);
    double end = a1.toNumber(exec);
    if (KJS::isNaN(start))
      start = 0;
    if (KJS::isNaN(end))
      end = 0;
    if (start < 0)
      start = 0;
    if (end < 0)
      end = 0;
    if (start > len)
      start = len;
    if (end > len)
      end = len;
    if (a1.type() == UndefinedType)
      end = len;
    if (start > end) {
      double temp = end;
      end = start;
      start = temp;
    }
    result = String(s.substr((int)start, (int)end-(int)start));
    }
    break;
  case ToLowerCase:
    u = s;
    for (i = 0; i < len; i++)
      u[i] = u[i].toLower();
    result = String(u);
    break;
  case ToUpperCase:
    u = s;
    for (i = 0; i < len; i++)
      u[i] = u[i].toUpper();
    result = String(u);
    break;
#ifndef KJS_PURE_ECMA
  case Big:
    result = String("<BIG>" + s + "</BIG>");
    break;
  case Small:
    result = String("<SMALL>" + s + "</SMALL>");
    break;
  case Blink:
    result = String("<BLINK>" + s + "</BLINK>");
    break;
  case Bold:
    result = String("<B>" + s + "</B>");
    break;
  case Fixed:
    result = String("<TT>" + s + "</TT>");
    break;
  case Italics:
    result = String("<I>" + s + "</I>");
    break;
  case Strike:
    result = String("<STRIKE>" + s + "</STRIKE>");
    break;
  case Sub:
    result = String("<SUB>" + s + "</SUB>");
    break;
  case Sup:
    result = String("<SUP>" + s + "</SUP>");
    break;
  case Fontcolor:
    result = String("<FONT COLOR=" + a0.toString(exec) + ">"
		    + s + "</FONT>");
    break;
  case Fontsize:
    result = String("<FONT SIZE=" + a0.toString(exec) + ">"
		    + s + "</FONT>");
    break;
  case Anchor:
    result = String("<a name=" + a0.toString(exec) + ">"
		    + s + "</a>");
    break;
  case Link:
    result = String("<a href=" + a0.toString(exec) + ">"
		    + s + "</a>");
    break;
#endif
  }

  return result;
}

// ------------------------------ StringObjectImp ------------------------------

StringObjectImp::StringObjectImp(ExecState *exec,
                                 FunctionPrototypeImp *funcProto,
                                 StringPrototypeImp *stringProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  // ECMA 15.5.3.1 String.prototype
  put(exec,"prototype", Object(stringProto), DontEnum|DontDelete|ReadOnly);

  put(exec,"fromCharCode", Object(new StringObjectFuncImp(exec,funcProto)), DontEnum);

  // no. of arguments for constructor
  put(exec,"length", Number(1), ReadOnly|DontDelete|DontEnum);
}


bool StringObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.5.2
Object StringObjectImp::construct(ExecState *exec, const List &args)
{
  Object proto = exec->interpreter()->builtinStringPrototype();
  Object obj(new StringInstanceImp(proto ));

  UString s;
  if (args.size() > 0)
    s = args.begin()->toString(exec);
  else
    s = UString("");

  obj.setInternalValue(String(s));
  obj.put(exec, "length", Number(s.size()), ReadOnly|DontEnum|DontDelete);

  return obj;
}

bool StringObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.5.1
Value StringObjectImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  if (args.isEmpty())
    return String("");
  else {
    Value v = args[0];
    return String(v.toString(exec));
  }
}

// ------------------------------ StringObjectFuncImp --------------------------

// ECMA 15.5.3.2 fromCharCode()
StringObjectFuncImp::StringObjectFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto)
  : InternalFunctionImp(funcProto)
{
  Value protect(this);
  put(exec,"length",Number(1),DontDelete|ReadOnly|DontEnum);
}

bool StringObjectFuncImp::implementsCall() const
{
  return true;
}

Value StringObjectFuncImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  UString s;
  if (args.size()) {
    UChar *buf = new UChar[args.size()];
    UChar *p = buf;
    ListIterator it = args.begin();
    while (it != args.end()) {
      unsigned short u = it->toUInt16(exec);
      *p++ = UChar(u);
      it++;
    }
    s = UString(buf, args.size(), false);
  } else
    s = "";

  return String(s);
}
