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

#ifndef _BOOL_OBJECT_H_
#define _BOOL_OBJECT_H_

#include "internal.h"
#include "function_object.h"

namespace KJS {

  class BooleanInstanceImp : public ObjectImp {
  public:
    BooleanInstanceImp(const Object &proto);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * The initial value of Boolean.prototype (and thus all objects created
   * with the Boolean constructor
   */
  class BooleanPrototypeImp : public BooleanInstanceImp {
  public:
    BooleanPrototypeImp(ExecState *exec,
                        ObjectPrototypeImp *objectProto,
                        FunctionPrototypeImp *funcProto);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Boolean.prototype object
   */
  class BooleanProtoFuncImp : public InternalFunctionImp {
  public:
    BooleanProtoFuncImp(ExecState *exec,
                        FunctionPrototypeImp *funcProto, int i, int len);

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);

    enum { ToString, ValueOf };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "Boolean" property
   */
  class BooleanObjectImp : public InternalFunctionImp {
    friend class BooleanProtoFuncImp;
  public:
    BooleanObjectImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                     BooleanPrototypeImp *booleanProto);

    virtual bool implementsConstruct() const;
    virtual Object construct(ExecState *exec, const List &args);

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);
  };

}; // namespace

#endif
