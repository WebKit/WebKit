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

#include <stdio.h>

#include "kjs.h"
#include "operations.h"
#include "types.h"
#include "internal.h"
#include "regexp.h"
#include "regexp_object.h"

using namespace KJS;

RegExpObject::RegExpObject(const Object& funcProto, const Object &regProto)
    : ConstructorImp(funcProto, 2)
{
  // ECMA 15.10.5.1 RegExp.prototype
  setPrototypeProperty(regProto);
}

// ECMA 15.9.2
Completion RegExpObject::execute(const List &)
{
  return Completion(ReturnValue, Undefined());
}

// ECMA 15.9.3
Object RegExpObject::construct(const List &args)
{
  /* TODO: regexp arguments */
  String p = args[0].toString();
  String f = args[1].toString();
  UString flags = f.value();

  RegExpImp *dat = new RegExpImp();
  Object obj(dat); // protect from GC

  bool global = (flags.find("g") >= 0);
  bool ignoreCase = (flags.find("i") >= 0);
  bool multiline = (flags.find("m") >= 0);
  /* TODO: throw an error on invalid flags */

  dat->put("global", Boolean(global));
  dat->put("ignoreCase", Boolean(ignoreCase));
  dat->put("multiline", Boolean(multiline));

  dat->put("source", String(p.value()));
  dat->put("lastIndex", 0, DontDelete | DontEnum);

  dat->setRegExp(new RegExp(p.value() /* TODO flags */));
  obj.setClass(RegExpClass);
  obj.setPrototype(Global::current().get("[[RegExp.prototype]]"));

  return obj;
}

// ECMA 15.9.4
RegExpPrototype::RegExpPrototype(const Object& proto)
  : ObjectImp(RegExpClass, String(""), proto)
{
  // The constructor will be added later in RegExpObject's constructor
}

KJSO RegExpPrototype::get(const UString &p) const
{
  int id = -1;
  if (p == "exec")
    id = RegExpProtoFunc::Exec;
  else if (p == "test")
    id = RegExpProtoFunc::Test;
  else if (p == "toString")
    id = RegExpProtoFunc::ToString;

  if (id >= 0)
    return Function(new RegExpProtoFunc(id));
  else
    return Imp::get(p);
}

Completion RegExpProtoFunc::execute(const List &args)
{
  KJSO result;

  Object thisObj = Object::dynamicCast(thisValue());

  if (thisObj.getClass() != RegExpClass) {
    result = Error::create(TypeError);
    return Completion(ReturnValue, result);
  }

  RegExp *re = static_cast<RegExpImp*>(thisObj.imp())->regExp();
  String s;
  KJSO lastIndex, tmp;
  UString str;
  int length, i;
  switch (id) {
  case Exec:
  case Test:
    s = args[0].toString();
    length = s.value().size();
    lastIndex = thisObj.get("lastIndex");
    i = lastIndex.toInt32();
    tmp = thisObj.get("global");
    if (tmp.toBoolean().value() == false)
      i = 0;
    if (i < 0 || i > length) {
      thisObj.put("lastIndex", 0);
      result = Null();
      break;
    }
    str = re->match(s.value(), i);
    if (id == Test) {
      result = Boolean(str.size() != 0);
      break;
    }
    /* TODO complete */
    result = String(str);
    break;
  case ToString:
    s = thisObj.get("source").toString();
    str = "/";
    str += s.value();
    str += "/";
    result = String(str);
    break;
  }

  return Completion(ReturnValue, result);
}
