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

#ifndef _NUMBER_OBJECT_H_
#define _NUMBER_OBJECT_H_

#include "internal.h"
#include "function_object.h"

namespace KJS {

  class NumberInstanceImp : public ObjectImp {
  public:
    NumberInstanceImp(const Object &proto);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * The initial value of Number.prototype (and thus all objects created
   * with the Number constructor
   */
  class NumberPrototypeImp : public NumberInstanceImp {
  public:
    NumberPrototypeImp(ExecState *exec,
                       ObjectPrototypeImp *objProto,
                       FunctionPrototypeImp *funcProto);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Number.prototype object
   */
  class NumberProtoFuncImp : public InternalFunctionImp {
  public:
    NumberProtoFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                       int i, int len);

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);

    enum { ToString, ToLocaleString, ValueOf };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "Number" property
   */
  class NumberObjectImp : public InternalFunctionImp {
  public:
    NumberObjectImp(ExecState *exec,
                    FunctionPrototypeImp *funcProto,
                    NumberPrototypeImp *numberProto);

    virtual bool implementsConstruct() const;
    virtual Object construct(ExecState *exec, const List &args);

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);

    Value get(ExecState *exec, const UString &p) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    enum { NaNValue, NegInfinity, PosInfinity, MaxValue, MinValue };

    Completion execute(const List &);
    Object construct(const List &);
  };

}; // namespace

#endif
