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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */

#ifndef _STRING_OBJECT_H_
#define _STRING_OBJECT_H_

#include "internal.h"
#include "function_object.h"

namespace KJS {

  class StringInstanceImp : public ObjectImp {
  public:
    StringInstanceImp(const Object &proto);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * The initial value of String.prototype (and thus all objects created
   * with the String constructor
   */
  class StringPrototypeImp : public StringInstanceImp {
  public:
    StringPrototypeImp(ExecState *exec,
                       ObjectPrototypeImp *objProto);
    Value get(ExecState *exec, const UString &p) const;
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * String.prototype object
   */
  class StringProtoFuncImp : public InternalFunctionImp {
  public:
    StringProtoFuncImp(ExecState *exec, int i, int len);

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);

    enum { ToString, ValueOf, CharAt, CharCodeAt, Concat, IndexOf, LastIndexOf,
	   Match, Replace, Search, Slice, Split,
	   Substr, Substring, FromCharCode, ToLowerCase, ToUpperCase
#ifndef KJS_PURE_ECMA
	   , Big, Small, Blink, Bold, Fixed, Italics, Strike, Sub, Sup,
	   Fontcolor, Fontsize, Anchor, Link
#endif
    };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "String" property
   */
  class StringObjectImp : public InternalFunctionImp {
  public:
    StringObjectImp(ExecState *exec,
                    FunctionPrototypeImp *funcProto,
                    StringPrototypeImp *stringProto);

    virtual bool implementsConstruct() const;
    virtual Object construct(ExecState *exec, const List &args);
    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * String object
   */
  class StringObjectFuncImp : public InternalFunctionImp {
  public:
    StringObjectFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto);
    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);
  };

}; // namespace

#endif

