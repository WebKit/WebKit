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
 */

#include "kjs.h"
#include "operations.h"
#include "types.h"
#include "regexp.h"
#include "string_object.h"
#include <stdio.h>

using namespace KJS;

StringObject::StringObject(const Object &funcProto, const Object &stringProto)
  : ConstructorImp(funcProto, 1)
{
  // ECMA 15.5.3.1 String.prototype
  setPrototypeProperty(stringProto);
}

KJSO StringObject::get(const UString &p) const
{
  if (p == "fromCharCode")
    return Function(new StringObjectFunc());
  else
    return Imp::get(p);
}

// ECMA 15.5.1
Completion StringObject::execute(const List &args)
{
  KJSO v;
  String s;

  if (args.isEmpty())
    s = String("");
  else {
    v = args[0];
    s = v.toString();
  }

  return Completion(ReturnValue, s);
}

// ECMA 15.5.2
Object StringObject::construct(const List &args)
{
  String s;
  if (args.size() > 0)
    s = args.begin()->toString();
  else
    s = String("");

  return Object::create(StringClass, s);
}

// ECMA 15.5.3.2 fromCharCode()
Completion StringObjectFunc::execute(const List &args)
{
  UString s;
  if (args.size()) {
    UChar *buf = new UChar[args.size()];
    UChar *p = buf;
    ListIterator it = args.begin();
    while (it != args.end()) {
      unsigned short u = it->toUInt16();
      *p++ = UChar(u);
      it++;
    }
    s = UString(buf, args.size(), false);
  } else
    s = "";

  return Completion(ReturnValue, String(s));
}

// ECMA 15.5.4
StringPrototype::StringPrototype(const Object& proto)
  : ObjectImp(StringClass, String(""), proto)
{
  // The constructor will be added later in StringObject's constructor
}

KJSO StringPrototype::get(const UString &p) const
{
  int id;

  if (p == "toString")
    id = StringProtoFunc::ToString;
  else if (p == "valueOf")
    id = StringProtoFunc::ValueOf;
  else if (p == "charAt")
    id = StringProtoFunc::CharAt;
  else if (p == "charCodeAt")
    id = StringProtoFunc::CharCodeAt;
  else if (p == "indexOf")
    id = StringProtoFunc::IndexOf;
  else if (p == "lastIndexOf")
    id = StringProtoFunc::LastIndexOf;
  else if (p == "match")
    id = StringProtoFunc::Match;
  else if (p == "replace")
    id = StringProtoFunc::Replace;
  else if (p == "search")
    id = StringProtoFunc::Search;
  else if (p == "split")
    id = StringProtoFunc::Split;
  else if (p == "substr")
    id = StringProtoFunc::Substr;
  else if (p == "substring")
    id = StringProtoFunc::Substring;
  else if (p == "toLowerCase")
    id = StringProtoFunc::ToLowerCase;
  else if (p == "toUpperCase")
    id = StringProtoFunc::ToUpperCase;
#ifndef KJS_PURE_ECMA
  else if (p == "big")
    id = StringProtoFunc::Big;
  else if (p == "small")
    id = StringProtoFunc::Small;
  else if (p == "blink")
    id = StringProtoFunc::Blink;
  else if (p == "bold")
    id = StringProtoFunc::Bold;
  else if (p == "fixed")
    id = StringProtoFunc::Fixed;
  else if (p == "italics")
    id = StringProtoFunc::Italics;
  else if (p == "strike")
    id = StringProtoFunc::Strike;
  else if (p == "sub")
    id = StringProtoFunc::Sub;
  else if (p == "sup")
    id = StringProtoFunc::Sup;
  else if (p == "fontcolor")
    id = StringProtoFunc::Fontcolor;
  else if (p == "fontsize")
    id = StringProtoFunc::Fontsize;
  else if (p == "anchor")
    id = StringProtoFunc::Anchor;
  else if (p == "link")
    id = StringProtoFunc::Link;
#endif
  else
    return Imp::get(p);

  return Function(new StringProtoFunc(id));
}

StringProtoFunc::StringProtoFunc(int i)
  : id(i)
{
}

// ECMA 15.5.4.2 - 15.5.4.20
Completion StringProtoFunc::execute(const List &args)
{
  KJSO result;

  Object thisObj = Object::dynamicCast(thisValue());

  // toString and valueOf are no generic function.
  if (id == ToString || id == ValueOf) {
    if (thisObj.isNull() || thisObj.getClass() != StringClass) {
      result = Error::create(TypeError);
      return Completion(ReturnValue, result);
    }
  }

  String s2;
  Number n, m;
  UString u, u2, u3;
  int pos, p0, i;
  double d, d2;
  KJSO v = thisObj.internalValue();
  String s = v.toString();
  int len = s.value().size();
  KJSO a0 = args[0];
  KJSO a1 = args[1];

  switch (id) {
  case ToString:
  case ValueOf:
    result = v.toString();
    break;
  case CharAt:
    n = a0.toInteger();
    pos = (int) n.value();
    if (pos < 0 || pos >= len)
      u = "";
    else
      u = s.value().substr(pos, 1);
    result = String(u);
    break;
  case CharCodeAt:
    n = a0.toInteger();
    pos = (int) n.value();
    if (pos < 0 || pos >= len)
      d = NaN;
    else {
      UChar c = s.value()[pos];
      d = (c.high() << 8) + c.low();
    }
    result = Number(d);
    break;
  case IndexOf:
    s2 = a0.toString();
    if (a1.isA(UndefinedType))
      pos = 0;
    else
      pos = a1.toInteger().intValue();
    d = s.value().find(s2.value(), pos);
    result = Number(d);
    break;
  case LastIndexOf:
    s2 = a0.toString();
    if (a1.isA(UndefinedType))
      pos = len;
    else
      pos = a1.toInteger().intValue();
    d = s.value().rfind(s2.value(), pos);
    result = Number(d);
    break;
  case Match:
  case Search:
    u = s.value();
    if (a0.isA(ObjectType) && a0.toObject().getClass() == RegExpClass) {
      s2 = a0.get("source").toString();
      RegExp reg(s2.value());
      UString mstr = reg.match(u, -1, &pos);
      if (id == Search) {
        result = Number(pos);
        break;
      }
      if (mstr.isNull()) {
	result = Null();
	break;
      }
      /* TODO return an array, with the matches, etc. */
      result = String(mstr);
    } else
    {
      printf("Match/Search. Argument is not a RegExp - returning Undefined\n");
      result = Undefined(); // No idea what to do here
    }
    break;
  case Replace:
    /* TODO: this is just a hack to get the most common cases going */
    u = s.value();
    if (a0.isA(ObjectType) && a0.toObject().getClass() == RegExpClass) {
      s2 = a0.get("source").toString();
      RegExp reg(s2.value());
      UString mstr = reg.match(u, -1, &pos);
      len = mstr.size();
    } else {
      s2 = a0.toString();
      u2 = s2.value();
      pos = u.find(u2);
      len = u2.size();
    }
    if (pos == -1)
	result = s;
    else {
	u3 = u.substr(0, pos) + a1.toString().value() +
	     u.substr(pos + len);
	result = String(u3);
    }
    break;
  case Split:
    result = Object::create(ArrayClass);
    u = s.value();
    i = p0 = 0;
    d = a1.isDefined() ? a1.toInteger().intValue() : -1; // optional max number
    if (a0.isA(ObjectType) && Object(a0.imp()).getClass() == RegExpClass) {
      RegExp reg(a0.get("source").toString().value());
      if (u.isEmpty() && !reg.match(u, 0).isNull()) {
	// empty string matched by regexp -> empty array
	result.put("length", 0);
	break;
      }
      int mpos;
      pos = 0;
      while (1) {
	/* TODO: back references */
	UString mstr = reg.match(u, pos, &mpos);
	if (mpos < 0)
	  break;
	pos = mpos + (mstr.isEmpty() ? 1 : mstr.size());
	if (mpos != p0 || !mstr.isEmpty()) {
	  result.put(UString::from(i), String(u.substr(p0, mpos-p0)));
	  p0 = mpos + mstr.size();
	  i++;
	}
      }
    } else if (a0.isDefined()) {
      u2 = a0.toString().value();
      if (u2.isEmpty()) {
	if (u.isEmpty()) {
	  // empty separator matches empty string -> empty array
	  put("length", 0);
	  break;
	} else {
	  while (i != d && i < u.size())
	    result.put(UString::from(i++), String(u.substr(p0++, 1)));
	}
      } else {
	while (i != d && (pos = u.find(u2, p0)) >= 0) {
	  result.put(UString::from(i), String(u.substr(p0, pos-p0)));
	  p0 = pos + u2.size();
	  i++;
	}
      }
    }
    // add remaining string, if any
    if (i != d && (p0 < len || i == 0))
      result.put(UString::from(i++), String(u.substr(p0)));
    result.put("length", i);
    break;
  case Substr:
    n = a0.toInteger();
    m = a1.toInteger();
    if (n.value() >= 0)
      d = n.value();
    else
      d = max(len + n.value(), 0);
    if (a1.isA(UndefinedType))
      d2 = len - d;
    else
      d2 = min(max(m.value(), 0), len - d);
    result = String(s.value().substr((int)d, (int)d2));
    break;
  case Substring:
    n = a0.toInteger();
    m = a1.toInteger();
    d = min(max(n.value(), 0), len);
    if (a1.isA(UndefinedType))
      d2 = len - d;
    else {
      d2 = min(max(m.value(), 0), len);
      d2 = max(d2-d, 0);
    }
    result = String(s.value().substr((int)d, (int)d2));
    break;
  case ToLowerCase:
    u = UString(s.value());
    for (i = 0; i < len; i++)
      u[i] = u[i].toLower();
    result = String(u);
    break;
  case ToUpperCase:
    u = UString(s.value());
    for (i = 0; i < len; i++)
      u[i] = u[i].toUpper();
    result = String(u);
    break;
#ifndef KJS_PURE_ECMA
  case Big:
    result = String("<BIG>" + s.value() + "</BIG>");
    break;
  case Small:
    result = String("<SMALL>" + s.value() + "</SMALL>");
    break;
  case Blink:
    result = String("<BLINK>" + s.value() + "</BLINK>");
    break;
  case Bold:
    result = String("<B>" + s.value() + "</B>");
    break;
  case Fixed:
    result = String("<TT>" + s.value() + "</TT>");
    break;
  case Italics:
    result = String("<I>" + s.value() + "</I>");
    break;
  case Strike:
    result = String("<STRIKE>" + s.value() + "</STRIKE>");
    break;
  case Sub:
    result = String("<SUB>" + s.value() + "</SUB>");
    break;
  case Sup:
    result = String("<SUP>" + s.value() + "</SUP>");
    break;
  case Fontcolor:
    result = String("<FONT COLOR=" + a0.toString().value() + ">"
		    + s.value() + "</FONT>");
    break;
  case Fontsize:
    result = String("<FONT SIZE=" + a0.toString().value() + ">"
		    + s.value() + "</FONT>");
    break;
  case Anchor:
    result = String("<a name=" + a0.toString().value() + ">"
		    + s.value() + "</a>");
    break;
  case Link:
    result = String("<a href=" + a0.toString().value() + ">"
		    + s.value() + "</a>");
    break;
#endif
  }

  return Completion(ReturnValue, result);
}
